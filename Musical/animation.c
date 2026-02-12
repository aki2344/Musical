/*
* @file animation.c
* @brief animation構造体処理の実装
*/
#include "animation.h"
#include "sprite.h"
#include "easing.h"
#include <stdio.h>
#include <stdbool.h>

static Animation list[ANIMATION_MAX];
static Animation *current;

static Uint8 alphaFromFloat(float alpha) {
    if (alpha <= 0.0f) {
        return 0;
    }
    if (alpha >= 1.0f) {
        return 255;
    }
    return (Uint8)(alpha * 255.0f + 0.5f);
}

/**
* @brief イージングアニメーションを1つ有効化して、それを返す
*
* @param g グラフィック
* @return 有効化したイージングデータへの参照
*/
static Animation* get(Sprite* g) {

    int i;
    Animation* data;

    for (i = 0; i < ANIMATION_MAX; i++)
    {
        if (!list[i].isEnabled) {
            data = &list[i];
            break;
        }
    }
    if (i == ANIMATION_MAX) return NULL;

    data->id = i;
    if(g != NULL)
        data->startValue = data->targetValue = *g;
    data->g = g;
    data->easing = linear;
    data->onFinished = NULL;
    data->isEnabled = true;
    data->startTime = SDL_GetTicks();
    data->duration = 500;
    data->endTime = data->startTime + data->duration;
    data->delay = 0;
    data->position = data->scale = data->rotation = data->alpha = false;
    data->loop = false;
    data->pastTime = 0;
    data->pause = false;
    data->loopType = STRAIGHT;
    data->callbackTarget = NULL;

    return data;
}

/**
* @brief イージングアニメーションの更新
*/
void easingUpdate() {

    int deltaTime = 0;
    static int time;

    if (time > 0) {
        deltaTime = SDL_GetTicks() - time;
        deltaTime = deltaTime > 20 ? 20 : deltaTime;
    }
    time = SDL_GetTicks();

    for (int i = 0; i < ANIMATION_MAX; i++)
    {
        if (list[i].isEnabled) {
            Animation *e = &list[i];

            if (e->pause) {
                continue;
            }

            e->pastTime += deltaTime;
            if (e->pastTime < 0)
                continue;

            float ratio = e->easing((float)e->pastTime / e->duration);

            if (e->position) {
                e->g->position.x = e->startValue.position.x + (e->targetValue.position.x - e->startValue.position.x) * ratio;
                e->g->position.y = e->startValue.position.y + (e->targetValue.position.y - e->startValue.position.y) * ratio;
            }
            if (e->scale) {
                e->g->scale = e->startValue.scale + (e->targetValue.scale - e->startValue.scale) * ratio;
            }
            if (e->scaleX) {
                e->g->scaleX = e->startValue.scaleX + (e->targetValue.scaleX - e->startValue.scaleX) * ratio;
            }
            if (e->scaleY) {
                e->g->scaleY = e->startValue.scaleY + (e->targetValue.scaleY - e->startValue.scaleY) * ratio;
            }
            if(e->rotation)
                e->g->rotation = e->startValue.rotation + (e->targetValue.rotation - e->startValue.rotation) * ratio;
            if (e->alpha) {
                float start = (float)e->startValue.color.a;
                float target = (float)e->targetValue.color.a;
                e->g->color.a = (Uint8)(start + (target - start) * ratio + 0.5f);
            }
            if (e->color) {
                SDL_Color start = e->startValue.color;
                SDL_Color target = e->targetValue.color;
                e->g->color.r = (Uint8)(start.r + (target.r - start.r) * ratio + 0.5f);
                e->g->color.g = (Uint8)(start.g + (target.g - start.g) * ratio + 0.5f);
                e->g->color.b = (Uint8)(start.b + (target.b - start.b) * ratio + 0.5f);
            }

            //easingの終了
            if (e->startTime + e->pastTime >= e->endTime) {

                if (e->onFinished) {
                    e->onFinished(e->callbackTarget ? e->callbackTarget : e->g);
                }
                if (e->loop) {
                    if (e->loopType == STRAIGHT) {
                        if (e->position) e->g->position = e->startValue.position;
                        if (e->scale) e->g->scale = e->startValue.scale;
                        if (e->rotation) e->g->rotation = e->startValue.rotation;
                        if (e->alpha) e->g->color.a = e->startValue.color.a;
                    }
                    else if (e->loopType == PING_PONG) {
                        Sprite t = e->targetValue;
                        e->targetValue = e->startValue;
                        e->startValue = t;
                    }
                    e->startTime = time;
                    e->endTime = e->startTime + e->duration;
                    e->pastTime = 0;
                    e->delay = 0;
                }
                else {
                    e->isEnabled = false;
                }
                
                continue;
            }
        }
    }
}


/**
* @brief 移動アニメーションの停止
*
* @param g 停止させたいアニメーションのSprite型構造体の参照
*/
static void stopMove(Sprite* g) {

    for (int i = 0; i < ANIMATION_MAX; i++)
    {
        if (list[i].isEnabled && list[i].g == g && list[i].position) {
            list[i].isEnabled = false;
        }
    }
}
/**
* @brief スケールアニメーションの停止
*
* @param g 停止させたいアニメーションのSprite型構造体の参照
*/
static void stopScale(Sprite* g) {

    for (int i = 0; i < ANIMATION_MAX; i++)
    {
        if (list[i].isEnabled && list[i].g == g && list[i].scale) {
            list[i].isEnabled = false;
        }
    }
}
/**
* @brief スケールXアニメーションの停止
*
* @param g 停止させたいアニメーションのSprite型構造体の参照
*/
static void stopScaleX(Sprite* g) {

    for (int i = 0; i < ANIMATION_MAX; i++)
    {
        if (list[i].isEnabled && list[i].g == g && list[i].scaleX) {
            list[i].isEnabled = false;
        }
    }
}
/**
* @brief スケールYアニメーションの停止
*
* @param g 停止させたいアニメーションのSprite型構造体の参照
*/
static void stopScaleY(Sprite* g) {

    for (int i = 0; i < ANIMATION_MAX; i++)
    {
        if (list[i].isEnabled && list[i].g == g && list[i].scaleY) {
            list[i].isEnabled = false;
        }
    }
}
/**
* @brief 回転アニメーションの停止
*
* @param g 停止させたいアニメーションのSprite型構造体の参照
*/
static void stopRotation(Sprite* g) {

    for (int i = 0; i < ANIMATION_MAX; i++)
    {
        if (list[i].isEnabled && list[i].g == g && list[i].rotation) {
            list[i].isEnabled = false;
        }
    }
}
/**
* @brief 透明度アニメーションの停止
*
* @param g 停止させたいアニメーションのSprite型構造体の参照
*/
static void stopAlpha(Sprite* g) {

    for (int i = 0; i < ANIMATION_MAX; i++)
    {
        if (list[i].isEnabled && list[i].g == g && list[i].alpha) {
            list[i].isEnabled = false;
        }
    }
}
/**
* @brief 色のアニメーションの停止
*
* @param g 停止させたいアニメーションのSprite型構造体の参照
*/
static void stopColor(Sprite* g) {

    for (int i = 0; i < ANIMATION_MAX; i++)
    {
        if (list[i].isEnabled && list[i].g == g && list[i].color) {
            list[i].isEnabled = false;
        }
    }
}

/**
* @brief 指定座標に移動
*
* @param g グラフィック
* @param x 目標X座標
* @param y 目標Y座標
*/
void moveTo(Sprite* g, float x, float y) {
    Animation* data = get(g);
    if (data == NULL)return;

    stopMove(g);

    data->targetValue.position.x = x;
    data->targetValue.position.y = y;
    data->position = true;
    current = data;
}

/**
* @brief 指定の大きさに変化
*
* @param g グラフィック
* @param scale 目標の大きさ
*/
void scaleTo(Sprite* g, float scale) {
    Animation* data = get(g);
    if (data == NULL)return;

    stopScale(g);

    data->targetValue.scale = scale;
    data->scale = true;
    current = data;
}

/**
* @brief 指定の大きさXに変化
*
* @param g グラフィック
* @param scale 目標の大きさ
*/
void scaleXTo(Sprite* g, float scale) {
    Animation* data = get(g);
    if (data == NULL)return;

    stopScaleX(g);

    data->targetValue.scaleX = scale;
    data->scaleX = true;
    current = data;
}

/**
* @brief 指定の大きさYに変化
*
* @param g グラフィック
* @param scale 目標の大きさ
*/
void scaleYTo(Sprite* g, float scale) {
    Animation* data = get(g);
    if (data == NULL)return;

    stopScaleY(g);

    data->targetValue.scaleY = scale;
    data->scaleY = true;
    current = data;
}

/**
* @brief 指定の角度に回転
*
* @param g グラフィック
* @param rotation 目標角度
*/
void rotateTo(Sprite* g, float rotation) {
    Animation* data = get(g);
    if (data == NULL)return;

    stopRotation(g);

    data->targetValue.rotation = rotation;
    data->rotation = true;
    current = data;
}

/**
* @brief 指定の透明度に変化
*
* @param g グラフィック
* @param alpha 目標の透明度
*/
void alphaTo(Sprite* g, float alpha) {
    Animation* data = get(g);
    if (data == NULL)return;

    stopAlpha(g);

    data->targetValue.color.a = alphaFromFloat(alpha);
    data->alpha = true;
    current = data;
}

/**
* @brief 指定の透明度に変化
*
* @param g グラフィック
* @param alpha 目標の透明度
*/
void colorTo(Sprite* g, SDL_Color color) {
    Animation* data = get(g);
    if (data == NULL)return;

    stopColor(g);

    data->targetValue.color = color;
    data->color = true;
    current = data;
}

/**
* @brief 現在の位置から、指定した分移動
*
* @param g グラフィック
* @param x 目標X座標
* @param y 目標Y座標
*/
void moveAdd(Sprite* g, float x, float y) {
    Animation* data = get(g);
    if (data == NULL)return;

    stopMove(g);

    data->targetValue.position.x = data->startValue.position.x + x;
    data->targetValue.position.y = data->startValue.position.y + y;
    data->position = true;
    current = data;
}

/**
* @brief 現在の大きさから、指定した大きさだけ変化
*
* @param g グラフィック
* @param scale 目標の大きさ
*/
void scaleAdd(Sprite* g, float scale) {
    Animation* data = get(g);
    if (data == NULL)return;

    stopScale(g);

    data->targetValue.scale = data->startValue.scale + scale;
    data->scale = true;
    current = data;
}

/**
* @brief 現在の大きさXから、指定した大きさだけ変化
*
* @param g グラフィック
* @param scale 目標の大きさ
*/
void scaleXAdd(Sprite* g, float scale) {
    Animation* data = get(g);
    if (data == NULL)return;

    stopScaleX(g);

    data->targetValue.scaleX = data->startValue.scaleX + scale;
    data->scaleX = true;
    current = data;
}

/**
* @brief 現在の大きさYから、指定した大きさだけ変化
*
* @param g グラフィック
* @param scale 目標の大きさ
*/
void scaleYAdd(Sprite* g, float scale) {
    Animation* data = get(g);
    if (data == NULL)return;

    stopScaleY(g);

    data->targetValue.scaleY = data->startValue.scaleY + scale;
    data->scaleY = true;
    current = data;
}

/**
* @brief 現在の角度から、指定した角度だけ回転
*
* @param g グラフィック
* @param rotation 目標角度
*/
void rotateAdd(Sprite* g, float rotation) {
    Animation* data = get(g);
    if (data == NULL)return;

    stopRotation(g);

    data->targetValue.rotation = data->startValue.rotation + rotation;
    data->rotation = true;
    current = data;
}

/**
* @brief 現在の透明度から、指定した分変化
*
* @param g グラフィック
* @param alpha 目標の透明度
*/
void alphaAdd(Sprite* g, float alpha) {
    Animation* data = get(g);
    if (data == NULL)return;

    stopAlpha(g);

    int value = (int)data->startValue.color.a + (int)alphaFromFloat(alpha);
    if (value > 255) {
        data->targetValue.color.a = 255;
    }
    else if (value < 0) {
        data->targetValue.color.a = 0;
    }
    else {
        data->targetValue.color.a = (Uint8)value;
    }
    data->alpha = true;
    current = data;
}

/**
* @brief 現在の透明度から、指定した分変化
*
* @param g グラフィック
* @param alpha 目標の透明度
*/
void colorAdd(Sprite* g, SDL_Color color) {
    Animation* data = get(g);
    if (data == NULL)return;

    stopColor(g);
    data->targetValue.color.r = SDL_clamp(data->startValue.color.r + color.r, 0, 255);
    data->targetValue.color.g = SDL_clamp(data->startValue.color.g + color.g, 0, 255);
    data->targetValue.color.b = SDL_clamp(data->startValue.color.b + color.b, 0, 255);
    data->color = true;
    current = data;
}

/**
* @brief アニメーションの停止
*
* @param g 停止させたいアニメーションのSprite型構造体変数の参照
*/
void stopAnimation(Sprite* g) {

    for (int i = 0; i < ANIMATION_MAX; i++)
    {
        if (list[i].isEnabled && list[i].g == g) {
            list[i].isEnabled = false;
        }
    }

}

/**
* @brief アニメーションの一時停止
*
* @param g 一時停止させたいアニメーションのSprite型構造体変数の参照
*/
void pauseAnimation(Sprite* g) {

    for (int i = 0; i < ANIMATION_MAX; i++)
    {
        if (list[i].isEnabled && list[i].g == g) {
            list[i].pause = true;
        }
    }

}

/**
* @brief アニメーションの一時停止解除
*
* @param g 一時停止解除させたいアニメーションのSprite型構造体変数の参照
*/
void resumeAnimation(Sprite* g) {

    for (int i = 0; i < ANIMATION_MAX; i++)
    {
        if (list[i].isEnabled && list[i].g == g) {
            list[i].pause = false;
        }
    }
}

/**
* @brief 全てのアニメーションの停止
*/
void stopAnimationAll() {

    for (int i = 0; i < ANIMATION_MAX; i++)
    {
        list[i].isEnabled = false;
    }
}

/**
* @brief 全てのアニメーションの一時停止
*/
void pauseAnimationAll() {

    for (int i = 0; i < ANIMATION_MAX; i++)
    {
        if (list[i].isEnabled) {
            list[i].pause = true;
        }
    }
}

/**
* @brief 全てのアニメーションの停止解除
*/
void resumeAnimationAll() {

    for (int i = 0; i < ANIMATION_MAX; i++)
    {
        if (list[i].isEnabled) {
            list[i].pause = false;
        }
    }
}

/**
* @brief アニメーションの開始までの時間を設定する
* 
* @param delay 設定したい遅延時間（ミリ秒）
*/
void setDelay(int delay) {
    current->delay = delay;
    current->startTime = SDL_GetTicks() + delay;
    current->endTime = current->startTime + current->duration;
    current->pastTime = -delay;
}

/**
* @brief アニメーションの長さをミリ秒単位で設定する
*
* @param duration 設定したい長さ
*/
void setDuration(int duration) {
    current->endTime = current->startTime + duration;
    current->duration = duration;
}

/**
* @brief アニメーションのeasingを設定する
*
* @param easing 設定したいeasing
*/
void setEasing(float(*easing)(float ratio)) {
    current->easing = easing;
}

/**
* @brief アニメーション完了時のイベント処理設定をする
*
* @param onFinished 設定したい関数
*/
void setOnFinished(void(*onFinished)()) {
    current->onFinished = onFinished;
}

/**
* @brief アニメーションのループ設定をする
*/
void setLoop() {
    current->loop = true;
}

/**
* @brief アニメーションのループタイプ設定をする
*
* @param type 設定したいループタイプ
*/
void setLoopType(LoopType type) {
    current->loopType = type;
}

/**
* @brief 遅延実行イベント処理設定をする
*
* @param onFinished 設定したい関数
* @param duration 遅延実行時間
*/
void setTimeout(Sprite* g, void(*onFinished)(), int duration) {
    Animation* data = get(g);
    if (data == NULL)return;

    data->onFinished = onFinished;
    data->endTime = data->startTime + duration;
    data->duration = duration;
    current = data;
}

/**
* @brief OnFinishedに渡される引数を指定
*
* @param p 引数に渡される変数の参照
*/
void setCallbackTarget(void* p) {
    current->callbackTarget = p;
}

/**
* @brief スプライトを無効化
*
* @param s スプライト
*/
void disable(Sprite* p) {
    p->isEnabled = false;
}
