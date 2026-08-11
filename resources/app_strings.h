#ifndef APP_STRINGS_H
#define APP_STRINGS_H

// String building macros
#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)

// Base application name (single source of truth)
#define APP_NAME_BASE Primary

// Application name in different formats
#define APP_NAME_A                      STRINGIFY(APP_NAME_BASE)  // ANSI version
#define APP_NAME                        WIDEN(APP_NAME_A)         // Wide string version
// Version — single source of truth. Must match the release git tag (v1.0.0).
// The release workflow fails the build if the tag and these values disagree.
#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 0
#define APP_VERSION_PATCH 0

// "1.0.0" and L"1.0.0". JOIN3 produces the single preprocessing-number token
// 1.0.0, so STRINGIFY yields one string literal and WIDEN can paste on the L
// prefix — the same idiom as APP_NAME above.
#define JOIN3_IMPL(a, b, c) a.b.c
#define JOIN3(a, b, c) JOIN3_IMPL(a, b, c)

#define APP_VERSION_A   STRINGIFY(JOIN3(APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH))
#define APP_VERSION     WIDEN(APP_VERSION_A)
#define APP_WINDOW_CLASS                APP_NAME L"WindowClass"

// Registry keys and values
#define APP_REGISTRY_VALUE              APP_NAME
#define APP_SETTINGS_REGISTRY_KEY       L"Software\\" APP_NAME

// UI Strings
#define APP_TRAY_TOOLTIP                APP_NAME L" - Double-click either button to flip"
#define APP_OPTIONS_DIALOG_CAPTION      APP_NAME_A " Options"
#define APP_ABOUT_DIALOG_CAPTION        "About " APP_NAME_A
#define APP_STARTUP_CHECKBOX_TEXT       "Start " APP_NAME_A " when Windows starts"

// Authorship and description, used by the version resource and the About box
#define APP_AUTHOR_A                    "Jonathan Bnayahu"
#define APP_AUTHOR                      WIDEN(APP_AUTHOR_A)
#define APP_COPYRIGHT_A                 "Copyright (c) 2026 Jonathan Bnayahu"
#define APP_COPYRIGHT                   WIDEN(APP_COPYRIGHT_A)
#define APP_DESCRIPTION_A               "Toggle mouse button configuration from the system tray"
#define APP_DESCRIPTION                 WIDEN(APP_DESCRIPTION_A)

#endif // APP_STRINGS_H
