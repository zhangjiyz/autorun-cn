#include <assert.h>
#include <stdio.h>
#include "wine/nx_stick_run.h"

int main(void)
{
    struct wine_nx_stick_run state = {0};
    const unsigned int up = 1u << WINE_NX_STICK_FIRST_KEY;
    const unsigned int right = 1u << (WINE_NX_STICK_FIRST_KEY + 3);
    const unsigned int dpad = 1u << 0;

    assert( wine_nx_stick_run_keys( &state, up | dpad, 1000 ) == (up | dpad) );
    assert( wine_nx_stick_run_keys( &state, up | dpad, 1063 ) == (up | dpad) );
    assert( wine_nx_stick_run_keys( &state, up | dpad, 1064 ) == dpad );
    assert( wine_nx_stick_run_keys( &state, up | dpad, 1127 ) == dpad );
    assert( wine_nx_stick_run_keys( &state, up | dpad, 1128 ) == (up | dpad) );
    assert( wine_nx_stick_run_keys( &state, up | dpad, 5000 ) == (up | dpad) );
    assert( wine_nx_stick_run_keys( &state, right | dpad, 5001 ) == (right | dpad) );
    assert( wine_nx_stick_run_keys( &state, right | dpad, 5065 ) == dpad );
    assert( wine_nx_stick_run_keys( &state, dpad, 5066 ) == dpad );
    assert( wine_nx_stick_run_keys( &state, right | dpad, 5067 ) == (right | dpad) );
    assert( wine_nx_stick_run_keys( &state, right | dpad, 5131 ) == dpad );
    assert( wine_nx_stick_run_keys( &state, right | dpad, 5195 ) == (right | dpad) );
    unsigned short vkeys[28] = {0};
    vkeys[0] = vkeys[WINE_NX_STICK_FIRST_KEY] = 0x26;
    assert( wine_nx_stick_run_key_event( 0, up | dpad, vkeys, 28, 0 ) == 1 );
    assert( wine_nx_stick_run_key_event( 0, up | dpad, vkeys, 28,
                                         WINE_NX_STICK_FIRST_KEY ) == -1 );
    assert( wine_nx_stick_run_key_event( up, up | dpad, vkeys, 28, 0 ) == -1 );
    assert( wine_nx_stick_run_key_event( up | dpad, dpad, vkeys, 28,
                                         WINE_NX_STICK_FIRST_KEY ) == -1 );
    assert( wine_nx_stick_run_key_event( dpad, 0, vkeys, 28, 0 ) == 0 );
    puts( "left stick double-tap timing, direction change, release and d-pad isolation passed" );
    return 0;
}
