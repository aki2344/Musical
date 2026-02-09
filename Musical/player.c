#define _USE_MATH_DEFINES
#include "player.h"
#include "enemy.h"  
#include "main.h"                                       
#include "image.h"        
#include "key.h"                                
#include "mouse.h"     
#include "sprite.h"
#include "animation.h"
#include "easing.h"
#include "particle.h"
#include "dynamic_font_atlas.h"
#include "gamepad.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL_mixer.h>

static Mix_Chunk* trackGet;
static Mix_Chunk* trackDamage;
static Mix_Chunk* trackJump;
static Mix_Chunk* trackLanding;
static Mix_Chunk* trackLevelUp;
static Mix_Chunk* trackDash;
static int dashChannel = 0;
static Sprite body;
static Sprite textText;
static Sprite levelUpText;
static Sprite heart;
static Sprite dropHeart;
static Sprite dashGauge;
static Sprite dashGaugeBG;
static DFA_FontID text;
static int life;
static int level;
static float x;
static float y;
static float speed;
static float stamina;
static float staminaMax;
static float defaultScale;
static Sprite* field;
static bool isGrounded;
static ParticleSetting dashSetteing;
static DFA_TextLayout layoutCenterMiddle;
static const float gravity = -0.5f;

Sprite* getPlayer() {
    return &body;
}

void playerInit(DFA_FontID fontID, Sprite* f){
    field = f;

    defaultScale = 2;
    spriteInit(&body, loadImage("img/player.png"), 0, 0, 16, 16);
    spriteAnimeInit(&body, 2, 3, -1);
    body.accel.y = gravity;
    body.radius = 8;
    x = -M_PI / 2;
    speed = 0.3f;

    body.scale = defaultScale * 0.5f;
    scaleTo(&body, defaultScale);
    setEasing(easeOutElastic);
    isGrounded = false;

    //particleの初期化
    Sprite particle;
    spriteInit(&particle, loadImage("img/circle16x16.png"), 0, 0, 16, 16);
    particle.blendmode = SDL_BLENDMODE_ADD;
    dashSetteing = particleSettingDefault(&particle);
    dashSetteing.count = 1;
    dashSetteing.duration = 0;
    particleSetScale(&dashSetteing, 2, 2, 2, linear);
    particleSetAlpha(&dashSetteing, 0.5f, 0.5f, 0, easeOutCirc);
    particleSetLifeTime(&dashSetteing, 200, 200);

    //ハートの初期化
    spriteInit(&heart, loadImage("img/heart.png"), 0, 0, 32, 32);
    heart.scale = 1;
    heart.position.x = WINDOW_WIDTH - heart.src.w;
    heart.position.y = 0;
    //消えるハートの初期化
    spriteInit(&dropHeart, heart.image, 0, 0, 32, 32);
    dropHeart.isEnabled = false;
    dropHeart.scale = heart.scale;

    spriteInit(&dashGauge, loadImage("img/gauge.png"), 0, 0, 32, 8);
    spriteInit(&dashGaugeBG, loadImage("img/gauge.png"), 0, 0, 32, 8);
    dashGauge.color = (SDL_Color){ 128, 255, 128, 230 };
    dashGaugeBG.color = (SDL_Color){ 0, 0, 0, 204 };

    spriteInit(&textText, 0, 0, 0, 0, 0);
    textText.color = (SDL_Color){ 255, 128, 128, 0 };
    text = fontID;
    spriteInit(&levelUpText, 0, 0, 0, 0, 0);
    levelUpText.color = (SDL_Color){ 255, 128, 128, 0 };
    text = fontID;

    layoutCenterMiddle = DFA_TextLayoutDefault();
    layoutCenterMiddle.align = DFA_ALIGN_CENTER;
    layoutCenterMiddle.vanchor = DFA_VANCHOR_MIDDLE;
    layoutCenterMiddle.max_width_px = 0;
    layoutCenterMiddle.wrap = DFA_WRAP_CHAR;

    //跳ね返る音の初期化
    trackGet = Mix_LoadWAV("sound/GB-Action01-01(Jump).mp3");
    Mix_VolumeChunk(trackGet, (int)(MIX_MAX_VOLUME * 0.75f));
    //ダメージ音の初期化
    trackDamage = Mix_LoadWAV("sound/NES-Shooter01-5(Damage).mp3");
    Mix_VolumeChunk(trackDamage, (int)(MIX_MAX_VOLUME * 0.75f));
    //ジャンプ音の初期化
    trackJump = Mix_LoadWAV("sound/GB-Action01-01(Jump).mp3");
    Mix_VolumeChunk(trackJump, (int)(MIX_MAX_VOLUME * 0.75f));
    //着地音の初期化
    trackLanding = Mix_LoadWAV("sound/Anime_Motion17-1(Low).mp3");
    Mix_VolumeChunk(trackLanding, (int)(MIX_MAX_VOLUME * 0.75f));
    //レベルアップ音の初期化
    trackLevelUp = Mix_LoadWAV("sound/NES-RPG03-09(Magic-Cure).mp3");
    Mix_VolumeChunk(trackLevelUp, (int)(MIX_MAX_VOLUME * 0.8f));
    //ダッシュ音の初期化
    trackDash = Mix_LoadWAV("sound/GB-General02-09(Pitch)_01.wav");
    Mix_VolumeChunk(trackDash, (int)(MIX_MAX_VOLUME * 0.5f));

    life = 3;
    stamina = staminaMax = 1;
    level = 0;
}

void playerHeartDraw() {
    for (int i = 0; i < life; i++)
    {
        spriteDrawOffset(&heart, -(heart.dst.w - 10) * i, heart.dst.h / 2 * 0.8f);
    }

    spriteDraw(&dropHeart);
}

static void dropHeartOnDamage() {
    dropHeart.position = (Vector2){ 
        heart.position.x -(heart.dst.w - 30) * (life - 1), 
        heart.position.y + heart.dst.h / 2 * 0.8f 
    };
    dropHeart.isEnabled = true;

    dropHeart.rotation = 0;
    rotateTo(&dropHeart, M_PI * 2);
    setEasing(easeOutExpo);
    setDuration(1000);

    dropHeart.scale = heart.scale;
    scaleTo(&dropHeart, 0);
    setEasing(easeOutCirc);
    setDuration(1000);
}

void playerUpdate() {

    float size = body.src.w * body.scale;
    float fieldDiameter = field->src.h * field->scale;
    float fieldLine = fieldDiameter / 2;

    //ダッシュ
    if (getKey(SDL_SCANCODE_LSHIFT) || 
        getGamepadButton(0, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
        if (stamina > 0) {
            speed = 0.8f;
            dashSetteing.position = body.position;
            particleStart(&dashSetteing);
            stamina -= 0.05f;
            if ((getKey(SDL_SCANCODE_LEFT) || getKey(SDL_SCANCODE_RIGHT)) ||
                (getGamepadButton(0, SDL_CONTROLLER_BUTTON_DPAD_UP) ||
                getGamepadButton(0, SDL_CONTROLLER_BUTTON_DPAD_DOWN))) {
                if (!Mix_Playing(dashChannel))
                    Mix_PlayChannel(dashChannel, trackDash, 0);
            }
        }
        else {
            speed = 0.25f;
        }
    }
    else {
        speed = 0.25f;
        stamina += 0.005f;
        stamina = stamina > staminaMax ? staminaMax : stamina;
    }

    //左右移動
    if (getKey(SDL_SCANCODE_LEFT) ||
        getGamepadButton(0, SDL_CONTROLLER_BUTTON_DPAD_UP)) {
        body.rotation -= speed;
        x -= speed * size / fieldDiameter;
    }
    else if (getKey(SDL_SCANCODE_RIGHT) ||
        getGamepadButton(0, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
        body.rotation += speed;
        x += speed * size / fieldDiameter;
    }
    else {
        body.rotation = x + M_PI / 2;
    }
    x = x < -M_PI / 6 * 4.7f ? -M_PI / 6 * 4.7f : x;
    x = x > -M_PI / 6 * 1.3f ? -M_PI / 6 * 1.3f : x;

    //ブレーキアニメーション
    if (isGrounded && 
        ((getKeyUp(SDL_SCANCODE_LEFT) || getKeyUp(SDL_SCANCODE_RIGHT)) &&
        (getGamepadButtonUp(0, SDL_CONTROLLER_BUTTON_DPAD_UP) || 
        getGamepadButtonUp(0, SDL_CONTROLLER_BUTTON_DPAD_DOWN)))) {
        body.scale = 1;
        scaleTo(&body, 2);
        setEasing(easeOutElastic);
        Mix_PlayChannel(1, trackLanding, 0);
    }
    //ジャンプ
    if (isGrounded && (getKeyDown(SDL_SCANCODE_SPACE) ||
        getGamepadButton(0, SDL_CONTROLLER_BUTTON_A))) {
        body.v.y = 12;
        isGrounded = false;

        Mix_PlayChannel(1, trackJump, 0);
    }

    //着地
    if (!isGrounded) {
        body.v.y += body.accel.y;
        y += body.v.y;
        if (y < fieldLine + size / 2) {
            y = fieldLine + size / 2;
            isGrounded = true;
            body.scale = 1;
            scaleTo(&body, 2);
            setEasing(easeOutElastic);
            Mix_PlayChannel(1, trackLanding, 0);
        }
    }

    //実座標に反映
    body.position.x = field->position.x + cosf(x) * y;
    body.position.y = field->position.y + sinf(x) * y;

    dashGauge.position = body.position;
    dashGaugeBG.position = body.position;
    dashGauge.position.y -= 30;
    dashGaugeBG.position.y -= 30;
    dashGauge.src.w = dashGaugeBG.src.w * (stamina / staminaMax);
}

void playerLevelUp() {
    level++;
    staminaMax += 0.2f;

    levelUpText.position = body.position;
    levelUpText.position.y -= 40;
    moveAdd(&levelUpText, 0, -30);
    setDuration(500);
    setEasing(easeOutQuint);

    levelUpText.scale = 0.1f;
    scaleTo(&levelUpText,0.5f);
    setDuration(500);
    setEasing(easeOutElastic);

    levelUpText.color.a = 255;
    alphaTo(&levelUpText, 0);
    setDelay(1000);
    setDuration(500);
    setEasing(easeOutCubic);

    Mix_PlayChannel(2, trackLevelUp, 0);
}

void playerDraw() {
    spriteDraw(&body);

    DFA_DrawText(text,
        textText.position.x,
        textText.position.y, 0.5f, 0,
        textText.color, &layoutCenterMiddle, "イテッ！");

    DFA_DrawText(text,
        levelUpText.position.x,
        levelUpText.position.y, levelUpText.scale, 0,
        levelUpText.color, &layoutCenterMiddle, "LEVEL UP!");

    DFA_DrawText(text,
        body.position.x,
        body.position.y - 45, 0.5f, 0,
        (SDL_Color){255, 255, 255, 255}, &layoutCenterMiddle, "ｽﾀﾐﾅLv%2d", level + 1);

    spriteDraw(&dashGaugeBG);
    spriteDraw(&dashGauge);

    DFA_Update(64);
}

bool isPlayerDead() {
    return life <= 0;
}

static void setNormalFace() {
    if(life > 0)
        body.src.y = 0;
}

void playerOnHitEnemy(Enemy* e) {
    dropHeartOnDamage();
    life--;

    body.scale = defaultScale * 0.5f;
    scaleTo(&body, defaultScale);
    setDuration(1000);
    setEasing(easeOutElastic);

    body.src.y = body.src.h;
    setTimeout(NULL, setNormalFace, 1000);

    textText.position = body.position;
    textText.position.y -= 10;
    moveAdd(&textText, 0, -50);
    setDuration(1000);
    setEasing(easeOutQuint);

    textText.color.a = 255;
    alphaTo(&textText, 0);
    setDelay(500);
    setDuration(500);
    setEasing(easeOutCubic);

    Mix_PlayChannel(3, trackDamage, 0);
}