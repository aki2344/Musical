/**
* @file vector2.h
* @brief 2次元ベクトル用ヘッダ
*
* 2次元ベクトル用関連処理
*/
#pragma once

#include <SDL2/SDL.h>
#include <stdbool.h>

/**
* @brief 2次元ベクトル構造体
* @author 秋山
* @version 2.0
* @date 2025/10/15
*/
typedef struct {
    float x;               ///< X成分
    float y;               ///< Y成分
} Vector2;

Vector2 vector2Lerp(const Vector2* v1, const Vector2* v2, float fraction);
Vector2 vector2Add(const Vector2* v1, const Vector2* v2);
Vector2 vector2Subtract(const Vector2* v1, const Vector2* v2);
Vector2 vector2Multiply(const Vector2* v, float scalar);
float vector2GetRotation(const Vector2* v);
float vector2Dot(const Vector2* v1, const Vector2* v2);
float vector2Closs(const Vector2* v1, const Vector2* v2);
float vector2Magnitude(const Vector2* v);
float vector2SqrMagnitude(const Vector2* v);
Vector2 vector2Normalized(const Vector2* v);
float vector2Distance(const Vector2* v1, const Vector2* v2);
