/*
Selected Windows Capture
Copyright (C) 2026 obs-selected-windows-moncap contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include <windows.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SWC_MAX_MONITORS 16
#define SWC_MAX_WINDOWS 128
#define SWC_STR 1024

enum swc_match_mode {
	SWC_MATCH_CLASS_EXE = 0,
	SWC_MATCH_EXE = 1,
	SWC_MATCH_EXACT = 2,
};

enum swc_window_priority {
	SWC_PRIORITY_CLASS = 0,
	SWC_PRIORITY_TITLE = 1,
	SWC_PRIORITY_EXE = 2,
};

enum swc_filter_mode {
	SWC_MODE_INCLUDE = 0,
	SWC_MODE_EXCLUDE = 1,
};

enum swc_layout {
	SWC_LAYOUT_MASK = 0,
	SWC_LAYOUT_COMPOSITE = 1,
};

enum swc_enum_flags {
	SWC_ENUM_TOOLWINDOWS = 1u << 0,
	SWC_ENUM_PICKER = 1u << 1,
	SWC_ENUM_MINIMIZED = 1u << 2,
};

struct swc_monitor {
	char device_id[128];
	char alt_id[32];
	char name[128];
	char desc[256];
	RECT rect;
	HMONITOR handle;
	bool primary;
};

struct swc_window {
	HWND hwnd;
	RECT rect;
	char title[512];
	char class_name[256];
	char exe[256];
	char encoded[SWC_STR];
	char desc[SWC_STR];
};

int swc_enum_monitors(struct swc_monitor *out, int max);
bool swc_find_monitor(const char *monitor_id, struct swc_monitor *out);
bool swc_active_monitor(struct swc_monitor *out);
int swc_enum_windows(const RECT *monitor_rect, uint32_t flags, struct swc_window *out, int max);

void swc_encode_window(char *dst, size_t dst_sz, const char *title, const char *class_name, const char *exe);
bool swc_parse_encoded(const char *encoded, char *title, size_t title_sz, char *class_name, size_t class_sz, char *exe,
		       size_t exe_sz);
bool swc_window_matches(enum swc_match_mode mode, const struct swc_window *window, const char *encoded);
bool swc_match_by_exe_only(const char *exe, const char *class_name);
bool swc_is_terminal(const char *exe, const char *class_name);
bool swc_is_paint_tool_sai(const char *exe, const char *class_name);
bool swc_needs_wgc_only(const char *exe, const char *class_name);
bool swc_wgc_client_area(const char *exe, const char *class_name);
bool swc_class_is_unstable(const char *class_name);
int swc_window_capture_priority(const char *exe, const char *class_name);
