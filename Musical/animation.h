/*
* @file animation.h
* @brief animation構造体処理ヘッダ
*
* animation構造体を定義
*/

#pragma once

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "sprite.h"

#define ANIMATION_MAX    4000
typedef enum LoopType{
    STRAIGHT,
    PING_PONG
}LoopType;

/**
* @brief アニメーションの構造体
* @author 秋山
* @version 1.0
* @date 2017/10/17
*/
typedef struct {
    int id;                             ///< ID
    Sprite *g;                          ///< アニメーション対象のSprite変数への参照
    Sprite startValue;                  ///< 開始時の値
    Sprite targetValue;                 ///< 終了時の値
    int startTime;                      ///< 開始時間
    int endTime;                        ///< 終了時間
    int pastTime;                       ///< 経過時間
    int duration;                       ///< 長さ
    int delay;                          ///< 開始までの時間
    void (*onFinished)();               ///< 終了時コールバック関数
    void* callbackTarget;
    float (*easing)(float ratio);     ///< イージング関数
    bool position;                       ///< 位置のアニメーションが有効かどうかのフラグ
    bool scale;                          ///< 大きさのアニメーションが有効かどうかのフラグ
    bool scaleX;                          ///< 大きさのアニメーションが有効かどうかのフラグ
    bool scaleY;                          ///< 大きさのアニメーションが有効かどうかのフラグ
    bool rotation;                       ///< 角度のアニメーションが有効かどうかのフラグ
    bool alpha;                          ///< 透明度のアニメーションが有効かどうかのフラグ
    bool isEnabled;                     ///< アクティブかどうかのフラグ
    bool loop;                          ///< ループするかどうかのフラグ
    bool pause;                         ///< ポーズ中かどうかのフラグ
    LoopType loopType;                  ///< ループのタイプ
} Animation;

void easingUpdate(void);
void moveTo(Sprite* g, float x, float y);
void scaleTo(Sprite* g, float scale);
void scaleXTo(Sprite* g, float scale);
void scaleYTo(Sprite* g, float scale);
void rotateTo(Sprite* g, float angle);
void alphaTo(Sprite* g, float alpha);
void moveAdd(Sprite* g, float x, float y);
void scaleAdd(Sprite* g, float scale);
void scaleXAdd(Sprite* g, float scale);
void scaleYAdd(Sprite* g, float scale);
void rotateAdd(Sprite* g, float angle);
void alphaAdd(Sprite* g, float alpha);
void stopAnimation(Sprite* g);
void pauseAnimation(Sprite* g);
void resumeAnimation(Sprite* g);
void stopAnimationAll(void);
void pauseAnimationAll(void);
void resumeAnimationAll(void);
void setDelay(int delay);
void setDuration(int duration);
void setEasing(float(*easing)(float ratio));
void setOnFinished(void(*onFinished)());
void setLoop(void); 
void setLoopType(LoopType type);
void setTimeout(Sprite* g, void(*onFinished)(), int duration);
void setCallbackTarget(void* p);
void disable(Sprite* p);
