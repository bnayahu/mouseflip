#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <new>
#include <commctrl.h>
#include <strsafe.h>
#include "../resources/resource.h"
#include "../resources/app_strings.h"

// Global variables
const wchar_t* CLASS_NAME = APP_WINDOW_CLASS;
const wchar_t* REGISTRY_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* REGISTRY_VALUE = APP_REGISTRY_VALUE;
const wchar_t* SETTINGS_REGISTRY_KEY = APP_SETTINGS_REGISTRY_KEY;
const wchar_t* AUTOSWITCH_VALUE = L"AutoSwitch";
const wchar_t* BASE_MOUSE_COUNT_VALUE = L"BaseMouseCount";
const wchar_t* EXTERNAL_MOUSE_LEFTHANDED_VALUE = L"ExternalMouseIsLeftHanded";
// Session-local (no "Global\" prefix): the tray is per-session, so each user
// or remote-desktop session gets its own instance.
const wchar_t* MUTEX_NAME = APP_NAME L"-SingleInstance";
NOTIFYICONDATA g_nid = {};
HWND g_hwndMain = NULL;
// Registered ID of Explorer's "TaskbarCreated" broadcast, or 0 if it could not
// be registered. Explorer sends this to all top-level windows when it rebuilds
// the notification area, which is our cue to re-add the tray icon.
UINT g_uTaskbarCreatedMsg = 0;
// Tri-state result of the external-mouse check. UNKNOWN means enumeration
// failed and no conclusion should be drawn from it.
enum ExternalMouseState {
    EXTERNAL_MOUSE_UNKNOWN = -1,
    EXTERNAL_MOUSE_ABSENT  = 0,
    EXTERNAL_MOUSE_PRESENT = 1
};

// Last observed external-mouse state. Starts UNKNOWN so the first successful
// check always differs from it and applies the correct orientation.
ExternalMouseState g_lastExternalMouseState = EXTERNAL_MOUSE_UNKNOWN;

// Forward declarations
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK OptionsDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AboutDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
bool GetCurrentMouseState();
void ApplyMouseOrientation(HWND hwnd, bool leftHanded);
UINT GetIconForCurrentState();
void AddTrayIcon(HWND hwnd, UINT iconID);
void UpdateTrayIcon(HWND hwnd, UINT iconID);
void RemoveTrayIcon(HWND hwnd);
void ShowContextMenu(HWND hwnd, POINT pt);
void UpdateMenuChecks(HMENU hMenu);
void ShowAboutDialog(HWND hwnd);
void ShowOptionsDialog(HWND hwnd);
bool IsStartupEnabled();
bool SetStartupEnabled(bool enable);
bool IsAutoSwitchEnabled();
bool SetAutoSwitchEnabled(bool enable);
int GetCurrentMouseDeviceCount();
int GetBaseMouseCount();
bool SetBaseMouseCount(int count);
bool IsExternalMouseLeftHanded();
bool SetExternalMouseLeftHanded(bool leftHanded);
ExternalMouseState GetExternalMouseState();
void CheckAndApplyAutoSwitch();
void StartAutoSwitchMonitoring(HWND hwnd);
void StopAutoSwitchMonitoring(HWND hwnd);
wchar_t* GetExecutablePath();

// Entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Refuse to start a second instance. A second tray icon would duplicate
    // the menu and run a second auto-switch timer against the same setting.
    HANDLE hInstanceMutex = CreateMutex(NULL, TRUE, MUTEX_NAME);
    if (hInstanceMutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
        // Already running. There is no window to bring forward, so exit
        // quietly rather than nagging the user on every login.
        CloseHandle(hInstanceMutex);
        return 0;
    }

    // Pair with the Common Controls v6 dependency in the manifest so dialog
    // controls are themed.
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    // Must be registered before the window exists so WndProc can recognise it.
    g_uTaskbarCreatedMsg = RegisterWindowMessage(L"TaskbarCreated");

    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_APP));
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_APP));

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, L"Window registration failed!", APP_NAME, MB_ICONERROR | MB_OK);
        if (hInstanceMutex) {
            ReleaseMutex(hInstanceMutex);
            CloseHandle(hInstanceMutex);
        }
        return 1;
    }

    // Create hidden window for message processing
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        APP_NAME,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL) {
        MessageBox(NULL, L"Window creation failed!", APP_NAME, MB_ICONERROR | MB_OK);
        if (hInstanceMutex) {
            ReleaseMutex(hInstanceMutex);
            CloseHandle(hInstanceMutex);
        }
        return 1;
    }

    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (hInstanceMutex) {
        ReleaseMutex(hInstanceMutex);
        CloseHandle(hInstanceMutex);
    }

    return (int)msg.wParam;
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Explorer restarted and rebuilt the notification area, discarding our
    // icon. Re-add it. The ID is registered at runtime, so it cannot be a
    // case label.
    //
    // Note: this relies on the window being a normal top-level window.
    // A message-only (HWND_MESSAGE) window does not receive broadcasts, so
    // this window must not be "optimised" into one.
    if (g_uTaskbarCreatedMsg != 0 && msg == g_uTaskbarCreatedMsg) {
        AddTrayIcon(hwnd, GetIconForCurrentState());
        return 0;
    }

    switch (msg) {
        case WM_CREATE:
            // WM_CREATE is dispatched from inside CreateWindowEx, before it
            // returns, so publish the handle here rather than at the call site.
            // CheckAndApplyAutoSwitch() below needs it to update the icon.
            g_hwndMain = hwnd;

            // Initialize tray icon with current system state
            AddTrayIcon(hwnd, GetIconForCurrentState());

            // Start auto-switch monitoring if enabled
            if (IsAutoSwitchEnabled()) {
                StartAutoSwitchMonitoring(hwnd);
                CheckAndApplyAutoSwitch();  // Apply immediately
            }
            return 0;

        case WM_TIMER:
            if (wParam == TIMER_AUTOSWITCH) {
                CheckAndApplyAutoSwitch();
            }
            return 0;

        case WM_TRAYICON:
            switch (LOWORD(lParam)) {
                case WM_LBUTTONDBLCLK:
                case WM_RBUTTONDBLCLK:
                    // Double-click (either button): flip mouse orientation
                    ApplyMouseOrientation(hwnd, !GetCurrentMouseState());
                    break;

                case WM_RBUTTONUP:
                    // Right single-click: show context menu
                    POINT pt;
                    GetCursorPos(&pt);
                    ShowContextMenu(hwnd, pt);
                    break;
            }
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_RIGHTHANDED:
                    ApplyMouseOrientation(hwnd, false);
                    break;

                case IDM_LEFTHANDED:
                    ApplyMouseOrientation(hwnd, true);
                    break;

                case IDM_OPTIONS:
                    ShowOptionsDialog(hwnd);
                    break;

                case IDM_ABOUT:
                    ShowAboutDialog(hwnd);
                    break;

                case IDM_EXIT:
                    RemoveTrayIcon(hwnd);
                    PostQuitMessage(0);
                    break;
            }
            return 0;

        case WM_DESTROY:
            StopAutoSwitchMonitoring(hwnd);
            RemoveTrayIcon(hwnd);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Get current mouse button configuration
// Returns true if buttons are swapped (left-handed), false if normal (right-handed)
bool GetCurrentMouseState() {
    return GetSystemMetrics(SM_SWAPBUTTON) != 0;
}

// Apply a mouse orientation and update the tray icon to match.
//
// The icon is derived from the state we just requested, not from a fresh
// GetSystemMetrics(SM_SWAPBUTTON) read: that metric can still report the old
// value immediately after SwapMouseButton() returns, which would leave the
// icon disagreeing with the actual configuration.
void ApplyMouseOrientation(HWND hwnd, bool leftHanded) {
    SwapMouseButton(leftHanded ? TRUE : FALSE);
    UpdateTrayIcon(hwnd, leftHanded ? IDI_ICON_LEFT : IDI_ICON_RIGHT);
}

// Get icon resource ID based on current system state
UINT GetIconForCurrentState() {
    return GetCurrentMouseState() ? IDI_ICON_LEFT : IDI_ICON_RIGHT;
}

// Add tray icon
void AddTrayIcon(HWND hwnd, UINT iconID) {
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(iconID));
    wcsncpy(g_nid.szTip, APP_TRAY_TOOLTIP,
            sizeof(g_nid.szTip) / sizeof(wchar_t) - 1);
    g_nid.szTip[sizeof(g_nid.szTip) / sizeof(wchar_t) - 1] = L'\0';

    Shell_NotifyIcon(NIM_ADD, &g_nid);
}

// Update tray icon
void UpdateTrayIcon(HWND hwnd, UINT iconID) {
    g_nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(iconID));
    Shell_NotifyIcon(NIM_MODIFY, &g_nid);
}

// Remove tray icon
void RemoveTrayIcon(HWND hwnd) {
    Shell_NotifyIcon(NIM_DELETE, &g_nid);
}

// Show context menu
void ShowContextMenu(HWND hwnd, POINT pt) {
    HMENU hMenu = CreatePopupMenu();
    if (hMenu) {
        AppendMenu(hMenu, MF_STRING, IDM_RIGHTHANDED, L"Right-handed");
        AppendMenu(hMenu, MF_STRING, IDM_LEFTHANDED, L"Left-handed");
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hMenu, MF_STRING, IDM_OPTIONS, L"Options...");
        AppendMenu(hMenu, MF_STRING, IDM_ABOUT, L"About");
        AppendMenu(hMenu, MF_STRING, IDM_EXIT, L"Exit");

        UpdateMenuChecks(hMenu);

        // Required for proper menu behavior
        SetForegroundWindow(hwnd);

        TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);

        DestroyMenu(hMenu);
    }
}

// Update menu checkmarks based on current state
void UpdateMenuChecks(HMENU hMenu) {
    bool isLeftHanded = GetCurrentMouseState();

    CheckMenuItem(hMenu, IDM_RIGHTHANDED, MF_BYCOMMAND | (isLeftHanded ? MF_UNCHECKED : MF_CHECKED));
    CheckMenuItem(hMenu, IDM_LEFTHANDED, MF_BYCOMMAND | (isLeftHanded ? MF_CHECKED : MF_UNCHECKED));
}

// About dialog procedure
INT_PTR CALLBACK AboutDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // Set the about text. StringCchPrintf always null-terminates, even
            // on truncation, unlike wsprintf which has no bounds at all.
            wchar_t message[512];
            StringCchPrintf(message, ARRAYSIZE(message),
                            L"%s v%s\n\n"
                            L"Quickly toggle mouse button configuration\n"
                            L"between right-handed and left-handed modes.\n\n"
                            L"Double-click the tray icon with either button to flip.\n"
                            L"Right-click for menu.\n\n"
                            L"%s\n"
                            L"Licensed under the Apache License 2.0.",
                            APP_NAME, APP_VERSION, APP_COPYRIGHT);
            SetDlgItemText(hwndDlg, IDC_ABOUT_TEXT, message);
            return TRUE;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, LOWORD(wParam));
                return TRUE;
            }
            break;
    }

    return FALSE;
}

// Show about dialog
void ShowAboutDialog(HWND hwnd) {
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUT), hwnd, AboutDialogProc);
}

// Get full path to the executable
wchar_t* GetExecutablePath() {
    static wchar_t path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    return path;
}

// Check if startup is enabled
bool IsStartupEnabled() {
    HKEY hKey;
    bool enabled = false;

    if (RegOpenKeyEx(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t value[MAX_PATH];
        DWORD size = sizeof(value);
        DWORD type;

        if (RegQueryValueEx(hKey, REGISTRY_VALUE, NULL, &type, (LPBYTE)value, &size) == ERROR_SUCCESS) {
            if (type == REG_SZ) {
                enabled = true;
            }
        }

        RegCloseKey(hKey);
    }

    return enabled;
}

// Enable or disable startup with Windows
bool SetStartupEnabled(bool enable) {
    HKEY hKey;
    bool success = false;
    LONG result;

    result = RegOpenKeyEx(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_WRITE, &hKey);

    if (enable) {
        // Add to startup - key must exist
        if (result == ERROR_SUCCESS) {
            wchar_t* path = GetExecutablePath();
            DWORD size = (wcslen(path) + 1) * sizeof(wchar_t);

            if (RegSetValueEx(hKey, REGISTRY_VALUE, 0, REG_SZ, (LPBYTE)path, size) == ERROR_SUCCESS) {
                success = true;
            }
            RegCloseKey(hKey);
        }
    } else {
        // Remove from startup
        if (result == ERROR_SUCCESS) {
            // Key exists, try to delete the value
            LONG deleteResult = RegDeleteValue(hKey, REGISTRY_VALUE);
            if (deleteResult == ERROR_SUCCESS || deleteResult == ERROR_FILE_NOT_FOUND) {
                success = true;
            }
            RegCloseKey(hKey);
        } else if (result == ERROR_FILE_NOT_FOUND) {
            // Key doesn't exist, so value doesn't exist either - this is success for deletion
            success = true;
        }
    }

    return success;
}

// Options dialog procedure
INT_PTR CALLBACK OptionsDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // Set checkbox states based on current settings
            CheckDlgButton(hwndDlg, IDC_STARTUP_CHECKBOX,
                          IsStartupEnabled() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwndDlg, IDC_AUTOSWITCH_CHECKBOX,
                          IsAutoSwitchEnabled() ? BST_CHECKED : BST_UNCHECKED);

            // Direction radios: exactly one is always selected.
            CheckRadioButton(hwndDlg,
                             IDC_EXTERNAL_LEFT_RADIO, IDC_EXTERNAL_RIGHT_RADIO,
                             IsExternalMouseLeftHanded() ? IDC_EXTERNAL_LEFT_RADIO
                                                         : IDC_EXTERNAL_RIGHT_RADIO);

            // The direction only means anything while auto-switch is on.
            {
                BOOL autoSwitchOn = (IsDlgButtonChecked(hwndDlg, IDC_AUTOSWITCH_CHECKBOX) == BST_CHECKED);
                EnableWindow(GetDlgItem(hwndDlg, IDC_EXTERNAL_LEFT_RADIO), autoSwitchOn);
                EnableWindow(GetDlgItem(hwndDlg, IDC_EXTERNAL_RIGHT_RADIO), autoSwitchOn);
            }

            // Display current detected mouse device count
            wchar_t countStr[16];
            int detectedCount = GetCurrentMouseDeviceCount();
            if (detectedCount < 0) {
                SetDlgItemText(hwndDlg, IDC_DETECTED_DEVICES_LABEL, L"unknown");
            } else {
                StringCchPrintf(countStr, ARRAYSIZE(countStr), L"%d", detectedCount);
                SetDlgItemText(hwndDlg, IDC_DETECTED_DEVICES_LABEL, countStr);
            }

            // Set base mouse count in edit control
            int baseCount = GetBaseMouseCount();
            StringCchPrintf(countStr, ARRAYSIZE(countStr), L"%d", baseCount);
            SetDlgItemText(hwndDlg, IDC_BASE_DEVICES_EDIT, countStr);

            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_AUTOSWITCH_CHECKBOX: {
                    // Keep the direction radios greyed out while auto-switch
                    // is off, so the setting cannot look active when it isn't.
                    BOOL autoSwitchOn = (IsDlgButtonChecked(hwndDlg, IDC_AUTOSWITCH_CHECKBOX) == BST_CHECKED);
                    EnableWindow(GetDlgItem(hwndDlg, IDC_EXTERNAL_LEFT_RADIO), autoSwitchOn);
                    EnableWindow(GetDlgItem(hwndDlg, IDC_EXTERNAL_RIGHT_RADIO), autoSwitchOn);
                    return TRUE;
                }

                case IDOK: {
                    // Get checkbox states
                    bool startupEnabled = (IsDlgButtonChecked(hwndDlg, IDC_STARTUP_CHECKBOX) == BST_CHECKED);
                    bool autoSwitchEnabled = (IsDlgButtonChecked(hwndDlg, IDC_AUTOSWITCH_CHECKBOX) == BST_CHECKED);
                    bool externalIsLeftHanded =
                        (IsDlgButtonChecked(hwndDlg, IDC_EXTERNAL_LEFT_RADIO) == BST_CHECKED);

                    // Get base mouse count from edit control
                    wchar_t countStr[16];
                    GetDlgItemText(hwndDlg, IDC_BASE_DEVICES_EDIT, countStr, 16);
                    int baseCount = _wtoi(countStr);

                    // Validate base count
                    if (baseCount < 1) {
                        MessageBox(hwndDlg,
                                  L"Base device count must be at least 1.",
                                  L"Invalid Input",
                                  MB_ICONWARNING | MB_OK);
                        return TRUE;
                    }

                    // Apply startup setting
                    if (!SetStartupEnabled(startupEnabled)) {
                        MessageBox(hwndDlg,
                                  L"Failed to update startup settings. Please check your permissions.",
                                  L"Error",
                                  MB_ICONERROR | MB_OK);
                    }

                    // Write the direction before applying auto-switch, so the
                    // immediate re-check below uses the new value.
                    if (!SetExternalMouseLeftHanded(externalIsLeftHanded)) {
                        MessageBox(hwndDlg,
                                  L"Failed to update the auto-switch direction. Please check your permissions.",
                                  L"Error",
                                  MB_ICONERROR | MB_OK);
                    }

                    // Apply auto-switch setting
                    if (!SetAutoSwitchEnabled(autoSwitchEnabled)) {
                        MessageBox(hwndDlg,
                                  L"Failed to update auto-switch settings. Please check your permissions.",
                                  L"Error",
                                  MB_ICONERROR | MB_OK);
                    } else {
                        // Start or stop monitoring based on setting
                        if (autoSwitchEnabled) {
                            StartAutoSwitchMonitoring(g_hwndMain);
                            CheckAndApplyAutoSwitch();  // Apply immediately (will detect change and apply)
                        } else {
                            StopAutoSwitchMonitoring(g_hwndMain);
                        }
                    }

                    // Apply base mouse count setting
                    if (!SetBaseMouseCount(baseCount)) {
                        MessageBox(hwndDlg,
                                  L"Failed to update base mouse count. Please check your permissions.",
                                  L"Error",
                                  MB_ICONERROR | MB_OK);
                    } else {
                        // Re-check if auto-switch is enabled, to apply new settings immediately
                        if (autoSwitchEnabled) {
                            CheckAndApplyAutoSwitch();
                        }
                    }

                    // Settings may have changed the mapping without the device
                    // state changing, so discard the cached state to force the
                    // next check to apply the new configuration.
                    if (autoSwitchEnabled) {
                        g_lastExternalMouseState = EXTERNAL_MOUSE_UNKNOWN;
                        CheckAndApplyAutoSwitch();
                    }

                    EndDialog(hwndDlg, IDOK);
                    return TRUE;
                }

                case IDCANCEL:
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }

    return FALSE;
}

// Show options dialog
void ShowOptionsDialog(HWND hwnd) {
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_OPTIONS), hwnd, OptionsDialogProc);
}

// Check if auto-switch is enabled (default: true)
bool IsAutoSwitchEnabled() {
    HKEY hKey;
    bool enabled = true;  // Default to enabled if setting doesn't exist

    if (RegOpenKeyEx(HKEY_CURRENT_USER, SETTINGS_REGISTRY_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD value = 1;  // Default to 1 (enabled)
        DWORD size = sizeof(value);
        DWORD type;

        if (RegQueryValueEx(hKey, AUTOSWITCH_VALUE, NULL, &type, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            if (type == REG_DWORD) {
                enabled = (value != 0);
            }
        }
        // If value doesn't exist, keep default (true)

        RegCloseKey(hKey);
    }
    // If registry key doesn't exist at all, keep default (true)

    return enabled;
}

// Enable or disable auto-switch
bool SetAutoSwitchEnabled(bool enable) {
    HKEY hKey;
    bool success = false;
    DWORD disposition;

    // Create or open the settings key
    if (RegCreateKeyEx(HKEY_CURRENT_USER, SETTINGS_REGISTRY_KEY, 0, NULL, 0,
                       KEY_WRITE, NULL, &hKey, &disposition) == ERROR_SUCCESS) {
        // Always write the value (1 for enabled, 0 for disabled)
        DWORD value = enable ? 1 : 0;
        if (RegSetValueEx(hKey, AUTOSWITCH_VALUE, 0, REG_DWORD, (LPBYTE)&value, sizeof(value)) == ERROR_SUCCESS) {
            success = true;
        }

        RegCloseKey(hKey);
    }

    return success;
}

// Get base mouse device count (default: 1)
int GetBaseMouseCount() {
    HKEY hKey;
    int count = 1;  // Default to 1 if setting doesn't exist

    if (RegOpenKeyEx(HKEY_CURRENT_USER, SETTINGS_REGISTRY_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD value = 1;  // Default to 1
        DWORD size = sizeof(value);
        DWORD type;

        if (RegQueryValueEx(hKey, BASE_MOUSE_COUNT_VALUE, NULL, &type, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            if (type == REG_DWORD && value > 0) {
                count = (int)value;
            }
        }

        RegCloseKey(hKey);
    }

    return count;
}

// Set base mouse device count
bool SetBaseMouseCount(int count) {
    if (count < 1) {
        return false;  // Invalid count
    }

    HKEY hKey;
    bool success = false;
    DWORD disposition;

    // Create or open the settings key
    if (RegCreateKeyEx(HKEY_CURRENT_USER, SETTINGS_REGISTRY_KEY, 0, NULL, 0,
                       KEY_WRITE, NULL, &hKey, &disposition) == ERROR_SUCCESS) {
        DWORD value = (DWORD)count;
        if (RegSetValueEx(hKey, BASE_MOUSE_COUNT_VALUE, 0, REG_DWORD, (LPBYTE)&value, sizeof(value)) == ERROR_SUCCESS) {
            success = true;
        }

        RegCloseKey(hKey);
    }

    return success;
}

// Which orientation to use when an external mouse is connected (default: true,
// meaning left-handed, which matches the behaviour before this was settable).
bool IsExternalMouseLeftHanded() {
    HKEY hKey;
    bool leftHanded = true;

    if (RegOpenKeyEx(HKEY_CURRENT_USER, SETTINGS_REGISTRY_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD value = 1;
        DWORD size = sizeof(value);
        DWORD type;

        if (RegQueryValueEx(hKey, EXTERNAL_MOUSE_LEFTHANDED_VALUE, NULL, &type,
                            (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            if (type == REG_DWORD) {
                leftHanded = (value != 0);
            }
        }

        RegCloseKey(hKey);
    }

    return leftHanded;
}

// Set which orientation an external mouse maps to.
bool SetExternalMouseLeftHanded(bool leftHanded) {
    HKEY hKey;
    bool success = false;
    DWORD disposition;

    if (RegCreateKeyEx(HKEY_CURRENT_USER, SETTINGS_REGISTRY_KEY, 0, NULL, 0,
                       KEY_WRITE, NULL, &hKey, &disposition) == ERROR_SUCCESS) {
        DWORD value = leftHanded ? 1 : 0;
        if (RegSetValueEx(hKey, EXTERNAL_MOUSE_LEFTHANDED_VALUE, 0, REG_DWORD,
                          (LPBYTE)&value, sizeof(value)) == ERROR_SUCCESS) {
            success = true;
        }

        RegCloseKey(hKey);
    }

    return success;
}

// Get the current number of mouse devices detected, or -1 if the device list
// could not be enumerated.
//
// GetRawInputDeviceList is inherently racy: it must be called once to size the
// buffer and again to fill it, and a device arriving in between makes the
// second call fail with ERROR_INSUFFICIENT_BUFFER. That is a transient
// condition, not an absence of mice, so it is retried rather than reported as
// zero — reporting zero would make auto-switch flip the user's buttons.
int GetCurrentMouseDeviceCount() {
    for (int attempt = 0; attempt < 3; attempt++) {
        UINT numDevices = 0;
        if (GetRawInputDeviceList(NULL, &numDevices, sizeof(RAWINPUTDEVICELIST)) != 0) {
            return -1;  // Could not determine how many devices exist
        }

        if (numDevices == 0) {
            return 0;  // Genuinely no input devices
        }

        RAWINPUTDEVICELIST* deviceList = new (std::nothrow) RAWINPUTDEVICELIST[numDevices];
        if (deviceList == NULL) {
            return -1;
        }

        UINT count = numDevices;
        UINT result = GetRawInputDeviceList(deviceList, &count, sizeof(RAWINPUTDEVICELIST));
        if (result == (UINT)-1) {
            DWORD error = GetLastError();
            delete[] deviceList;
            if (error == ERROR_INSUFFICIENT_BUFFER) {
                continue;  // Device list grew; size it again and retry
            }
            return -1;
        }

        int mouseCount = 0;
        for (UINT i = 0; i < result; i++) {
            if (deviceList[i].dwType == RIM_TYPEMOUSE) {
                mouseCount++;
            }
        }

        delete[] deviceList;
        return mouseCount;
    }

    return -1;  // Device list kept changing under us
}

// Determine whether an external mouse is connected.
// Returns UNKNOWN if the device list could not be read, in which case callers
// must not change the mouse configuration.
ExternalMouseState GetExternalMouseState() {
    int currentCount = GetCurrentMouseDeviceCount();
    if (currentCount < 0) {
        return EXTERNAL_MOUSE_UNKNOWN;
    }

    // Devices above the configured base count are external mice.
    return (currentCount > GetBaseMouseCount())
        ? EXTERNAL_MOUSE_PRESENT
        : EXTERNAL_MOUSE_ABSENT;
}

// Check whether an external mouse is connected and apply the matching
// mouse configuration.
void CheckAndApplyAutoSwitch() {
    ExternalMouseState state = GetExternalMouseState();

    // Hold the current configuration if enumeration failed, and do nothing if
    // the state has not changed.
    if (state == EXTERNAL_MOUSE_UNKNOWN || state == g_lastExternalMouseState) {
        return;
    }

    g_lastExternalMouseState = state;

    // One orientation is configured for "external mouse connected"; the
    // opposite applies when only the built-in pointing device is present.
    bool externalIsLeftHanded = IsExternalMouseLeftHanded();
    bool leftHanded = (state == EXTERNAL_MOUSE_PRESENT)
        ? externalIsLeftHanded
        : !externalIsLeftHanded;

    ApplyMouseOrientation(g_hwndMain, leftHanded);
}

// Start auto-switch monitoring
void StartAutoSwitchMonitoring(HWND hwnd) {
    // Set timer to check every 2 seconds
    SetTimer(hwnd, TIMER_AUTOSWITCH, 2000, NULL);
}

// Stop auto-switch monitoring
void StopAutoSwitchMonitoring(HWND hwnd) {
    KillTimer(hwnd, TIMER_AUTOSWITCH);
}
