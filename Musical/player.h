#pragma once

#include "sprite.h"
#include "dynamic_font_atlas.h"

typedef struct Enemy Enemy;

Sprite* getPlayer();
void playerInit(DFA_FontID fontID, Sprite* field);
void playerUpdate();
void playerDraw();
void playerOnHitEnemy(Enemy* e);
void playerLevelUp();
bool isPlayerDead();
void playerHeartDraw();
