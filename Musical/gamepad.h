#pragma once

#include <SDL2/SDL.h>
#include <stdbool.h>

#define GAMEPAD_MAX 4
#define GAMEPAD_BUTTON_MAX 15

typedef struct ButtonState {
    bool pre;
    bool now;
}ButtonState;

typedef enum AxisValue {
    AXIS_MINUS,
    AXIS_NEUTRAL,
    AXIS_PLUS
}AxisValue;

typedef struct AxisState {
    AxisValue pre;
    AxisValue now;
}AxisState;

typedef struct Gamepad {
    SDL_JoystickID id;
    SDL_GameController* pad;
    bool isEnabled;
    int index;
    ButtonState button[GAMEPAD_BUTTON_MAX];
    AxisState axis[SDL_CONTROLLER_AXIS_MAX];
}Gamepad;

void openPad(int device_index);
void closePad(SDL_JoystickID id);
float getGamepadAxis(int index, SDL_GameControllerAxis axis);
bool getGamepadAxisDown(int index, SDL_GameControllerAxis axis, AxisValue value);
bool getGamepadAxisUp(int index, SDL_GameControllerAxis axis, AxisValue value);
bool getGamepadButton(int index, SDL_GameControllerButton button);
bool getGamepadButtonDown(int index, SDL_GameControllerButton button);
bool getGamepadButtonUp(int index, SDL_GameControllerButton button);
void gamepadUpdate(void);
