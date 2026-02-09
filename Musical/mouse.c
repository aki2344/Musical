/*
* @file mouse.c
* @brief マウス入力処理の実装
*/
#include "mouse.h"

static float x;           ///< x座標
static float y;           ///< y座標
static float xrel;        ///< x方向の移動量
static float yrel;        ///< y方向の移動量
static Uint8 currState;  ///< 現在の状態
static Uint8 prevState;  ///< 1フレーム前の状態

/**
* @brief マウス変数を更新
*/
void mouseUpdate(void) {
    prevState = currState;
    currState = SDL_GetMouseState(&x, &y);
    currState = SDL_GetRelativeMouseState(&xrel, &yrel);
}

/**
* @brief 指定した番号のボタンが押されているかの判定
*
* <ol>
* <li>左ボタン</li>
* <li>真ん中ボタン</li>
* <li>右ボタン</li>
* </ol>
*
* @param num 判定するマウスの番号
*/
bool getMouse(int num) {
    return currState & SDL_BUTTON(num);
}

/**
* @brief 指定した番号のボタンが今押されたかの判定
*
* <ol>
* <li>左ボタン</li>
* <li>真ん中ボタン</li>
* <li>右ボタン</li>
* </ol>
*
* @param num 判定するマウスの番号
*/
bool getMouseDown(int num) {
    return (currState & SDL_BUTTON(num)) && !(prevState & SDL_BUTTON(num));
}

/**
* @brief 指定した番号のボタンが今放されたかの判定
*
* <ol>
* <li>左ボタン</li>
* <li>真ん中ボタン</li>
* <li>右ボタン</li>
* </ol>
*
* @param num 判定するマウスの番号
*/
bool getMouseUp(int num) {
    return !(currState & SDL_BUTTON(num)) && (prevState & SDL_BUTTON(num));
}

/**
* @brief マウスのX座標の取得
*/
float getMouseX(void) {
    return x;
}

/**
* @brief マウスのY座標の取得
*/
float getMouseY(void) {
    return y;
}

/**
* @brief マウスの相対X座標の取得
*/
float getMouseXrel(void) {
    return xrel;
}

/**
* @brief マウスの相対Y座標の取得
*/
float getMouseYrel(void) {
    return yrel;
}

/**
* @brief マウスのWindow座標への更新
* 
* @param renderer レンダラー
*/
void setRenderCoodinatesFromWindow(SDL_Renderer* renderer) {

    SDL_RenderWindowToLogical(renderer, (int)x, (int)y, &x, &y);
    SDL_RenderWindowToLogical(renderer, (int)xrel, (int)yrel, &xrel, &yrel);
}
