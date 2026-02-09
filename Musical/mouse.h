/*
* @file mouse.h
* @brief マウス入力処理ヘッダ
*
* マウス入力処理
*/
#pragma once

#include <SDL2/SDL.h>
#include <stdbool.h>

void mouseUpdate(void);
bool getMouse(int num);
bool getMouseDown(int num);
bool getMouseUp(int num);
float getMouseX(void);
float getMouseY(void);
float getMouseXrel(void);
float getMouseYrel(void);
void setRenderCoodinatesFromWindow(SDL_Renderer* renderer);
