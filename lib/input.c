/*  hidppctl -- Configure HID++ compatible peripherals.

    Copyright (C) 2026 Предраг Јовановић
    SPDX-FileCopyrightText: 2026 Предраг Јовановић
    SPDX-License-Identifier: GPL-3.0-or-later

    Look at the COPYING file for more information.
*/

#include <hidpp.h>

#include <allocator.h>
#include <allocator_std.h>

#include <assert.h>
#include <pf_ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct hidpp_input_t {
    int fd; /* unused on Windows */
};

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
    int code;
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

#endif

static_assert(
    sizeof(hidpp_input_table[0].name) == 12,
    "hidpp_input_table name field size changed"
);

int hidpp_input_key(const char *name) {
    if (!name)
        return HIDPP_EINVAL;

    /* Only treat the input as a raw numeric code if it is made up
       entirely of digits (optionally signed / 0x-prefixed) -- this
       stops single-digit names like "0".."9" from being shadowed by
       strtol() before the name table (which maps them to KEY_0..KEY_9,
       not the literal value 0..9) ever gets consulted. */
    char *end;
    int value = strtol(name, &end, 0);

    if (end != name && *end == '\0' && !pf_isalpha((unsigned char)name[0]))
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

hidpp_input_t *hidpp_input_new(allocator_t *allocator) {
    if (!allocator)
        allocator = &standard_allocator;

    hidpp_input_t *input = allocate(allocator, sizeof(*input));

    if (!input)
        return NULL;

#ifdef _WIN32
    input->fd = 0;
#else
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    if (fd < 0) {
        deallocate(allocator, input, sizeof(*input));
        return NULL;
    }

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_SYN);
    ioctl(fd, UI_SET_EVBIT, EV_REL);

    for (int i = 0; i < KEY_MAX; i++)
        ioctl(fd, UI_SET_KEYBIT, i);

    ioctl(fd, UI_SET_RELBIT, REL_X);
    ioctl(fd, UI_SET_RELBIT, REL_Y);
    ioctl(fd, UI_SET_RELBIT, REL_WHEEL);
    ioctl(fd, UI_SET_RELBIT, REL_HWHEEL);

    struct uinput_setup usetup = { 0 };
    usetup.id.bustype = BUS_VIRTUAL;
    usetup.id.vendor = 0x0000;
    usetup.id.product = 0x682b;
    strncpy(usetup.name, "hidppctl", UINPUT_MAX_NAME_SIZE);

    ioctl(fd, UI_DEV_SETUP, &usetup);
    ioctl(fd, UI_DEV_CREATE);
    sleep(1);

    input->fd = fd;
#endif

    return input;
}

void hidpp_input_free(hidpp_input_t *input) {
    if (!input)
        return;

#ifndef _WIN32
    ioctl(input->fd, UI_DEV_DESTROY);
    close(input->fd);
#else
    (void)input;
#endif
}

int hidpp_input_set(
    hidpp_input_t *input, int key, int *mods, size_t count, int value
) {
    if (!input || (!mods && count > 0))
        return HIDPP_EINVAL;

#ifdef _WIN32
    if (count + 1 > HIDPP_MAX_INPUT)
        return HIDPP_ENOMEM;

    INPUT inputs[HIDPP_MAX_INPUT];

    for (size_t i = 0; i <= count; i++) {
        int code = i == count ? key : mods[i - 1];

        if (code < 0)
            return HIDPP_EINVAL;

        inputs[i] = make_input(code, value ? 0 : KEYEVENTF_KEYUP);
    }

    UINT sent = SendInput(count + 1, inputs, sizeof(INPUT));

    if (sent != count + 1)
        return HIDPP_EIO;

    return HIDPP_OK;
#else
    for (size_t i = 0; i < count; i++)
        uinput_emit(input->fd, EV_KEY, mods[i], value ? 1 : 0);
    if (count > 0)
        uinput_emit(input->fd, EV_SYN, SYN_REPORT, 0);

    uinput_emit(input->fd, EV_KEY, key, value ? 1 : 0);
    uinput_emit(input->fd, EV_SYN, SYN_REPORT, 0);

    return HIDPP_OK;
#endif
}

int hidpp_input_press(hidpp_input_t *input, int key, int *mods, size_t count) {
    return hidpp_input_set(input, key, mods, count, 1);
}

int hidpp_input_release(
    hidpp_input_t *input, int key, int *mods, size_t count
) {
    return hidpp_input_set(input, key, mods, count, 0);
}

int hidpp_input_move(hidpp_input_t *input, int dx, int dy) {
    if (!input)
        return HIDPP_EINVAL;

#ifdef _WIN32
    INPUT in = { 0 };
    in.type = INPUT_MOUSE;
    in.mi.dx = dx;
    in.mi.dy = dy;
    in.mi.dwFlags = MOUSEEVENTF_MOVE;

    if (SendInput(1, &in, sizeof(INPUT)) != 1)
        return HIDPP_EIO;

    return HIDPP_OK;
#else
    if (dx)
        uinput_emit(input->fd, EV_REL, REL_X, dx);
    if (dy)
        uinput_emit(input->fd, EV_REL, REL_Y, dy);
    uinput_emit(input->fd, EV_SYN, SYN_REPORT, 0);

    return HIDPP_OK;
#endif
}

int hidpp_input_button(hidpp_input_t *input, int button, int value) {
    if (!input)
        return HIDPP_EINVAL;

#ifdef _WIN32
    DWORD flags;

    switch (button) {
    case HIDPP_BTN_LEFT:
        flags = value ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        break;
    case HIDPP_BTN_RIGHT:
        flags = value ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
        break;
    case HIDPP_BTN_MIDDLE:
        flags = value ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
        break;
    case HIDPP_BTN_SIDE:
    case HIDPP_BTN_EXTRA:
        flags = value ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
        break;
    default:
        return HIDPP_EINVAL;
    }

    INPUT in = { 0 };
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = flags;

    if (button == HIDPP_BTN_SIDE)
        in.mi.mouseData = XBUTTON1;
    else if (button == HIDPP_BTN_EXTRA)
        in.mi.mouseData = XBUTTON2;

    if (SendInput(1, &in, sizeof(INPUT)) != 1)
        return HIDPP_EIO;

    return HIDPP_OK;
#else
    int code;

    switch (button) {
    case HIDPP_BTN_LEFT:
        code = BTN_LEFT;
        break;
    case HIDPP_BTN_RIGHT:
        code = BTN_RIGHT;
        break;
    case HIDPP_BTN_MIDDLE:
        code = BTN_MIDDLE;
        break;
    case HIDPP_BTN_SIDE:
        code = BTN_SIDE;
        break;
    case HIDPP_BTN_EXTRA:
        code = BTN_EXTRA;
        break;
    default:
        return HIDPP_EINVAL;
    }

    uinput_emit(input->fd, EV_KEY, code, value ? 1 : 0);
    uinput_emit(input->fd, EV_SYN, SYN_REPORT, 0);

    return HIDPP_OK;
#endif
}

int hidpp_input_scroll(hidpp_input_t *input, int dx, int dy) {
    if (!input)
        return HIDPP_EINVAL;

#ifdef _WIN32
    int ret = HIDPP_OK;

    if (dy) {
        INPUT in = { 0 };
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = MOUSEEVENTF_WHEEL;
        in.mi.mouseData = dy * WHEEL_DELTA;

        if (SendInput(1, &in, sizeof(INPUT)) != 1)
            ret = HIDPP_EIO;
    }

    if (dx) {
        INPUT in = { 0 };
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = MOUSEEVENTF_HWHEEL;
        in.mi.mouseData = dx * WHEEL_DELTA;

        if (SendInput(1, &in, sizeof(INPUT)) != 1)
            ret = HIDPP_EIO;
    }

    return ret;
#else
    /* REL_WHEEL/REL_HWHEEL follow "positive is up/right" like the
       public API; uinput's convention already matches that, so no
       sign flip is needed here. */
    if (dy)
        uinput_emit(input->fd, EV_REL, REL_WHEEL, dy);
    if (dx)
        uinput_emit(input->fd, EV_REL, REL_HWHEEL, dx);
    uinput_emit(input->fd, EV_SYN, SYN_REPORT, 0);

    return HIDPP_OK;
#endif
}

/* Maps punctuation characters not already covered by hidpp_input_table
   (letters/digits) to a key name plus whether shift is required, for
   hidpp_input_type(). US-layout only. */
static const struct {
    char ch;
    const char *key;
    int shift;
} hidpp_char_table[] = {
    /* clang-format off */
    {'\t', "tab", 0},
    {'-', "minus", 0},      {'_', "minus", 1},
    {'=', "equal", 0},      {'+', "equal", 1},
    {'[', "leftbrace", 0},  {'{', "leftbrace", 1},
    {']', "rightbrace", 0}, {'}', "rightbrace", 1},
    {'\\', "backslash", 0}, {'|', "backslash", 1},
    {';', "semicolon", 0},  {':', "semicolon", 1},
    {'\'', "apostrophe", 0},{'"', "apostrophe", 1},
    {'`', "grave", 0},      {'~', "grave", 1},
    {',', "comma", 0},      {'<', "comma", 1},
    {'.', "dot", 0},        {'>', "dot", 1},
    {'/', "slash", 0},      {'?', "slash", 1},
    {'1', "1", 0}, {'!', "1", 1},
    {'2', "2", 0}, {'@', "2", 1},
    {'3', "3", 0}, {'#', "3", 1},
    {'4', "4", 0}, {'$', "4", 1},
    {'5', "5", 0}, {'%', "5", 1},
    {'6', "6", 0}, {'^', "6", 1},
    {'7', "7", 0}, {'&', "7", 1},
    {'8', "8", 0}, {'*', "8", 1},
    {'9', "9", 0}, {'(', "9", 1},
    {'0', "0", 0}, {')', "0", 1},
    /* clang-format on */
};

int hidpp_input_type(hidpp_input_t *input, const char *text) {
    if (!input || !text)
        return HIDPP_EINVAL;

    int shiftKey = hidpp_input_key("shift");

    if (shiftKey < 0)
        return shiftKey;

    for (const char *p = text; *p; p++) {
        char c = *p;
        const char *keyName = NULL;
        int shift = 0;

        if (c == ' ') {
            keyName = "space";
        } else if (c == '\n') {
            keyName = "enter";
        } else if (pf_isalpha((unsigned char)c)) {
            static char letter[2] = { 0, 0 };
            letter[0] = (char)pf_tolower((unsigned char)c);
            keyName = letter;
            shift = pf_isupper((unsigned char)c);
        } else {
            for (size_t i = 0;
                 i < sizeof(hidpp_char_table) / sizeof(hidpp_char_table[0]);
                 i++) {
                if (hidpp_char_table[i].ch == c) {
                    keyName = hidpp_char_table[i].key;
                    shift = hidpp_char_table[i].shift;
                    break;
                }
            }
        }

        if (!keyName)
            continue; /* unmappable character: skip it */

        int key = hidpp_input_key(keyName);

        if (key < 0)
            continue;

        int mods[1];
        size_t modCount = 0;

        if (shift) {
            mods[0] = shiftKey;
            modCount = 1;
        }

        int ret = hidpp_input_press(input, key, mods, modCount);
        if (ret < 0)
            return ret;

        ret = hidpp_input_release(input, key, mods, modCount);
        if (ret < 0)
            return ret;
    }

    return HIDPP_OK;
}