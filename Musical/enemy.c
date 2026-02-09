#define _USE_MATH_DEFINES
#include "enemy.h"   
#include "main.h"   
#include "sprite.h"
#include "animation.h"
#include "easing.h"
#include "particle.h"
#include "jewelry.h"
#include "dynamic_font_atlas.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL_mixer.h>

static Enemy enemies[ENEMY_MAX];
static Mix_Chunk* trackDamage;
static int imageNormalBody;
static ParticleSetting trail;
static ParticleSetting broken;

Enemy* getEnemies() {
    return enemies;
}

void enemyInit(DFA_FontID fontID) {
    imageNormalBody = loadImage("img/togeBody.png");
    for (int i = 0; i < ENEMY_MAX; i++)
    {
        enemies[i].body.isEnabled = false;
    }

    trackDamage = Mix_LoadWAV("sound/NES-Shooter01-4(Damage).mp3");
    Mix_VolumeChunk(trackDamage, (int)(MIX_MAX_VOLUME * 0.75f));
    //particleの初期化
    Sprite particle;
    spriteInit(&particle, loadImage("img/circle16x16.png"), 0, 0, 16, 16);
    particle.blendmode = SDL_BLENDMODE_ADD;
    //trailの初期化
    trail = particleSettingDefault(&particle);
    trail.count = 1;
    trail.radius = 10;
    trail.duration = 0;
    particleSetScale(&trail, 0.2f, 2, 0.1f, linear);
    particleSetAlpha(&trail, 127, 127, 0, easeOutCirc);
    particleSetLifeTime(&trail, 1000, 1500);
    //brokenの初期化
    particle.blendmode = SDL_BLENDMODE_ADD;
    broken = particleSettingDefault(&particle);
    broken.count = 20;
    broken.radius = 20;
    broken.duration = 200;
    particleSetScale(&broken, 0.2f, 2, 0.1f, linear);
    particleSetAlpha(&broken, 255, 255, 0, easeOutCirc);
    particleSetLifeTime(&broken, 500, 1000);
    particleSetPosition(&broken, 20, 60, easeOutQuint);
    particleSetRotation(&broken, 0, M_PI * 2, 0, NULL);
}

void enemyInitByType(Enemy* e, EnemyType type) {
    switch (type) {
    case ENEMY_TYPE_NORMAL:
        spriteInit(&e->body, imageNormalBody, 0, 0, 32, 32);
        e->body.color = (SDL_Color){ 128, 128, 255, 179 };
        e->body.blendmode = SDL_BLENDMODE_ADD;
        e->body.radius = 12;
        e->scale = 1.5f;
        e->speed = 1;
        e->jewelryType = JEWELRY_TYPE_BLUE;
        break;
    case ENEMY_TYPE_RED:
        spriteInit(&e->body, imageNormalBody, 0, 0, 32, 32);
        e->body.color = (SDL_Color){ 255, 128, 128, 179 };
        e->body.radius = 12;
        e->scale = 1.5f;
        e->speed = 4;
        e->jewelryType = JEWELRY_TYPE_RED;
        break;
    case ENEMY_TYPE_GREEN:
        spriteInit(&e->body, imageNormalBody, 0, 0, 32, 32);
        e->body.color = (SDL_Color){ 128, 255, 128, 179 };
        e->body.radius = 12;
        e->scale = 1.5f;
        e->speed = 5;
        e->jewelryType = JEWELRY_TYPE_GREEN;
        break;
    case ENEMY_TYPE_GOLD:
        spriteInit(&e->body, imageNormalBody, 0, 0, 32, 32);
        e->body.color = (SDL_Color){ 230, 230, 128, 255 };
        e->body.radius = 12;
        e->scale = 1.3f;
        e->speed = 5;
        e->jewelryType = JEWELRY_TYPE_GOLD;
        break;
    }
    //エフェクトの初期化
    e->trail = trail;
    e->trail.sprite.color = e->body.color;
    e->broken = broken;
    e->broken.sprite.color = e->trail.sprite.color;

    Vector2 destination = {
        - 100 + randomFloat() * (WINDOW_WIDTH + 200),
        WINDOW_HEIGHT
    };
    e->body.scale = e->scale;
    e->body.isEnabled = true;
    e->body.v.y = e->speed;
    e->body.position.x = rand() % WINDOW_WIDTH;
    e->body.position.y = -e->body.dst.h * e->scale;
    e->body.color.a = 255;
    e->body.v = vector2Subtract(&destination, &e->body.position);
    e->body.v = vector2Normalized(&e->body.v);
    e->body.v = vector2Multiply(&e->body.v, e->speed);
    e->body.accel = vector2Multiply(&e->body.v, 0.1f);
}

void enemyGenerate(bool isRandom) {
    if (isRandom && rand() % 50 > 0)return;

    for (int i = 0; i < ENEMY_MAX; i++)
    {
        Enemy* e = &enemies[i];
        if (!e->body.isEnabled) {
            int x = rand() % 10001;
            if(x < 9000)         enemyInitByType(e, ENEMY_TYPE_NORMAL);
            else if (x < 9650)   enemyInitByType(e, ENEMY_TYPE_RED);
            else if (x < 9900)   enemyInitByType(e, ENEMY_TYPE_GREEN);
            else enemyInitByType(e, ENEMY_TYPE_GOLD);
            break;
        }
    }
}

void enemyUpdate(const Vector2* target) {

    enemyGenerate(true);

    for (int i = 0; i < ENEMY_MAX; i++)
    {
        Enemy* e = &enemies[i];
        if (e->body.isEnabled) {
            e->body.v.x += e->body.accel.x;
            e->body.v.y += e->body.accel.y;
            e->body.position.x += e->body.v.x;
            e->body.position.y += e->body.v.y;
            e->body.rotation += 0.3f;
            Vector2 pos = e->body.v;
            pos = vector2Normalized(&pos);
            e->trail.position = e->body.position;
            e->trail.scale = e->scale * 2;
            particleSetScale(&e->trail, 0.2f, e->trail.scale, 0.1f, linear);
            particleStart(&e->trail);
            if (e->body.position.y > WINDOW_HEIGHT + e->body.dst.h) {
                e->body.isEnabled = false;
            }
        }
    }
}

void enemyDraw() {
    for (int i = 0; i < ENEMY_MAX; i++)
    {
        Enemy* e = &enemies[i];
        if (e->body.isEnabled) {
            spriteDraw(&e->body);
        }
    }
}

void enemyOnHitPlayer(Enemy* e, const Vector2* playerPosition) {
    e->body.isEnabled = false;

    e->broken.position = e->body.position;
    particleStart(&e->broken);
}

void enemyOnHitField(Enemy* e) {
    e->body.isEnabled = false;
    Mix_PlayChannel(4, trackDamage, 0);

    e->broken.position = e->body.position;
    particleStart(&e->broken);

    jewelryGenerate(&e->body.position, e->body.color, e->jewelryType);
}

void enemyDeleteAll() {
    for (int i = 0; i < ENEMY_MAX; i++)
    {
        Enemy* e = &enemies[i];
        if (e->body.isEnabled) {
            alphaTo(&e->body, 0);
            setDuration(1000);
            setOnFinished(disable);
        }
    }
}
