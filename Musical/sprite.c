/*
* @file sprite.c
* @brief Sprite構造体処理の実装
*/
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include "sprite.h"
#include "image.h"

/**
* @brief スプライトの初期化
*
* @param s スプライト
* @param id イメージのID
* @param srcX 元画像のx座標
* @param srcY 元画像のy座標
* @param srcW 元画像の幅
* @param srcH 元画像の高さ
*/
void spriteInit(Sprite *s, int id, float srcX, float srcY, float srcW, float srcH) {

    //src矩形の設定
    s->src = (SDL_Rect){ srcX, srcY, srcW, srcH };

    //画像の読み込み
    s->image = id;

    //表示座標の指定
    s->position = (Vector2) { 0, 0 };

    //倍率の指定
    s->scale = s->scaleX = s->scaleY = 1;

    //角度の指定
    s->rotation = 0;

    //向きの指定
    s->direction = 0;

    //pivotの指定
    s->pivot = (SDL_FPoint){ 0.5f, 0.5f };

    //表示をON
    s->isEnabled = true;

    //dst矩形と中心点の設定
    s->dst.w = s->src.w * s->scale;
    s->dst.h = s->src.h * s->scale;
    s->center.x = s->dst.w / 2;
    s->center.y = s->dst.h / 2;
    s->dst.x = s->position.x - s->center.x;
    s->dst.y = s->position.y - s->center.y;

    //アニメーションのデフォルト値をセット
    spriteAnimeInit(s, 0, 0, 0);

    //色
    s->color = (SDL_Color){ 255, 255, 255, 255 };

    //ブレンドモード
    s->blendmode = SDL_BLENDMODE_BLEND;

    //反転フラグ
    s->flip = SDL_FLIP_NONE;

    //速度と加速度
    s->v = s->accel = (Vector2){ 0,0 };

    //アニメーションをOFFにする
    s->pause = true;
}

/**
* @brief スプライトのアニメーションで使用する値の初期化
*
* @param s スプライト
* @param frameMax アニメーション枚数
* @param interval アニメーション間隔
* @param loopCount アニメーションループ回数
*/
void spriteAnimeInit(Sprite *s, int frameMax, int interval, int loopCount) {
    s->count = 0;
    s->frame = 0;
    s->frameMax = frameMax;
    s->interval = interval;
    s->loopCount = loopCount;
    s->pause = false;
    s->isEnabled = true;
}

/*
* @brief スプライトのアニメーション（X方向のアニメーション）
*
* ループ数が0になると、スプライトを非表示にする<br>
* ループ数に負の値がセットされていた場合は、ループし続ける。
*
* @param s スプライト
*/
void spriteAnime(Sprite *s) {

    if (s->pause)
        return;

    if (s->loopCount != 0) {
        s->frame = (++(s->count) / s->interval) % s->frameMax;

        if (s->count % s->interval == 0 && s->frame == 0) {
            if (s->loopCount > 0)
                s->loopCount--;
            if (s->loopCount == 0) {
                s->frame = s->frameMax - 1;
                s->isEnabled = false;
            }
        }
        //画像の矩形に反映
        s->src.x = s->src.w * s->frame;
    }
}

/**
* @brief スプライト描画
*
* @param s スプライト
*/
void spriteDraw(Sprite *s) {
    spriteDrawOffset(s, 0, 0);
}

/**
* @brief 指定分ずらした位置にスプライト描画
*
* @param s スプライト
* @param x オフセット値x
* @param y オフセット値y
*/
void spriteDrawOffset(Sprite *s, float x, float y) {
    if (s->isEnabled) {
        s->dst.w = s->src.w * s->scale * s->scaleX;
        s->dst.h = s->src.h * s->scale * s->scaleY;
        s->center.x = s->dst.w * s->pivot.x;
        s->center.y = s->dst.h * s->pivot.y;
        s->dst.x = s->position.x - s->center.x;
        s->dst.y = s->position.y - s->center.y;
        SDL_FRect dst = s->dst;
        dst.x += x;
        dst.y += y;
        spriteAnime(s);
        setAlpha(s->image, (int)s->color.a);
        setColor(s->image, s->color.r, s->color.g, s->color.b);
        setBlendMode(s->image, s->blendmode);
        drawImage(s->image, &s->src, &dst,
            s->rotation / M_PI * 180, &s->center, s->flip);
    }
}
/**
* @brief 指定した位置、大きさ、角度でスプライトを描画
*
* @param s スプライト
* @param position 表示座標
* @param rotation 角度
* @param scale 大きさ
*/
void spriteDrawEx(Sprite* s, Vector2 position, float rotation, float scale) {
    if (s->isEnabled) {
        s->dst.w = s->src.w * s->scale * s->scaleX;
        s->dst.h = s->src.h * s->scale * s->scaleY;
        s->center.x = s->dst.w * s->pivot.x;
        s->center.y = s->dst.h * s->pivot.y;
        s->dst.x = position.x - s->center.x;
        s->dst.y = position.y - s->center.y;
        SDL_FRect dst = s->dst;
        spriteAnime(s);
        setAlpha(s->image, s->color.a);
        setColor(s->image, s->color.r, s->color.g, s->color.b);
        setBlendMode(s->image, s->blendmode);
        drawImage(s->image, &s->src, &dst,
            rotation / M_PI * 180, &s->center, s->flip);
    }
}

/**
* @brief スプライトの判定矩形をセットする
*
* @param s スプライト
* @param w 判定矩形の幅
* @param h 判定矩形の高さ
*/
void spriteSetCollision(Sprite *s, float w, float h) {
    s->col.w = w;
    s->col.h = h;
}

/**
* @brief スプライトの当たり判定
*
* @param s1 スプライト
* @param s2 スプライト
* @return 触れていればtrueを返す。そうでなければfalseを返す。
*/
bool spriteIntersectsRect(const Sprite *s1, const Sprite *s2) {

    int dx = abs(s1->position.x - s2->position.x);
    int dy = abs(s1->position.y - s2->position.y);
    int w = s1->col.w * s1->scale * s1->scaleX + s2->col.w * s2->scale * s2->scaleX;
    int h = s1->col.h * s1->scale * s1->scaleY + s2->col.h * s2->scale * s2->scaleY;

    return dx <= w / 2 && dy <= h / 2;
}
/**
* @brief スプライトの当たり判定
*
* @param s1 スプライト
* @param s2 スプライト
* @return 触れていればtrueを返す。そうでなければfalseを返す。
*/
bool spriteIntersectsCircle(const Sprite *s1, const Sprite *s2) {

    double dx = s1->position.x - s2->position.x;
    double dy = s1->position.y - s2->position.y;

    double d = dx * dx + dy * dy;
    double r = s1->radius * s1->scale + s2->radius * s2->scale;

    return d < r * r;
}

/**
* @brief スプライトと点との当たり判定
*
* @param s スプライト
* @param x 判定をチェックしたい座標のX座標
* @param y 判定をチェックしたい座標のY座標
* @return 触れていればtrueを返す。そうでなければfalseを返す。
*/
bool spriteIntersectsPoint(const Sprite *s, float x, float y) {

    if (s->radius > 0) {
        double dx = x - s->position.x;
        double dy = y - s->position.y;

        double d = dx * dx + dy * dy;
        double r = s->radius * s->scale;

        return d < r * r;
    }
    else {
        int dx = abs(x - s->position.x);
        int dy = abs(y - s->position.y);
        int w = s->col.w * s->scale * s->scaleX;
        int h = s->col.h * s->scale * s->scaleY;

        return dx <= w / 2 && dy <= h / 2;

    }
}


