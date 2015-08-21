//
// gcc sdl-fftest.c `sdl2-config --libs --cflags` -o sdl-fftest
//
//
#include "SDL.h"

int test_haptic( SDL_Joystick * joystick ) {
 SDL_Haptic *haptic;
 SDL_HapticEffect effect;
 int effect_id;

 // Open the device
 haptic = SDL_HapticOpenFromJoystick( joystick );
 if (haptic == NULL) return -1; // Most likely joystick isn't haptic

 // See if it can do sine waves
 if ((SDL_HapticQuery(haptic) & SDL_HAPTIC_SINE)==0) {
  SDL_HapticClose(haptic); // No sine effect
  return -1;
 }

 // Create the effect
 memset( &effect, 0, sizeof(SDL_HapticEffect) ); // 0 is safe default
 effect.type = SDL_HAPTIC_SINE;
 effect.periodic.direction.type = SDL_HAPTIC_POLAR; // Polar coordinates
 effect.periodic.direction.dir[0] = 18000; // Force comes from south
 effect.periodic.period = 1000; // 1000 ms
 effect.periodic.magnitude = 20000; // 20000/32767 strength
 effect.periodic.length = 5000; // 5 seconds long
 effect.periodic.attack_length = 1000; // Takes 1 second to get max strength
 effect.periodic.fade_length = 1000; // Takes 1 second to fade away

 // Upload the effect
 effect_id = SDL_HapticNewEffect( haptic, &effect );

 // Test the effect
 SDL_HapticRunEffect( haptic, effect_id, 1 );
 SDL_Delay( 5000); // Wait for the effect to finish

 // We destroy the effect, although closing the device also does this
 SDL_HapticDestroyEffect( haptic, effect_id );

 // Close the device
 SDL_HapticClose(haptic);

 return 0; // Success
}

int main(void)
{
	SDL_Joystick *joy;
	
	if (SDL_Init(SDL_INIT_JOYSTICK|SDL_INIT_HAPTIC) != 0) {
		fprintf(stderr, "error: %s\n", SDL_GetError());
		return 1;
	}

	if (SDL_NumJoysticks() < 0) {
		fprintf(stderr, "No joystick\n");
		return 1;
	}

	joy = SDL_JoystickOpen(0);
	if (joy) {
		printf("Opened Joystick 0\n");
		printf("Name: %s\n", SDL_JoystickNameForIndex(0));
		printf("Number of Axes: %d\n", SDL_JoystickNumAxes(joy));
		printf("Number of Buttons: %d\n", SDL_JoystickNumButtons(joy));
		printf("Number of Balls: %d\n", SDL_JoystickNumBalls(joy));
	} else {
		printf("Couldn't open Joystick 0\n");
	}

	test_haptic(joy);

	SDL_Quit();
}
