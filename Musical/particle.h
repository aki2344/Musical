#pragma once
#include "sprite.h"

#define PARTICLE_MAX	2000

typedef struct ParticleSetting {
	Sprite sprite;
	Vector2 position;
	int duration;
	int count;
	float radius;

	float minRotation;
	float maxRotation;
	float rotation;

	float minScale;
	float maxScale;
	float scale;

	float minAlpha;
	float maxAlpha;
	float alpha;

	float minPosition;
	float maxPosition;
	int minLifeTime;
	int maxLifeTime;
	float (*easingPosition)(float);     ///< イージング関数
	float (*easingRotation)(float);     ///< イージング関数
	float (*easingScale)(float);     ///< イージング関数
	float (*easingAlpha)(float);     ///< イージング関数
}ParticleSetting;

ParticleSetting particleSettingDefault(const Sprite* sprite);
void particleStart(const ParticleSetting* setting);
void particleDraw(void);
void particleSetPosition(ParticleSetting* setting, float min, float max, float(*easing)(float));
void particleSetScale(ParticleSetting* setting, float min, float max, float to, float(*easing)(float));
void particleSetRotation(ParticleSetting* setting, float min, float max, float to, float(*easing)(float));
void particleSetAlpha(ParticleSetting* setting, float min, float max, float to, float(*easing)(float));
void particleSetLifeTime(ParticleSetting* setting, float min, float max);