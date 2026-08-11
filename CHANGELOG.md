# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-08-11

First public release.

### Added

- System tray application to toggle mouse buttons between right-handed and
  left-handed, by double-clicking the tray icon with either button or via the
  right-click menu.
- Auto-switch based on external mouse detection, enabled by default, polling
  connected input devices every 2 seconds.
- Configurable auto-switch direction: choose whether an external mouse means
  left-handed (default) or right-handed. Stored as `ExternalMouseIsLeftHanded`.
- Configurable base device count, so systems with more than one built-in
  pointing device detect external mice correctly.
- Optional start with Windows.
- Single-instance guard: starting Primary while it is already running is a no-op.
- The tray icon is re-added automatically if Windows Explorer restarts.
- Version resource and application manifest: per-monitor DPI awareness, themed
  Common Controls v6, `asInvoker` privileges.

### Fixed

- The tray icon could disagree with the actual mouse configuration when the
  orientation was changed from the tray menu or by auto-switch, because
  `SM_SWAPBUTTON` can lag behind `SwapMouseButton()`.
- The tray icon did not update on first launch when auto-switch changed the
  orientation at startup.
- A transient `GetRawInputDeviceList` failure was read as "no mice connected",
  which silently swapped the user to right-handed. Enumeration errors are now
  distinguished from a genuine zero and the configuration is held unchanged.

### Changed

- Dialogs use Segoe UI 9 and themed controls instead of MS Sans Serif 8.
- The binary is built with `-Os` and stripped, reducing it from 376 KB to
  about 119 KB.

### Removed

- The PowerShell implementation (`scripts/Primary.ps1`, `scripts/Primary.cmd`).
  It resolved icon paths to a directory that does not exist and used
  PowerShell 7.0 syntax while its own launcher invoked Windows PowerShell 5.1.
  The C++ application is the sole implementation.

[1.0.0]: https://github.com/bnayahu/primary/releases/tag/v1.0.0
