/*
* @file key.c
* @brief キー入力処理の実装
*/
#include "key.h"
#include <string.h>
#include <stdio.h>

Uint8 currState[SDL_NUM_SCANCODES];    ///< 現在の状態
Uint8 prevState[SDL_NUM_SCANCODES];    ///< 1フレーム前の状態

/**
* @brief キー入力用変数の更新
*/
void keyUpdate(void) {
    const Uint8 *keyState = SDL_GetKeyboardState(NULL);

    //キーの直前の状態を更新
    memcpy(prevState, currState, SDL_NUM_SCANCODES );

    //キーの現在の状態を更新
    memcpy(currState, keyState, SDL_NUM_SCANCODES );

}

/**
* @brief 指定したキーが押されているかの判定
*
* @param k 判定するキーの定数
*/
bool getKey(SDL_Scancode k) {
    return currState[k];
}

/**
* @brief 指定したキーが今押されたのかの判定
*
* @param k 判定するキーの定数
*/
bool getKeyDown(SDL_Scancode k) {
    return (currState[k] && currState[k] != prevState[k]);
}

/**
* @brief 指定したキーが今はなされたかの判定
*
* @param k 判定するキーの定数
*/
bool getKeyUp(SDL_Scancode k) {
    return (!currState[k] && currState[k] != prevState[k]);
}
