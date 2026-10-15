/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include "common.h"

extern int cmd_action(hidppctl_t *ctl);
extern int cmd_alias(hidppctl_t *ctl);
extern int cmd_backlight(hidppctl_t *ctl);
extern int cmd_battery(hidppctl_t *ctl);
extern int cmd_brightness(hidppctl_t *ctl);
extern int cmd_cache(hidppctl_t *ctl);
extern int cmd_crown(hidppctl_t *ctl);
extern int cmd_disable_keys(hidppctl_t *ctl);
extern int cmd_divert(hidppctl_t *ctl);
extern int cmd_dpi(hidppctl_t *ctl);
extern int cmd_eq(hidppctl_t *ctl);
extern int cmd_fn_inversion(hidppctl_t *ctl);
extern int cmd_host(hidppctl_t *ctl);
extern int cmd_illumination(hidppctl_t *ctl);
extern int cmd_keymap(hidppctl_t *ctl);
extern int cmd_led(hidppctl_t *ctl);
extern int cmd_list_events(hidppctl_t *ctl);
extern int cmd_list_features(hidppctl_t *ctl);
extern int cmd_list_keycodes(hidppctl_t *ctl);
extern int cmd_mode(hidppctl_t *ctl);
extern int cmd_nickname(hidppctl_t *ctl);
extern int cmd_perkey(hidppctl_t *ctl);
extern int cmd_platform(hidppctl_t *ctl);
extern int cmd_pointer(hidppctl_t *ctl);
extern int cmd_poll(hidppctl_t *ctl);
extern int cmd_remap(hidppctl_t *ctl);
extern int cmd_report_rate(hidppctl_t *ctl);
extern int cmd_scroll(hidppctl_t *ctl);
extern int cmd_sidetone(hidppctl_t *ctl);
extern int cmd_smartshift(hidppctl_t *ctl);
extern int cmd_solar(hidppctl_t *ctl);
extern int cmd_status(hidppctl_t *ctl);
extern int cmd_thumbwheel(hidppctl_t *ctl);
extern int cmd_touch_mouse(hidppctl_t *ctl);
extern int cmd_touchpad(hidppctl_t *ctl);
extern int cmd_wheel(hidppctl_t *ctl);

#define DEVICE HIDPPCTL_NEEDS_DEVICE

/* clang-format off */
const struct hidppctl_command_spec hidppctl_commands[hidppctl_command_max] = {
    [HIDPPCTL_NONE] = {
        "none", NULL, HIDPPCTL_NEEDS_NOTHING, 0, 0, "<command>",
        "",
        "Configure HID++ compatible devices.",
    },
    [HIDPPCTL_ACTION] = {
        "action", cmd_action, DEVICE, 0, 5,
        "[list|get <control>|set <control> <type> [value] [modifiers]|reset <control>|all]",
        "shows and changes persistent control actions.",
        "Shows or changes what the controls of a device do. The action is stored on the"
        "\ndevice itself, once for every host (see '--host')."
        "\n"
        "\nTypes: none, key, mouse-button, x, y, vertical-roller, horizontal-roller,"
        "\nconsumer, internal, power. Key actions take a HID usage as the value and"
        "\nan optional list of modifiers, for example 'lctrl,lshift'.",
    },
    [HIDPPCTL_ALIAS] = {
        "alias", cmd_alias, HIDPPCTL_NEEDS_NOTHING, 0, -1,
        "[list|show <name>|set <name> [<key>=<value>...]|remove <name>]",
        "names groups of device picking options.",
        "Saves the options which pick a device under a name. Use that name with"
        "\n'--alias' instead of repeating the options:"
        "\n    hidppctl -r 046d:c548 -d 1 --timeout 500 alias set mouse"
        "\n    hidppctl --alias mouse dpi"
        "\n"
        "\n'set' saves the receiver, socket, device, interface, swid and timeout"
        "\noptions given on the command line, and any <key>=<value> arguments."
        "\nOptions given together with '--alias' override the alias. Aliases are"
        "\nstored in a text file (see '--alias-file'), one per line.",
    },
    [HIDPPCTL_BACKLIGHT] = {
        "backlight", cmd_backlight, DEVICE, 0, 10,
        "[show|set <option>=<value>...|effect <name>]",
        "shows and changes the keyboard backlight.",
        "Shows the backlight capabilities, configuration and state."
        "\n"
        "\n'set' changes the stored configuration. Options: enabled, wow, crown,"
        "\npower-save (on/off), mode (none, auto, temp-manual, perm-manual), effect,"
        "\nlevel, hands-out, hands-in, powered (durations in seconds)."
        "\n'effect' plays an effect until the next power cycle.",
    },
    [HIDPPCTL_BATTERY] = {
        "battery", cmd_battery, DEVICE, 0, 0, "",
        "shows the battery level and capabilities.",
        "Shows the battery level, charging status and capabilities of a device.",
    },
    [HIDPPCTL_BRIGHTNESS] = {
        "brightness", cmd_brightness, DEVICE, 0, 2, "[on|off|set <value>]",
        "controls the brightness of cosmetic lighting.",
        "Shows or changes the brightness of the cosmetic and RGB lighting. The"
        "\nbrightness of white illumination devices is controlled using 'illumination'.",
    },
    [HIDPPCTL_CACHE] = {
        "cache", cmd_cache, DEVICE, 0, 1, "<collect|clear>",
        "manipulates cache files for specified device.",
        "Manipulate the selected device's cache file."
        "\nThe program caches device information automatically both at runtime and"
        "\nbetween sessions. This can significantly improve the speed of most operations"
        "\nand in most cases shouldn't produce problems. However, the user can turn off"
        "\nfile-based hashing by using the '--no-cache' option. Runtime caching cannot"
        "\nbe disabled."
        "\n"
        "\nPassing 'collect' as a parameter will cache all information from all features"
        "\nthat the given device supports.",
    },
    [HIDPPCTL_CROWN] = {
        "crown", cmd_crown, DEVICE, 0, 6, "[set <option>=<value>...]",
        "shows and configures a digital crown.",
        "Shows or changes the crown. Options: reporting (hid, diverted), ratchet"
        "\n(freespin, ratchet), rotation-timeout, short-long-timeout and"
        "\ndouble-tap-speed (milliseconds).",
    },
    [HIDPPCTL_DISABLE_KEYS] = {
        "disable-keys", cmd_disable_keys, DEVICE, 0, 34,
        "[set <keys>|usages <disable|enable|clear> [usages]...]",
        "disables keys of a keyboard.",
        "Disables keys such as caps-lock, num-lock, scroll-lock, insert and windows."
        "\n'set' takes a comma separated list (or 'none') and enables all other keys."
        "\n'usages' disables individual keys by their HID keyboard usage.",
    },
    [HIDPPCTL_DIVERT] = {
        "divert", cmd_divert, DEVICE, 1, HIDPP_MAX_DIVERT, "<buttons...>",
        "diverts events of reprogrammable buttons.",
        "Diverts specified device's buttons and prints associated events."
        "\nYou can also rebind device's buttons using an argument like this:"
        "\n    '<button code>=<key code>[+<modifier>]'"
        "\nFor example: '0x0104=home+lshift'.",
    },
    [HIDPPCTL_DPI] = {
        "dpi", cmd_dpi, DEVICE, 0, 4,
        "[show|set <x> [y] [lod]|led <level> [hold] [button]|calibration [<x|y> <correction>]]",
        "shows and changes the sensor resolution.",
        "Shows or changes the resolution of a motion sensor, which is selected using"
        "\n'--sensor'. The lift off distance is low, medium or high.",
    },
    [HIDPPCTL_EQ] = {
        "eq", cmd_eq, DEVICE, 0, 17,
        "[set <volatile|both|stored> <gains...>|noise-reduction <on|off>]",
        "controls the equalizer of an audio device.",
        "Shows or changes the equalizer. 'set' needs the gain (in dB) of every band;"
        "\nuse '--' before the gains if some are negative.",
    },
    [HIDPPCTL_FN_INVERSION] = {
        "fn-inversion", cmd_fn_inversion, DEVICE, 0, 1, "[on|off]",
        "swaps the function and special keys.",
        "Shows or changes whether the F-keys perform their function (on) or their"
        "\nspecial function (off). Select a host using '--host'.",
    },
    [HIDPPCTL_HOST] = {
        "host", cmd_host, DEVICE, 0, 6,
        "[list|current|switch <host>|name <host> [name]|move <host> <index>|delete <host>|os <host> [...]|cookie <host> [value]|descriptor <host>]",
        "manages the hosts of a multi-host device.",
        "Lists the hosts a device is paired with, switches to another host, or"
        "\nchanges their names and order. Hosts are numbered from 0.",
    },
    [HIDPPCTL_ILLUMINATION] = {
        "illumination", cmd_illumination, DEVICE, 0, -1,
        "[show|on|off|brightness <lumen>|temperature <kelvin>|levels <control> ...]",
        "controls a white illumination device.",
        "Shows or changes a light. 'levels' changes the values the user can select:"
        "\n    levels <control> reset"
        "\n    levels <control> list <values>..."
        "\n    levels <control> linear <min> <max> <step>",
    },
    [HIDPPCTL_KEYMAP] = {
        "keymap", cmd_keymap, DEVICE, 0, 0, "",
        "shows keymap information for specified device.",
        "Shows keymap information for specified device.",
    },
    [HIDPPCTL_LED] = {
        "led", cmd_led, DEVICE, 0, -1,
        "[show|get|set <effect> [<param>=<value>...]|power ...|control ...|shutdown|color|clear|cycling <dir>|pattern <n>]",
        "controls color LEDs and their effects.",
        "Shows or plays effects of the color LEDs. The zone is selected using '--zone'"
        "\n(255 selects all zones)."
        "\n"
        "\nParameters of 'set': color, color2 (RRGGBB), period (ms), speed, intensity,"
        "\nwaveform, direction, ramp, delay, persistence (volatile, stored, both)."
        "\n'power' shows or changes the RGB power mode (full, save, off); 'power"
        "\ntimeouts <save> <off>' changes its timeouts in seconds.",
    },
    [HIDPPCTL_LIST_EVENTS] = {
        "list-events", cmd_list_events, HIDPPCTL_NEEDS_NOTHING, 0, 0, "",
        "lists event identifiers.",
        "Lists supported events and their identifiers.",
    },
    [HIDPPCTL_LIST_FEATURES] = {
        "list-features", cmd_list_features, DEVICE, 0, 0, "",
        "lists supported features for specified device.",
        "Lists supported features for specified device.",
    },
    [HIDPPCTL_LIST_KEYCODES] = {
        "list-keycodes", cmd_list_keycodes, HIDPPCTL_NEEDS_NOTHING, 0, 0, "",
        "lists program's supported keycodes.",
        "Lists supported OS key identifiers.",
    },
    [HIDPPCTL_MODE] = {
        "mode", cmd_mode, DEVICE, 0, 1, "[performance|endurance]",
        "shows and changes the performance mode.",
        "Shows or changes whether the device is in the performance or the"
        "\nendurance (battery saving) mode.",
    },
    [HIDPPCTL_NICKNAME] = {
        "nickname", cmd_nickname, DEVICE, 0, 2, "[set <name>|reset]",
        "shows and changes the device's nickname.",
        "Shows, changes or resets the nickname stored on the device.",
    },
    [HIDPPCTL_PERKEY] = {
        "perkey", cmd_perkey, DEVICE, 0, -1,
        "[zones|set <RRGGBB> <zones...> [volatile|stored]]",
        "controls per-key lighting.",
        "Lists the lighting zones of a keyboard or colors some of them. Zones are"
        "\nwritten as '5', '10-20' or 'all'.",
    },
    [HIDPPCTL_PLATFORM] = {
        "platform", cmd_platform, DEVICE, 0, 2, "[set <platform>]",
        "shows and changes the platform of a host.",
        "Shows or changes the platform (apple, other) a host is configured for."
        "\nSelect the host using '--host'.",
    },
    [HIDPPCTL_POINTER] = {
        "pointer", cmd_pointer, DEVICE, 0, 0, "",
        "shows mouse pointer information.",
        "Shows the typical resolution and suggested acceleration of a mouse.",
    },
    [HIDPPCTL_POLL] = {
        "poll", cmd_poll, HIDPPCTL_NEEDS_RECEIVER, 0, -1, "[events...]",
        "polls specified devices for incoming events.",
        "Polls specified device for incoming events. Pass event names (see"
        "\n'list-events') to only print those.",
    },
    [HIDPPCTL_REMAP] = {
        "remap", cmd_remap, DEVICE, 2, 2, "<control-id> <remap-id>",
        "remaps device's control to a different one.",
        "Remaps device's specified control to a different one.",
    },
    [HIDPPCTL_REPORT_RATE] = {
        "report-rate", cmd_report_rate, DEVICE, 0, 2, "[set <rate>]",
        "shows and changes the report rate.",
        "Shows or changes the report rate. The rate can be written as '1000hz',"
        "\n'1ms' or '1000us' (a number alone means microseconds).",
    },
    [HIDPPCTL_SCROLL] = {
        "scroll", cmd_scroll, DEVICE, 0, 2, "[highres <on|off>]",
        "shows and changes vertical scrolling.",
        "Shows information about the scroll wheel or turns high resolution"
        "\nscrolling on or off.",
    },
    [HIDPPCTL_SIDETONE] = {
        "sidetone", cmd_sidetone, DEVICE, 0, 3, "[set <level>|mute <channels> <on|off>]",
        "controls the sidetone of a headset.",
        "Shows or changes the sidetone level (0 to 100) and which channels"
        "\n(1 to 8, comma separated, or 'all') are muted.",
    },
    [HIDPPCTL_SMARTSHIFT] = {
        "smartshift", cmd_smartshift, DEVICE, 0, 5, "[set <option>=<value>...]",
        "shows and configures SmartShift.",
        "Shows or changes the SmartShift wheel. Options: mode (freespin, ratchet),"
        "\ndisengage (1-254 or 'never'), default-disengage and torque (1-100).",
    },
    [HIDPPCTL_SOLAR] = {
        "solar", cmd_solar, DEVICE, 1, 3, "<measure <count> <period>|led <color>>",
        "measures light with a solar keyboard.",
        "Prints light measurements of a solar keyboard, or lights one of its LEDs"
        "\n(off, red, orange, green) for a moment.",
    },
    [HIDPPCTL_STATUS] = {
        "status", cmd_status, HIDPPCTL_NEEDS_ANYTHING, 0, 0, "",
        "shows status information about HID++ devices.",
        "Shows status information about HID++ devices. Without options, it lists"
        "\nall receivers. With '-r', it lists the devices connected to the receiver"
        "\nand with '-d' it shows information about a single device.",
    },
    [HIDPPCTL_THUMBWHEEL] = {
        "thumbwheel", cmd_thumbwheel, DEVICE, 0, 3, "[set diverted=<on|off> inverted=<on|off>]",
        "shows and configures the thumbwheel.",
        "Shows or changes how the thumbwheel reports its rotation.",
    },
    [HIDPPCTL_TOUCH_MOUSE] = {
        "touch-mouse", cmd_touch_mouse, DEVICE, 0, 2, "[mode <mode>]",
        "shows and configures a touch mouse.",
        "Shows or changes the raw mode: native, raw, raw-gestures, raw-always,"
        "\nraw-gestures-z.",
    },
    [HIDPPCTL_TOUCHPAD] = {
        "touchpad", cmd_touchpad, DEVICE, 0, 2, "[mode <reports>]",
        "shows and configures a raw touchpad.",
        "Shows or changes the raw reports: a comma separated list (or 'none') of raw,"
        "\nforce, enhanced, width-height, native-gestures, major-minor, width-height8.",
    },
    [HIDPPCTL_WHEEL] = {
        "wheel", cmd_wheel, DEVICE, 0, 5, "[set <option>=<on|off>...|analytics]",
        "shows and configures a high resolution wheel.",
        "Shows or changes how the wheel reports. Options: diverted, highres,"
        "\ninverted, analytics. 'analytics' reads (and clears) the collected data.",
    },
};
/* clang-format on */
