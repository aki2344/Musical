/**
* @file vector2.c
* @brief Vector2構造体処理の実装
*/
#include "vector2.h"
#include <math.h>

/**
* @brief ベクトルの線形補間
*
* @param v1 ベクトル1
* @param v2 ベクトル2
* @param fraction 補間の割合
* @return 補間したベクトル
*/
Vector2 vector2Lerp(const Vector2* v1, const Vector2* v2, float fraction) {
	Vector2 v = *v1;
	fraction = fraction > 1 ? 1 : (fraction < 0 ? 0 : fraction);
	v.x += (v2->x - v1->x) * fraction;
	v.y += (v2->y - v1->y) * fraction;
	return v;
}

/**
* @brief ベクトルの和を求める
*
* @param v1 ベクトル1
* @param v2 ベクトル2
* @return 和のベクトル
*/
Vector2 vector2Add(const Vector2* v1, const Vector2* v2) {
	Vector2 v = *v1;
	v.x += v2->x;
	v.y += v2->y;
	return v;
}

/**
* @brief ベクトルの差を求める
*
* @param v1 ベクトル1
* @param v2 ベクトル2
* @return 差（v1 - v2）のベクトル
*/
Vector2 vector2Subtract(const Vector2* v1, const Vector2* v2) {
	Vector2 v = *v1;

	v.x -= v2->x;
	v.y -= v2->y;
	return v;
}

/**
* @brief ベクトルのスカラー倍を求める
*
* @param v ベクトル1
* @param scalar スカラー
* @return 和のベクトル
*/
Vector2 vector2Multiply(const Vector2* v, float scalar) {
	Vector2 t = *v;
	t.x *= scalar;
	t.y *= scalar;
	return t;
}

/**
* @brief ベクトルの角度（ラジアン）を求める
*
* @param v ベクトル
* @return 角度（ラジアン）
*/
float vector2GetRotation(const Vector2* v) {
	if (v->y != v->x && v->x != 0.0)
		return atan2f(v->y, v->x);
	return 0.0;
}

/**
* @brief ベクトルの内積を求める
*
* @param v1 ベクトル1
* @param v2 ベクトル2
* @return 内積
*/
float vector2Dot(const Vector2* v1, const Vector2* v2) {
	return v1->x * v2->x + v1->y * v2->y;
}

/**
* @brief ベクトルの外積を求める
*
* @param v1 ベクトル1
* @param v2 ベクトル2
* @return 外積
*/
float vector2Closs(const Vector2* v1, const Vector2* v2) {
	return v1->x * v2->y - v1->y * v2->x;
}

/**
* @brief ベクトルの長さを求める
*
* @param v ベクトル
* @return ベクトルの長さ
*/
float vector2Magnitude(const Vector2* v) {
	return sqrtf(v->x * v->x + v->y * v->y);
}

/**
* @brief ベクトルの2乗の長さを求める
*
* @param v ベクトル
* @return ベクトルの2乗の長さ
*/
float vector2SqrMagnitude(const Vector2* v) {
	return v->x * v->x + v->y * v->y;
}

/**
* @brief 同じ向きで長さが1のベクトルを求める
*
* @param v ベクトル
* @return 同じ向きで長さが1のベクトル
*/
Vector2 vector2Normalized(const Vector2* v) {
	Vector2 t;
	float length = vector2Magnitude(v);
	t.x = v->x / length;
	t.y = v->y / length;
	return t;
}

/**
* @brief 2つのベクトルの距離を算出
*
* @param v1 ベクトル1
* @param v2 ベクトル2
* @return 2つのベクトルの距離
*/
float vector2Distance(const Vector2* v1, const Vector2* v2) {
	Vector2 v = *v1;
	v.x -= v2->x;
	v.y -= v2->y;
	return vector2Magnitude(&v);
}

