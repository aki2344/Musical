/**
* @file main.h
* @brief プログラム全体共通で使用するヘッダ
*/

#pragma once

#include <stdbool.h>
#include <SDL2/SDL.h>

#define WINDOW_WIDTH        640     ///< ウィンドウの幅
#define WINDOW_HEIGHT       480     ///< ウィンドウの高さ

/**
* @brief 流れの定数
*/
enum sequenceID {
    TITLE,      ///< タイトル
    MAINGAME,   ///< メインゲーム
    END         ///< 終了
};

void setSequence(int s);
int getSequence(void);
void setScore(int s);
void addScore(int s);
int getScore(void);
void setHiScore(int s);
int getHiScore(void);
void eventInput(bool* isRunning);
void wait(void);
SDL_Color lerpColor(SDL_Color a, SDL_Color b, float t);
float lerp(float a, float b, float t);
float randomFloat();
void openPad(SDL_JoystickID id);
void closePad(SDL_JoystickID id);
