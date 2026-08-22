# Selected Windows Capture for OBS Studio

Windows OBS source that captures a **display**, then shows **only the windows you pick**.

Use it when Display Capture is too broad (chat, password managers, extra monitors of the same desktop) and Window Capture is too narrow (you want several windows, laid out as they are on the screen).

## What it does

Adds a source named **Selected Windows Capture**:

1. Pick a display (same identifiers OBS Display Capture uses).
2. Add one or more windows from the current display.
3. Choose the filter:
   - **Show only selected windows** — everything else is removed (transparent holes, or a solid fill).
   - **Hide selected windows** — full display capture minus those windows (typical “hide Discord / browser” case).

Two layouts:

| Layout | How it works | Best for |
| --- | --- | --- |
| **Screen mask** (default) | One display capture + a live z-order mask | Looks like the real desktop, including overlap |
| **Window composite** | Separate capture of each selected window, drawn at its screen position | Selected windows never share a capture with hidden ones |

Matching defaults to **class + executable**, so Chrome/Electron title changes do not drop the window. You can match by executable only, or by exact title.

## Requirements

- Windows 10 or later
- OBS Studio 31.x (built against 31.1.1)
- Visual Studio 2022 Build Tools with the Windows 10 SDK (10.0.22621 or newer) to compile

## Build

From the repo root:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
```

The first configure downloads OBS deps and compiles `libobs`. That can take several minutes.

Or:

```powershell
.\scripts\build.ps1
```

## Install

Close OBS, then:

```powershell
.\scripts\install.ps1
```

This copies the plugin into the layout OBS 31 loads on Windows:

`C:\ProgramData\obs-studio\plugins\obs-selected-windows-moncap\`

```
obs-selected-windows-moncap\
  bin\64bit\obs-selected-windows-moncap.dll
  data\locale\en-US.ini
  data\window-mask.effect
```

A copy is also placed under `%APPDATA%\obs-studio\plugins\` for convenience. Close OBS before installing if it is running, then reopen it and add **Selected Windows Capture** as a source.

## Use

1. Add the source to a scene. Size matches the chosen display.
2. Select the display.
3. Pick a window in **Window to add** and click **Add window** (or **Add all windows on this display**).
4. Leave **Show only selected windows** on if you want a privacy-cropped desktop. Switch to **Hide selected windows** if you want a normal display capture with a few apps punched out.
5. Put an image or color source **under** this source if you want something other than transparency in the holes.

## Limits

- Windows only. Exclusive-fullscreen games still belong in **Game Capture**.
- Screen mask uses a display capture internally, so hidden window pixels still go through the GPU capture path; they are not shown. Use **Window composite** if you do not want those pixels captured at all (up to 16 windows).
- Masks are axis-aligned rectangles (DWM frame bounds). Windows 11 rounded corners and drop shadows are approximate.
- Rotated displays may misalign the mask.
- The plugin enumerates windows similarly to OBS Window Capture (visible, non-minimized, non-cloaked).

## License

GPL-2.0-or-later, same as OBS Studio and the [OBS Plugin Template](https://github.com/obsproject/obs-plugintemplate).
