#include <assert.h>
#include "../source/game_com32.h"

int main(void)
{
    assert( game_com32_valid_name( "dinput8.dll" ) );
    assert( game_com32_valid_name( "a-b_0.dll" ) );
    assert( !game_com32_valid_name( "../dinput8.dll" ) );
    assert( !game_com32_valid_name( "C:\\dinput8.dll" ) );
    assert( !game_com32_valid_name( "DINPUT8.dll" ) );
    assert( !game_com32_valid_name( "dinput8.DLL" ) );
    assert( !game_com32_valid_name( "foo.bar.dll" ) );
    assert( !game_com32_valid_name( "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.dll" ) );
    return 0;
}
