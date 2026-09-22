#include <assert.h>
#include <stdio.h>

#include "wine/nx_fixed_click.h"

int main(void)
{
    struct wine_nx_fixed_click_point point = {0};
    struct wine_nx_fixed_click_queue queue = {0};
    int x = -1, y = -1;

    assert( wine_nx_fixed_click_parse( "click:1260,700", &point ) );
    assert( point.enabled && point.x == 1260 && point.y == 700 && point.clicks == 1 );
    assert( wine_nx_fixed_click_parse( "click:1240,40,2", &point ) );
    assert( point.x == 1240 && point.y == 40 && point.clicks == 2 );
    assert( !wine_nx_fixed_click_parse( "click:1280,20", &point ) );
    assert( !wine_nx_fixed_click_parse( "click:20,720", &point ) );
    assert( !wine_nx_fixed_click_parse( "click:20,", &point ) );
    assert( !wine_nx_fixed_click_parse( "click:-1,20", &point ) );
    assert( !wine_nx_fixed_click_parse( "click:20,30junk", &point ) );
    assert( !wine_nx_fixed_click_parse( "click:20,30,3", &point ) );

    /* Background polls must not turn a held button into repeat clicks. */
    wine_nx_fixed_click_poll( &queue, 4, 1, 1, 1260, 700, 1 );
    wine_nx_fixed_click_poll( &queue, 4, 1, 1, 1260, 700, 1 );
    assert( wine_nx_fixed_click_take( &queue, &x, &y ) && x == 1260 && y == 700 );
    assert( !wine_nx_fixed_click_take( &queue, &x, &y ) );

    /* A short release and another press remain distinct before Wine polls. */
    wine_nx_fixed_click_poll( &queue, 4, 0, 1, 1260, 700, 1 );
    wine_nx_fixed_click_poll( &queue, 4, 1, 1, 1260, 700, 1 );
    wine_nx_fixed_click_poll( &queue, 10, 1, 1, 1240, 40, 2 );
    assert( wine_nx_fixed_click_take( &queue, &x, &y ) && x == 1260 && y == 700 );
    assert( wine_nx_fixed_click_take( &queue, &x, &y ) && x == 1240 && y == 40 );
    assert( wine_nx_fixed_click_take( &queue, &x, &y ) && x == 1240 && y == 40 );
    assert( !wine_nx_fixed_click_take( &queue, &x, &y ) );

    /* XInput owns the button while active; a held press cannot leak later. */
    wine_nx_fixed_click_poll( &queue, 4, 0, 0, 1260, 700, 1 );
    wine_nx_fixed_click_poll( &queue, 4, 1, 0, 1260, 700, 1 );
    wine_nx_fixed_click_poll( &queue, 4, 1, 1, 1260, 700, 1 );
    assert( !wine_nx_fixed_click_take( &queue, &x, &y ) );
    puts( "fixed click: OK" );
    return 0;
}
