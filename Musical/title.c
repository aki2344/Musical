/*
 * @file title.c
 * @brief タイトル画面処理を実装
 */
#define _USE_MATH_DEFINES
#include "title.h"       
#include "main.h"
#include "sprite.h"            
#include "image.h"                         
#include "key.h"                                
#include "mouse.h"   
#include "animation.h"        
#include "easing.h"       
#include "gamepad.h"
#include "dynamic_font_atlas.h"
#include <SDL2/SDL_mixer.h>
#include <stdbool.h>
#include <stdio.h>

 /**
 * @brief 流れの定数
 */
static enum {
    START,    ///< スタート
    PLAY,    ///< プレイ中
    EXIT    ///< 終了時
}  nowSequence;

static Sprite fade;            ///< フェード用変数
static Sprite titleText;
static Sprite startText;
static Sprite halo1;
static Sprite halo2;
static Sprite halo3;
static Sprite field;
static const SDL_Color dark = { 0, 0, 13, 255 };
static const SDL_Color light = { 230, 230, 255, 255 };
static SDL_Color outline = { 51, 51, 102, 255 };
static SDL_Color bgColor;
static float t;
static int direction;
static Mix_Music* trackbgm;
static Mix_Chunk* trackSE;
static DFA_FontID text;
static DFA_TextLayout layoutCenterMiddle;

static bool isRunning;   ///< ループ状態チェック変数

/**
* @brief 終了処理
*/
static void end() {
    isRunning = false;
    setSequence(MAINGAME);
}

/**
* @brief 開始時のフェード後の処理
*/
static void start() {
    nowSequence = PLAY;
}

/**
 * @brief タイトル画面の初期化
 * 
 * タイトル画面の各種初期化を行う
 */
static void init(){

    stopAnimationAll();

    layoutCenterMiddle = DFA_TextLayoutDefault();
    layoutCenterMiddle.align = DFA_ALIGN_CENTER;
    layoutCenterMiddle.vanchor = DFA_VANCHOR_MIDDLE;
    layoutCenterMiddle.max_width_px = 0;
    layoutCenterMiddle.wrap = DFA_WRAP_CHAR;

    //titleTextの初期化
    spriteInit(&titleText, 0, 0, 0, 0, 0);
    titleText.color = (SDL_Color){ 255, 237, 255, 255 };
    titleText.position.x = WINDOW_WIDTH / 2;
    titleText.position.y = WINDOW_HEIGHT / 2 - 10;
    titleText.scale = 0.5f;
    scaleTo(&titleText, 1);
    setDuration(3000);
    setEasing(easeOutQuint);

    //startTextの初期化
    spriteInit(&startText, 0, 0, 0, 0, 0);
    startText.color = (SDL_Color){ 255, 237, 255, 255 };
    startText.position.x = WINDOW_WIDTH / 2;
    startText.position.y = WINDOW_HEIGHT / 2 + 140;
    startText.scale = 0.5f;
    alphaTo(&startText, 1);
    setDuration(500);
    setEasing(easeInOutSine);
    setLoopType(PING_PONG);
    setLoop();

    //テキストの初期化
    TTF_Init();
    text = DFA_Init(
        getRenderer(), "text",
        "x12y16pxMaruMonica.ttf",
        32, TTF_STYLE_NORMAL,
        2048, 2048, 1, true, "cache/x12y16pxMaruMonica");

    DFA_RequestText(text, "彗星の　カケラpushentrico0123456789");
    
    //フェードの初期化
    spriteInit(&fade, loadImage("img/white.png"),0, 0, 8, 8);
    fade.position.x = WINDOW_WIDTH / 2;
    fade.position.y = WINDOW_HEIGHT / 2;
    fade.scale = WINDOW_WIDTH / fade.src.w;

    //fieldの初期化
    spriteInit(&field, loadImage("img/field.png"), 0, 0, 256, 256);
    field.position.x = WINDOW_WIDTH / 2;
    field.position.y = WINDOW_HEIGHT * 1.5f;
    field.scale = 3;

    //haloの初期化
    spriteInit(&halo1, loadImage("img/circle256x256.png"), 0, 0, 256, 256);
    halo1.position = field.position;
    halo1.scale = 3.8f;
    halo1.color.a = 6;
    halo1.blendmode = SDL_BLENDMODE_ADD;
    halo3 = halo2 = halo1;
    halo2.scale *= 1.2f;
    halo3.scale *= 1.4f;

    //ミキサーの生成
    //BGMの初期化
    trackbgm = Mix_LoadMUS("sound/White_snow_chill_days.mp3");
    Mix_PlayMusic(trackbgm, -1);

    //決定音の初期化
    trackSE = Mix_LoadWAV("sound/決定ボタンを押す41.mp3");
    Mix_VolumeChunk(trackSE, (int)(MIX_MAX_VOLUME * 0.75f));

    //フェードインの開始
    alphaTo(&fade, 0);
    setDuration(500);
    setOnFinished(start);

    SDL_ShowCursor(SDL_DISABLE);

    bgColor = dark;
    direction = 1;
    t = 0;

    nowSequence = START;
    isRunning = true;
}

static void colorAnimationUpdate(void) {

    bgColor = lerpColor(dark, light, t);
    t += 0.0001f * direction;
    if (t >= 1) {
        t = 1;
        direction *= -1;
    }
    if (t <= 0) {
        t = 0;
        direction *= -1;
    }
    field.color.a = (Uint8)((0.3f + 0.7f * (1 - t)) * 255.0f);
}

/**
 * @brief 流れ毎の処理
 */
static void update(){

    bool enter =
        getKeyDown(SDL_SCANCODE_RETURN) ||
        getGamepadButtonDown(0, SDL_CONTROLLER_BUTTON_A);
    bool esc =
        getKeyDown(SDL_SCANCODE_ESCAPE) ||
        getGamepadButtonDown(0, SDL_CONTROLLER_BUTTON_GUIDE);
    switch(nowSequence){

    //フェードイン
    case START:

        break;

    //プレイ中
    case PLAY:
        //ENTERキーで決定
        if(enter){
            Mix_PlayChannel(-1, trackSE, 0);

            startText.rotation = -M_PI / 6;
            rotateTo(&startText, 0);
            setDuration(500);
            setEasing(easeOutElastic);

            alphaTo(&fade, 1);
            setDuration(1000);
            setOnFinished(end);

            nowSequence = EXIT;
        }
        colorAnimationUpdate();
        break;

    //フェードアウト中
    case EXIT:

        break;
    }
    if (esc) {
        isRunning = false;
        setSequence(END);
    }
}

static void drawOutlineTitleText(const char* str, int offset) {
    for (int i = 1; i <= offset; i++) {
        DFA_DrawText(text,
            titleText.position.x + i,
            titleText.position.y + i, titleText.scale, 0,
            outline, &layoutCenterMiddle, str);
        DFA_DrawText(text,
            titleText.position.x - i,
            titleText.position.y - i, titleText.scale, 0,
            outline, &layoutCenterMiddle, str);
        DFA_DrawText(text,
            titleText.position.x - i,
            titleText.position.y + i, titleText.scale, 0,
            outline, &layoutCenterMiddle, str);
        DFA_DrawText(text,
            titleText.position.x + i,
            titleText.position.y - i, titleText.scale, 0,
            outline, &layoutCenterMiddle, str);
    }
    DFA_DrawText(text,
        titleText.position.x,
        titleText.position.y, titleText.scale, 0,
        titleText.color, &layoutCenterMiddle, str);
}

/**
 * @brief 描画
 */ 
static void draw(){
    
    //描画
    fillRect(&(SDL_FRect) { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT }, 
        bgColor.r / 255.0f, bgColor.g / 255.0f, bgColor.b / 255.0f, bgColor.a / 255.0f);

    spriteDraw(&halo3);
    spriteDraw(&halo2);
    spriteDraw(&halo1);
    spriteDraw(&field);

    drawOutlineTitleText("彗　星　の　カ　ケ　ラ", 1);

    DFA_DrawText(text,
        startText.position.x,
        startText.position.y, startText.scale, startText.rotation,
        startText.color, &layoutCenterMiddle, "push enter");

    DFA_DrawText(text,
        WINDOW_WIDTH / 2,
        startText.position.y + 40, startText.scale, 0,
        titleText.color, &layoutCenterMiddle,
        "HI-SCORE  %05d", getHiScore());

    DFA_Update(64);

    spriteDraw(&fade);
        
    //ウィンドウの更新
    flip();
}

/**
 * @brief 終了
 */
static void quit() {
    Mix_FreeMusic(trackbgm);
    Mix_FreeChunk(trackSE);
    DFA_Quit();
    TTF_Quit();
    freeImage();
}

/**
 * @brief タイトル処理
 */
void title(){

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
