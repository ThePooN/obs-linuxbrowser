/*
Copyright (C) 2017 by Azat Khasanshin <azat.khasanshin@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <obs-module.h>
#include <pthread.h>
#include <stdio.h>

#include "manager.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("linuxbrowser-source", "en-US")

struct browser_data {
	/* settings */
	char *url;
	uint32_t width;
	uint32_t height;
	uint32_t fps;
	char *flash_path;
	char *flash_version;

	/* internal data */
	obs_source_t *source;
	gs_texture_t *activeTexture;
	pthread_mutex_t textureLock;
	browser_manager_t *manager;
};

static const char* browser_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("LinuxBrowser");
}

static void browser_update(void *vptr, obs_data_t *settings)
{
	struct browser_data *data = vptr;

	uint32_t width = obs_data_get_int(settings, "width");
	uint32_t height = obs_data_get_int(settings, "height");
	bool resize = width != data->width || height != data->height;
	data->width = width;
	data->height = height;
	data->fps = obs_data_get_int(settings, "fps");

	bool is_local = obs_data_get_bool(settings, "is_local_file");
	const char *url;
	if (is_local)
		url = obs_data_get_string(settings, "local_file");
	else
		url = obs_data_get_string(settings, "url");

	const char *flash_path = obs_data_get_string(settings, "flash_path");
	const char *flash_version = obs_data_get_string(settings, "flash_version");

	if (!data->manager)
		data->manager = create_browser_manager(data->width, data->height,
			data->fps, flash_path, flash_version);

	if (!data->url || strcmp(url, data->url) != 0) {
		if (data->url)
			bfree(data->url);
		if (is_local) {
			size_t len = strlen("file://") + strlen(url) + 1;
			data->url = bzalloc(len);
			snprintf(data->url, len, "file://%s", url);
		} else {
			data->url = bstrdup(url);
		}
		browser_manager_change_url(data->manager, data->url);
	}
	if (!data->flash_path || strcmp(flash_path, data->flash_path) != 0) {
		if (data->flash_path)
			bfree(data->flash_path);
		data->flash_path = bstrdup(flash_path);
		browser_manager_set_flash(data->manager, data->flash_path, data->flash_version);
	}
	if (!data->flash_version || strcmp(flash_version, data->flash_version) != 0) {
		if (data->flash_version)
			bfree(data->flash_version);
		data->flash_version = bstrdup(flash_version);
		browser_manager_set_flash(data->manager, data->flash_path, data->flash_version);
	}

	pthread_mutex_lock(&data->textureLock);
	obs_enter_graphics();
	if (resize || !data->activeTexture) {
		if (resize)
			browser_manager_change_size(data->manager, data->width, data->height);
		if (data->activeTexture)
			gs_texture_destroy(data->activeTexture);
		data->activeTexture = gs_texture_create(width, height, GS_BGRA, 1, NULL, GS_DYNAMIC);
	}
	obs_leave_graphics();
	pthread_mutex_unlock(&data->textureLock);
}

static void *browser_create(obs_data_t *settings, obs_source_t *source)
{
	struct browser_data *data = bzalloc(sizeof(struct browser_data));
	data->source = source;
	pthread_mutex_init(&data->textureLock, NULL);

	browser_update(data, settings);
	return data;
}

static void browser_destroy(void *vptr)
{
	struct browser_data *data = vptr;
	if (!data)
		return;

	pthread_mutex_destroy(&data->textureLock);
	if (data->activeTexture) {
		obs_enter_graphics();
		gs_texture_destroy(data->activeTexture);
		data->activeTexture = NULL;
		obs_leave_graphics();
	}

	if (data->manager)
		destroy_browser_manager(data->manager);

	if (data->url)
		bfree(data->url);
	if (data->flash_path)
		bfree(data->flash_path);
	if (data->flash_version)
		bfree(data->flash_version);
	bfree(data);
}

static uint32_t browser_get_width(void *vptr)
{
	struct browser_data *data = vptr;
	return data->width;
}

static uint32_t browser_get_height(void *vptr)
{
	struct browser_data *data = vptr;
	return data->height;
}


static bool reload_button_clicked(obs_properties_t *props, obs_property_t *property, void *vptr)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	struct browser_data *data = vptr;
	browser_manager_reload_page(data->manager);
	return true;
}

static bool restart_button_clicked(obs_properties_t *props, obs_property_t *property, void *vptr)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	struct browser_data *data = vptr;
	browser_manager_restart_browser(data->manager);
	browser_manager_change_url(data->manager, data->url);
	return true;
}

static bool is_local_file_modified(obs_properties_t *props, obs_property_t *prop,
		obs_data_t *settings)
{
	UNUSED_PARAMETER(prop);
	
	bool enabled = obs_data_get_bool(settings, "is_local_file");
	obs_property_t *url = obs_properties_get(props, "url");
	obs_property_t *local_file = obs_properties_get(props, "local_file");
	obs_property_set_visible(url, !enabled);
	obs_property_set_visible(local_file, enabled);

	return true;
}

static obs_properties_t *browser_get_properties(void *vptr)
{
	UNUSED_PARAMETER(vptr);
	obs_properties_t *props = obs_properties_create();

	obs_property_t *prop = obs_properties_add_bool(props, "is_local_file",
			obs_module_text("LocalFile"));
	obs_property_set_modified_callback(prop, is_local_file_modified);
	obs_properties_add_path(props, "local_file", obs_module_text("LocalFile"),
			OBS_PATH_FILE, "*.*", NULL);

	obs_properties_add_text(props, "url", obs_module_text("URL"), OBS_TEXT_DEFAULT);
	obs_properties_add_int(props, "width", obs_module_text("Width"), 1, MAX_BROWSER_WIDTH, 1);
	obs_properties_add_int(props, "height", obs_module_text("Height"), 1, MAX_BROWSER_HEIGHT, 1);
	obs_properties_add_int(props, "fps", obs_module_text("FPS"), 1, 60, 1);

	obs_properties_add_button(props, "reload", obs_module_text("ReloadPage"), reload_button_clicked);


	obs_properties_add_path(props, "flash_path", obs_module_text("FlashPath"),
			OBS_PATH_FILE, "*.so", NULL);
	obs_properties_add_text(props, "flash_version", obs_module_text("FlashVersion"), OBS_TEXT_DEFAULT);

	obs_properties_add_button(props, "restart", obs_module_text("RestartBrowser"), restart_button_clicked);

	return props;
}

static void browser_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "url", "http://www.obsproject.com");
	obs_data_set_default_int(settings, "width", 800);
	obs_data_set_default_int(settings, "height", 600);
	obs_data_set_default_int(settings, "fps", 30);
	obs_data_set_default_string(settings, "flash_path", "");
	obs_data_set_default_string(settings, "flash_version", "");
}

static void browser_tick(void *vptr, float seconds)
{
	UNUSED_PARAMETER(seconds);
	struct browser_data *data = vptr;
	pthread_mutex_lock(&data->textureLock);

	if (!data->activeTexture || !obs_source_showing(data->source)) {
		pthread_mutex_unlock(&data->textureLock);
		return;
	}

	lock_browser_manager(data->manager);
	obs_enter_graphics();
	gs_texture_set_image(data->activeTexture, get_browser_manager_data(data->manager),
			data->width * 4, false);
	obs_leave_graphics();
	unlock_browser_manager(data->manager);

	pthread_mutex_unlock(&data->textureLock);
}

static void browser_render(void *vptr, gs_effect_t *effect)
{
	struct browser_data *data = vptr;
	pthread_mutex_lock(&data->textureLock);

	if (!data->activeTexture) {
		pthread_mutex_unlock(&data->textureLock);
		return;
	}

	gs_reset_blend_state();
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), data->activeTexture);
	gs_draw_sprite(data->activeTexture, 0, data->width, data->height);

	pthread_mutex_unlock(&data->textureLock);
}

bool obs_module_load(void)
{
	struct obs_source_info info = {};
	info.id             = "linuxbrowser-source";
	info.type           = OBS_SOURCE_TYPE_INPUT;
	info.output_flags   = OBS_SOURCE_VIDEO;

	info.get_name       = browser_get_name;
	info.create         = browser_create;
	info.destroy        = browser_destroy;
	info.update         = browser_update;
	info.get_width      = browser_get_width;
	info.get_height     = browser_get_height;
	info.get_properties = browser_get_properties;
	info.get_defaults   = browser_get_defaults;
	info.video_tick     = browser_tick;
	info.video_render   = browser_render;

	obs_register_source(&info);
	return true;
}
