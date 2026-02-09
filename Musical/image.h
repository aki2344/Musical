/**
 * @file image.h
 * @brief 画像関係処理ヘッダ
 */
#pragma once

#include <SDL2/SDL.h>
#include <stdbool.h>

void screenInit(int width, int height);
void screenQuit(void);
int loadImage(const char* fileName);
void freeImage(void);
void drawImage(int id, SDL_Rect *src, SDL_FRect *dst, double angle, const SDL_FPoint *center, SDL_RendererFlip flip);
void setAlpha(int id, Uint8 alpha);
void setColor(int id, Uint8 r, Uint8 g, Uint8 b);
void setBlendMode(int id, SDL_BlendMode mode);
void setFullScreen(bool isFull);
void clearScreen(float r, float g, float b);
void flip(void);
void drawPoint(float x, float y, float r, float g, float b, float a);
void drawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a);
void drawRect(const SDL_FRect* rect, float r, float g, float b, float a);
void fillRect(const SDL_FRect* rect, float r, float g, float b, float a);
void setDrawMode(SDL_BlendMode mode);
void drawCircle(float x, float y, float radius, float r, float g, float b, float a);
void drawArc(float x, float y, float radius, float direction, double angle, float r, float g, float b, float a);
SDL_Renderer* getRenderer(void);
