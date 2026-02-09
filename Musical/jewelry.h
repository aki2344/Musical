#pragma once

#include "sprite.h"
#include "particle.h"
#include "dynamic_font_atlas.h"

#define JEWELRY_MAX	100


typedef enum JewelryType {
	JEWELRY_TYPE_BLUE,
	JEWELRY_TYPE_RED,
	JEWELRY_TYPE_GREEN,
	JEWELRY_TYPE_GOLD,
	JEWELRY_TYPE_MAX
} JewelryType;

typedef struct Jewelry {
	Sprite body;
	Sprite text;
	float scale;
	float x;
	float y;
	int point;
	bool isGrounded;
	ParticleSetting trail;
	ParticleSetting broken;
	JewelryType type;
} Jewelry;

Jewelry* getJewelries();
void jewelryInit(DFA_FontID fontID, Sprite* f);
void jewelryUpdate(void);
void jewelryDraw();
void jewelryOnHitPlayer(Jewelry* j);
void jewelryInitByType(Jewelry* e, SDL_Color color, JewelryType type);
void jewelryGenerate(const Vector2* pos, SDL_Color color, JewelryType jewelryType);
