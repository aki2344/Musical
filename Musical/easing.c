/*
* @file easinfg.c
* @brief 値の変化関数を実装
*/
#define _USE_MATH_DEFINES
#include <math.h>

float cramp(float n) {
    if (n < 0) {
        return 0;
    }
    else if(n > 1){
        return 1;
    }
    return n;
}

float linear(float n) {
    n = cramp(n);
    return n;
}

float easeInQuad(float n) {
    n = cramp(n);
    return powf(n, 2);
}

float easeOutQuad(float n) {
    n = cramp(n);
    return -n * (n - 2);
}

float easeInOutQuad(float n) {
    n = cramp(n);
    if (n < 0.5) {
        return 2 * powf(n, 2);
    }
    else {
        n = n * 2 - 1;
    }
    return -0.5f * (n*(n - 2) - 1);
}

float easeInCubic(float n) {
    n = cramp(n);
    return powf(n, 3);
}
float easeOutCubic(float n) {
    n = cramp(n);
    n = n - 1;
    return powf(n, 3) + 1;
}

float easeInOutCubic(float n) {
    n = cramp(n);
    n = 2 * n;
    if (n < 1) {
        return 0.5f * powf(n, 3);
    }
    else {
        n = n - 2;
        return 0.5f * (powf(n, 3) + 2);
    }
}

float easeInQuart(float n) {
    n = cramp(n);
    return powf(n, 4);
}

float easeOutQuart(float n) {
    n = cramp(n);
    n = n - 1;
    return -(powf(n, 4) - 1);
}

float easeInOutQuart(float n) {
    n = cramp(n);
    n = 2 * n;
    if (n < 1) {
        return 0.5 * powf(n, 4);
    }
    else {
        n = n - 2;
        return -0.5f * (powf(n, 4) - 2);
    }
}

float easeInQuint(float n) {
    n = cramp(n);
    return powf(n, 5);
}

float easeOutQuint(float n) {
    n = cramp(n);
    n = n - 1;
    return powf(n, 5) + 1;
}

float easeInOutQuint(float n) {
    n = cramp(n);
    n = 2 * n;
    if (n < 1) {
        return 0.5f * powf(n, 5);
    }
    else {
        n = n - 2;
        return 0.5f * (powf(n, 5) + 2);
    }
}


float easeInSine(float n) {
    n = cramp(n);
    return -1 * cosf(n * M_PI / 2) + 1;
}

float easeOutSine(float n) {
    n = cramp(n);
    return sinf(n * M_PI / 2);
}

float easeInOutSine(float n) {
    n = cramp(n);
    return -0.5f * (cosf(M_PI * n) - 1);
}


float easeInExpo(float n) {
    n = cramp(n);
    if (n == 0) {
        return 0;
    }
    else {
        return powf(2, (10 * (n - 1)));
    }
}


float easeOutExpo(float n) {
    n = cramp(n);
    if (n == 1) {
        return 1;
    }
    else {
        return -powf(2, (-10 * n)) + 1;
    }
}


float easeInOutExpo(float n) {
    n = cramp(n);
    if (n == 0) {
        return 0;
    }
    else if (n == 1) {
        return 1;
    }
    else {
        n = n * 2;
        if (n < 1) {
            return 0.5f * powf(2, (10 * (n - 1)));
        }
        else {
            n -= 1;
            return 0.5f * (-1 * powf(2,(-10 * n)) + 2);
        }
    }
}


float easeInCirc(float n) {
    n = cramp(n);
    return -1 * (sqrtf(1 - n * n) - 1);
}


float easeOutCirc(float n) {
    n = cramp(n);
    n -= 1;
    return sqrtf(1 - (n * n));
}



float easeInOutCirc(float n) {
    n = cramp(n);
    n = n * 2;
    if (n < 1) {
        return -0.5f * (sqrtf(1 - powf(n, 2) - 1));
    }
    else {
        n = n - 2;
        return 0.5f * (sqrtf(1 - powf(n, 2)) + 1);
    }
}

float easeOutElastic(float n) {
    float amplitude = 1, period = 0.3f, s;

    n = cramp(n);

    if (amplitude < 1) {
        amplitude = 1;
        s = period / 4;
    }
    else {
        s = period / (2 * M_PI) * asinf(1 / amplitude);
    }
    return amplitude * powf(2, (-10 * n)) * sinf((n - s)*(2 * M_PI / period)) + 1;
}

float easeInElastic(float n) {
    n = cramp(n);
    return 1 - easeOutElastic(1 - n);
}

float easeInOutElastic(float n) {
    float amplitude = 1, period = 0.5f;

    n = cramp(n);

    n *= 2;
    if (n < 1) {
        return easeInElastic(n) / 2;
    }
    else {
        return easeOutElastic(n - 1) / 2 + 0.5f;
    }
}


float easeInBack(float n) {
    float s = 1.70158f;

    n = cramp(n);

    return n * n * ((s + 1) * n - s);
}

float easeOutBack(float n) {
    float s = 1.70158f;

    n = cramp(n);

    n = n - 1;
    return n * n * ((s + 1) * n + s) + 1;
}


float easeInOutBack(float n) {
    float s = 1.70158f;

    n = cramp(n);

    n = n * 2;
    if (n < 1) {
        s *= 1.525f;
        return 0.5f * (n * n * ((s + 1) * n - s));
    }
    else {
        n -= 2;
        s *= 1.525f;
    }
    return 0.5f * (n * n * ((s + 1) * n + s) + 2);
}


float easeOutBounce(float n) {

    n = cramp(n);
    if (n < (1 / 2.75f)) {
        return 7.5625f * n * n;
    }
    else if (n < (2 / 2.75f)) {
        n -= (1.5f / 2.75f);
        return 7.5625f * n * n + 0.75f;
    }
    else if (n < (2.5f / 2.75f)) {
        n -= (2.25f / 2.75f);
        return 7.5625f * n * n + 0.9375f;
    }
    else {
        n -= (2.65f / 2.75f);
        return 7.5625f * n * n + 0.984375f;
    }
}
float easeInBounce(float n) {

    n = cramp(n);

    return 1 - easeOutBounce(1 - n);
}

    


float easeInOutBounce(float n) {

    n = cramp(n);

    if (n < 0.5f) {
        return easeInBounce(n * 2) * 0.5f;
    }
    else {
        return easeOutBounce(n * 2 - 1) * 0.5f + 0.5f;
    }
}
