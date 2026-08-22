/*
Selected Windows Capture
Copyright (C) 2026 obs-selected-windows-moncap contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "win-enum.h"

#include <dwmapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DWMWA_EXTENDED_FRAME_BOUNDS
#define DWMWA_EXTENDED_FRAME_BOUNDS 9
#endif
#ifndef DWMWA_CLOAKED
#define DWMWA_CLOAKED 14
#endif

static const char *skip_classes[] = {
	"Progman",
	"WorkerW",
	"IME",
	"MSCTFIME UI",
	"Windows.UI.Core.CoreWindow",
	NULL,
};

static const char *internal_exes_exact[] = {
	"startmenuexperiencehost.exe",
	"peopleexperiencehost.exe",
	"shellexperiencehost.exe",
	"textinputhost.exe",
	"searchapp.exe",
	"searchui.exe",
	"lockapp.exe",
	"gamebar.exe",
	"tabtip.exe",
	NULL,
};

static void utf8_from_wide(char *dst, size_t dst_sz, const wchar_t *src)
{
	if (!dst || !dst_sz)
		return;
	dst[0] = '\0';
	if (!src)
		return;
	WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, (int)dst_sz, NULL, NULL);
	dst[dst_sz - 1] = '\0';
}

static void append_encoded(char **dst, size_t *left, const char *src)
{
	if (!src)
		src = "";
	for (; *src && *left > 1; src++) {
		if (*src == '#') {
			if (*left <= 3)
				break;
			memcpy(*dst, "#22", 3);
			*dst += 3;
			*left -= 3;
		} else if (*src == ':') {
			if (*left <= 3)
				break;
			memcpy(*dst, "#3A", 3);
			*dst += 3;
			*left -= 3;
		} else {
			**dst = *src;
			(*dst)++;
			(*left)--;
		}
	}
	**dst = '\0';
}

void swc_encode_window(char *dst, size_t dst_sz, const char *title, const char *class_name, const char *exe)
{
	if (!dst || !dst_sz)
		return;
	dst[0] = '\0';
	size_t left = dst_sz;
	char *p = dst;
	append_encoded(&p, &left, title);
	if (left > 1) {
		*p++ = ':';
		left--;
		*p = '\0';
	}
	append_encoded(&p, &left, class_name);
	if (left > 1) {
		*p++ = ':';
		left--;
		*p = '\0';
	}
	append_encoded(&p, &left, exe);
}

static bool decode_part(const char **src, char *dst, size_t dst_sz, char stop)
{
	if (!dst || !dst_sz)
		return false;
	size_t n = 0;
	const char *s = *src;
	while (*s && *s != stop && n + 1 < dst_sz) {
		if (s[0] == '#' && s[1] == '3' && s[2] == 'A') {
			dst[n++] = ':';
			s += 3;
		} else if (s[0] == '#' && s[1] == '2' && s[2] == '2') {
			dst[n++] = '#';
			s += 3;
		} else {
			dst[n++] = *s++;
		}
	}
	dst[n] = '\0';
	if (stop && *s != stop)
		return false;
	if (stop && *s == stop)
		s++;
	*src = s;
	return true;
}

bool swc_parse_encoded(const char *encoded, char *title, size_t title_sz, char *class_name, size_t class_sz, char *exe,
		       size_t exe_sz)
{
	if (!encoded)
		return false;
	const char *s = encoded;
	if (!decode_part(&s, title, title_sz, ':'))
		return false;
	if (!decode_part(&s, class_name, class_sz, ':'))
		return false;
	if (!decode_part(&s, exe, exe_sz, '\0'))
		return false;
	return true;
}

bool swc_window_matches(enum swc_match_mode mode, const struct swc_window *window, const char *encoded)
{
	char title[512];
	char class_name[256];
	char exe[256];

	if (!window || !encoded || !encoded[0])
		return false;
	if (!swc_parse_encoded(encoded, title, sizeof(title), class_name, sizeof(class_name), exe, sizeof(exe)))
		return false;

	if (mode == SWC_MATCH_EXE)
		return exe[0] && _stricmp(window->exe, exe) == 0;
	if (mode == SWC_MATCH_CLASS_EXE) {
		if (swc_match_by_exe_only(window->exe, window->class_name) ||
		    swc_match_by_exe_only(exe, class_name))
			return exe[0] && _stricmp(window->exe, exe) == 0;
		return exe[0] && class_name[0] && _stricmp(window->exe, exe) == 0 &&
		       strcmp(window->class_name, class_name) == 0;
	}

	return strcmp(window->title, title) == 0 && strcmp(window->class_name, class_name) == 0 &&
	       _stricmp(window->exe, exe) == 0;
}

bool swc_is_paint_tool_sai(const char *exe, const char *class_name)
{
	if (class_name && class_name[0] &&
	    (strcmp(class_name, "sflRootWindow") == 0 || strcmp(class_name, "sflChildWindow") == 0 ||
	     strcmp(class_name, "sflNoOperationWindow") == 0))
		return true;
	if (!exe || !exe[0])
		return false;
	return _stricmp(exe, "sai2.exe") == 0 || _stricmp(exe, "sai.exe") == 0;
}

bool swc_class_is_unstable(const char *class_name)
{
	if (!class_name || !class_name[0])
		return false;
	if (strncmp(class_name, "HwndWrapper[", 12) == 0)
		return true;
	if (strncmp(class_name, "WindowsForms10.", 15) == 0)
		return true;
	if (strncmp(class_name, "WindowsForms11.", 15) == 0)
		return true;
	return false;
}

bool swc_wgc_client_area(const char *exe, const char *class_name)
{
	static const char *exes[] = {"chrome.exe", "msedge.exe", "firefox.exe", "brave.exe",
				     "opera.exe",  "code.exe",   "discord.exe", "slack.exe",
				     "affinity.exe", "affinityphoto.exe", "affinitydesigner.exe",
				     "affinitypublisher.exe", NULL};

	if (swc_class_is_unstable(class_name) || swc_is_paint_tool_sai(exe, class_name))
		return true;
	if (class_name && (strstr(class_name, "Chrome") || strstr(class_name, "Mozilla") ||
			   strstr(class_name, "Electron")))
		return true;
	if (!exe || !exe[0])
		return false;
	for (int i = 0; exes[i]; i++) {
		if (_stricmp(exe, exes[i]) == 0)
			return true;
	}
	return false;
}

bool swc_needs_wgc_only(const char *exe, const char *class_name)
{
	static const char *exes[] = {"chrome.exe",
				     "msedge.exe",
				     "firefox.exe",
				     "brave.exe",
				     "opera.exe",
				     "code.exe",
				     "discord.exe",
				     "slack.exe",
				     "steamwebhelper.exe",
				     "msedgewebview2.exe",
				     NULL};

	if (class_name && (strstr(class_name, "Chrome") || strstr(class_name, "Mozilla") ||
			   strstr(class_name, "Electron")))
		return true;
	if (!exe || !exe[0])
		return false;
	for (int i = 0; exes[i]; i++) {
		if (_stricmp(exe, exes[i]) == 0)
			return true;
	}
	return false;
}

int swc_window_capture_priority(const char *exe, const char *class_name)
{
	if (class_name && strcmp(class_name, "ApplicationFrameWindow") == 0)
		return SWC_PRIORITY_TITLE;
	if (swc_class_is_unstable(class_name))
		return SWC_PRIORITY_EXE;
	if (swc_is_paint_tool_sai(exe, class_name) || swc_is_terminal(exe, class_name))
		return SWC_PRIORITY_CLASS;
	if (swc_match_by_exe_only(exe, class_name))
		return SWC_PRIORITY_EXE;
	return SWC_PRIORITY_CLASS;
}

bool swc_is_terminal(const char *exe, const char *class_name)
{
	static const char *exes[] = {"windowsterminal.exe",
				     "openconsole.exe",
				     "conhost.exe",
				     "wezterm-gui.exe",
				     "wezterm.exe",
				     "alacritty.exe",
				     "ghostty.exe",
				     "warp.exe",
				     "tabby.exe",
				     NULL};

	if (class_name && class_name[0]) {
		if (strstr(class_name, "CASCADIA") != NULL)
			return true;
		if (strcmp(class_name, "ConsoleWindowClass") == 0)
			return true;
	}
	if (!exe || !exe[0])
		return false;
	for (int i = 0; exes[i]; i++) {
		if (_stricmp(exe, exes[i]) == 0)
			return true;
	}
	return false;
}

bool swc_match_by_exe_only(const char *exe, const char *class_name)
{
	static const char *exes[] = {"chrome.exe",    "msedge.exe", "firefox.exe", "brave.exe",
				     "opera.exe",     "code.exe",   "discord.exe", "slack.exe",
				     "telegram.exe",  "wechat.exe", "qq.exe",      "spotify.exe",
				     "steam.exe",     "windowsterminal.exe", "openconsole.exe",
				     "conhost.exe",   "wezterm-gui.exe", "wezterm.exe",
				     "alacritty.exe", "ghostty.exe", "warp.exe", "tabby.exe",
				     "obs64.exe",     "obs32.exe",  "sai2.exe", "sai.exe", NULL};

	if (swc_is_paint_tool_sai(exe, class_name) || swc_class_is_unstable(class_name))
		return true;
	if (class_name && (strstr(class_name, "Chrome") || strstr(class_name, "Mozilla") ||
			   strstr(class_name, "Electron") || strstr(class_name, "CASCADIA") ||
			   strstr(class_name, "TXGui") || strcmp(class_name, "ConsoleWindowClass") == 0))
		return true;
	if (!exe || !exe[0])
		return false;
	for (int i = 0; exes[i]; i++) {
		if (_stricmp(exe, exes[i]) == 0)
			return true;
	}
	return false;
}

static bool class_is_skipped(const char *class_name)
{
	for (int i = 0; skip_classes[i]; i++) {
		if (strcmp(class_name, skip_classes[i]) == 0)
			return true;
	}
	return false;
}

static bool always_include_window(const char *exe, const char *class_name)
{
	if (swc_is_terminal(exe, class_name))
		return true;
	if (exe && (_stricmp(exe, "obs64.exe") == 0 || _stricmp(exe, "obs32.exe") == 0 ||
		    _stricmp(exe, "obs.exe") == 0))
		return true;
	return false;
}

static bool exe_is_internal(const char *exe)
{
	for (int i = 0; internal_exes_exact[i]; i++) {
		if (_stricmp(exe, internal_exes_exact[i]) == 0)
			return true;
	}
	return false;
}

static bool get_exe_basename(HWND hwnd, char *dst, size_t dst_sz)
{
	DWORD pid = 0;
	wchar_t path[MAX_PATH];
	DWORD path_sz = MAX_PATH;

	GetWindowThreadProcessId(hwnd, &pid);
	path[0] = 0;

	if (pid == GetCurrentProcessId()) {
		if (!GetModuleFileNameW(NULL, path, MAX_PATH))
			return false;
	} else {
		HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		if (!process)
			return false;
		if (!QueryFullProcessImageNameW(process, 0, path, &path_sz)) {
			CloseHandle(process);
			return false;
		}
		CloseHandle(process);
	}

	wchar_t *slash = wcsrchr(path, L'\\');
	utf8_from_wide(dst, dst_sz, slash ? slash + 1 : path);
	return dst[0] != '\0';
}

static bool window_cloaked(HWND hwnd)
{
	DWORD cloaked = 0;
	HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
	return SUCCEEDED(hr) && cloaked;
}

static bool get_window_rect_dwm(HWND hwnd, RECT *rect)
{
	if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, rect, sizeof(*rect))))
		return true;
	return GetWindowRect(hwnd, rect) != FALSE;
}

static bool GetMonitorTarget(LPCWSTR device, DISPLAYCONFIG_TARGET_DEVICE_NAME *target)
{
	UINT32 numPath = 0;
	UINT32 numMode = 0;
	if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &numPath, &numMode) != ERROR_SUCCESS)
		return false;
	if (!numPath || !numMode)
		return false;

	DISPLAYCONFIG_PATH_INFO *paths = malloc(numPath * sizeof(*paths));
	DISPLAYCONFIG_MODE_INFO *modes = malloc(numMode * sizeof(*modes));
	if (!paths || !modes) {
		free(paths);
		free(modes);
		return false;
	}

	bool found = false;
	if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &numPath, paths, &numMode, modes, NULL) == ERROR_SUCCESS) {
		for (UINT32 i = 0; i < numPath; i++) {
			DISPLAYCONFIG_SOURCE_DEVICE_NAME source;
			memset(&source, 0, sizeof(source));
			source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
			source.header.size = sizeof(source);
			source.header.adapterId = paths[i].sourceInfo.adapterId;
			source.header.id = paths[i].sourceInfo.id;
			if (DisplayConfigGetDeviceInfo(&source.header) == ERROR_SUCCESS &&
			    wcscmp(device, source.viewGdiDeviceName) == 0) {
				target->header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
				target->header.size = sizeof(*target);
				target->header.adapterId = paths[i].sourceInfo.adapterId;
				target->header.id = paths[i].targetInfo.id;
				found = DisplayConfigGetDeviceInfo(&target->header) == ERROR_SUCCESS;
				break;
			}
		}
	}

	free(paths);
	free(modes);
	return found;
}

static void get_monitor_friendly_name(HMONITOR handle, char *name, size_t count)
{
	MONITORINFOEXW mi;
	DISPLAYCONFIG_TARGET_DEVICE_NAME target;

	memset(&mi, 0, sizeof(mi));
	memset(&target, 0, sizeof(target));
	mi.cbSize = sizeof(mi);
	if (GetMonitorInfoW(handle, (LPMONITORINFO)&mi) && GetMonitorTarget(mi.szDevice, &target) &&
	    target.monitorFriendlyDeviceName[0]) {
		utf8_from_wide(name, count, target.monitorFriendlyDeviceName);
	} else {
		snprintf(name, count, "Display");
	}
}

struct enum_mon_ctx {
	struct swc_monitor *out;
	int max;
	int count;
};

static BOOL CALLBACK enum_monitor_cb(HMONITOR handle, HDC hdc, LPRECT rect, LPARAM param)
{
	struct enum_mon_ctx *ctx = (struct enum_mon_ctx *)param;
	MONITORINFOEXA mi;
	DISPLAY_DEVICEA device;

	(void)hdc;
	(void)rect;

	if (ctx->count >= ctx->max)
		return FALSE;

	memset(&mi, 0, sizeof(mi));
	mi.cbSize = sizeof(mi);
	if (!GetMonitorInfoA(handle, (LPMONITORINFO)&mi))
		return TRUE;

	struct swc_monitor *m = &ctx->out[ctx->count];
	memset(m, 0, sizeof(*m));
	m->handle = handle;
	m->rect = mi.rcMonitor;
	m->primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
	snprintf(m->alt_id, sizeof(m->alt_id), "%s", mi.szDevice);

	memset(&device, 0, sizeof(device));
	device.cb = sizeof(device);
	if (EnumDisplayDevicesA(mi.szDevice, 0, &device, EDD_GET_DEVICE_INTERFACE_NAME) && device.DeviceID[0])
		snprintf(m->device_id, sizeof(m->device_id), "%s", device.DeviceID);
	else
		snprintf(m->device_id, sizeof(m->device_id), "%s", mi.szDevice);

	get_monitor_friendly_name(handle, m->name, sizeof(m->name));
	snprintf(m->desc, sizeof(m->desc), "%s: %dx%d @ %d,%d%s", m->name, mi.rcMonitor.right - mi.rcMonitor.left,
		 mi.rcMonitor.bottom - mi.rcMonitor.top, mi.rcMonitor.left, mi.rcMonitor.top,
		 m->primary ? " (Primary)" : "");

	ctx->count++;
	return TRUE;
}

int swc_enum_monitors(struct swc_monitor *out, int max)
{
	struct enum_mon_ctx ctx;
	if (!out || max <= 0)
		return 0;
	ctx.out = out;
	ctx.max = max;
	ctx.count = 0;
	EnumDisplayMonitors(NULL, NULL, enum_monitor_cb, (LPARAM)&ctx);
	return ctx.count;
}

bool swc_find_monitor(const char *monitor_id, struct swc_monitor *out)
{
	struct swc_monitor mons[SWC_MAX_MONITORS];
	int n = swc_enum_monitors(mons, SWC_MAX_MONITORS);
	const bool want_primary = !monitor_id || !monitor_id[0] || strcmp(monitor_id, "DUMMY") == 0;

	for (int i = 0; i < n; i++) {
		if (want_primary) {
			if (mons[i].primary) {
				*out = mons[i];
				return true;
			}
		} else if (strcmp(mons[i].device_id, monitor_id) == 0 || strcmp(mons[i].alt_id, monitor_id) == 0) {
			*out = mons[i];
			return true;
		}
	}

	if (n > 0) {
		*out = mons[0];
		return true;
	}
	return false;
}

bool swc_active_monitor(struct swc_monitor *out)
{
	HWND fg = GetForegroundWindow();
	HMONITOR handle = NULL;
	struct swc_monitor mons[SWC_MAX_MONITORS];
	int n;

	if (!out)
		return false;

	if (fg && IsWindow(fg))
		handle = MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST);
	if (!handle) {
		POINT pt = {0, 0};
		handle = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
	}

	n = swc_enum_monitors(mons, SWC_MAX_MONITORS);
	for (int i = 0; i < n; i++) {
		if (mons[i].handle == handle) {
			*out = mons[i];
			return true;
		}
	}
	return swc_find_monitor("", out);
}

struct enum_win_ctx {
	const RECT *monitor_rect;
	uint32_t flags;
	struct swc_window *out;
	int max;
	int count;
};

static BOOL CALLBACK enum_windows_cb(HWND hwnd, LPARAM param)
{
	struct enum_win_ctx *ctx = (struct enum_win_ctx *)param;
	struct swc_window w;
	RECT rect;
	RECT dummy;
	LONG_PTR styles;
	LONG_PTR ex_styles;
	wchar_t wtitle[512];
	wchar_t wclass[256];

	if (ctx->count >= ctx->max)
		return FALSE;
	if (!IsWindowVisible(hwnd))
		return TRUE;
	if (IsIconic(hwnd) && !(ctx->flags & SWC_ENUM_MINIMIZED))
		return TRUE;

	styles = GetWindowLongPtrW(hwnd, GWL_STYLE);
	if (styles & WS_CHILD)
		return TRUE;

	memset(&w, 0, sizeof(w));
	w.hwnd = hwnd;

	wtitle[0] = 0;
	wclass[0] = 0;
	GetWindowTextW(hwnd, wtitle, 512);
	GetClassNameW(hwnd, wclass, 256);
	utf8_from_wide(w.title, sizeof(w.title), wtitle);
	utf8_from_wide(w.class_name, sizeof(w.class_name), wclass);
	if (!get_exe_basename(hwnd, w.exe, sizeof(w.exe)))
		snprintf(w.exe, sizeof(w.exe), "unknown");

	ex_styles = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
	if (window_cloaked(hwnd) && !always_include_window(w.exe, w.class_name))
		return TRUE;
	if (!always_include_window(w.exe, w.class_name)) {
		if (!(ctx->flags & SWC_ENUM_TOOLWINDOWS) && (ex_styles & WS_EX_TOOLWINDOW))
			return TRUE;
		if (!w.class_name[0] || class_is_skipped(w.class_name))
			return TRUE;
		if (swc_match_by_exe_only(w.exe, w.class_name) && (ex_styles & WS_EX_TOOLWINDOW))
			return TRUE;
		if (ctx->flags & SWC_ENUM_PICKER) {
			if (exe_is_internal(w.exe))
				return TRUE;
			if (_stricmp(w.exe, "explorer.exe") == 0 && !w.title[0])
				return TRUE;
		}
	}

	if (IsIconic(hwnd)) {
		WINDOWPLACEMENT wp;
		memset(&wp, 0, sizeof(wp));
		wp.length = sizeof(wp);
		if (!GetWindowPlacement(hwnd, &wp))
			return TRUE;
		rect = wp.rcNormalPosition;
	} else if (!get_window_rect_dwm(hwnd, &rect)) {
		return TRUE;
	}
	if (!IsIconic(hwnd) && ((rect.right - rect.left) <= 1 || (rect.bottom - rect.top) <= 1))
		return TRUE;
	if (ctx->monitor_rect && !IsIconic(hwnd) && !IntersectRect(&dummy, &rect, ctx->monitor_rect))
		return TRUE;

	w.rect = rect;
	if (!w.title[0])
		snprintf(w.title, sizeof(w.title), "(%s)", w.class_name[0] ? w.class_name : w.exe);

	swc_encode_window(w.encoded, sizeof(w.encoded), w.title, w.class_name, w.exe);
	snprintf(w.desc, sizeof(w.desc), "[%s]: %s", w.exe, w.title[0] ? w.title : w.class_name);

	ctx->out[ctx->count++] = w;
	return TRUE;
}

int swc_enum_windows(const RECT *monitor_rect, uint32_t flags, struct swc_window *out, int max)
{
	struct enum_win_ctx ctx;
	if (!out || max <= 0)
		return 0;
	ctx.monitor_rect = monitor_rect;
	ctx.flags = flags;
	ctx.out = out;
	ctx.max = max;
	ctx.count = 0;
	EnumWindows(enum_windows_cb, (LPARAM)&ctx);
	return ctx.count;
}
