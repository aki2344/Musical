/**
* @file sprite.h
* @brief スプライト用ヘッダ
*
* スプライト用関連処理
*/
#pragma once

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "vector2.h"

/**
* @brief スプライトの構造体
* @author 秋山
* @version 4.0
* @date 2025/10/15
*/
typedef struct {
    int image;              ///< 画像
    SDL_Rect src;           ///< 元画像矩形
    SDL_FRect dst;           ///< 出力矩形
    SDL_FRect col;           ///< 当たり判定用矩形
    SDL_FPoint pivot;       ///< 中心点
    SDL_FPoint center;       ///< 中心点
    float radius;          ///< 当たり判定半径
    Vector2 position;       ///< 表示座標
    int count;              ///< アニメーションのカウント
    int frame;              ///< アニメーションの現在のフレーム数
    int frameMax;           ///< アニメーションのフレーム最大数
    int interval;           ///< アニメーション間隔
    int loopCount;          ///< アニメーションのループ回数
    bool pause;             ///< アニメーションがポーズ中かどうかのフラグ
    bool isEnabled;         ///< 表示のONOFFフラグ
    float rotation;        ///< 角度（ラジアン）
    float scale;           ///< 拡大率
    float scaleX;           ///< 拡大率X
    float scaleY;           ///< 拡大率Y
    float direction;       ///< 向き
    Vector2 v;              ///< 速度
    Vector2 accel;          ///< 加速度
    SDL_Color color;        ///< 色
    SDL_BlendMode blendmode;///< ブレンドモード
    SDL_RendererFlip flip;  ///< 反転
} Sprite;

void spriteDraw(Sprite *s);
void spriteDrawOffset(Sprite *s, float x, float y);
void spriteDrawEx(Sprite* s, Vector2 position, float rotation, float scale);
void spriteInit(Sprite *s, int id, float srcX, float srcY, float srcW, float srcH);
void spriteAnimeInit(Sprite *s, int frameMax, int interval, int loopCount);
void spriteAnime(Sprite* s);
void spriteSetCollision(Sprite *s, float w, float h);
bool spriteIntersectsRect(const Sprite *s1, const Sprite *s2);
bool spriteIntersectsCircle(const Sprite *s1, const Sprite *s2);
bool spriteIntersectsPoint(const Sprite *s, float x, float y);
