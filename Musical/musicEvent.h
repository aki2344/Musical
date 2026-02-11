
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "midi_smf.h"

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


#define EVQ_CAP 2048  // 十分（イベント頻度は低い）

typedef enum { EV_TICK, EV_FRAME, EV_MIDI_NOTE } EvKind;

typedef struct {
    EvKind kind;
    int id;         // tickId or frameEventId
    int musicFrame; // デバッグ用    
    // ★MIDI用
    uint8_t track;  // MIDI track index
    uint8_t on;     // 1=NoteOn, 0=NoteOff
    uint8_t note;   // 0..127
    uint8_t vel;    // 0..127
    int64_t sample; // （任意）イベントのsample位置
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

struct AppState;
// AppStateに追加
typedef void (*MidiTrackHandler)(struct AppState* st, uint8_t track, uint8_t note, uint8_t vel, bool on);
typedef void (*TickHandler)(struct AppState* st);
typedef void (*FrameHandler)(struct AppState* st, int musicFrame);

typedef struct AppState {
    SDL_AudioDeviceID dev;
    SDL_AudioSpec spec;      // 実デバイスフォーマット

    float* music;            // interleaved float32
    int64_t  musicFrames;         // frames (not samples)
    int64_t  musicPos;            // frame index
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

    // MIDI track → function
    MidiTrackHandler midiTrackMap[128];
    bool midiTrackEnabled[128];

    // Tick/frame handlers
    TickHandler tickHandlers[TICK_MAX];
    FrameHandler frameHandlers[FRAME_EV_MAX];

    // 連打抑制（メインスレッド側で使う想定でもOKだが、ここに置くと楽）
    int64_t debounceSamples;
    int64_t lastFiredSample[128];

    MidiSong song;
    int nextEvIndex;
} AppState;


bool musicEventInit(const char* musicPath, const char* midiPath);
void musicEventUpdate();
char* getInfo();
void musicEventQuit();

bool musicEventRegisterTickHandler(TickId id, TickHandler handler);
void musicEventUnregisterTickHandler(TickId id);
void musicEventSetTickEnabled(TickId id, bool enabled);

bool musicEventRegisterFrameHandler(int id, FrameHandler handler);
void musicEventUnregisterFrameHandler(int id);
void musicEventSetFrameEventEnabled(int id, bool enabled);
bool musicEventAddFrameEvent(int id, int frame, bool enabled);
bool musicEventRemoveFrameEvent(int id);

bool musicEventRegisterMidiTrackHandler(uint8_t track, MidiTrackHandler handler);
void musicEventUnregisterMidiTrackHandler(uint8_t track);
void musicEventSetMidiTrackEnabled(uint8_t track, bool enabled);
