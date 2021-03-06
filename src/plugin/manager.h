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

#pragma once

#include <obs-module.h>

#include "shared.h"

#define blog(level, msg, ...) blog(level, "obs-linuxbrowser: " msg, ##__VA_ARGS__)

typedef struct browser_manager {
	int fd;
	int pid;
	int qid;
	char *shmname;
	char *flash_path;
	char *flash_version;
	struct shared_data *data;
} browser_manager_t;

browser_manager_t *create_browser_manager(uint32_t width, uint32_t height, int fps,
		const char *flash_path, const char *flash_version);
void destroy_browser_manager(browser_manager_t *manager);
void lock_browser_manager(browser_manager_t *manager);
void unlock_browser_manager(browser_manager_t *manager);
uint8_t *get_browser_manager_data(browser_manager_t *manager);

void browser_manager_change_url(browser_manager_t *manager, const char *url);
void browser_manager_change_size(browser_manager_t *manager, uint32_t width, uint32_t height);
void browser_manager_reload_page(browser_manager_t *manager);
void browser_manager_restart_browser(browser_manager_t *manager);

void browser_manager_set_flash(browser_manager_t *manager,
		const char *flash_path, const char *flash_version);
