/* Real SDL input after a native keyboard closes: closing presses cannot edit again. */
#include <assert.h>
#include <stdio.h>
#include "launcher_ui.h"

int main(void)
{
    struct ui ui = {0};
    struct ui_input input;
    SDL_Event event = {0};
    assert( !SDL_Init( SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER ) );
    int device = SDL_JoystickAttachVirtual( SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                          SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0 );
    assert( device >= 0 && SDL_IsGameController( device ) );
    ui.controller = SDL_GameControllerOpen( device );
    assert( ui.controller );
    SDL_Joystick *joystick = SDL_GameControllerGetJoystick( ui.controller );
    ui.running = 1;

    /* The prompt's A is still held; a saved Enter and SDL's closing tap are stale. */
    assert( !SDL_JoystickSetVirtualButton( joystick, UI_A, 1 ) );
    ui.held = UI_RIGHT;
    ui.stick_x = ui.touch.active = 1;
    ui.queued_count = 1;
    ui.queued[0].type = SDL_KEYDOWN;
    ui.queued[0].key.keysym.sym = SDLK_RETURN;
    event.type = SDL_FINGERUP;
    event.tfinger.x = event.tfinger.y = 0.2f;
    assert( SDL_PushEvent( &event ) == 1 );
    ui_resume_after_prompt( &ui );
    assert( ui.wait_input_release && !ui.queued_count && !ui.touch.active && !ui.held );
    assert( ui_begin_frame( &ui ) && !ui_poll( &ui, &input ) );

    /* Release re-arms the list; a fresh A works once, without leaving a latch. */
    assert( !SDL_JoystickSetVirtualButton( joystick, UI_A, 0 ) );
    SDL_PumpEvents();
    assert( !ui_poll( &ui, &input ) && !ui.wait_input_release );
    assert( !SDL_JoystickSetVirtualButton( joystick, UI_A, 1 ) );
    SDL_PumpEvents();
    assert( ui_poll( &ui, &input ) && input.button == UI_A );
    assert( !SDL_JoystickSetVirtualButton( joystick, UI_A, 0 ) );
    SDL_PumpEvents();
    while (ui_poll( &ui, &input ));

    /* Holding Right cannot generate another edit while the closing input is blocked. */
    assert( !SDL_JoystickSetVirtualButton( joystick, UI_RIGHT, 1 ) );
    SDL_PumpEvents();
    ui_resume_after_prompt( &ui );
    ui.held_since = ui.held_last = SDL_GetTicks() - 1000;
    assert( ui.wait_input_release && ui_begin_frame( &ui ) );
    assert( !ui_poll( &ui, &input ) && !ui.queued_count );
    assert( !SDL_JoystickSetVirtualButton( joystick, UI_RIGHT, 0 ) );
    SDL_PumpEvents();
    assert( !ui_poll( &ui, &input ) && !ui.wait_input_release );

    /* Quit is a lifecycle event and must not disappear with stale input. */
    event.type = SDL_QUIT;
    assert( SDL_PushEvent( &event ) == 1 );
    ui_resume_after_prompt( &ui );
    assert( !ui.running );
    SDL_GameControllerClose( ui.controller );
    assert( !SDL_JoystickDetachVirtual( device ) );
    SDL_Quit();
    puts( "native prompt input: stale Enter/touch, held A/Right, release/retry and quit passed" );
    return 0;
}
