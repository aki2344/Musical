/**
* @file image.c
* @brief 画像関係処理の実装
*/
#include "image.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#define IMAGE_MAX    64

static SDL_Texture *image[IMAGE_MAX];
static SDL_Window *window;
static SDL_Renderer *renderer;

/**
* @brief ウィンドウの初期化
*
* 幅と高さを指定してウィンドウを生成
*
* @param width ウィンドウの幅
* @param height ウィンドウの高さ
*/
void screenInit(int width, int height) {
    window = SDL_CreateWindow(
        "SDL",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_ALWAYS_ON_TOP);
    renderer = SDL_CreateRenderer(window, -1, 0);
    if (window == NULL) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
    }
    if (renderer == NULL) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
    }
    if (SDL_RenderSetLogicalSize(renderer, width, height) != 0) {
        SDL_Log("SDL_RenderSetLogicalSize failed: %s", SDL_GetError());
    }
    if (SDL_RenderSetIntegerScale(renderer, SDL_TRUE) != 0) {
        SDL_Log("SDL_RenderSetIntegerScale failed: %s", SDL_GetError());
    }
    //画像のスケールモードを変更
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    //SDL_RenderSetVSync(renderer, 1);

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer, &info) == 0) {
        SDL_Log("Renderer name=%s flags=0x%x", info.name, info.flags);
    }
    else {
        SDL_Log("SDL_GetRendererInfo failed: %s", SDL_GetError());
    }
    SDL_Log("Video driver=%s", SDL_GetCurrentVideoDriver());
}
/**
* @brief ウィンドウの終了
*
* windowとrendererを破棄する
*/
void screenQuit(void) {
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
}

/**
* @brief 画像の読み込み
*
* 指定したIDで、指定したファイルの画像を読み込む
*
* @param fileName 画像のファイル名
* @return 読み込みができない場合は-1を返す
*/
int loadImage(const char* fileName) {

    for (int i = 0; i < IMAGE_MAX; i++)
    {
        if (image[i] == NULL) {
            image[i] = IMG_LoadTexture(renderer, fileName);
            return i;
        }
    }
    return -1;
}

/**
* @brief 読み込んだ画像の開放
*
* 読み込んだ全ての画像を開放する<br>
* 画像の取得に失敗したら強制終了する
*/
void freeImage(void) {
    
    for (int i = 0; i < IMAGE_MAX; i++) {
        if (image[i]) {
            SDL_DestroyTexture(image[i]);
            image[i] = NULL;
        }
    }
}

/**
* @brief 指定IDの画像が読み込まれているかチェック
*
* @param id 画像のID
* @return 指定IDの画像が読み込まれていればtrueを返す。そうでなければfalseを返す。
*/
static bool isLoaded(int id) {
    return id >= 0 && id < IMAGE_MAX && image[id];
}

/**
* @brief 画像の描画
*
* @param id 画像のID
* @param src 描画元矩形
* @param dst 出力先矩形
* @param angle 表示角度
* @param center 回転時の中心点
* @param flip 上下、左右反転フラグ
*/
void drawImage(int id, SDL_Rect *src, SDL_FRect *dst, 
    double angle, const SDL_FPoint *center, SDL_RendererFlip flip) {
    if (isLoaded(id)) {
        SDL_RenderCopyExF(renderer, image[id], src, dst, angle, center, flip);
    }
}

/**
* @brief 透明度のセット
*
* @param id 画像のID
* @param alpha 透明度の値(0-1)
*/
void setAlpha(int id, Uint8 alpha) {
    if (isLoaded(id)) {
        SDL_SetTextureAlphaMod(image[id], alpha);
    }
}

/**
* @brief 色のセット
*
* @param id 画像のID
* @param r 赤の値(0-1)
* @param g 緑の値(0-1)
* @param b 青の値(0-1)
*/
void setColor(int id, Uint8 r, Uint8 g, Uint8 b) {
    if (isLoaded(id)) {
        SDL_SetTextureColorMod(image[id], r, g, b);
    }
}

/**
* @brief ブレンドモードのセット
*
* @param id 画像のID
* @param mode ブレンドモード
*/
void setBlendMode(int id, SDL_BlendMode mode) {
    if (isLoaded(id)) {
        SDL_SetTextureBlendMode(image[id], mode);
    }
}

/**
* @brief フルスクリーン切り替え
*/
void setFullScreen(bool isFull) {
    if (!window) {
        return;
    }
    SDL_SetWindowFullscreen(window, isFull ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

/**
* @brief 画面をクリア
*/
void clearScreen(float r, float g, float b) {
    SDL_SetRenderDrawColor(
        renderer,
        (Uint8)(r * 255.0f),
        (Uint8)(g * 255.0f),
        (Uint8)(b * 255.0f),
        255);
    SDL_RenderClear(renderer);
}

/**
* @brief Windowの更新
*/
void flip(void) {
    SDL_RenderPresent(renderer);
}

/**
* @brief 点の描画
*
* @param x x
* @param y y
* @param r 線の色 RED
* @param g 線の色 ENEMY_TYPE_GREEN
* @param b 線の色 BLUE
* @param a 線の色 ALPHA
*/
void drawPoint(float x, float y, float r, float g, float b, float a) {
    SDL_SetRenderDrawColor(
        renderer,
        (Uint8)(r * 255.0f),
        (Uint8)(g * 255.0f),
        (Uint8)(b * 255.0f),
        (Uint8)(a * 255.0f));
    SDL_RenderDrawPointF(renderer, x, y);
}

/**
* @brief 直線の描画
*
* @param x1 始点x
* @param y1 始点y
* @param x2 終点x
* @param y2 終点y
* @param r 線の色 RED
* @param g 線の色 ENEMY_TYPE_GREEN
* @param b 線の色 BLUE
* @param a 線の色 ALPHA
*/
void drawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a) {
    SDL_SetRenderDrawColor(
        renderer,
        (Uint8)(r * 255.0f),
        (Uint8)(g * 255.0f),
        (Uint8)(b * 255.0f),
        (Uint8)(a * 255.0f));
    SDL_RenderDrawLineF(renderer, x1, y1, x2, y2);
}
/**
* @brief 矩形の描画
*
* @param r 矩形情報
* @param r 線の色 RED
* @param g 線の色 ENEMY_TYPE_GREEN
* @param b 線の色 BLUE
* @param a 線の色 ALPHA
*/
void drawRect(const SDL_FRect* rect, float r, float g, float b, float a) {
    SDL_SetRenderDrawColor(
        renderer,
        (Uint8)(r * 255.0f),
        (Uint8)(g * 255.0f),
        (Uint8)(b * 255.0f),
        (Uint8)(a * 255.0f));
    SDL_RenderDrawRectF(renderer, rect);
}
/**
* @brief 矩形の描画
*
* @param r 矩形情報
* @param r 線の色 RED
* @param g 線の色 ENEMY_TYPE_GREEN
* @param b 線の色 BLUE
* @param a 線の色 ALPHA
*/
void fillRect(const SDL_FRect* rect, float r, float g, float b, float a) {
    SDL_SetRenderDrawColor(
        renderer,
        (Uint8)(r * 255.0f),
        (Uint8)(g * 255.0f),
        (Uint8)(b * 255.0f),
        (Uint8)(a * 255.0f));
    SDL_RenderFillRectF(renderer, rect);
}
/**
* @brief 基本図形の描画モード変更
*
* @param mode ブレンドモード
*/
void setDrawMode(SDL_BlendMode mode) {
    SDL_SetRenderDrawBlendMode(renderer, mode);
}
/**
* @brief 円形の描画
*
* @param x x
* @param y y
* @param radius 半径
* @param r 線の色 RED
* @param g 線の色 ENEMY_TYPE_GREEN
* @param b 線の色 BLUE
* @param a 線の色 ALPHA
*/
void drawCircle(float x, float y, float radius, float r, float g, float b, float a) {
    SDL_SetRenderDrawColor(
        renderer,
        (Uint8)(r * 255.0f),
        (Uint8)(g * 255.0f),
        (Uint8)(b * 255.0f),
        (Uint8)(a * 255.0f));
    bool drawingLine = false;
    SDL_FPoint start;
    SDL_FPoint end;
    for (int i = y - radius; i < y + radius; i++) {
        drawingLine = false;
        for (int j = x - radius; j < x + radius; j++) {
            int dx = j - x;
            int dy = i - y;
            int d = dx * dx + dy * dy;
            if (d < radius * radius) {
                if (drawingLine) {
                    end = (SDL_FPoint){ j, i };
                }
                else {
                    drawingLine = true;
                    start = (SDL_FPoint){ j, i };
                    end = start;
                }
            }
            else if (drawingLine) {
                SDL_RenderDrawLineF(renderer, start.x, start.y, end.x, end.y);
                drawingLine = false;
            }
        }
        if (drawingLine) {
            SDL_RenderDrawLineF(renderer, start.x, start.y, end.x, end.y);
}
    }
}
/**
* @brief 扇形の描画
*
* @param x x
* @param y y
* @param radius 半径
* @param direction 扇型の中心方向
* @param angle 扇形の半分の角度
* @param r 線の色 RED
* @param g 線の色 ENEMY_TYPE_GREEN
* @param b 線の色 BLUE
* @param a 線の色 ALPHA
*/
void drawArc(float x, float y, float radius, float direction, double angle, float r, float g, float b, float a) {
    SDL_SetRenderDrawColor(
        renderer,
        (Uint8)(r * 255.0f),
        (Uint8)(g * 255.0f),
        (Uint8)(b * 255.0f),
        (Uint8)(a * 255.0f));
    float cos_dir = cosf(direction);
    float sin_dir = sinf(direction);
    float cos_angle = cosf(angle);
    bool drawingLine = false;
    SDL_FPoint start;
    SDL_FPoint end;
    for (int i = y - radius; i < y + radius; i++) {
        drawingLine = false;
        for (int j = x - radius; j < x + radius; j++) {
            int dx = j - x;
            int dy = i - y;
            int d = dx * dx + dy * dy;
            double norm = sqrtf(d);
            double product = dx * cos_dir + dy * sin_dir;
            if (product / norm > cos_angle &&
                d < radius * radius) {

                if (drawingLine) {
                    end = (SDL_FPoint){ j, i };
                }
                else{
                    drawingLine = true;
                    start = (SDL_FPoint){ j, i };
                    end = start;
                }
            }
            else if (drawingLine) {
                SDL_RenderDrawLineF(renderer, start.x, start.y, end.x, end.y);
                drawingLine = false;
            }
        }
        if (drawingLine) {
            SDL_RenderDrawLineF(renderer, start.x, start.y, end.x, end.y);
}
    }
}

SDL_Renderer* getRenderer(void)
{
    return renderer;
}
