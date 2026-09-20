/* Switch controller actions stored alongside Windows virtual-key codes. */
#ifndef __WINE_NX_INPUT_CODES_H
#define __WINE_NX_INPUT_CODES_H

#define WINE_NX_MOUSE_LEFT  0x100
#define WINE_NX_MOUSE_RIGHT 0x101

/* Controller mappings name the dedicated navigation keys, not the keypad.
 * The null driver's US table lists keypad aliases first, so MapVirtualKeyEx
 * can return bare 48/50/4b/4d even for VK_UP/DOWN/LEFT/RIGHT. */
static inline unsigned int wine_nx_keyboard_scan( unsigned int vkey, unsigned int mapped_scan )
{
    switch (vkey)
    {
    case 0x21: return 0xe049; /* Page Up */
    case 0x22: return 0xe051; /* Page Down */
    case 0x23: return 0xe04f; /* End */
    case 0x24: return 0xe047; /* Home */
    case 0x25: return 0xe04b; /* Left */
    case 0x26: return 0xe048; /* Up */
    case 0x27: return 0xe04d; /* Right */
    case 0x28: return 0xe050; /* Down */
    case 0x2d: return 0xe052; /* Insert */
    case 0x2e: return 0xe053; /* Delete */
    default: return mapped_scan;
    }
}

#endif
