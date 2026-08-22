/*
Selected Windows Capture
Copyright (C) 2026 obs-selected-windows-moncap contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include <obs-module.h>

extern struct obs_source_info selected_windows_capture_info;

void swc_capture_module_load(void);
void swc_capture_module_unload(void);
