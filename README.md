# Primary

A lightweight Windows system tray application that allows you to quickly toggle mouse button configuration between right-handed and left-handed modes.

**Cross-compiled on Linux using MinGW-w64 for Windows targets.**

## Installation

1. Download `Primary.exe` from the [latest release](https://github.com/bnayahu/primary/releases/latest).
2. Optionally verify the download against `SHA256SUMS.txt` from the same release:
   ```powershell
   Get-FileHash Primary.exe -Algorithm SHA256
   ```
3. Run it. The icon appears in the notification area; there is no installer.

> **The binary is not code-signed.** Windows SmartScreen will warn on first
> run. Choose **More info → Run anyway**, or verify the checksum above if you
> prefer to check before running. Alternatively, build from source — see
> [Building from Source](#building-from-source).

## Features

- **Quick Toggle**: Double-click the tray icon with either mouse button to instantly swap mouse buttons
- **Visual Feedback**: Icon changes to reflect current mouse orientation
- **Auto-Switch** (enabled by default): Automatically switches mouse orientation
  when an external mouse is connected or disconnected. Which orientation maps to
  "external mouse connected" is configurable; the default is left-handed, with
  right-handed when only the built-in pointing device is present.
- **Single instance**: Launching Primary while it is already running does nothing
- **Startup with Windows**: Optional setting to launch automatically when Windows starts
- **Context Menu**: Right-click for menu with orientation options, settings, about dialog, and exit
- **Lightweight**: Minimal resource usage, runs silently in system tray
- **Native**: Pure Win32 API, no external dependencies

> **On first run, Primary changes your mouse configuration.** Auto-switch is
> enabled by default, so within about two seconds Primary sets left-handed mode
> if it detects an external mouse and right-handed mode if it does not. To keep
> manual control, open **Options** and untick **Auto-switch based on external
> mouse detection**.

## Use Cases

- Ambidextrous users who switch hands frequently
- Shared computers with users who have different preferences
- Accessibility needs requiring quick mouse configuration changes
- Testing applications with different mouse button configurations

## Requirements

### Runtime
- Windows 10 or Windows 11
- No additional runtime dependencies required

### Building
- Ubuntu/Debian Linux (or WSL)
- MinGW-w64 cross-compiler toolchain
- GCC/G++ compiler

## Building from Source

### Installing MinGW-w64

On Ubuntu/Debian:
```bash
sudo apt update
sudo apt install mingw-w64 g++
```

On other distributions, install the equivalent `mingw-w64` package.

### Building

1. **Clone or navigate to the project directory**
   ```bash
   cd primary
   ```

2. **Run the build script**
   ```bash
   ./build.sh
   ```

   The script will:
   - Check for MinGW-w64 installation
   - Compile resources with `windres`
   - Compile and link with `x86_64-w64-mingw32-g++`
   - Create `Primary.exe` in the project root

### Manual Build Commands

If you need to build manually:

```bash
# Compile resources
x86_64-w64-mingw32-windres resources/primary.rc -O coff -o resources/primary.res

# Compile and link
x86_64-w64-mingw32-g++ -std=c++11 -Wall -Wextra -Wno-unused-parameter \
     -Os -DUNICODE -D_UNICODE \
     -mwindows -municode \
     src/primary.cpp \
     resources/primary.res \
     -o Primary.exe \
     -luser32 -lshell32 -lcomctl32 -static-libgcc -static-libstdc++ \
     -Wl,-s
```

`-municode` is required: the entry point is `wWinMain`, and without it the link
fails with `undefined reference to 'WinMain'`.

## Usage

### Running the Application

1. Transfer `Primary.exe` to your Windows machine
2. Double-click `Primary.exe`
3. The application icon will appear in your system tray (notification area)

### Operations

- **Double-click tray icon** (with either mouse button): Toggle between right-handed and left-handed mouse modes
- **Right-click tray icon**: Open context menu with the following options:
  - **Right-handed**: Set mouse to right-handed mode (standard)
  - **Left-handed**: Set mouse to left-handed mode (buttons swapped)
  - **Options...**: Configure startup behavior and other settings
  - **About**: Display application information
  - **Exit**: Close the application and remove tray icon

### Options Dialog

Access the Options dialog by right-clicking the tray icon and selecting "Options...":

**Startup Settings:**
- **Start Primary when Windows starts**: Enable this checkbox to automatically launch Primary when you log in to Windows. The setting is stored in the Windows registry (HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)

**Auto-Switch Settings:**
- **Auto-switch based on external mouse detection**: (**Enabled by default**)
  Switches mouse orientation automatically as pointing devices come and go.
  Primary polls connected input devices every 2 seconds.
  - **When an external mouse is connected, use**: choose **Left-handed**
    (default) or **Right-handed**. The opposite orientation applies when no
    external mouse is present. These radio buttons are greyed out while
    auto-switch is off.
  - **To disable**: untick the checkbox for fully manual control.
  - A manual choice from the tray menu holds until the next time a device is
    connected or disconnected, at which point auto-switch applies again.
  - If the device list cannot be read, Primary leaves the current configuration
    alone rather than guessing.
  - Settings are stored in `HKEY_CURRENT_USER\Software\Primary`
    (`AutoSwitch`, `BaseMouseCount`, `ExternalMouseIsLeftHanded`).

**Mouse Device Configuration:**
- **Currently detected devices**: Shows the real-time count of mouse devices detected by the system
- **Base device count (undocked)**: Configure how many mouse devices are present in your bare configuration (typically 1 for a single trackpad, but some systems have 2 built-in mouse devices)
  - Devices above this count are considered external mice
  - This setting allows the auto-switch feature to work correctly on systems with multiple built-in pointing devices
  - Default value is 1
  - Stored in HKEY_CURRENT_USER\Software\Primary

### What Gets Changed

When you flip the mouse orientation:
- Left mouse button and right mouse button functions are swapped system-wide
- The change persists until you flip it back or restart Windows
- The tray icon updates to reflect the current state

## Project Structure

```
primary/
├── src/
│   └── primary.cpp            # Main application source (single translation unit)
├── resources/
│   ├── primary.rc             # Resource script: icons, dialogs, version info
│   ├── primary.manifest       # DPI awareness, Common Controls v6, privileges
│   ├── resource.h             # Resource and control ID constants
│   ├── app_strings.h          # App name, version, and metadata (single source of truth)
│   ├── app_icon.ico           # Application icon
│   ├── icon_right.ico         # Right-handed mouse icon
│   └── icon_left.ico          # Left-handed mouse icon
├── .github/workflows/
│   ├── build.yml              # Build validation on push and pull request
│   └── release.yml            # Tag-triggered release build
├── build.sh                   # Build script (Linux)
├── CHANGELOG.md               # Release history
├── LICENSE                    # Apache License 2.0
└── README.md                  # This file
```

## Technical Details

### Architecture
- Pure Win32 API application
- Window class with hidden window for message processing
- System tray integration via `Shell_NotifyIcon`
- Popup menu for user interaction
- State synchronization with system settings

### Key Windows APIs Used
- `Shell_NotifyIcon()`: System tray icon management
- `SwapMouseButton()`: Toggle mouse button configuration
- `GetSystemMetrics(SM_SWAPBUTTON)`: Query current mouse state
- `GetRawInputDeviceList()`: Enumerate connected input devices for external mouse detection
- `CreatePopupMenu()`, `TrackPopupMenu()`: Context menu
- Standard window management APIs

### Cross-Compilation
- Built on Linux using MinGW-w64 (Minimalist GNU for Windows)
- Targets Windows x64 platform
- Static linking for portability (no DLL dependencies)
- GCC/G++ compiler with Windows headers

### State Management
The application always queries the actual system state rather than maintaining internal state. This ensures the icon accurately reflects the current mouse configuration even if changed by other means (Control Panel, Settings app, other applications).

## Compiler Flags Explained

- `-std=c++11`: Use C++11 standard
- `-Wall -Wextra`: Enable comprehensive warnings
- `-DUNICODE -D_UNICODE`: Build with Unicode support
- `-mwindows`: Build as Windows GUI application (no console)
- `-static-libgcc -static-libstdc++`: Static linking for portability
- `-luser32 -lshell32`: Link Windows system libraries

## Troubleshooting

### Build Issues

**MinGW-w64 not found**
- Install with: `sudo apt install mingw-w64`
- Verify with: `x86_64-w64-mingw32-g++ --version`

**windres command not found**
- MinGW-w64 package includes windres
- Check installation: `which x86_64-w64-mingw32-windres`

**Compilation errors**
- Ensure all source files are present
- Check file permissions on build.sh
- Review error output for specific issues

### Runtime Issues (on Windows)

**Application won't start**
- Ensure you're running on Windows 10 or later
- Try running as administrator (though it shouldn't be required)
- Check Windows Event Viewer for errors

**Icon doesn't appear in system tray**
- Check the notification area overflow ("Show hidden icons") and Windows
  notification settings — Windows hides new tray icons by default
- Primary may already be running: only one instance is allowed, and starting a
  second does nothing. Check Task Manager for an existing `Primary.exe`
- Primary re-adds its icon automatically if Explorer restarts

**Icon doesn't update when mouse buttons change**
- Primary only tracks changes it makes itself plus its own auto-switch. If you
  change the setting from Control Panel or the Settings app while Primary is
  running, the icon updates the next time Primary changes the orientation

## Known Limitations

- The button swap is a per-user system setting and applies to **all** pointing
  devices at once. Windows offers no way to swap buttons for one mouse only, so
  Primary cannot provide per-device orientation.
- The swap does not survive a reboot unless auto-switch reapplies it, because
  `SwapMouseButton()` sets the live session state rather than the persisted
  Control Panel value.
- External-mouse detection counts mouse-class input devices. Some docks and
  virtual-KVM drivers register phantom devices; the **Base device count**
  setting exists to compensate.
- The released binary is not code-signed, so SmartScreen warns on first run.

## Contributing

Issues and pull requests are welcome at
[github.com/bnayahu/primary](https://github.com/bnayahu/primary).

All commits must carry a Developer Certificate of Origin sign-off — commit with
`git commit -s` — and CI must pass, which requires a warning-free build.

Ideas that would be welcome:
- Keyboard shortcut support
- Optional notification on orientation change
- Per-application orientation profiles

## License

Copyright 2026 Jonathan Bnayahu

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for the
full text. Built with the MinGW-w64 cross-compiler on Linux.
