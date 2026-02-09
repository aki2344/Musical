#include "gamepad.h"
#include <math.h>
#include <stdio.h>

static Gamepad pads[GAMEPAD_MAX];
static int padCount;
const float deadzone = 0.15f;

SDL_GameControllerButton buttons[] = {
    SDL_CONTROLLER_BUTTON_A,           /**< Bottom face button (e.g. Xbox A button) */
    SDL_CONTROLLER_BUTTON_B,           /**< Right face button (e.g. Xbox B button) */
    SDL_CONTROLLER_BUTTON_X,           /**< Left face button (e.g. Xbox X button) */
    SDL_CONTROLLER_BUTTON_Y,           /**< Top face button (e.g. Xbox Y button) */
    SDL_CONTROLLER_BUTTON_BACK,
    SDL_CONTROLLER_BUTTON_GUIDE,
    SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_LEFTSTICK,
    SDL_CONTROLLER_BUTTON_RIGHTSTICK,
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    SDL_CONTROLLER_BUTTON_DPAD_UP,
    SDL_CONTROLLER_BUTTON_DPAD_DOWN
};

SDL_GameControllerAxis axises[] = {
    SDL_CONTROLLER_AXIS_LEFTX,
    SDL_CONTROLLER_AXIS_LEFTY,
    SDL_CONTROLLER_AXIS_RIGHTX,
    SDL_CONTROLLER_AXIS_RIGHTY,
    SDL_CONTROLLER_AXIS_TRIGGERLEFT,
    SDL_CONTROLLER_AXIS_TRIGGERRIGHT
};

static Gamepad* getPad(SDL_JoystickID id) {
    for (int i = 0; i < GAMEPAD_MAX; i++)
    {
        if (pads[i].id == id)return &pads[i];
    }
    return NULL;
}

static Gamepad* getPadByIndex(int index) {
    if (index < 0 || index >= GAMEPAD_MAX)return NULL;
    return &pads[index];
}

static Gamepad* getEmpty() {
    for (int i = 0; i < GAMEPAD_MAX; i++)
    {
        pads[i].index = i;
        if (!pads[i].isEnabled)return &pads[i];
    }
    return NULL;
}

void openPad(int device_index) {
    if (padCount == GAMEPAD_MAX)return;
    SDL_GameController* pad = SDL_GameControllerOpen(device_index);
    if (!pad) {
        SDL_Log("SDL_GameControllerOpen(%d) failed: %s", device_index, SDL_GetError());
        return;
    }

    SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad));
    SDL_Log("Gamepad opened: id=%d name=%s", (int)id, SDL_GameControllerName(pad));

    // 任意：PlayerIndex（複数パッドの識別に便利）
    Gamepad* empty = getEmpty();
    SDL_GameControllerSetPlayerIndex(pad, empty->index);
    empty->id = id;
    empty->pad = pad;
    empty->isEnabled = true;
    padCount++;
}

void closePad(SDL_JoystickID id) {
    Gamepad* gamepad = getPad(id);
    if (!gamepad) {
        return;
    }
    gamepad->isEnabled = false;
    SDL_GameControllerClose(gamepad->pad);
    padCount--;
}

float getGamepadAxis(int index, SDL_GameControllerAxis axis) {
    int v = SDL_GameControllerGetAxis(getPadByIndex(index)->pad, axis);
    // -32768..32767 を -1..+1 に
    float f = (v < 0) ? (v / 32768.0f) : (v / 32767.0f);
    float a = fabs(f);
    if (a < deadzone) return 0.0f;
    // deadzone外側を 0..1 に再スケール（簡易）
    float s = (a - deadzone) / (1.0f - deadzone);
    return (f < 0) ? -s : s;
}

bool getGamepadAxisDown(int index, SDL_GameControllerAxis axis, AxisValue value) {
    Gamepad* pad = getPadByIndex(index);
    if (!pad)return false;
    AxisState state = pad->axis[axis];
    return state.now == value && state.pre != value;
}

bool getGamepadAxisUp(int index, SDL_GameControllerAxis axis, AxisValue value) {
    Gamepad* pad = getPadByIndex(index);
    if (!pad)return false;
    AxisState state = pad->axis[axis];
    return state.now != value && state.pre == value;
}

bool getGamepadButton(int index, SDL_GameControllerButton button) {
    Gamepad* pad = getPadByIndex(index);
    if (!pad)return false;
    return pad->button[button].now;
}

bool getGamepadButtonDown(int index, SDL_GameControllerButton button) {
    Gamepad* pad = getPadByIndex(index);
    if (!pad)return false;
    ButtonState state = pad->button[button];
    return state.now && !state.pre;
}

bool getGamepadButtonUp(int index, SDL_GameControllerButton button) {
    Gamepad* pad = getPadByIndex(index);
    if (!pad)return false;
    ButtonState state = pad->button[button];
    return !state.now && state.pre;
}

void gamepadUpdate(void) {
    for (int i = 0; i < GAMEPAD_MAX; i++)
    {
        if (pads[i].isEnabled) {
            for (int j = 0; j < GAMEPAD_BUTTON_MAX; j++)
            {
                pads[i].button[j].pre = pads[i].button[j].now;
                pads[i].button[j].now = SDL_GameControllerGetButton(pads[i].pad, buttons[j]);
            }
            for (int j = 0; j < SDL_CONTROLLER_AXIS_MAX; j++)
            {
                float a = getGamepadAxis(pads[i].index, axises[j]);
                pads[i].axis[j].pre = pads[i].axis[j].now;
                pads[i].axis[j].now = a > 0.5f ? AXIS_PLUS : (a < -0.5f ? AXIS_MINUS : AXIS_NEUTRAL);
            }
        }
    }
}
