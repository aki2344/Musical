/**
 * @file mainGame.c
 * @brief メインゲーム処理を実装
 */
#define _USE_MATH_DEFINES
#include "mainGame.h"   
#include "main.h"                                       
#include "image.h"        
#include "key.h"                                
#include "mouse.h"     
#include "sprite.h"
#include "animation.h"
#include "easing.h"
#include "dynamic_font_atlas.h"
#include "enemy.h"
 //#include "jewelry.h"
 //#include "player.h"
 //#include "particle.h"
 #include "star.h"
#include "gamepad.h"
#include "musicEvent.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL_mixer.h>
 /*
 * @brief 流れの管理用
 */
static enum {
    START,  ///< スタート
    PLAY,   ///< プレイ中
    PAUSE,  ///< ポーズ中
    EXIT    ///< 終了時
} nowSequence;

#define BLOCK_MAX   80

static bool isRunning;       ///< ループ状態チェック変数
static Sprite fade;        ///< フェード用変数
static Sprite pauseText;
static Sprite gameOverText;
static Sprite infoText;
static Sprite board;
static Sprite gauge[6];
static Sprite blocks[BLOCK_MAX][BLOCK_MAX];
static DFA_FontID text;
static DFA_TextLayout layoutCenterMiddle;
static DFA_TextLayout layoutLeftCenter;
static Mix_Music* trackbgm;
static Mix_Chunk* trackGameOver;
static Mix_Chunk* trackSE;
static Mix_Chunk* trackHiScore;
static float fieldLine;
static int image;
static float cycleX;
static float cycleY;
static bool lack;
static float speed;
static float amplitude;
static float scale;
//static ParticleSetting setting;

static float r;

static void handle_midi_test(AppState* st, uint8_t track, uint8_t note, uint8_t vel, bool on) {
    (void)st;
    printf("[MIDI] track=%u note=%u vel=%u on=%d\n", track, note, vel, on);
}

static void end() {
    isRunning = false;
    setSequence(TITLE);
}
static void start() {
    nowSequence = PLAY;
}
static void init() {

    //乱数の初期化
    srand((unsigned int)time(NULL));

    //infoTextの初期化
    spriteInit(&infoText, 0, 0, 0, 0, 0);
    infoText.color = (SDL_Color){ 255, 255, 255, 255 };
    infoText.scale = 0.5f;

    //フェードの初期化
    spriteInit(&fade, loadImage("img/white.png"), 0, 0, 1, 1);
    fade.color.r = fade.color.g = fade.color.b = 0;
    fade.position.x = WINDOW_WIDTH / 2;
    fade.position.y = WINDOW_HEIGHT / 2;
    fade.scale = WINDOW_WIDTH / fade.src.w;

    //テキストの初期化
    TTF_Init();
    text = DFA_Init(
        getRenderer(), "text",
        "x12y16pxMaruMonica.ttf",
        32, TTF_STYLE_NORMAL,
        2048, 2048, 1, true, "cache/x12y16pxMaruMonica");

    DFA_RequestText(text, "0123456789");

    //フェードインの開始
    alphaTo(&fade, 0);
    setOnFinished(start);

    musicEventInit("sound/ss.wav", "sound/song.mid");
    
    enemyInit(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
    starInit();

    nowSequence = START;

    isRunning = true;
}

/**
 * @brief 流れ毎の処理
 */
static void update() {
    musicEventUpdate();
    bool esc =
        getKeyDown(SDL_SCANCODE_ESCAPE) ||
        getGamepadButtonDown(0, SDL_CONTROLLER_BUTTON_GUIDE);
    if (esc) {
        isRunning = false;
        setSequence(END);
    }
    
}

/**
 * @brief 描画
 */
static void draw() {

    //描画
    fillRect(&(SDL_FRect) { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT }, 0.0f, 0.0f, 0.05f, 0.8f); 
    DFA_DrawText(text, 10, 0, infoText.scale, 0, infoText.color, &layoutLeftCenter, getInfo());
    DFA_Update(64);

    starDraw();
    enemyDraw();

    //ウィンドウの更新
    flip();
}

/**
 * @brief 終了
 */
static void quit() {
    Mix_FreeMusic(trackbgm);
    Mix_FreeChunk(trackGameOver);
    Mix_FreeChunk(trackSE);
    Mix_FreeChunk(trackHiScore); 

    musicEventQuit();;

    DFA_Quit();
    TTF_Quit();
    freeImage();
}

/**
 * @brief メイン関数
 */
void mainGame() {

    //初期化
    init();
    //ループ
    while (isRunning) {
        //更新処理
        update();

        //描画
        draw();

        easingUpdate();
        eventInput(&isRunning);

        //入力の更新
        keyUpdate();
        mouseUpdate();
        gamepadUpdate();

        wait();
    }
    quit();
}
