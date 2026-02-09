#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------- Public structs ----------------
typedef struct {
    int32_t tick;     // absolute tick from song start
    int64_t sample;   // absolute sample(frame) from song start
    uint8_t on;       // 1=NoteOn, 0=NoteOff
    uint8_t note;     // 0..127
    uint8_t vel;      // 0..127 (NoteOff is 0)
} MidiNoteEvent;

typedef struct {
    int32_t startTick;
    int64_t startUs;
    int32_t tempoUsPerQN; // microseconds per quarter note
    int64_t startSample;
} MidiTempoSeg;

typedef struct {
    int tpqn;
    int32_t lengthTicks;
    int64_t lengthSamples;

    MidiTempoSeg* seg; int segCount;

    MidiNoteEvent* ev; int evCount; // sample-sorted
} MidiSong;

typedef struct { const uint8_t* p; const uint8_t* end; } Cur;

// ---------------- vectors ----------------
typedef struct { MidiNoteEvent* a; int n, cap; } NoteVec;
typedef struct { int32_t tick, tempo; } TempoEv;
typedef struct { TempoEv* a; int n, cap; } TempoVec;

int64_t midi_tick_to_sample(const MidiSong* s, int32_t tick, int sampleRate);
int32_t midi_sample_to_tick(const MidiSong* s, int64_t sample, int sampleRate);
int lower_bound_note_by_sample(const MidiNoteEvent* ev, int n, int64_t s);
bool load_midi_build_events(const char* path, int sampleRate, MidiSong* outSong);
void free_midi_song(MidiSong* s);