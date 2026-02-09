/**
 * @file mainGame.h
 * @brief メインゲーム処理ヘッダ
 */
#pragma once
#include <stdbool.h>
#include <SDL2/SDL.h>

typedef enum {
    TICK_BEAT = 0,
    TICK_HALF,
    TICK_QUARTER,
    TICK_TRIPLET,
    TICK_OFFBEAT, 
    TICK_BACKBEAT_2_4,
    TICK_TRIPLET_OFF,
    TICK_MAX
} TickId;

typedef struct {
    bool enabled;
    double stepBeat;   // 1.0 / 0.5 / 0.25 / 1/3 ...
    double phaseBeat;  // 例: 0.0(表), 0.5(裏), 1/3 ...
    double nextBeat;   // 次に発火する拍位置（beatAbs基準）
} TickLane;


#define EVQ_CAP 256  // 十分（イベント頻度は低い）

typedef enum { EV_TICK, EV_FRAME } EvKind;

typedef struct {
    EvKind kind;
    int id;         // tickId or frameEventId
    int musicFrame; // デバッグ用
} AppEvent;
typedef struct {
    SDL_SpinLock lock;
    int w, r;
    AppEvent buf[EVQ_CAP];
} EventQueue;
typedef struct {
    int frame;       // 発火したい musicPos（オーディオフレーム）
    int id;          // どの関数を呼ぶか
    bool enabled;
    bool fired;      // 1回だけ発火したい場合
} FrameEvent;

#define FRAME_EV_MAX 128
typedef struct {
    FrameEvent ev[FRAME_EV_MAX];
    int count;
    int next;        // 次に見るイベント（frame昇順にしておく）
} FrameScheduler;

typedef struct {
    SDL_AudioDeviceID dev;
    SDL_AudioSpec spec;      // 実デバイスフォーマット

    float* music;            // interleaved float32
    int musicFrames;         // frames (not samples)
    int musicPos;            // frame index
    bool musicLoop;

    float* click;            // interleaved float32
    int clickFrames;
    int clickPos;            // frame index
    bool clickActive;
    bool metroEnable;

    double bpm;
    double samplesPerBeat;   // frames per beat (double)
    double offsetFrames;     // 累積表示用（位相オフセットの合計を保持）

    float musicGain;
    float clickGain;

    double beatAbs;      // 経過拍数（連続）
    double beatInc;      // 1フレームあたりの拍増分 = 1/samplesPerBeat
    TickLane lanes[TICK_MAX];

    EventQueue evq;
    FrameScheduler fsch;
} AppState;

void mainGame(void);