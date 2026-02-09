#pragma once

#include "sprite.h"
#include "particle.h"
#include "jewelry.h"
#include "dynamic_font_atlas.h"

#define ENEMY_MAX	20


typedef struct Enemy {
	Sprite body;
	float scale;
	float speed;
	JewelryType jewelryType;
	ParticleSetting trail;
	ParticleSetting broken;
}Enemy;

typedef enum EnemyType {
	ENEMY_TYPE_NORMAL,
	ENEMY_TYPE_GREEN,
	ENEMY_TYPE_RED,
	ENEMY_TYPE_GOLD,
	ENEMY_TYPE_MAX
}EnemyType;

Enemy* getEnemies();
void enemyInit(DFA_FontID fontID);
void enemyUpdate(const Vector2* target);
void enemyDraw();
void enemyOnHitPlayer(Enemy* e, const Vector2* playerPosition);
void enemyOnHitField(Enemy* e);
void enemyGenerate(bool isRandom);
void enemyDeleteAll();
