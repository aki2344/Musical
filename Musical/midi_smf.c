// ============================================================
// midi_smf_allinone.c  (paste-ready)
// SMF(Type1/TPQN) MIDI loader -> note events in sample order
// - Extract: NoteOn (vel=0 -> NoteOff), NoteOff
// - Tempo: Set Tempo meta event (0xFF 0x51)
// - Ignore: time signature, others
//
// Public API:
//   typedef struct MidiSong {...}
//   bool load_midi_build_events(const char* path, int sampleRate, MidiSong* outSong);
//   void free_midi_song(MidiSong* s);
//   int  lower_bound_note_by_sample(const MidiNoteEvent* ev, int n, int64_t s);
//   int64_t midi_tick_to_sample(const MidiSong* s, int32_t tick, int sampleRate);
//   int32_t midi_sample_to_tick(const MidiSong* s, int64_t sample, int sampleRate);
// ============================================================
#include "midi_smf.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------- helpers (byte reading) ----------------
static uint32_t rd_u32be(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | ((uint32_t)p[3]);
}
static uint16_t rd_u16be(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}


static bool need(Cur* c, size_t n) { return (size_t)(c->end - c->p) >= n; }
static bool read_u8(Cur* c, uint8_t* out) {
    if (!need(c, 1)) return false;
    *out = *c->p++;
    return true;
}
static bool skip_n(Cur* c, size_t n) {
    if (!need(c, n)) return false;
    c->p += n;
    return true;
}
static bool read_vlq(Cur* c, uint32_t* out) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t b;
        if (!read_u8(c, &b)) return false;
        v = (v << 7) | (b & 0x7F);
        if ((b & 0x80) == 0) {
            *out = v;
            return true;
        }
    }
    return false;
}


static bool push_note(NoteVec* v, int32_t tick, uint8_t on, uint8_t note, uint8_t vel) {
    if (v->n >= v->cap) {
        int nc = v->cap ? v->cap * 2 : 1024;
        void* p = realloc(v->a, (size_t)nc * sizeof(MidiNoteEvent));
        if (!p) return false;
        v->a = (MidiNoteEvent*)p;
        v->cap = nc;
    }
    v->a[v->n++] = (MidiNoteEvent){ tick, 0, on, note, vel };
    return true;
}
static bool push_tempo(TempoVec* v, int32_t tick, int32_t tempo) {
    if (v->n >= v->cap) {
        int nc = v->cap ? v->cap * 2 : 64;
        void* p = realloc(v->a, (size_t)nc * sizeof(TempoEv));
        if (!p) return false;
        v->a = (TempoEv*)p;
        v->cap = nc;
    }
    v->a[v->n++] = (TempoEv){ tick, tempo };
    return true;
}

static int cmp_note_by_tick(const void* a, const void* b) {
    const MidiNoteEvent* A = (const MidiNoteEvent*)a;
    const MidiNoteEvent* B = (const MidiNoteEvent*)b;
    if (A->tick < B->tick) return -1;
    if (A->tick > B->tick) return  1;
    // same tick: Off first to avoid "On then Off at same time"
    if (A->on != B->on) return (A->on ? 1 : -1);
    return (int)A->note - (int)B->note;
}
static int cmp_note_by_sample(const void* a, const void* b) {
    const MidiNoteEvent* A = (const MidiNoteEvent*)a;
    const MidiNoteEvent* B = (const MidiNoteEvent*)b;
    if (A->sample < B->sample) return -1;
    if (A->sample > B->sample) return  1;
    if (A->on != B->on) return (A->on ? 1 : -1);
    return (int)A->note - (int)B->note;
}
static int cmp_tempo_by_tick(const void* a, const void* b) {
    const TempoEv* A = (const TempoEv*)a;
    const TempoEv* B = (const TempoEv*)b;
    if (A->tick < B->tick) return -1;
    if (A->tick > B->tick) return  1;
    return 0;
}

// ---------------- track parser ----------------
static bool parse_track(Cur* c, NoteVec* notes, TempoVec* tempos, int32_t* outEndTick) {
    if (!need(c, 8)) return false;
    if (memcmp(c->p, "MTrk", 4) != 0) return false;
    uint32_t len = rd_u32be(c->p + 4);
    c->p += 8;
    if (!need(c, len)) return false;

    Cur t = { c->p, c->p + len };
    c->p += len;

    int32_t absTick = 0;
    uint8_t running = 0;

    while (t.p < t.end) {
        uint32_t delta;
        if (!read_vlq(&t, &delta)) return false;
        absTick += (int32_t)delta;

        uint8_t b;
        if (!read_u8(&t, &b)) return false;

        uint8_t status;
        if (b & 0x80) {
            status = b;
        }
        else {
            if (running == 0) return false;
            status = running;
            t.p--; // treat b as data byte
        }

        // Meta
        if (status == 0xFF) {
            uint8_t metaType; if (!read_u8(&t, &metaType)) return false;
            uint32_t mlen;    if (!read_vlq(&t, &mlen)) return false;
            if (!need(&t, mlen)) return false;

            if (metaType == 0x2F) {
                // End of Track
                (void)skip_n(&t, mlen);
                break;
            }
            if (metaType == 0x51 && mlen == 3) {
                int32_t tempo = ((int32_t)t.p[0] << 16) | ((int32_t)t.p[1] << 8) | (int32_t)t.p[2];
                if (tempo <= 0) tempo = 500000;
                if (!push_tempo(tempos, absTick, tempo)) return false;
            }
            if (!skip_n(&t, mlen)) return false;
            continue;
        }

        // SysEx
        if (status == 0xF0 || status == 0xF7) {
            uint32_t syxLen; if (!read_vlq(&t, &syxLen)) return false;
            if (!skip_n(&t, syxLen)) return false;
            continue;
        }

        uint8_t hi = status & 0xF0;
        if (hi >= 0x80 && hi <= 0xE0) running = status;

        // channel messages
        if (hi == 0x80 || hi == 0x90 || hi == 0xA0 || hi == 0xB0 || hi == 0xE0) {
            uint8_t d1, d2;
            if (!read_u8(&t, &d1)) return false;
            if (!read_u8(&t, &d2)) return false;

            if (hi == 0x90) {
                if (d2 == 0) {
                    if (!push_note(notes, absTick, 0, d1, 0)) return false;
                }
                else {
                    if (!push_note(notes, absTick, 1, d1, d2)) return false;
                }
            }
            else if (hi == 0x80) {
                if (!push_note(notes, absTick, 0, d1, 0)) return false;
            }
        }
        else if (hi == 0xC0 || hi == 0xD0) {
            uint8_t d1;
            if (!read_u8(&t, &d1)) return false;
            (void)d1;
        }
        else {
            // unhandled / invalid
            return false;
        }
    }

    if (outEndTick) *outEndTick = absTick;
    return true;
}

// ---------------- tempo segments + conversion ----------------
static bool build_tempo_segments(MidiSong* song, const TempoVec* tempos, int sampleRate) {
    (void)sampleRate;

    // sort tempo by tick
    int n = tempos->n;
    TempoEv* tmp = NULL;

    if (n > 0) {
        tmp = (TempoEv*)malloc((size_t)n * sizeof(TempoEv));
        if (!tmp) return false;
        memcpy(tmp, tempos->a, (size_t)n * sizeof(TempoEv));
        qsort(tmp, (size_t)n, sizeof(TempoEv), cmp_tempo_by_tick);
    }

    // uniq by tick (keep last at same tick)
    TempoEv* uniq = NULL; int un = 0;
    if (n > 0) {
        uniq = (TempoEv*)malloc((size_t)n * sizeof(TempoEv));
        if (!uniq) { free(tmp); return false; }
        int i = 0;
        while (i < n) {
            int j = i;
            TempoEv last = tmp[i];
            while (j + 1 < n && tmp[j + 1].tick == tmp[i].tick) {
                j++;
                last = tmp[j];
            }
            uniq[un++] = last;
            i = j + 1;
        }
    }
    free(tmp);

    // seg count = 1(default) + (uniq except tick0 maybe)
    int cap = 1 + un;
    song->seg = (MidiTempoSeg*)malloc((size_t)cap * sizeof(MidiTempoSeg));
    if (!song->seg) { free(uniq); return false; }

    int sc = 0;
    // default tempo at tick0
    song->seg[sc++] = (MidiTempoSeg){ 0, 0, 500000, 0 };

    int idx = 0;
    // if tempo at tick0 exists, override default
    if (un > 0 && uniq[0].tick == 0) {
        song->seg[0].tempoUsPerQN = uniq[0].tempo;
        idx = 1;
    }
    // others
    for (; idx < un; idx++) {
        song->seg[sc++] = (MidiTempoSeg){ uniq[idx].tick, 0, uniq[idx].tempo, 0 };
    }
    free(uniq);

    // accumulate startUs + startSample
    int64_t curUs = 0;
    for (int i = 0; i < sc; i++) {
        if (i == 0) {
            song->seg[i].startUs = 0;
        }
        else {
            int32_t dt = song->seg[i].startTick - song->seg[i - 1].startTick;
            int32_t tempo = song->seg[i - 1].tempoUsPerQN;
            double dus = (double)dt * (double)tempo / (double)song->tpqn;
            curUs += (int64_t)(dus + 0.5);
            song->seg[i].startUs = curUs;
        }
        // startSample will be computed in tick->sample or once sampleRate known; here keep consistent:
        song->seg[i].startSample = 0; // filled later per sampleRate if needed
    }

    song->segCount = sc;
    return true;
}

static void fill_seg_startSample(MidiSong* song, int sampleRate) {
    for (int i = 0; i < song->segCount; i++) {
        song->seg[i].startSample = (int64_t)((double)song->seg[i].startUs * (double)sampleRate / 1000000.0 + 0.5);
    }
}

static int seg_index_for_tick(const MidiSong* s, int32_t tick, int curIdx) {
    int i = curIdx;
    if (i < 0) i = 0;
    if (i >= s->segCount) i = s->segCount - 1;
    while (i + 1 < s->segCount && s->seg[i + 1].startTick <= tick) i++;
    return i;
}

int64_t midi_tick_to_sample(const MidiSong* s, int32_t tick, int sampleRate) {
    if (!s || s->segCount <= 0 || s->tpqn <= 0 || sampleRate <= 0) return 0;
    if (tick <= 0) return 0;

    // find segment by tick
    int idx = 0;
    idx = seg_index_for_tick(s, tick, idx);
    const MidiTempoSeg* g = &s->seg[idx];

    int32_t dt = tick - g->startTick;
    double dus = (double)dt * (double)g->tempoUsPerQN / (double)s->tpqn;
    int64_t us = g->startUs + (int64_t)(dus + 0.5);

    return (int64_t)((double)us * (double)sampleRate / 1000000.0 + 0.5);
}

static int seg_index_for_sample(const MidiSong* s, int64_t sample, int sampleRate) {
    if (sampleRate <= 0) return 0;
    // last seg with startSample <= sample
    int lo = 0, hi = s->segCount;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (s->seg[mid].startSample <= sample) lo = mid + 1; else hi = mid;
    }
    int idx = lo - 1;
    if (idx < 0) idx = 0;
    return idx;
}

int32_t midi_sample_to_tick(const MidiSong* s, int64_t sample, int sampleRate) {
    if (!s || s->segCount <= 0 || s->tpqn <= 0 || sampleRate <= 0) return 0;
    if (sample < 0) sample = 0;

    int idx = seg_index_for_sample(s, sample, sampleRate);
    const MidiTempoSeg* g = &s->seg[idx];

    int64_t us = (int64_t)((double)sample * 1000000.0 / (double)sampleRate + 0.5);
    int64_t deltaUs = us - g->startUs;
    if (deltaUs < 0) deltaUs = 0;

    double dtick = (double)deltaUs * (double)s->tpqn / (double)g->tempoUsPerQN;
    int32_t tick = g->startTick + (int32_t)(dtick + 0.5);
    if (tick < 0) tick = 0;
    if (tick > s->lengthTicks) tick = s->lengthTicks;
    return tick;
}

// ---------------- lower_bound by sample ----------------
int lower_bound_note_by_sample(const MidiNoteEvent* ev, int n, int64_t s) {
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (ev[mid].sample < s) lo = mid + 1; else hi = mid;
    }
    return lo;
}

// ---------------- load MIDI and build events ----------------
bool load_midi_build_events(const char* path, int sampleRate, MidiSong* outSong) {
    if (!outSong) return false;
    memset(outSong, 0, sizeof(*outSong));
    if (sampleRate <= 0) sampleRate = 48000;

    // read file
    FILE* fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "open fail: %s\n", path); return false; }
    fseek(fp, 0, SEEK_END);
    long fsz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsz <= 0) { fclose(fp); return false; }

    uint8_t* buf = (uint8_t*)malloc((size_t)fsz);
    if (!buf) { fclose(fp); return false; }
    if (fread(buf, 1, (size_t)fsz, fp) != (size_t)fsz) {
        free(buf); fclose(fp); return false;
    }
    fclose(fp);

    Cur c = { buf, buf + fsz };

    // header
    if (!need(&c, 14)) { free(buf); return false; }
    if (memcmp(c.p, "MThd", 4) != 0) { free(buf); return false; }
    uint32_t hlen = rd_u32be(c.p + 4);
    if (hlen < 6 || !need(&c, 8 + (size_t)hlen)) { free(buf); return false; }

    uint16_t fmt = rd_u16be(c.p + 8);
    uint16_t ntr = rd_u16be(c.p + 10);
    uint16_t div = rd_u16be(c.p + 12);

    c.p += 8 + hlen;

    if (fmt != 1) {
        fprintf(stderr, "Type1 only (fmt=%u)\n", (unsigned)fmt);
        free(buf); return false;
    }
    if (div & 0x8000) {
        fprintf(stderr, "SMPTE division not supported\n");
        free(buf); return false;
    }

    MidiSong song;
    memset(&song, 0, sizeof(song));
    song.tpqn = (int)(div & 0x7FFF);
    if (song.tpqn <= 0) { free(buf); return false; }

    NoteVec notes = { 0 };
    TempoVec tempos = { 0 };

    int32_t endTickMax = 0;
    for (int i = 0; i < (int)ntr; i++) {
        int32_t endTick = 0;
        if (!parse_track(&c, &notes, &tempos, &endTick)) {
            fprintf(stderr, "parse track fail: %d\n", i);
            free(notes.a); free(tempos.a); free(buf);
            return false;
        }
        if (endTick > endTickMax) endTickMax = endTick;
    }
    song.lengthTicks = endTickMax;

    if (!build_tempo_segments(&song, &tempos, sampleRate)) {
        free(notes.a); free(tempos.a); free(buf);
        return false;
    }
    fill_seg_startSample(&song, sampleRate);

    // convert tick->sample for note events
    if (notes.n > 0) {
        qsort(notes.a, (size_t)notes.n, sizeof(MidiNoteEvent), cmp_note_by_tick);

        int segIdx = 0;
        for (int i = 0; i < notes.n; i++) {
            segIdx = seg_index_for_tick(&song, notes.a[i].tick, segIdx);
            const MidiTempoSeg* g = &song.seg[segIdx];

            int32_t dt = notes.a[i].tick - g->startTick;
            double dus = (double)dt * (double)g->tempoUsPerQN / (double)song.tpqn;
            int64_t us = g->startUs + (int64_t)(dus + 0.5);

            notes.a[i].sample = (int64_t)((double)us * (double)sampleRate / 1000000.0 + 0.5);
        }

        qsort(notes.a, (size_t)notes.n, sizeof(MidiNoteEvent), cmp_note_by_sample);

        song.ev = notes.a;
        song.evCount = notes.n;
    }
    else {
        song.ev = NULL;
        song.evCount = 0;
    }

    // song length in samples (by last tick)
    song.lengthSamples = midi_tick_to_sample(&song, song.lengthTicks, sampleRate);
    if (song.lengthSamples <= 0) {
        int64_t mx = 0;
        for (int i = 0; i < song.evCount; i++) {
            if (song.ev[i].sample > mx) mx = song.ev[i].sample;
        }
        song.lengthSamples = mx;
    }

    free(tempos.a);
    free(buf);

    *outSong = song;
    return true;
}

void free_midi_song(MidiSong* s) {
    if (!s) return;
    free(s->seg);
    free(s->ev);
    memset(s, 0, sizeof(*s));
}
