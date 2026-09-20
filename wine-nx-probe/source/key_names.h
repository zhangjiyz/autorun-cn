/*
 * The controls a Switch has, and the keys they can be made to send.
 *
 * keys.txt is a NAME=code line for each control, where code is a Windows
 * virtual-key code or 0x100/0x101 for a mouse button. The Controls screen writes those lines, so it
 * needs the same names the runtime reads and a readable name for each code to
 * put on the screen. A code with no name here is shown as its number, which is
 * what a hand-written file may hold.
 */
#ifndef WINE_NX_KEY_NAMES_H
#define WINE_NX_KEY_NAMES_H

#include <stddef.h>
#include <stdio.h>
#include "wine/nx_input_codes.h"

struct wine_nx_control
{
    const char *name;      /* as keys.txt spells it */
    const char *label;     /* as the screen says it */
    const char *unset;     /* what no key at all means for this control */
    unsigned short sends;  /* what it sends with no line of its own */
};

/* In the order a hand falls on them: the face buttons, the shoulders, the
 * middle, then the two ways of steering. */
static const struct wine_nx_control wine_nx_controls[] =
{
    { "A",      "A",                 "Left mouse button",    0x00 },
    { "B",      "B",                 "Right mouse button",   0x00 },
    { "X",      "X",                 "Nothing",              0x20 },
    { "Y",      "Y",                 "Nothing",              0x46 },
    { "L",      "L",                 "Nothing",              0x09 },
    { "R",      "R",                 "Nothing",              0x10 },
    { "ZL",     "ZL",                "Nothing",              0x28 },
    { "ZR",     "ZR",                "Nothing",              0x26 },
    { "PLUS",   "Plus",              "Nothing",              0x1b },
    { "MINUS",  "Minus",             "Nothing",              0x09 },
    { "STICKL", "Left stick press",  "Nothing",              0x11 },
    { "STICKR", "Right stick press", "Nothing",              0x12 },
    { "UP",     "D-pad up",          "Nothing",              0x26 },
    { "DOWN",   "D-pad down",        "Nothing",              0x28 },
    { "LEFT",   "D-pad left",        "Nothing",              0x25 },
    { "RIGHT",  "D-pad right",       "Nothing",              0x27 },
    { "LUP",    "Left stick up",     "What the d-pad sends", 0x00 },
    { "LDOWN",  "Left stick down",   "What the d-pad sends", 0x00 },
    { "LLEFT",  "Left stick left",   "What the d-pad sends", 0x00 },
    { "LRIGHT", "Left stick right",  "What the d-pad sends", 0x00 },
    { "RUP",    "Right stick up",     "Nothing",             0x26 },
    { "RDOWN",  "Right stick down",   "Nothing",             0x28 },
    { "RLEFT",  "Right stick left",   "Nothing",             0x25 },
    { "RRIGHT", "Right stick right",  "Nothing",             0x27 },
    { "TUP",    "Finger up",          "Nothing",             0x26 },
    { "TDOWN",  "Finger down",        "Nothing",             0x28 },
    { "TLEFT",  "Finger left",        "Nothing",             0x25 },
    { "TRIGHT", "Finger right",       "Nothing",             0x27 },
};

#define WINE_NX_CONTROL_COUNT ((int)(sizeof(wine_nx_controls) / sizeof(wine_nx_controls[0])))

/* The three things that point. Each either moves the mouse or sends the four
 * keys of its own, which is what keys.txt says with LSTICK=mouse or RSTICK=keys.
 * `first` is where its four are in the list above. */
struct wine_nx_device
{
    const char *name;    /* as keys.txt spells it */
    const char *label;   /* as the screen says it */
    int first;           /* its Up control */
    int follows_dpad;    /* its keys unset means the d-pad's, as the left stick's do */
    int points;          /* what it does with no line of its own */
};

static const struct wine_nx_device wine_nx_devices[] =
{
    { "LSTICK", "Left stick",  16, 1, 0 },
    { "RSTICK", "Right stick", 20, 0, 1 },
    { "DPAD",   "D-pad",       12, 0, 0 },
    { "TOUCH",  "Finger drag", 24, 0, 1 },
};

#define WINE_NX_DEVICE_COUNT_UI ((int)(sizeof(wine_nx_devices) / sizeof(wine_nx_devices[0])))

/* Up, down, left, right, for the two sets of keys worth a name of their own. */
static const unsigned short wine_nx_preset_arrows[4] = { 0x26, 0x28, 0x25, 0x27 };
static const unsigned short wine_nx_preset_wasd[4]   = { 0x57, 0x53, 0x41, 0x44 };

struct wine_nx_key_name
{
    unsigned short code;
    const char *name;
};

/* The keys a game is likely to want, the common ones first so that holding a
 * direction on the list reaches them before the alphabet. */
static const struct wine_nx_key_name wine_nx_key_names[] =
{
    { 0x00, "Nothing" },
    { WINE_NX_MOUSE_LEFT, "Left mouse button" },
    { WINE_NX_MOUSE_RIGHT, "Right mouse button" },
    { 0x0d, "Enter" },       { 0x20, "Space" },        { 0x1b, "Escape" },
    { 0x09, "Tab" },         { 0x08, "Backspace" },
    { 0x10, "Shift" },       { 0x11, "Control" },      { 0x12, "Alt" },
    { 0x26, "Up arrow" },    { 0x28, "Down arrow" },
    { 0x25, "Left arrow" },  { 0x27, "Right arrow" },
    { 0x41, "A" }, { 0x42, "B" }, { 0x43, "C" }, { 0x44, "D" }, { 0x45, "E" },
    { 0x46, "F" }, { 0x47, "G" }, { 0x48, "H" }, { 0x49, "I" }, { 0x4a, "J" },
    { 0x4b, "K" }, { 0x4c, "L" }, { 0x4d, "M" }, { 0x4e, "N" }, { 0x4f, "O" },
    { 0x50, "P" }, { 0x51, "Q" }, { 0x52, "R" }, { 0x53, "S" }, { 0x54, "T" },
    { 0x55, "U" }, { 0x56, "V" }, { 0x57, "W" }, { 0x58, "X" }, { 0x59, "Y" },
    { 0x5a, "Z" },
    { 0x30, "0" }, { 0x31, "1" }, { 0x32, "2" }, { 0x33, "3" }, { 0x34, "4" },
    { 0x35, "5" }, { 0x36, "6" }, { 0x37, "7" }, { 0x38, "8" }, { 0x39, "9" },
    { 0x70, "F1" },  { 0x71, "F2" },  { 0x72, "F3" },  { 0x73, "F4" },
    { 0x74, "F5" },  { 0x75, "F6" },  { 0x76, "F7" },  { 0x77, "F8" },
    { 0x78, "F9" },  { 0x79, "F10" }, { 0x7a, "F11" }, { 0x7b, "F12" },
    { 0x2d, "Insert" },   { 0x2e, "Delete" },    { 0x24, "Home key" },
    { 0x23, "End" },      { 0x21, "Page up" },   { 0x22, "Page down" },
    { 0x14, "Caps lock" }, { 0x2c, "Print screen" }, { 0x13, "Pause" },
    { 0x60, "Numpad 0" }, { 0x61, "Numpad 1" }, { 0x62, "Numpad 2" },
    { 0x63, "Numpad 3" }, { 0x64, "Numpad 4" }, { 0x65, "Numpad 5" },
    { 0x66, "Numpad 6" }, { 0x67, "Numpad 7" }, { 0x68, "Numpad 8" },
    { 0x69, "Numpad 9" }, { 0x6a, "Numpad *" }, { 0x6b, "Numpad +" },
    { 0x6d, "Numpad -" }, { 0x6e, "Numpad ." }, { 0x6f, "Numpad /" },
    { 0xba, "Semicolon" }, { 0xbb, "Equals" },    { 0xbc, "Comma" },
    { 0xbd, "Minus" },     { 0xbe, "Period" },    { 0xbf, "Slash" },
    { 0xc0, "Backtick" },  { 0xdb, "Left bracket" }, { 0xdc, "Backslash" },
    { 0xdd, "Right bracket" }, { 0xde, "Apostrophe" },
};

#define WINE_NX_KEY_NAME_COUNT ((int)(sizeof(wine_nx_key_names) / sizeof(wine_nx_key_names[0])))

/* Where a code sits in the list, or -1 for one the list does not name. */
static inline int wine_nx_key_index( unsigned short code )
{
    int i;

    for (i = 0; i < WINE_NX_KEY_NAME_COUNT; i++)
        if (wine_nx_key_names[i].code == code) return i;
    return -1;
}

/* What to show for a control set to this code. A control that sends nothing
 * says what that means for it, which is not the same for every one: A with no
 * key of its own is the left mouse button, and the left stick steers with the
 * d-pad. */
static inline const char *wine_nx_key_label( int control, unsigned short code, char *out, size_t size )
{
    int i = wine_nx_key_index( code );

    if (!code && control >= 0 && control < WINE_NX_CONTROL_COUNT)
        snprintf( out, size, "%s", wine_nx_controls[control].unset );
    else if (i >= 0) snprintf( out, size, "%s", wine_nx_key_names[i].name );
    else snprintf( out, size, "0x%02x", code );
    return out;
}

#endif /* WINE_NX_KEY_NAMES_H */
