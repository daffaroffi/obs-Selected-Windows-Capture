/*
Selected Windows Capture
Copyright (C) 2026 obs-selected-windows-moncap contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <plugin-support.h>

#include "swc-filters.h"
#ifdef _WIN32
#include "selected-windows-capture.h"
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Capture selected Windows windows, plus a censor filter (blur, mosaic).";
}

bool obs_module_load(void)
{
	swc_register_filters();
#ifdef _WIN32
	swc_capture_module_load();
	obs_register_source(&selected_windows_capture_info);
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
#else
	obs_log(LOG_INFO, "Blur/mosaic filters loaded; window capture is Windows-only (version %s)", PLUGIN_VERSION);
#endif
	return true;
}

void obs_module_unload(void)
{
#ifdef _WIN32
	swc_capture_module_unload();
#endif
	obs_log(LOG_INFO, "plugin unloaded");
}
