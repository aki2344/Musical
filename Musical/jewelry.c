#define _USE_MATH_DEFINES
#include "jewelry.h"   
#include "main.h"
#include "sprite.h"
#include "animation.h"
#include "easing.h"
#include "particle.h"
#include "dynamic_font_atlas.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL_mixer.h>

static Jewelry jewelries[JEWELRY_MAX];
static Mix_Chunk* trackGet;
static Mix_Chunk* trackBonus;
static DFA_FontID text;
static Sprite* field;
static int image;
static ParticleSetting trail;
static const float gravity = -0.5f;

Jewelry* getJewelries()
{
	return jewelries;
}

void jewelryInit(DFA_FontID fontID, Sprite* f)
{
    field = f;

    image = loadImage("img/jewelry.png");
    for (int i = 0; i < JEWELRY_MAX; i++)
    {
        Jewelry* e = &jewelries[i];
        e->body.isEnabled = false;
        spriteInit(&e->text, 0, 0, 0, 0, 0);
        e->text.isEnabled = false;
    }
    text = fontID;

    //particleの初期化
    Sprite particle;
    spriteInit(&particle, loadImage("img/circle16x16.png"), 0, 0, 16, 16);
    particle.blendmode = SDL_BLENDMODE_ADD;
    //trailの初期化
    trail = particleSettingDefault(&particle);
    trail.count = 1;
    trail.radius = 2;
    trail.duration = 0;
    particleSetScale(&trail, 0.2f, 0.5f, 0.1f, linear);
    particleSetAlpha(&trail, 127, 127, 0, easeOutCirc);
    particleSetLifeTime(&trail, 500, 1000);

    trackGet = Mix_LoadWAV("sound/GB-Action01-08(Item).mp3");
    Mix_VolumeChunk(trackGet, (int)(MIX_MAX_VOLUME * 0.75f));
    trackBonus = Mix_LoadWAV("sound/GB-General02-11(Pitch).mp3");
    Mix_VolumeChunk(trackBonus, (int)(MIX_MAX_VOLUME * 0.75f));
}

void jewelryUpdate(void)
{
    float fieldLine = field->src.h * field->scale / 2;

    for (int i = 0; i < JEWELRY_MAX; i++)
    {
        Jewelry* e = &jewelries[i];
        if (e->body.isEnabled) {
            float size = e->body.src.w * e->body.scale;

            if (!e->isGrounded) {
                e->body.v.y += e->body.accel.y;
                e->x += e->body.v.x;
                e->y += e->body.v.y;
                e->body.rotation += 0.2f;
                if (e->y < fieldLine + size / 2) {
                    e->y = fieldLine + size / 2;
                    e->body.v.x *= 0.7f;
                    e->body.v.y *= -0.7f;
                    if (abs((int)e->body.v.y) < 1) {
                        e->body.v.x = e->body.v.y = 0;
                        e->isGrounded = true;
                        alphaTo(&e->body, 0);
                        setDelay(2000 + randomFloat() * 1000);
                        setDuration(1000);
                        setOnFinished(disable);
                    }
                }
                e->trail.position = e->body.position;
                particleStart(&e->trail);
            }
            //実座標に反映
            e->body.position.x = field->position.x + cosf(e->x) * e->y;
            e->body.position.y = field->position.y + sinf(e->x) * e->y;

        }
    }
}

void jewelryDraw()
{
    for (int i = 0; i < JEWELRY_MAX; i++)
    {
        Jewelry* e = &jewelries[i];
        if (e->body.isEnabled) {
            spriteDraw(&e->body);
        }
    }

    for (int i = 0; i < JEWELRY_MAX; i++)
    {
        Jewelry* e = &jewelries[i];
        if (e->text.isEnabled) {
            DFA_DrawText(text,
                e->text.position.x,
                e->text.position.y, e->text.scale, e->text.rotation,
                e->text.color, NULL, "%d", e->point);
        }
    }
    DFA_Update(64);
}

void jewelryOnHitPlayer(Jewelry* e)
{
    stopAnimation(&e->body);
    e->body.isEnabled = false;
    e->text.isEnabled = true;
    
    e->text.position = e->body.position;
    moveAdd(&e->text, 0, -50);
    setDuration(1000);
    setEasing(easeOutQuint);
    setOnFinished(disable);

    e->text.color = e->body.color;
    e->text.color.a = 255;
    alphaTo(&e->text, 0);
    setDelay(500);
    setDuration(500);
    setEasing(easeOutCubic);
    if (e->type == JEWELRY_TYPE_GOLD) {
        e->text.scale = 1.0f;
    }

    //生まれたてのカケラはポイントアップ
    float bonus = vector2Magnitude(&e->body.v) * 20;
    //1の位は切り捨て
    bonus = floorf(bonus / 10) * 10;
    if (bonus > 0) {
        Mix_PlayChannel(5, trackBonus, 0);
        e->point += bonus;
        e->text.scale = 1.0f;
    }
    else {
        Mix_PlayChannel(6, trackGet, 0);
    }
    addScore(e->point);
}

void jewelryInitByType(Jewelry* e, SDL_Color color, JewelryType type)
{
    spriteInit(&e->body, image, 0, 0, 8, 8);
    e->body.color = color;
    e->body.radius = 4;
    e->body.accel.y = gravity;
    e->isGrounded = false;
    e->type = type;
    e->text.scale = 0.75f;
    //エフェクトの初期化
    e->trail = trail;
    e->trail.sprite.color = color;

    switch (type) {
    case JEWELRY_TYPE_BLUE:
        e->body.scale = 1;
        e->point = 100;
        break;
    case JEWELRY_TYPE_RED:
        e->body.scale = 2;
        e->point = 300;
        break;
    case JEWELRY_TYPE_GREEN:
        e->body.scale = 3;
        e->point = 500;
        break;
    case JEWELRY_TYPE_GOLD:
        e->body.scale = 2;
        e->point = 1000;
        break;
    }
    float s = e->body.src.h / e->trail.sprite.src.h * e->body.scale;
    particleSetScale(&e->trail, s, s, 0.0f, linear);
}
void jewelryGenerate(const Vector2* pos, SDL_Color color, JewelryType jewelryType)
{
    for (int i = 0; i < JEWELRY_MAX; i++)
    {
        Jewelry* e = &jewelries[i];
        if (!e->body.isEnabled && !e->text.isEnabled) {
            jewelryInitByType(e, color, jewelryType);
            e->x = atan2f(
                pos->y - field->position.y,
                pos->x - field->position.x
            );
            e->y = vector2Distance(pos, &field->position);
            e->body.position = *pos;
            e->body.v.x = randomFloat() * 0.008f - 0.004f;
            e->body.v.y = randomFloat() * 3 + 6;
            break;
        }
    }
}
