#include "musicEvent.h"
#include <stdlib.h>

static AppState st;
static SDL_AudioSpec want;

static Uint32 lastTitle = 0;
static char info[256] = "";

static void dispatch_midi_note(AppState* st, const AppEvent* ev)
{
    if (ev->kind != EV_MIDI_NOTE) return;

    if (ev->track >= 128 || !st->midiTrackEnabled[ev->track]) return;

    uint8_t n = ev->note;

    if (ev->on) {
        int64_t last = st->lastFiredSample[n];
        if (last >= 0 && (ev->sample - last) < st->debounceSamples) {
            return;
        }
        st->lastFiredSample[n] = ev->sample;
    }

    MidiTrackHandler h = st->midiTrackMap[ev->track];
    if (h) h(st, ev->track, n, ev->vel, ev->on != 0);
}

static void evq_push(EventQueue* q, AppEvent e) {
    SDL_AtomicLock(&q->lock);
    int next = (q->w + 1) % EVQ_CAP;
    if (next != q->r) {
        q->buf[q->w] = e;
        q->w = next;
    }
    SDL_AtomicUnlock(&q->lock);
}

static bool evq_pop(EventQueue* q, AppEvent* out) {
    bool ok = false;
    SDL_AtomicLock(&q->lock);
    if (q->r != q->w) {
        *out = q->buf[q->r];
        q->r = (q->r + 1) % EVQ_CAP;
        ok = true;
    }
    SDL_AtomicUnlock(&q->lock);
    return ok;
}

static bool load_wav_as_f32(const char* path, const SDL_AudioSpec* target, float** outBuf, int* outFrames) {
    SDL_AudioSpec srcSpec;
    Uint8* srcBuf = NULL;
    Uint32 srcLen = 0;

    if (!SDL_LoadWAV(path, &srcSpec, &srcBuf, &srcLen)) {
        SDL_Log("SDL_LoadWAV failed: %s", SDL_GetError());
        return false;
    }

    SDL_AudioCVT cvt;
    if (SDL_BuildAudioCVT(&cvt,
        srcSpec.format, srcSpec.channels, srcSpec.freq,
        target->format, target->channels, target->freq) < 0) {
        SDL_Log("SDL_BuildAudioCVT failed: %s", SDL_GetError());
        SDL_FreeWAV(srcBuf);
        return false;
    }

    cvt.len = (int)srcLen;
    cvt.buf = (Uint8*)SDL_malloc(cvt.len * cvt.len_mult);
    if (!cvt.buf) {
        SDL_FreeWAV(srcBuf);
        return false;
    }
    SDL_memcpy(cvt.buf, srcBuf, srcLen);

    if (SDL_ConvertAudio(&cvt) < 0) {
        SDL_Log("SDL_ConvertAudio failed: %s", SDL_GetError());
        SDL_free(cvt.buf);
        SDL_FreeWAV(srcBuf);
        return false;
    }

    SDL_FreeWAV(srcBuf);

    int bytesPerFrame = (SDL_AUDIO_BITSIZE(target->format) / 8) * target->channels;
    int frames = cvt.len_cvt / bytesPerFrame;

    *outBuf = (float*)cvt.buf;
    *outFrames = frames;
    return true;
}

static void push_midi_range(AppState* st, int64_t endSampleExclusive)
{
    while (st->nextEvIndex < st->song.evCount &&
        st->song.ev[st->nextEvIndex].sample < endSampleExclusive)
    {
        MidiNoteEvent* e = &st->song.ev[st->nextEvIndex];
        AppEvent ae;
        ae.kind = EV_MIDI_NOTE;
        ae.on = e->on;
        ae.track = e->track;
        ae.note = e->note;
        ae.vel = e->vel;
        ae.sample = e->sample;
        evq_push(&st->evq, ae);
        st->nextEvIndex++;
    }
}

static int64_t music_pos_with_audio_offset(const AppState* st)
{
    int64_t pos = st->musicPos + st->audioOffsetFrames;
    if (st->musicLoop && st->musicFrames > 0) {
        pos %= st->musicFrames;
        if (pos < 0) pos += st->musicFrames;
    }
    return pos;
}

static int64_t music_pos_for_midi(const AppState* st)
{
    int64_t pos = st->musicPos;
    if (st->musicLoop && st->musicFrames > 0) {
        pos %= st->musicFrames;
        if (pos < 0) pos += st->musicFrames;
    }
    return pos;
}

static void audio_cb(void* userdata, Uint8* stream, int len)
{
    AppState* st = (AppState*)userdata;

    float* out = (float*)stream;
    int ch = st->spec.channels;
    int frames = len / (int)(sizeof(float) * ch);

    SDL_memset(out, 0, len);

    if (st->paused) {
        return;
    }

    {
        int64_t startS = music_pos_for_midi(st);
        int64_t endS = startS + (int64_t)frames;
        int64_t L = (int64_t)st->musicFrames;

        if (st->music && st->musicFrames > 0 && st->song.evCount > 0) {
            if (!st->musicLoop || L <= 0 || endS < L) {
                push_midi_range(st, endS);
            }
            else {
                push_midi_range(st, L);
                st->nextEvIndex = 0;

                int64_t wrapEnd = endS - L;
                if (wrapEnd < 0) wrapEnd = 0;
                push_midi_range(st, wrapEnd);
            }
        }
    }

    for (int i = 0; i < frames; i++) {
        bool musicOk = (st->music && st->musicFrames > 0);
        int64_t musicCurPos = music_pos_with_audio_offset(st);
        if (musicOk && st->musicPos >= st->musicFrames) {
            if (st->musicLoop) {
                st->musicPos = 0;
                st->nextEvIndex = 0;
                for (int j = 0; j < 128; j++) st->lastFiredSample[j] = -1;
                musicCurPos = music_pos_with_audio_offset(st);
            }
            else {
                musicOk = false;
            }
        }

        for (int c = 0; c < ch; c++) {
            float v = 0.0f;
            if (musicOk && musicCurPos < st->musicFrames) {
                v += st->music[musicCurPos * ch + c] * st->musicGain;
            }
            if (v > 1.0f) v = 1.0f;
            if (v < -1.0f) v = -1.0f;
            out[i * ch + c] = v;
        }

        if (musicOk && st->musicPos < st->musicFrames) {
            st->musicPos++;
        }
    }
}

bool musicEventInit(const char* musicPath, const char* midiPath) {
    SDL_zero(st);
    st.musicLoop = true;
    st.musicGain = 0.8f;
    st.paused = false;
    st.bpm = 188;
    st.audioOffsetMs = 0.0;
    st.audioOffsetFrames = 0;

    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 256;
    want.callback = audio_cb;
    want.userdata = &st;

    st.dev = SDL_OpenAudioDevice(NULL, 0, &want, &st.spec, 0);
    if (!st.dev) {
        SDL_Log("SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return false;
    }

    if (!load_wav_as_f32(musicPath, &st.spec, &st.music, &st.musicFrames)) {
        SDL_Log("Failed to load music wav: %s", musicPath);
        SDL_CloseAudioDevice(st.dev);
        return false;
    }

    if (!load_midi_build_events(midiPath, st.spec.freq, &st.song)) {
        SDL_Log("MIDI load failed");
    }
    else {
        st.nextEvIndex = lower_bound_note_by_sample(st.song.ev, st.song.evCount, (int64_t)st.musicPos);
    }

    SDL_zero(st.evq);

    st.audioOffsetMs = 840;
    st.audioOffsetFrames = (int64_t)llround(st.audioOffsetMs * (double)st.spec.freq / 1000.0);

    SDL_PauseAudioDevice(st.dev, 0);

    for (int i = 0; i < 128; i++) {
        st.midiTrackMap[i] = NULL;
        st.midiTrackEnabled[i] = true;
        st.lastFiredSample[i] = -1;
    }

    st.debounceSamples = (int64_t)((double)st.spec.freq * 30.0 / 1000.0 + 0.5);

    return true;
}

void musicEventUpdate() {
    SDL_Keymod mod = SDL_GetModState();
    double msStep = 1.0;
    if (mod & KMOD_ALT) msStep = 1000.0;
    if (mod & KMOD_LSHIFT) msStep = 100.0;
    if (mod & KMOD_SHIFT) msStep = 10.0;
    if (mod & KMOD_CTRL)  msStep = 0.1;

    SDL_LockAudioDevice(st.dev);
    bool left =
        getKeyDown(SDL_SCANCODE_RIGHT) ||
        getGamepadButtonDown(0, SDL_CONTROLLER_BUTTON_DPAD_UP);
    bool right =
        getKeyDown(SDL_SCANCODE_LEFT) ||
        getGamepadButtonDown(0, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    bool start =
        getKeyDown(SDL_SCANCODE_R) ||
        getGamepadButtonDown(0, SDL_CONTROLLER_BUTTON_BACK);
    if(left){
        st.audioOffsetMs -= msStep;
        st.audioOffsetFrames = (int64_t)llround(st.audioOffsetMs * (double)st.spec.freq / 1000.0);
        SDL_Log("[musicEvent] audioOffsetMs=%+0.2f (frames=%lld)", st.audioOffsetMs, (long long)st.audioOffsetFrames);
    }
    else if (right) {
        st.audioOffsetMs += msStep;
        st.audioOffsetFrames = (int64_t)llround(st.audioOffsetMs * (double)st.spec.freq / 1000.0);
        SDL_Log("[musicEvent] audioOffsetMs=%+0.2f (frames=%lld)", st.audioOffsetMs, (long long)st.audioOffsetFrames);
    }
    else if (start) {
        st.musicPos = 0;
        st.nextEvIndex = lower_bound_note_by_sample(st.song.ev, st.song.evCount, (int64_t)st.musicPos);
        for (int i = 0; i < 128; i++) st.lastFiredSample[i] = -1;
        st.evq.r = 0;
        st.evq.w = 0;
    }
    double audioOffsetMs = st.audioOffsetMs;
    int64_t frame = music_pos_with_audio_offset(&st);
    double sec = (double)frame / (double)st.spec.freq;
    SDL_UnlockAudioDevice(st.dev);

    SDL_snprintf(info, sizeof(info),
        "AudioOffset: %+0.2f ms | Sec: %f | Frame: %lld | Music: %s (SPACE/START) | Restart: SELECT\n",
        audioOffsetMs, sec, (long long)frame, st.paused ? "Paused" : "Playing");

    AppEvent ev;
    while (evq_pop(&st.evq, &ev)) {
        if (ev.kind == EV_MIDI_NOTE) {
            dispatch_midi_note(&st, &ev);
        }
    }
}

void musicEventSetPaused(bool paused) {
    if (!st.dev) return;
    SDL_LockAudioDevice(st.dev);
    st.paused = paused;
    SDL_UnlockAudioDevice(st.dev);
}

bool musicEventIsPaused(void) {
    bool paused = false;
    if (!st.dev) return false;
    SDL_LockAudioDevice(st.dev);
    paused = st.paused;
    SDL_UnlockAudioDevice(st.dev);
    return paused;
}

void musicEventTogglePaused(void) {
    if (!st.dev) return;
    SDL_LockAudioDevice(st.dev);
    st.paused = !st.paused;
    SDL_UnlockAudioDevice(st.dev);
}

bool musicEventRegisterMidiTrackHandler(uint8_t track, MidiTrackHandler handler) {
    if (track >= 128) return false;
    if (st.song.trackCount > 0 && track >= (uint8_t)st.song.trackCount) return false;
    if (st.dev) SDL_LockAudioDevice(st.dev);
    st.midiTrackMap[track] = handler;
    if (st.dev) SDL_UnlockAudioDevice(st.dev);
    return true;
}

void musicEventUnregisterMidiTrackHandler(uint8_t track) {
    if (track >= 128) return;
    if (st.dev) SDL_LockAudioDevice(st.dev);
    st.midiTrackMap[track] = NULL;
    if (st.dev) SDL_UnlockAudioDevice(st.dev);
}

void musicEventSetMidiTrackEnabled(uint8_t track, bool enabled) {
    if (track >= 128) return;
    if (st.dev) SDL_LockAudioDevice(st.dev);
    st.midiTrackEnabled[track] = enabled;
    if (st.dev) SDL_UnlockAudioDevice(st.dev);
}

char* getInfo() {
    return info;
}

void musicEventQuit() {
    free_midi_song(&st.song);
}
