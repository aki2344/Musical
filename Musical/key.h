/*
* @file key.h
* @brief キー入力処理ヘッダ
*
* キーボード入力処理
*/
#pragma once

#include <SDL2/SDL.h>
#include <stdbool.h>

void keyUpdate(void);
bool getKey(SDL_Scancode k);
bool getKeyDown(SDL_Scancode k);
bool getKeyUp(SDL_Scancode k);
