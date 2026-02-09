/**
 * @file main.c
 * @brief メイン関数の実装
 */
#include "main.h"  
#include "image.h"                        
#include "key.h"                                
#include "mouse.h"        
#include "title.h"
#include "mainGame.h"
#include <SDL2/SDL_mixer.h>
#include <stdlib.h>

static int sequence;    ///< 流れの管理用変数
static int score;
static int hiscore;
static float deltaTime;

 /**
 * @brief メイン関数
 * 
 * @param argc 実行時引数の数
 * @param argv 実行時引数文字列の配列
 */
int main(int argc, char* argv[]){

    //ビデオとオーディオを初期化
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS);

    //オーディオの初期化
    Mix_Init(MIX_INIT_MP3);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    
    //画面の初期化
    screenInit(WINDOW_WIDTH, WINDOW_HEIGHT);

    //setFullScreen(true);
    sequence = TITLE;
    sequence = MAINGAME;

    //ループ
    while(sequence != END){

        switch(sequence){

        case TITLE:
            title();
            break;

        case MAINGAME:
            mainGame();
            break;
        }
    }

    //読み込んだデータの開放                
    freeImage();

    //終了処理
    Mix_CloseAudio();
    Mix_Quit();
    SDL_Quit();                                
                        
    return 0;                    
}        

/**
* @brief 全体の流れの変数をセットする
*
* @param s セットしたい値
*/
void setSequence(int s) {
    sequence = s;
}
/**
* @brief 全体の流れの変数を取得する
* @return 全体の流れの変数の値
*/
int getSequence() {
    return sequence;
}
/**
* @brief score変数をセットする
*
* @param s セットしたい値
*/
void setScore(int s) {
    score = s;
}
/**
* @brief score変数を加算する
*
* @param s 加算したい値
*/
void addScore(int s) {
    score += s;
}
/**
* @brief score変数を取得する
* @return score変数の値
*/
int getScore() {
    return score;
}
/**
* @brief score変数をセットする
*
* @param s セットしたい値
*/
void setHiScore(int s) {
    hiscore = s;
}
/**
* @brief score変数を取得する
* @return score変数の値
*/
int getHiScore() {
    return hiscore;
}
/**
* @brief 入力イベント処理
*
* ESCAPEキーの入力で、isLoopの値をfalseにする
*
* @param isLoop メインのゲームのループ状態を管理する変数
*/
void eventInput(bool* isLoop) {

    //イベント情報格納用変数
    SDL_Event event;

    //すべてのイベントを抜き出す
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        //キーダウンイベント
        case SDL_KEYDOWN:
            switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_ESCAPE:
                sequence = END;
                *isLoop = false;
                break;
            case SDL_SCANCODE_F4:
                setFullScreen(true);
                break;
            case SDL_SCANCODE_F5:
                setFullScreen(false);
                break;
            }
            break;

        case SDL_CONTROLLERDEVICEADDED:
            openPad(event.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            closePad(event.cdevice.which);
            break;
        //終了時イベント
        case SDL_QUIT:
            sequence = END;
            *isLoop = false;
            break;
        }
    }
}

/**
* @brief score変数を取得する
* @return score変数の値
*/
void wait(void) {
    static int mTicksCount = 0;
    while (SDL_GetTicks() < mTicksCount + 16);
    deltaTime = (SDL_GetTicks() - mTicksCount) / 1000.0f;
    mTicksCount = SDL_GetTicks();
}


/**
* @brief 色の線形補間
*
* @param a 色1
* @param b 色2
* @param t 補間の割合
* @return 補間した色
*/
SDL_Color lerpColor(SDL_Color a, SDL_Color b, float t) {
    return (SDL_Color) {
        (Uint8)(a.r + (b.r - a.r) * t),
        (Uint8)(a.g + (b.g - a.g) * t),
        (Uint8)(a.b + (b.b - a.b) * t),
        (Uint8)(a.a + (b.a - a.a) * t)
    };
}

/**
* @brief floatの線形補間
*
* @param a 値1
* @param b 値2
* @param t 補間の割合
* @return 補間した値
*/
float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

/**
* @brief 0～1の乱数の生成
* @return 乱数の値
*/
float randomFloat() {
    return (rand() % 101) / 100.0f;
}
