#include "enemy.h"
#include "sprite.h"
#include "animation.h"
#include "easing.h"
#include "image.h"
#include "musicEvent.h"

static Sprite enemy;

static void kick(AppState* st, uint8_t track, uint8_t note, uint8_t vel, bool on) {
    if (!on)return;
    //printf("[MIDI] track=%u note=%u vel=%u on=%d\n", track, note, vel, on);
    printf("[MIDI] scale=%f alpha=%d x=%f y=%f\n", 
        enemy.scale, enemy.color.a, enemy.position.x, enemy.position.y);
    enemy.scale = 1.5f;
    scaleTo(&enemy, 2.0f);
    setEasing(easeOutElastic);
    setDuration(350);
    int range = 100;
    float x = -range / 2.0f + (float)(rand() % range) + 640 / 2.0f;
    float y = -range / 2.0f + (float)(rand() % range) + 480 / 2.0f;
    enemy.position.x = x;
    enemy.position.y = y;
}

void enemyInit(int x, int y) {
    //enemyÇÃèâä˙âª
    spriteInit(&enemy, loadImage("img/enemy.png"), 0, 0, 32, 32);
    enemy.scale = 2;
    enemy.position.x = x;
    enemy.position.y = y;

    musicEventRegisterMidiTrackHandler(1, kick);
    musicEventSetMidiTrackEnabled(1, true);
}

void enemyDraw(void) {
    spriteDraw(&enemy);
}