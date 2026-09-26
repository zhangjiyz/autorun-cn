/*
 * The Switch controller as an XInput gamepad, for Wine-NX.
 *
 * Shared by xinput1_3's main.c, which calls the runtime through a static unix
 * call table (wine-nx-probe/source/xinput_unix.c), and by the host test. The
 * buttons sit where an Xbox pad has them: the Switch's bottom button (B) is A,
 * its right one (A) is B, and so on. ZL and ZR are digital, so the triggers are
 * either released or fully pressed.
 */

#ifndef __WINE_XINPUT_NX_PAD_H
#define __WINE_XINPUT_NX_PAD_H

#include "wine/nx_input_codes.h"

enum nx_xinput_funcs
{
    nx_xinput_get_state,
    nx_xinput_set_state,
    nx_xinput_funcs_count
};

/* No pointers, so the 32-bit module and the runtime share the layout. */
struct nx_xinput_state_params
{
    DWORD index;
    DWORD connected;
    XINPUT_STATE state;
};

struct nx_xinput_vibration_params
{
    DWORD index;
    DWORD connected;
    XINPUT_VIBRATION vibration;
};

/* libnx's HidNpadButton bits; xinput_unix.c checks them against switch.h. */
#define NX_PAD_A      (1ull << 0)
#define NX_PAD_B      (1ull << 1)
#define NX_PAD_X      (1ull << 2)
#define NX_PAD_Y      (1ull << 3)
#define NX_PAD_STICKL (1ull << 4)
#define NX_PAD_STICKR (1ull << 5)
#define NX_PAD_L      (1ull << 6)
#define NX_PAD_R      (1ull << 7)
#define NX_PAD_ZL     (1ull << 8)
#define NX_PAD_ZR     (1ull << 9)
#define NX_PAD_PLUS   (1ull << 10)
#define NX_PAD_MINUS  (1ull << 11)
#define NX_PAD_LEFT   (1ull << 12)
#define NX_PAD_UP     (1ull << 13)
#define NX_PAD_RIGHT  (1ull << 14)
#define NX_PAD_DOWN   (1ull << 15)

static inline SHORT nx_xinput_axis( int value )
{
    if (value > 32767) return 32767;
    if (value < -32768) return -32768;
    return (SHORT)value;
}

/* A Switch pad state (libnx buttons, sticks in +-32767 with up positive, as
 * XInput has them) as an XINPUT_GAMEPAD. */
static inline void nx_xinput_map( unsigned long long buttons, int lx, int ly, int rx, int ry,
                                  XINPUT_GAMEPAD *gamepad )
{
    static const struct
    {
        unsigned long long nx;
        WORD xinput;
    } map[] =
    {
        { NX_PAD_UP, XINPUT_GAMEPAD_DPAD_UP },
        { NX_PAD_DOWN, XINPUT_GAMEPAD_DPAD_DOWN },
        { NX_PAD_LEFT, XINPUT_GAMEPAD_DPAD_LEFT },
        { NX_PAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT },
        { NX_PAD_PLUS, XINPUT_GAMEPAD_START },
        { NX_PAD_MINUS, XINPUT_GAMEPAD_BACK },
        { NX_PAD_STICKL, XINPUT_GAMEPAD_LEFT_THUMB },
        { NX_PAD_STICKR, XINPUT_GAMEPAD_RIGHT_THUMB },
        { NX_PAD_L, XINPUT_GAMEPAD_LEFT_SHOULDER },
        { NX_PAD_R, XINPUT_GAMEPAD_RIGHT_SHOULDER },
        { NX_PAD_B, XINPUT_GAMEPAD_A },
        { NX_PAD_A, XINPUT_GAMEPAD_B },
        { NX_PAD_Y, XINPUT_GAMEPAD_X },
        { NX_PAD_X, XINPUT_GAMEPAD_Y },
    };
    unsigned int i;

    gamepad->wButtons = 0;
    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (buttons & map[i].nx) gamepad->wButtons |= map[i].xinput;
    gamepad->bLeftTrigger = (buttons & NX_PAD_ZL) ? 255 : 0;
    gamepad->bRightTrigger = (buttons & NX_PAD_ZR) ? 255 : 0;
    gamepad->sThumbLX = nx_xinput_axis( lx );
    gamepad->sThumbLY = nx_xinput_axis( ly );
    gamepad->sThumbRX = nx_xinput_axis( rx );
    gamepad->sThumbRY = nx_xinput_axis( ry );
}

/* Keyboard and mouse entries keep the positional mapping. Only explicit
 * gamepad targets override it; rebuild the face bits so multiple physical
 * buttons targeting the same action cannot erase one another. */
static inline void nx_xinput_remap_faces( unsigned long long buttons,
                                         const unsigned short codes[4], XINPUT_GAMEPAD *gamepad )
{
    static const unsigned long long physical[] = { NX_PAD_A, NX_PAD_B, NX_PAD_X, NX_PAD_Y };
    static const WORD defaults[] = { XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_A,
                                    XINPUT_GAMEPAD_Y, XINPUT_GAMEPAD_X };
    unsigned int i;

    gamepad->wButtons &= ~(XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y);
    for (i = 0; i < 4; i++)
        if (buttons & physical[i])
            gamepad->wButtons |= wine_nx_gamepad_code( codes[i] ) ?
                                XINPUT_GAMEPAD_A << (codes[i] - WINE_NX_GAMEPAD_A) : defaults[i];
}

#endif /* __WINE_XINPUT_NX_PAD_H */
