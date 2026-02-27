/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include <hidpp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

    #include <windows.h>

    #define HIDPP_MAX_INPUT 32

static INPUT make_input(int vk, int flags) {
    INPUT in = { 0 };
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = (WORD)vk;
    in.ki.dwFlags = flags;
    return in;
}

struct {
    const char name[12];
    int vk;
} hidpp_input_table[] = {
    /* clang-format off */
    /* Letters (VK codes == uppercase ASCII) */
    {"a", 'A'}, {"b", 'B'}, {"c", 'C'}, {"d", 'D'}, {"e", 'E'},
    {"f", 'F'}, {"g", 'G'}, {"h", 'H'}, {"i", 'I'}, {"j", 'J'},
    {"k", 'K'}, {"l", 'L'}, {"m", 'M'}, {"n", 'N'}, {"o", 'O'},
    {"p", 'P'}, {"q", 'Q'}, {"r", 'R'}, {"s", 'S'}, {"t", 'T'},
    {"u", 'U'}, {"v", 'V'}, {"w", 'W'}, {"x", 'X'}, {"y", 'Y'},
    {"z", 'Z'},
    /* Digits */
    {"0", '0'}, {"1", '1'}, {"2", '2'}, {"3", '3'}, {"4", '4'},
    {"5", '5'}, {"6", '6'}, {"7", '7'}, {"8", '8'}, {"9", '9'},
    /* Modifiers */
    {"shift",  VK_SHIFT},   {"lshift", VK_LSHIFT},  {"rshift", VK_RSHIFT},
    {"ctrl",   VK_CONTROL}, {"lctrl",  VK_LCONTROL},{"rctrl",  VK_RCONTROL},
    {"alt",    VK_MENU},    {"lalt",   VK_LMENU},   {"ralt",   VK_RMENU},
    {"win",    VK_LWIN},    {"lwin",   VK_LWIN},    {"rwin",   VK_RWIN},
    /* Function keys */
    {"f1",  VK_F1},  {"f2",  VK_F2},  {"f3",  VK_F3},  {"f4",  VK_F4},
    {"f5",  VK_F5},  {"f6",  VK_F6},  {"f7",  VK_F7},  {"f8",  VK_F8},
    {"f9",  VK_F9},  {"f10", VK_F10}, {"f11", VK_F11}, {"f12", VK_F12},
    /* Navigation & editing */
    {"enter",      VK_RETURN},  {"return",    VK_RETURN},
    {"space",      VK_SPACE},   {"tab",       VK_TAB},
    {"backspace",  VK_BACK},    {"delete",    VK_DELETE},
    {"insert",     VK_INSERT},  {"escape",    VK_ESCAPE},
    {"esc",        VK_ESCAPE},  {"home",      VK_HOME},
    {"end",        VK_END},     {"pageup",    VK_PRIOR},
    {"pagedown",   VK_NEXT},    {"up",        VK_UP},
    {"down",       VK_DOWN},    {"left",      VK_LEFT},
    {"right",      VK_RIGHT},
    /* Locks */
    {"capslock",   VK_CAPITAL}, {"numlock",   VK_NUMLOCK},
    {"scrolllock", VK_SCROLL},
    /* Punctuation / OEM keys (standard US layout) */
    {"minus",      VK_OEM_MINUS},  {"equal",     VK_OEM_PLUS},
    {"leftbrace",  VK_OEM_4},      {"rightbrace",VK_OEM_6},
    {"backslash",  VK_OEM_5},      {"semicolon", VK_OEM_1},
    {"apostrophe", VK_OEM_7},      {"grave",     VK_OEM_3},
    {"comma",      VK_OEM_COMMA},  {"dot",       VK_OEM_PERIOD},
    {"slash",      VK_OEM_2},
    /* Media / extra */
    {"printscreen",VK_SNAPSHOT},   {"pause",     VK_PAUSE},
    {"apps",       VK_APPS},
    {"volup",      VK_VOLUME_UP},  {"voldown",   VK_VOLUME_DOWN},
    {"mute",       VK_VOLUME_MUTE},
    {"mediaplay",  VK_MEDIA_PLAY_PAUSE},
    {"medianext",  VK_MEDIA_NEXT_TRACK},
    {"mediaprev",  VK_MEDIA_PREV_TRACK},
    { "", 0 },
    /* clang-format on */
};

#else

    #include <fcntl.h>
    #include <linux/uinput.h>
    #include <unistd.h>

static void uinput_emit(int fd, int type, int code, int value) {
    struct input_event ev = { 0 };
    ev.type = type;
    ev.code = code;
    ev.value = value;
    write(fd, &ev, sizeof(ev));
}

struct {
    const char name[12];
    int code;
} hidpp_input_table[] = {
    /* clang-format off */
    /* Letters */
    {"a", KEY_A}, {"b", KEY_B}, {"c", KEY_C}, {"d", KEY_D},
    {"e", KEY_E}, {"f", KEY_F}, {"g", KEY_G}, {"h", KEY_H},
    {"i", KEY_I}, {"j", KEY_J}, {"k", KEY_K}, {"l", KEY_L},
    {"m", KEY_M}, {"n", KEY_N}, {"o", KEY_O}, {"p", KEY_P},
    {"q", KEY_Q}, {"r", KEY_R}, {"s", KEY_S}, {"t", KEY_T},
    {"u", KEY_U}, {"v", KEY_V}, {"w", KEY_W}, {"x", KEY_X},
    {"y", KEY_Y}, {"z", KEY_Z},
    /* Numbers */
    {"0", KEY_0}, {"1", KEY_1}, {"2", KEY_2}, {"3", KEY_3},
    {"4", KEY_4}, {"5", KEY_5}, {"6", KEY_6}, {"7", KEY_7},
    {"8", KEY_8}, {"9", KEY_9},
    /* Modifiers */
    {"shift",      KEY_LEFTSHIFT},  {"lshift",     KEY_LEFTSHIFT},
    {"rshift",     KEY_RIGHTSHIFT}, {"ctrl",       KEY_LEFTCTRL},
    {"lctrl",      KEY_LEFTCTRL},   {"rctrl",      KEY_RIGHTCTRL},
    {"alt",        KEY_LEFTALT},    {"lalt",       KEY_LEFTALT},
    {"ralt",       KEY_RIGHTALT},   {"meta",       KEY_LEFTMETA},
    {"super",      KEY_LEFTMETA},   {"lmeta",      KEY_LEFTMETA},
    /* Function keys */
    {"f1",  KEY_F1},  {"f2",  KEY_F2},  {"f3",  KEY_F3},  {"f4",  KEY_F4},
    {"f5",  KEY_F5},  {"f6",  KEY_F6},  {"f7",  KEY_F7},  {"f8",  KEY_F8},
    {"f9",  KEY_F9},  {"f10", KEY_F10}, {"f11", KEY_F11}, {"f12", KEY_F12},
    /* Navigation & special */
    {"enter",     KEY_ENTER},     {"return",    KEY_ENTER},
    {"space",     KEY_SPACE},     {"tab",       KEY_TAB},
    {"backspace", KEY_BACKSPACE}, {"escape",    KEY_ESC},
    {"esc",       KEY_ESC},       {"delete",    KEY_DELETE},
    {"insert",    KEY_INSERT},    {"home",      KEY_HOME},
    {"end",       KEY_END},       {"pageup",    KEY_PAGEUP},
    {"pagedown",  KEY_PAGEDOWN},  {"up",        KEY_UP},
    {"down",      KEY_DOWN},      {"left",      KEY_LEFT},
    {"right",     KEY_RIGHT},     {"capslock",  KEY_CAPSLOCK},
    {"printscreen", KEY_SYSRQ},   {"scrolllock",KEY_SCROLLLOCK},
    {"pause",     KEY_PAUSE},     {"numlock",   KEY_NUMLOCK},
    /* Punctuation */
    {"minus",     KEY_MINUS},     {"equal",     KEY_EQUAL},
    {"leftbrace", KEY_LEFTBRACE}, {"rightbrace",KEY_RIGHTBRACE},
    {"backslash",  KEY_BACKSLASH},{"semicolon", KEY_SEMICOLON},
    {"apostrophe",KEY_APOSTROPHE},{"grave",     KEY_GRAVE},
    {"comma",     KEY_COMMA},     {"dot",       KEY_DOT},
    {"slash",     KEY_SLASH},
    { "", 0 },
    /* clang-format on */
};

int hidpp_input_key(const char *name) {
    if (!name)
        return HIDPP_EINVAL;

    char *end;
    int value = strtol(name, &end, 0);

    if (end != name)
        return value;

    for (size_t i = 0; hidpp_input_table[i].code; i++) {
    #ifdef _WIN32
        if (_stricmp(name, hidpp_input_table[i].name) == 0)
            return hidpp_input_table[i].code;
    #else
        if (strcasecmp(name, hidpp_input_table[i].name) == 0)
            return hidpp_input_table[i].code;
    #endif
    }

    return HIDPP_ENOENT;
}

#endif

int hidpp_input_open(void) {
#ifdef _WIN32
    return 0;
#else
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    if (fd < 0)
        return -1;

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_SYN);

    for (int i = 0; i < KEY_MAX; i++)
        ioctl(fd, UI_SET_KEYBIT, i);

    struct uinput_setup usetup = { 0 };
    usetup.id.bustype = BUS_VIRTUAL;
    usetup.id.vendor = 0x0000;
    usetup.id.product = 0x682b;
    strncpy(usetup.name, "hidppctl", UINPUT_MAX_NAME_SIZE);

    ioctl(fd, UI_DEV_SETUP, &usetup);
    ioctl(fd, UI_DEV_CREATE);
    sleep(1);

    return fd;
#endif
}

void hidpp_input_close(int fd) {
#ifndef _WIN32
    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
#endif
}

int hidpp_input_set(int fd, int key, int *mods, size_t count, int value) {
    if (fd < 0 || (!mods && count > 0))
        return HIDPP_EINVAL;

#ifdef _WIN32
    if (count + 1 > HIDPP_MAX_INPUT)
        return HIDPP_ENOMEM;

    INPUT inputs[HIDPP_MAX_INPUT];

    for (size_t i = 0; i <= count; i++) {
        int code = i == count ? key : mods[i - 1];

        if (code < 0)
            return HIDPP_EINVAL;

        inputs[i] = make_key_input(code, value ? 0 : KEYEVENTF_KEYUP);
    }

    UINT sent = SendInput(count + 1, inputs, sizeof(INPUT));

    if (sent != count + 1)
        return HIDPP_EIO;

    return HIDPP_OK;
#else

    for (int i = 0; i < count; i++)
        uinput_emit(fd, EV_KEY, mods[i], value ? 1 : 0);
    if (count > 0)
        uinput_emit(fd, EV_SYN, SYN_REPORT, 0);

    uinput_emit(fd, EV_KEY, key, value ? 1 : 0);
    uinput_emit(fd, EV_SYN, SYN_REPORT, 0);

    return HIDPP_OK;
#endif
}

int hidpp_input_press(int fd, int key, int *mods, size_t count) {
    return hidpp_input_set(fd, key, mods, count, 1);
}

int hidpp_input_release(int fd, int key, int *mods, size_t count) {
    return hidpp_input_set(fd, key, mods, count, 0);
}
