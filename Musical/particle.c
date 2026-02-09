#define _USE_MATH_DEFINES
#include "particle.h"
#include "animation.h"
#include "easing.h"
#include "main.h"
#include <math.h>

static Sprite particle[PARTICLE_MAX];

ParticleSetting particleSettingDefault(const Sprite* sprite)
{
	ParticleSetting s = { 0 };
	s.sprite = *sprite;
	s.position = (Vector2){0,0};
	s.duration = 0;
	s.count = 1;
	s.radius = 0;

	s.minRotation = 0;
	s.maxRotation = 0;
	s.rotation = 0;

	s.minScale = 1;
	s.maxScale = 1;
	s.scale = 1;

	s.minAlpha = 255;
	s.maxAlpha = 255;
	s.alpha = 255;

	s.minPosition = 0;
	s.maxPosition = 0;
	s.minLifeTime = 500;
	s.maxLifeTime = 500;
	s.easingPosition = linear;
	s.easingRotation = linear;
	s.easingScale = linear;
	s.easingAlpha = linear;
	return s;
}

void particleStart(const ParticleSetting* s)
{
	int count = 0;
	int interval = s->duration / s->count;
	for (int i = 0; i < PARTICLE_MAX; i++) {
		Sprite* p = &particle[i];
		if (p->isEnabled == false) {
			*p = s->sprite;
			p->position = s->position;
			float r = randomFloat() * M_PI * 2;
			float radius = lerp(0, s->radius, randomFloat());
			p->position.x += cosf(r) * radius;
			p->position.y += sinf(r) * radius;

			float rotation = lerp(s->minRotation, s->maxRotation, randomFloat());
			float speed = lerp(s->minPosition, s->maxPosition, randomFloat());
			float scale = lerp(s->minScale, s->maxScale, randomFloat());
			float alpha = lerp(s->minAlpha, s->maxAlpha, randomFloat());
			int lifeTime = (int)lerp((float)s->minLifeTime, (float)s->maxLifeTime, randomFloat());
			int delay = interval * count;
			moveAdd(p, cosf(rotation) * speed, sinf(rotation) * speed);
			setDuration(lifeTime);
			setEasing(s->easingPosition);
			setDelay(delay);

			p->rotation = rotation;
			rotateTo(p, s->rotation);
			setDuration(lifeTime);
			setEasing(s->easingRotation);
			setDelay(delay);

			p->scale = scale;
			scaleTo(p, s->scale);
			setDuration(lifeTime);
			setEasing(s->easingScale);
			setDelay(delay);

			p->color.a = (int)alpha;
			alphaTo(p, s->alpha);
			p->color.a = 0;
			setDuration(lifeTime);
			setEasing(s->easingAlpha);
			setDelay(delay);

			setOnFinished(disable);

			if (++count >= s->count) {
				break;
			}
		}
	}
}

void particleDraw(void)
{
	for (int i = 0; i < PARTICLE_MAX; i++) {
		spriteDraw(&particle[i]);
	}
}

void particleSetPosition(ParticleSetting* setting, float min, float max, float(*easing)(float))
{
	setting->minPosition = min;
	setting->maxPosition = max;
	if(easing != NULL)
		setting->easingPosition = easing;
}

void particleSetScale(ParticleSetting* setting, float min, float max, float to, float(*easing)(float))
{
	setting->minScale = min;
	setting->maxScale = max;
	setting->scale = to;
	if (easing != NULL)
		setting->easingScale = easing;
}

void particleSetRotation(ParticleSetting* setting, float min, float max, float to, float(*easing)(float))
{
	setting->minRotation = min;
	setting->maxRotation = max;
	setting->rotation = to;
	if (easing != NULL)
		setting->easingRotation = easing;
}

void particleSetAlpha(ParticleSetting* setting, float min, float max, float to, float(*easing)(float))
{
	setting->minAlpha = min;
	setting->maxAlpha = max;
	setting->alpha = to;
	if (easing != NULL)
		setting->easingAlpha = easing;
}

void particleSetLifeTime(ParticleSetting* setting, float min, float max)
{
	setting->minLifeTime = min;
	setting->maxLifeTime = max;
}
