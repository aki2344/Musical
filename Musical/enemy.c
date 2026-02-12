#include "enemy.h"
#include "sprite.h"
#include "animation.h"
#include "easing.h"
#include "image.h"
#include "particle.h"
#include "musicEvent.h"

static Sprite enemy;
static Sprite particle;
static ParticleSetting crash;

static void kick(AppState* st, uint8_t track, uint8_t note, uint8_t vel, bool on) {
    if (!on)return;
    printf("[MIDI] track=%u note=%u vel=%u on=%d\n", track, note, vel, on);
    //printf("[MIDI] scale=%f alpha=%d x=%f y=%f\n", 
    //    enemy.scale, enemy.color.a, enemy.position.x, enemy.position.y);
    enemy.scale = 1.5f;
    scaleTo(&enemy, 2.0f);
    setEasing(easeOutElastic);
    setDuration(350);
    int range = 100;
    float x = -range / 2.0f + (float)(rand() % range) + 640 / 2.0f;
    float y = -range / 2.0f + (float)(rand() % range) + 480 / 2.0f;
    enemy.position.x = x;
    enemy.position.y = y;

    crash.position = enemy.position;
    particleStart(&crash);
}

void enemyInit(int x, int y) {
    //enemyÇÃèâä˙âª
    spriteInit(&enemy, loadImage("img/girl.png"), 0, 0, 32, 32);
    spriteAnimeInit(&enemy, 4, 6, -1);
    enemy.scale = 2;
    enemy.position.x = x;
    enemy.position.y = y;

    musicEventRegisterMidiTrackHandler(1, kick);
    musicEventSetMidiTrackEnabled(1, true);


    spriteInit(&particle, loadImage("img/heart.png"), 0, 0, 32, 32);
    crash = particleSettingDefault(&particle);
    crash.count = 8;
    crash.radius = 30;
    crash.duration = 20;
    particleSetScale(&crash, 0.2f, 2, 0.1f, linear);
    //particleSetAlpha(&crash, 255, 255, 0, easeOutCirc);
    particleSetLifeTime(&crash, 100, 300);
    particleSetPosition(&crash, 20, 60, easeOutQuint);
    particleSetRotation(&crash, 0, M_PI * 2, 0, NULL);
}

void enemyDraw(void) {
    particleDraw();
    spriteDraw(&enemy);
    
}