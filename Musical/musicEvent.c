#include "musicEvent.h"
#include <stdlib.h>

static AppState st;
static SDL_AudioSpec want;
static double sec;

static Uint32 lastTitle = 0;
static char info[256] = "";

static void on_tick_beat(AppState* st);
static void on_tick_half(AppState* st);
static void on_tick_quarter(AppState* st);
static void on_tick_triplet(AppState* st);
static void on_frame_event_0(AppState* st, int musicFrame);
static void on_frame_event_1(AppState* st, int musicFrame);

static void dispatch_midi_note(AppState* st, const AppEvent* ev)
{
    if (ev->kind != EV_MIDI_NOTE) return;

    if (ev->track >= 128 || !st->midiTrackEnabled[ev->track]) return;

    uint8_t n = ev->note;

    if (ev->on) {
        // 連打抑制（同ノートの短時間連打を弾く）
        int64_t last = st->lastFiredSample[n];
        if (last >= 0 && (ev->sample - last) < st->debounceSamples) {
            return;
        }
        st->lastFiredSample[n] = ev->sample;
    }

    MidiTrackHandler h = st->midiTrackMap[ev->track];
    if (h) h(st, ev->track, n, ev->vel, ev->on != 0);
}

static double wrap_phase(double phase, double step) {
    if (step <= 0) return 0;
    phase = fmod(phase, step);
    if (phase < 0) phase += step;
    return phase;
}


// xの小数部を [0,1) に（負もOK）
static double frac01(double x) {
    double f = x - floor(x);
    if (f < 0.0) f += 1.0;
    return f;
}

// lanes の nextBeat を「今のbeatAbs基準で次の境界」に合わせる
// TickLaneに phaseBeat がある前提（無いなら phase=0 でOK）
static void rebase_lanes(AppState* st) {
    for (int i = 0; i < TICK_MAX; i++) {
        TickLane* ln = &st->lanes[i];
        double step = ln->stepBeat;
        if (step <= 0.0) continue;

        double phase = 0.0;
        // ↓TickLaneに phaseBeat を入れている場合
        phase = wrap_phase(ln->phaseBeat, step);

        // beatAbs を (phase基準格子) に投影して「次」を取る
        double t = (st->beatAbs - phase) / step;
        double k = floor(t);
        ln->nextBeat = (k + 1.0) * step + phase;
    }
}
// beatAbs から「現在のオフセット（ms表示用）」を更新したい場合に使う
// ※offsetFramesは“現在の拍内位置”の表示用として揃える
static void sync_offset_from_beatAbs(AppState* st) {
    double beatFrac = frac01(st->beatAbs);                  // 0..1
    st->offsetFrames = beatFrac * st->samplesPerBeat;       // 0..samplesPerBeat
}

static void evq_push(EventQueue* q, AppEvent e) {
    SDL_AtomicLock(&q->lock);
    int next = (q->w + 1) % EVQ_CAP;
    if (next != q->r) {          // fullなら捨てる（まず溢れない）
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

static double clampd(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void lock_audio_if_ready(void) {
    if (st.dev) SDL_LockAudioDevice(st.dev);
}

static void unlock_audio_if_ready(void) {
    if (st.dev) SDL_UnlockAudioDevice(st.dev);
}

static int frame_event_index_by_id(const FrameScheduler* fs, int id) {
    for (int i = 0; i < fs->count; i++) {
        if (fs->ev[i].id == id) return i;
    }
    return -1;
}

static int cmp_frame_event_by_frame(const void* a, const void* b) {
    const FrameEvent* A = (const FrameEvent*)a;
    const FrameEvent* B = (const FrameEvent*)b;
    if (A->frame < B->frame) return -1;
    if (A->frame > B->frame) return 1;
    return A->id - B->id;
}

static void set_bpm_locked(AppState* st, double newBpm)
{
    newBpm = clampd(newBpm, 20.0, 300.0);

    // いまの位相（拍内の位置）を保持
    double beatInt = floor(st->beatAbs);
    double beatFrac = frac01(st->beatAbs);

    st->bpm = newBpm;
    st->samplesPerBeat = (double)st->spec.freq * 60.0 / st->bpm;
    st->beatInc = 1.0 / st->samplesPerBeat;

    // 位相維持
    st->beatAbs = beatInt + beatFrac;

    // 表示用オフセット更新
    sync_offset_from_beatAbs(st);

    // レーン境界を作り直す
    rebase_lanes(st);
}

// ===============================
// 位相シフト（frames単位）（SDL_LockAudioDevice中に呼ぶ）
//  - deltaFrames >0 で「遅らせる」(次のクリックが遅くなる方向)
// ===============================
static void shift_phase_frames_locked(AppState* st, double deltaFrames)
{
    if (st->samplesPerBeat <= 0.0) return;

    double deltaBeat = deltaFrames / st->samplesPerBeat; // frames→beats
    st->beatAbs += deltaBeat;

    sync_offset_from_beatAbs(st);
    rebase_lanes(st);
}

// msでシフトしたい場合の薄いラッパ
static void shift_phase_ms_locked(AppState* st, double deltaMs)
{
    double deltaFrames = deltaMs * (double)st->spec.freq / 1000.0;
    shift_phase_frames_locked(st, deltaFrames);
}
// ===============================
// Rリセット（SDL_LockAudioDevice中に呼ぶ）
//  - musicPos, clickを確実に停止
//  - beatAbs を (初期オフセットms) へ
//  - lanes/キュー/フレームイベントをリセット
// ===============================
static void reset_transport_locked(AppState* st, double resetOffsetMs)
{
    st->musicPos = 0;

    // クリック停止（確実）
    st->clickActive = false;
    st->clickPos = 0;

    // beatAbs を「指定msオフセット」へ
    double resetOffsetFrames = resetOffsetMs * (double)st->spec.freq / 1000.0;
    st->beatAbs = (st->samplesPerBeat > 0.0) ? (resetOffsetFrames / st->samplesPerBeat) : 0.0;
    st->beatInc = (st->samplesPerBeat > 0.0) ? (1.0 / st->samplesPerBeat) : 0.0;

    sync_offset_from_beatAbs(st);

    // キューを空に（audio_cb停止中なので lock不要）
    st->evq.r = 0;
    st->evq.w = 0;

    // レーン境界を作り直す
    rebase_lanes(st);

    // 指定フレームイベントも先頭へ（必要なら）
    st->fsch.next = 0;
    for (int i = 0; i < st->fsch.count; i++) st->fsch.ev[i].fired = false;

    // SDL_LockAudioDevice 下
    st->musicPos = 0;
    st->nextEvIndex = lower_bound_note_by_sample(st->song.ev, st->song.evCount, (int64_t)st->musicPos);
    for (int i = 0; i < 128; i++) st->lastFiredSample[i] = -1;

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

    // cvt.buf は target->format に変換済み
    int bytesPerFrame = (SDL_AUDIO_BITSIZE(target->format) / 8) * target->channels;
    int frames = cvt.len_cvt / bytesPerFrame;

    *outBuf = (float*)cvt.buf;
    *outFrames = frames;
    return true;
}

// クリック音を合成（ファイル不要）：短いサイン＋指数減衰
static bool make_click_f32(const SDL_AudioSpec* spec, float** outBuf, int* outFrames) {
    int sr = spec->freq;
    int ch = spec->channels;

    double dur = 0.03; // 30ms
    int frames = (int)(sr * dur);
    if (frames < 1) frames = 1;

    float* buf = (float*)SDL_malloc(sizeof(float) * frames * ch);
    if (!buf) return false;

    double freq = 1200.0;
    double decay = 0.008; // 小さいほど鋭い
    for (int i = 0; i < frames; i++) {
        double t = (double)i / (double)sr;
        double env = exp(-t / decay);
        double s = sin(2.0 * M_PI * freq * t) * env;

        for (int c = 0; c < ch; c++) {
            buf[i * ch + c] = (float)s;
        }
    }

    *outBuf = buf;
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

// MIDI同期は実オーディオの進行位置（musicPos）を基準にする。
// audioOffsetは「WAVをどこから鳴らすか」のみを動かし、MIDI位置には反映しない。
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

    // ---- MIDI: このcallback区間で到達するイベントをEVQへpush（catch-up）----
    {
        // WAVの再生フレームが時間基準
        int64_t startS = music_pos_for_midi(st);
        int64_t endS = startS + (int64_t)frames;
        int64_t L = (int64_t)st->musicFrames;

        // MIDIがロードされていて、WAVが存在するときだけ同期させる
        if (st->music && st->musicFrames > 0 && st->song.evCount > 0) {
            if (!st->musicLoop || L <= 0 || endS < L) {
                push_midi_range(st, endS);
            }
            else {
                // 末尾跨ぎ（loop）: [start..L) と [0..wrapEnd)
                push_midi_range(st, L);

                // ループした扱いになるので、MIDIの次indexも先頭へ戻す
                st->nextEvIndex = 0;

                int64_t wrapEnd = endS - L;
                if (wrapEnd < 0) wrapEnd = 0;
                push_midi_range(st, wrapEnd);
            }
        }
    }


    for (int i = 0; i < frames; i++) {

        // -------- 音楽の終端処理（先にやると分かりやすい）
        bool musicOk = (st->music && st->musicFrames > 0);
        int64_t musicCurPos = music_pos_with_audio_offset(st);
        if (musicOk && st->musicPos >= st->musicFrames) {
            if (st->musicLoop) {
                st->musicPos = 0;

                // フレームイベントを次ループ用にリセットするなら
                st->fsch.next = 0;
                for (int j = 0; j < st->fsch.count; j++) st->fsch.ev[j].fired = false;

                st->nextEvIndex = 0;
                for (int i = 0; i < 128; i++) st->lastFiredSample[i] = -1;

                musicCurPos = music_pos_with_audio_offset(st);
            }
            else {
                musicOk = false;
            }
        }

        // -------- 拍（beatAbs）を進める（1フレーム=1オーディオフレーム）
        st->beatAbs += st->beatInc;

        // -------- レーン境界検出 → イベント push & クリック発火
        for (int li = 0; li < TICK_MAX; li++) {
            TickLane* ln = &st->lanes[li];
            if (!ln->enabled) continue;

            while (st->beatAbs >= ln->nextBeat) {
                evq_push(&st->evq, (AppEvent) { .kind = EV_TICK, .id = (TickId)li });

                // クリックを鳴らすのは「拍」レーンだけ（ここが唯一の発火点）
                if (li == TICK_BEAT && st->metroEnable) {
                    st->clickActive = true;
                    st->clickPos = 0;
                }

                ln->nextBeat += ln->stepBeat;
            }
        }

        // -------- 指定フレームイベント（★musicPosを出す“直前”に判定）
        if (musicOk && musicCurPos < st->musicFrames) {
            int cur = (int)musicCurPos; // 今まさに出すフレーム(オフセット適用後)

            FrameScheduler* fs = &st->fsch;
            while (fs->next < fs->count) {
                FrameEvent* fe = &fs->ev[fs->next];

                if (!fe->enabled || fe->fired) { fs->next++; continue; }

                if (cur >= fe->frame) {
                    evq_push(&st->evq, (AppEvent) { .kind = EV_FRAME, .id = fe->id, .musicFrame = cur });
                    fe->fired = true;
                    fs->next++;
                    continue;
                }
                break;
            }
        }

        // -------- ミックス
        for (int c = 0; c < ch; c++) {
            float v = 0.0f;

            if (musicOk && musicCurPos < st->musicFrames) {
                v += st->music[musicCurPos * ch + c] * st->musicGain;
            }

            if (st->metroEnable && st->clickActive && st->click && st->clickPos < st->clickFrames) {
                v += st->click[st->clickPos * ch + c] * st->clickGain;
            }

            // （fx等があるならここで加算）

            if (v > 1.0f) v = 1.0f;
            if (v < -1.0f) v = -1.0f;

            out[i * ch + c] = v;
        }

        // -------- 位置更新
        if (musicOk && st->musicPos < st->musicFrames) {
            st->musicPos++;
        }

        if (st->metroEnable && st->clickActive) {
            st->clickPos++;
            if (st->clickPos >= st->clickFrames) st->clickActive = false;
        }
    }
}

bool musicEventInit(const char* musicPath, const char* midiPath) {


    SDL_zero(st);
    st.musicLoop = true;
    st.metroEnable = true;
    st.musicGain = 0.8f;
    st.clickGain = 0.9f;
    st.bpm = 188;
    st.audioOffsetMs = 0.0;
    st.audioOffsetFrames = 0;

    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 256; // 小さいほど操作が追従しやすい(ただし負荷は増える)
    want.callback = audio_cb;
    want.userdata = &st;

    st.dev = SDL_OpenAudioDevice(NULL, 0, &want, &st.spec, 0);
    if (!st.dev) {
        SDL_Log("SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return false;
    }

    // デバイスフォーマットに合わせて読み込み
    if (!load_wav_as_f32(musicPath, &st.spec, &st.music, &st.musicFrames)) {
        SDL_Log("Failed to load music wav: %s", musicPath);
        SDL_CloseAudioDevice(st.dev);
        return false;
    }

    if (!make_click_f32(&st.spec, &st.click, &st.clickFrames)) {
        SDL_Log("Failed to make click");
        SDL_free(st.music);
        SDL_CloseAudioDevice(st.dev);
        return false;
    }

    // SDL_OpenAudioDevice後（st.spec.freqが確定してから）
    if (!load_midi_build_events(midiPath, st.spec.freq, &st.song)) {
        SDL_Log("MIDI load failed");
    }
    else {
        st.nextEvIndex = lower_bound_note_by_sample(st.song.ev, st.song.evCount, (int64_t)st.musicPos);

    }

    // 初期BPM反映
    st.samplesPerBeat = (double)st.spec.freq * 60.0 / st.bpm;
    st.offsetFrames = 0 / 1000.0 * (double)st.spec.freq;
     
    st.beatAbs = st.offsetFrames / st.samplesPerBeat;

    // 1フレームあたりの拍増分
    st.beatInc = 1.0 / st.samplesPerBeat;

    // レーン定義
    st.lanes[TICK_BEAT] = (TickLane){ .enabled = true,  .stepBeat = 1.0 };
    st.lanes[TICK_HALF] = (TickLane){ .enabled = false, .stepBeat = 0.5 };
    st.lanes[TICK_QUARTER] = (TickLane){ .enabled = false, .stepBeat = 0.25 };
    st.lanes[TICK_TRIPLET] = (TickLane){ .enabled = false, .stepBeat = 1.0 / 3.0 };
    st.lanes[TICK_OFFBEAT] = (TickLane){ .enabled = false,.stepBeat = 1.0,.phaseBeat = 0.5 };
    st.lanes[TICK_BACKBEAT_2_4] = (TickLane){ .enabled = false,.stepBeat = 2.0,.phaseBeat = 1.0 };
    st.lanes[TICK_TRIPLET_OFF] = (TickLane){ .enabled = false,.stepBeat = 1.0,.phaseBeat = 1.0 / 3.0 };


    SDL_zero(st.evq);  // w,r,lock=0
    rebase_lanes(&st);

    st.fsch.count = 0;
    st.fsch.next = 0;

    st.audioOffsetMs = 840;
    st.audioOffsetFrames = (int64_t)llround(st.audioOffsetMs * (double)st.spec.freq / 1000.0);

    SDL_PauseAudioDevice(st.dev, 0);

    for (int i = 0; i < 128; i++) {
        st.midiTrackMap[i] = NULL;
        st.midiTrackEnabled[i] = true;
        st.lastFiredSample[i] = -1;
    }
    for (int i = 0; i < TICK_MAX; i++) st.tickHandlers[i] = NULL;
    for (int i = 0; i < FRAME_EV_MAX; i++) st.frameHandlers[i] = NULL;

    // 連打抑制 30ms（好みで20～50ms）
    st.debounceSamples = (int64_t)((double)st.spec.freq * 30.0 / 1000.0 + 0.5);

    return true;
}

void musicEventUpdate() {

    // 変更量
    SDL_Keymod mod = SDL_GetModState();
    double bpmStep = 0.001;
    if (mod & KMOD_ALT) bpmStep = 1;
    if (mod & KMOD_LSHIFT) bpmStep = 0.1;
    if (mod & KMOD_RSHIFT) bpmStep = 0.01;
    if (mod & KMOD_CTRL)  bpmStep = 0.0001;

    // 位相ズレ量（ms単位で指定→framesに変換）
    double msStep = 1.0;
    if (mod & KMOD_ALT) msStep = 1000.0;
    if (mod & KMOD_LSHIFT) msStep = 100.0;
    if (mod & KMOD_SHIFT) msStep = 10.0;
    if (mod & KMOD_CTRL)  msStep = 0.1;

    SDL_LockAudioDevice(st.dev);
    if (getKeyDown(SDL_SCANCODE_UP)) {
        set_bpm_locked(&st, st.bpm + bpmStep);
    }
    else if (getKeyDown(SDL_SCANCODE_DOWN)) {
        set_bpm_locked(&st, st.bpm - bpmStep);
    }
    else if (getKeyDown(SDL_SCANCODE_LEFT)) {
        // 左＝内部オーディオ再生位置を早める
        st.audioOffsetMs -= msStep;
        st.audioOffsetFrames = (int64_t)llround(st.audioOffsetMs * (double)st.spec.freq / 1000.0);
        SDL_Log("[musicEvent] audioOffsetMs=%+0.2f (frames=%lld)", st.audioOffsetMs, (long long)st.audioOffsetFrames);
    }
    else if (getKeyDown(SDL_SCANCODE_RIGHT)) {
        // 右＝内部オーディオ再生位置を遅らせる
        st.audioOffsetMs += msStep;
        st.audioOffsetFrames = (int64_t)llround(st.audioOffsetMs * (double)st.spec.freq / 1000.0);
        SDL_Log("[musicEvent] audioOffsetMs=%+0.2f (frames=%lld)", st.audioOffsetMs, (long long)st.audioOffsetFrames);
    }
    else if (getKeyDown(SDL_SCANCODE_SPACE)) {
        st.metroEnable = !st.metroEnable;
        st.clickActive = false;
        st.clickPos = 0;
    }
    else if (getKeyDown(SDL_SCANCODE_R)) {
        reset_transport_locked(&st, -st.audioOffsetMs);
    }
    /*else if (getKeyDown(SDL_SCANCODE_ESCAPE)) {
        isRunning = false;
    }*/
    SDL_UnlockAudioDevice(st.dev);
    // タイトル更新（10Hz程度）
    Uint32 now = SDL_GetTicks();
    if (now - lastTitle > 100) {
        lastTitle = now;

        SDL_LockAudioDevice(st.dev);
        double bpm = st.bpm;
        double offsetMs = st.offsetFrames * 1000.0 / (double)st.spec.freq;
        double audioOffsetMs = st.audioOffsetMs;
        bool metro = st.metroEnable;
        int64_t frame = music_pos_with_audio_offset(&st);
        sec = (double)frame / (double)st.spec.freq;
        SDL_UnlockAudioDevice(st.dev);

        SDL_snprintf(info, sizeof(info),
            "BPM: %.3f | Offset: %+0.2f ms | AudioOffset: %+0.2f ms | Metronome: %s | Sec: %f | Frame: %lld\n",
            bpm, offsetMs, audioOffsetMs, metro ? "ON" : "OFF", sec, (long long)frame);
    }

    AppEvent ev;
    while (evq_pop(&st.evq, &ev)) {

        if (ev.kind == EV_MIDI_NOTE) {
            dispatch_midi_note(&st, &ev);
            continue;
        }

        if (ev.kind == EV_FRAME) {
            if (ev.id >= 0 && ev.id < FRAME_EV_MAX) {
                FrameHandler handler = st.frameHandlers[ev.id];
                if (handler) handler(&st, ev.musicFrame);
            }
            continue;
        }

        // それ以外はTick
        if (ev.id >= 0 && ev.id < TICK_MAX) {
            TickHandler handler = st.tickHandlers[ev.id];
            if (handler) handler(&st);
        }
    }
}

bool musicEventRegisterTickHandler(TickId id, TickHandler handler) {
    if (id < 0 || id >= TICK_MAX) return false;
    lock_audio_if_ready();
    st.tickHandlers[id] = handler;
    unlock_audio_if_ready();
    return true;
}

void musicEventUnregisterTickHandler(TickId id) {
    if (id < 0 || id >= TICK_MAX) return;
    lock_audio_if_ready();
    st.tickHandlers[id] = NULL;
    unlock_audio_if_ready();
}

void musicEventSetTickEnabled(TickId id, bool enabled) {
    if (id < 0 || id >= TICK_MAX) return;
    lock_audio_if_ready();
    st.lanes[id].enabled = enabled;
    if (enabled) {
        rebase_lanes(&st);
    }
    unlock_audio_if_ready();
}

bool musicEventRegisterFrameHandler(int id, FrameHandler handler) {
    if (id < 0 || id >= FRAME_EV_MAX) return false;
    lock_audio_if_ready();
    st.frameHandlers[id] = handler;
    unlock_audio_if_ready();
    return true;
}

void musicEventUnregisterFrameHandler(int id) {
    if (id < 0 || id >= FRAME_EV_MAX) return;
    lock_audio_if_ready();
    st.frameHandlers[id] = NULL;
    unlock_audio_if_ready();
}

void musicEventSetFrameEventEnabled(int id, bool enabled) {
    lock_audio_if_ready();
    int idx = frame_event_index_by_id(&st.fsch, id);
    if (idx >= 0) {
        st.fsch.ev[idx].enabled = enabled;
        st.fsch.next = 0;
    }
    unlock_audio_if_ready();
}

bool musicEventAddFrameEvent(int id, int frame, bool enabled) {
    if (id < 0 || id >= FRAME_EV_MAX) return false;
    lock_audio_if_ready();
    int idx = frame_event_index_by_id(&st.fsch, id);
    if (idx >= 0) {
        st.fsch.ev[idx].frame = frame;
        st.fsch.ev[idx].enabled = enabled;
        st.fsch.ev[idx].fired = false;
    }
    else {
        if (st.fsch.count >= FRAME_EV_MAX) {
            unlock_audio_if_ready();
            return false;
        }
        st.fsch.ev[st.fsch.count++] = (FrameEvent){ .frame = frame, .id = id, .enabled = enabled, .fired = false };
    }
    qsort(st.fsch.ev, (size_t)st.fsch.count, sizeof(FrameEvent), cmp_frame_event_by_frame);
    st.fsch.next = 0;
    unlock_audio_if_ready();
    return true;
}

bool musicEventRemoveFrameEvent(int id) {
    lock_audio_if_ready();
    int idx = frame_event_index_by_id(&st.fsch, id);
    if (idx < 0) {
        unlock_audio_if_ready();
        return false;
    }
    for (int i = idx; i + 1 < st.fsch.count; i++) {
        st.fsch.ev[i] = st.fsch.ev[i + 1];
    }
    st.fsch.count--;
    st.fsch.next = 0;
    unlock_audio_if_ready();
    return true;
}

bool musicEventRegisterMidiTrackHandler(uint8_t track, MidiTrackHandler handler) {
    if (track >= 128) return false;
    if (st.song.trackCount > 0 && track >= (uint8_t)st.song.trackCount) return false;
    lock_audio_if_ready();
    st.midiTrackMap[track] = handler;
    unlock_audio_if_ready();
    return true;
}

void musicEventUnregisterMidiTrackHandler(uint8_t track) {
    if (track >= 128) return;
    lock_audio_if_ready();
    st.midiTrackMap[track] = NULL;
    unlock_audio_if_ready();
}

void musicEventSetMidiTrackEnabled(uint8_t track, bool enabled) {
    if (track >= 128) return;
    lock_audio_if_ready();
    st.midiTrackEnabled[track] = enabled;
    unlock_audio_if_ready();
}

char* getInfo() {
    return info;
}

void musicEventQuit() {


    free_midi_song(&st.song);
}
