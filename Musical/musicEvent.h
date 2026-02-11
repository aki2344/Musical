
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "midi_smf.h"

#define EVQ_CAP 2048  // 十分（イベント頻度は低い）

typedef enum { EV_MIDI_NOTE } EvKind;

typedef struct {
    EvKind kind;
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

struct AppState;
// AppStateに追加
typedef void (*MidiTrackHandler)(struct AppState* st, uint8_t track, uint8_t note, uint8_t vel, bool on);

typedef struct AppState {
    SDL_AudioDeviceID dev;
    SDL_AudioSpec spec;      // 実デバイスフォーマット

    float* music;            // interleaved float32
    int64_t  musicFrames;         // frames (not samples)
    int64_t  musicPos;            // frame index
    bool musicLoop;
    bool paused;

    double bpm;
    double audioOffsetMs;    // WAV再生位置へ加算するオフセット(ms)
    int64_t audioOffsetFrames; // WAV再生位置へ加算するオフセット(frames)

    float musicGain;

    EventQueue evq;

    // MIDI track → function
    MidiTrackHandler midiTrackMap[128];
    bool midiTrackEnabled[128];

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
void musicEventSetPaused(bool paused);
bool musicEventIsPaused(void);
void musicEventTogglePaused(void);

bool musicEventRegisterMidiTrackHandler(uint8_t track, MidiTrackHandler handler);
void musicEventUnregisterMidiTrackHandler(uint8_t track);
void musicEventSetMidiTrackEnabled(uint8_t track, bool enabled);
