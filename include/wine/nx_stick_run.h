/* Convert a held left-stick direction into press, release, press-and-hold.
 * PAL3 runs on a double press of an arrow, rather than Shift + arrow. */
#ifndef __WINE_NX_STICK_RUN_H
#define __WINE_NX_STICK_RUN_H

#define WINE_NX_STICK_FIRST_KEY 16
#define WINE_NX_STICK_DIRECTION_COUNT 4
#define WINE_NX_STICK_TAP_MS 64u

struct wine_nx_stick_run
{
    unsigned int since[WINE_NX_STICK_DIRECTION_COUNT];
    unsigned char phase[WINE_NX_STICK_DIRECTION_COUNT];
};

static inline unsigned int wine_nx_stick_run_keys( struct wine_nx_stick_run *state,
                                                    unsigned int held, unsigned int now )
{
    unsigned int result = held;

    for (unsigned int i = 0; i < WINE_NX_STICK_DIRECTION_COUNT; i++)
    {
        unsigned int bit = 1u << (WINE_NX_STICK_FIRST_KEY + i);
        if (!(held & bit)) { state->phase[i] = 0; continue; }
        if (!state->phase[i]) { state->phase[i] = 1; state->since[i] = now; }
        else if (state->phase[i] == 1 && now - state->since[i] >= WINE_NX_STICK_TAP_MS)
        { state->phase[i] = 2; state->since[i] = now; }
        else if (state->phase[i] == 2 && now - state->since[i] >= WINE_NX_STICK_TAP_MS)
            state->phase[i] = 3;
        if (state->phase[i] == 2) result &= ~bit;
    }
    return result;
}

/* Return -1 for no event, 0 for key-up, 1 for key-down. Two controls naming
 * the same virtual key must act as one held key during the synthetic gap. */
static inline int wine_nx_stick_run_key_event( unsigned int before, unsigned int after,
                                                const unsigned short *vkeys, unsigned int count,
                                                unsigned int index )
{
    unsigned int changed = before ^ after;
    int old_down = 0, new_down = 0;

    for (unsigned int i = 0; i < index; i++)
        if ((changed & (1u << i)) && vkeys[i] == vkeys[index]) return -1;
    for (unsigned int i = 0; i < count; i++)
    {
        if (vkeys[i] != vkeys[index]) continue;
        old_down |= !!(before & (1u << i));
        new_down |= !!(after & (1u << i));
    }
    return old_down == new_down ? -1 : new_down;
}

#endif
