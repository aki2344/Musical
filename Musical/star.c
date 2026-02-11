#include "star.h"
#include "sprite.h"
#include "animation.h"
#include "easing.h"
#include "main.h"
#include <stdlib.h>

static Sprite stars[STAR_MAX];

void starInit()
{
    int image = loadImage("img/circle16x16.png");
    for (int i = 0; i < STAR_MAX; i++)
    {
        Sprite* e = &stars[i];
        spriteInit(e, image, 0, 0, 16, 16);
        float scale = 0.05f + randomFloat() * 0.1f;
        e->scale = scale;
        e->color.a = 64 + randomFloat() * 192;
        scaleAdd(e, scale);
        setDuration(500 + rand() % 1000);
        setEasing(linear);
        setLoopType(PING_PONG);
        setLoop();
        e->position = (Vector2){
            //WINDOW_WIDTH * 0.04f / 2 + randomFloat() * WINDOW_WIDTH * 0.96f,
            //WINDOW_HEIGHT * 0.1f / 2 + randomFloat() * WINDOW_HEIGHT * 0.9f
            WINDOW_WIDTH * randomFloat(),
            WINDOW_HEIGHT * randomFloat()
        };
    }
}

void starDraw()
{
    for (int i = 0; i < STAR_MAX; i++)
    {
        spriteDraw(&stars[i]);
    }
}
