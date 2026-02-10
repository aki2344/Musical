# Build

$ cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=./toolchain-aarch64.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/ --debug-find-pkg=SDL2

$ cmake --build build -j

---
# Environment construction

## WSL install (Windows)
wsl --install Ubuntu

## arm64
$ sudo apt-get update

$ sudo dpkg --add-architecture arm64

$ sudo tee /etc/apt/sources.list.d/ubuntu-ports-arm64.list >/dev/null <<'EOF'

deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports noble main universe multiverse restricted

deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports noble-updates main universe multiverse restricted

deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports noble-backports main universe multiverse restricted

deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports noble-security main universe multiverse restricted

EOF

$ sudo vi /etc/apt/sources.list.d/ubuntu.sources

```
Types: deb
URIs: http://archive.ubuntu.com/ubuntu/
Suites: noble noble-updates noble-backports
Architectures: amd64
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

## Ubuntu security updates. Aside from URIs and Suites,
## this should mirror your choices in the previous section.
Types: deb
URIs: http://security.ubuntu.com/ubuntu/
Suites: noble-security
Architectures: amd64
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
```
## Install Tools and Modules

$ sudo apt-get install -y git cmake ninja-build pkg-config gcc-aarch64-linux-gnu g++-aarch64-linux-gnu binutils-aarch64-linux-gnu

$ sudo apt-get install -y libfreetype6-dev:arm64 libharfbuzz-dev:arm64

---
# Change the project

Modify the project name and source file list in CMakeLists.txt

The startup shell script only changes the project name.

---
# MIDI/イベント処理 ドキュメント

本節では、`midi_smf.h` / `musicEvent.h` に定義された構造体・列挙型の役割と、`midi_smf.c` / `musicEvent.c` に実装された機能の全体構造を解説します。設計の流れとしては、**MIDIファイルを読み込み → テンポ変換でサンプル位置へ変換 → オーディオコールバック中にイベント生成 → メインスレッドでイベント消費**という構成です。全体の関係性は最後にまとめています。  

## 1. 構造体・列挙型の解説（midi_smf.h / musicEvent.h）

### 1.1 MIDI関連（midi_smf.h）

#### `MidiNoteEvent`
* **役割**: MIDIノートイベント（NoteOn/NoteOff）を保持する最小単位。  
* **重要なフィールド**  
  * `tick`/`sample`: 楽曲開始からの絶対位置（tick/サンプル）。  
  * `on`: 1=NoteOn, 0=NoteOff。  
  * `note`/`vel`: ノート番号とベロシティ。  
* **利用先**: `midi_smf.c` でtick→sample変換後、`MidiSong.ev` に格納され、`musicEvent.c` でイベント発火に使用。  

#### `MidiTempoSeg`
* **役割**: テンポ変更区間をまとめたセグメント。  
* **重要なフィールド**  
  * `startTick`/`startUs`/`startSample`: そのセグメントの開始位置。  
  * `tempoUsPerQN`: その区間のテンポ（四分音符のマイクロ秒数）。  
* **利用先**: tick↔sample変換の基準。  

#### `MidiSong`
* **役割**: MIDI解析結果の集合。  
* **重要なフィールド**  
  * `tpqn`: ticks per quarter note。  
  * `lengthTicks`/`lengthSamples`: 曲の長さ。  
  * `seg`/`segCount`: テンポセグメント配列。  
  * `ev`/`evCount`: `MidiNoteEvent`配列（サンプル順ソート）。  
* **利用先**: オーディオ再生と同期しつつイベントを取り出す中心構造。  

#### `Cur`
* **役割**: バッファ読み取りのカーソル（`p`〜`end`）。  
* **利用先**: `midi_smf.c` のSMFパーサ。  

#### `NoteVec` / `TempoEv` / `TempoVec`
* **役割**: 解析途中で使う可変長配列。  
* **利用先**: `load_midi_build_events` 内でノート/テンポイベントを一時保存。  

---

### 1.2 イベント・再生関連（musicEvent.h）

#### `TickId`（列挙）
* **役割**: 拍・サブディビジョンの識別子。  
* **代表値**: `TICK_BEAT`（1拍）、`TICK_HALF`（2分音符）、`TICK_TRIPLET`（3連）、`TICK_BACKBEAT_2_4` など。  
* **利用先**: `TickLane`・`AppEvent` で拍イベントを生成。  

#### `TickLane`
* **役割**: 「何拍ごとにイベントを出すか」を定義。  
* **重要なフィールド**  
  * `stepBeat`: 発火間隔（拍単位）。  
  * `phaseBeat`: 位相シフト。  
  * `nextBeat`: 次に発火する絶対拍位置。  
* **利用先**: audio callback 中に `beatAbs` と比較し、tickイベントを生成。  

#### `EvKind`（列挙）
* **役割**: アプリ内イベントの種類を識別（Tick/Frame/MIDI）。  

#### `AppEvent`
* **役割**: メインスレッドで処理されるイベント形式。  
* **重要なフィールド**  
  * `kind`: `EV_TICK`, `EV_FRAME`, `EV_MIDI_NOTE`。  
  * `id`: tickやframeイベントのID。  
  * `on`/`note`/`vel`/`sample`: MIDIイベント情報。  
* **利用先**: `EventQueue` に入り、`musicEventUpdate` で処理。  

#### `EventQueue`
* **役割**: audio callback → メインスレッド間のリングバッファ。  
* **重要なフィールド**  
  * `w`/`r`: write/read index。  
  * `lock`: スピンロックで排他。  

#### `FrameEvent`
* **役割**: 特定フレームに到達したら発火するイベント。  
* **利用先**: `FrameScheduler` から `EV_FRAME` を生成。  

#### `FrameScheduler`
* **役割**: `FrameEvent` の管理。  
* **重要なフィールド**: `count`（登録数）、`next`（次に評価するインデックス）。  

#### `NoteHandler`
* **役割**: MIDIノートに対応するコールバック型。  
* **利用先**: `AppState.noteMap` のディスパッチに使用。  

#### `AppState`
* **役割**: 再生・イベント・MIDI同期の中心状態。  
* **主要構成**  
  * **音源関連**: `music`, `click`, `musicPos`, `musicFrames`, `musicGain`, `clickGain` など。  
  * **テンポ管理**: `bpm`, `samplesPerBeat`, `beatAbs`, `beatInc`, `offsetFrames`。  
  * **拍イベント**: `lanes[TICK_MAX]`。  
  * **イベント駆動**: `evq`, `fsch`。  
  * **MIDI**: `song`、`nextEvIndex`、`noteMap`。  

---

## 2. 機能解説（midi_smf.c）

### 2.1 目的と全体の流れ
`midi_smf.c` は **SMF(Type1)のMIDIファイルを読み込み、NoteOn/NoteOff をサンプル順イベント列に変換する** モジュールです。主要な処理の流れは以下の通りです。

1. MIDIファイルをバッファに読み込む。  
2. ヘッダ解析（Type1 / TPQN を取得）。  
3. 各トラックを解析し、ノートイベントとテンポイベントを抽出。  
4. テンポイベントを元にテンポセグメントを構築。  
5. ノートイベントの tick を sample へ変換し、サンプル順にソート。  
6. `MidiSong` を完成させて返却。  

---

### 2.2 主要機能

#### (1) バイナリ読み取りユーティリティ
* `rd_u32be`, `rd_u16be`, `read_u8`, `read_vlq` など。  
* VLQ（可変長量）を扱うためのヘルパ。  
* `Cur` を使って範囲チェックを行い、バッファ越境を防ぐ。  

#### (2) イベント格納
* `push_note` / `push_tempo`  
  * `NoteVec` / `TempoVec` に可変長で格納。  
  * realloc で自動拡張。  

#### (3) トラック解析 `parse_track`
* `MTrk` チャンクを読み取る。  
* **MIDIメッセージ解釈**  
  * 0x90/0x80 を NoteOn/NoteOff として抽出。  
  * NoteOnベロシティ0はNoteOff扱い。  
* **メタイベント**  
  * 0xFF 0x51 でテンポ取得。  
  * 0xFF 0x2F でトラック終了。  
* **SysEx** はスキップ。  
* トラック終了tickを `outEndTick` で返す。  

#### (4) テンポセグメント構築 `build_tempo_segments`
* テンポイベントを tick順にソートし、同tickは最後の値で採用。  
* デフォルトテンポ（500000us/qn）を基準に `MidiTempoSeg` を構成。  
* 各セグメントの `startUs` を累積計算。  
* `fill_seg_startSample` で sampleRate から `startSample` を算出。  

#### (5) tick↔sample変換
* `midi_tick_to_sample`  
  * 該当テンポセグメントを取得し、tick差分からマイクロ秒→サンプル変換。  
* `midi_sample_to_tick`  
  * `startSample` を二分探索し、対応するtickに逆変換。  

#### (6) MIDIロードのメイン `load_midi_build_events`
* すべてのトラックからイベント抽出。  
* ノートイベントは tick順 → sample計算 → sample順に再ソート。  
* `MidiSong` に格納し、長さ（ticks/samples）を計算。  

#### (7) リソース解放 `free_midi_song`
* `seg` / `ev` を解放して初期化。  

### 2.3 SMF(Type1) バイナリ構造と `midi_smf.c` の変数対応

ここでは、Standard MIDI File (SMF) Type1 の「実際のバイト列の意味」と、`midi_smf.c` での格納先を対応づけて整理します。

#### (A) ファイル全体レイアウト

SMF(Type1) は、次のチャンク列です。

1. `MThd` ヘッダチャンク（固定で先頭）
2. `MTrk` トラックチャンク × `ntrks` 個

`midi_smf.c` では、まずファイル全体を `buf` に読み込み、`Cur c = { buf, buf + fsz };` で走査します。ヘッダ読み取り後は `c.p` を進めながら、トラック数ぶん `parse_track` を呼び出します。

#### (B) `MThd` ヘッダチャンク（14バイト以上）

ヘッダの代表的なバイト構造（ビッグエンディアン）:

* `0..3` : `"MThd"`
* `4..7` : `header length`（通常 6）
* `8..9` : `format`
* `10..11`: `ntrks`（トラック数）
* `12..13`: `division`

`midi_smf.c` 側の対応:

* `uint32_t hlen = rd_u32be(c.p + 4);`
  * ヘッダ長を取得。`c.p += 8 + hlen;` で次チャンクへ進む。
* `uint16_t fmt = rd_u16be(c.p + 8);`
  * SMFフォーマット。`fmt != 1` はエラー（Type1のみ対応）。
* `uint16_t ntr = rd_u16be(c.p + 10);`
  * トラック数。`for (i=0; i<ntr; i++) parse_track(...)` で使用。
* `uint16_t div = rd_u16be(c.p + 12);`
  * 時間分解能。`div & 0x8000`（SMPTE）を拒否し、`song.tpqn = div & 0x7FFF` を採用。

#### (C) `MTrk` トラックチャンク

各トラックは以下の形です。

* `"MTrk"` (4 byte)
* `length` (4 byte, BE)
* `track event stream` (`length` byte)

`parse_track` では:

* `len = rd_u32be(c->p + 4)` でトラックデータ長を取得。
* `Cur t = { c->p, c->p + len };` を作り、以降は `t` 内でイベントを読む。
* トラック内の絶対tickは `int32_t absTick` に蓄積。

#### (D) トラックイベントのバイナリ構造

トラックイベントは概ね次の並びです。

1. `delta-time`（VLQ）
2. `event`（ステータス+データ、またはランニングステータス適用）

`midi_smf.c` では:

* `read_vlq(&t, &delta)` で `delta-time` を取り、`absTick += delta`。
* 先頭1バイト `b` を読んで、
  * `b & 0x80 != 0` なら新規ステータスバイト。
  * それ以外なら `running` をステータスとして使い、`t.p--` で `b` をデータとして再利用。
* チャネルメッセージ（`0x8n..0xEn`）で `running = status` 更新。

#### (E) 本実装で解釈しているイベントと格納先

1. **Note On (`0x9n`)**
   * データ: `note(d1), velocity(d2)`
   * `d2==0` は NoteOff として扱う。
   * 格納: `push_note(notes, absTick, track, on, d1, d2or0)`
   * 一時保存先: `NoteVec notes`（`notes.a[notes.n]`）

2. **Note Off (`0x8n`)**
   * データ: `note(d1), velocity(d2)`（実装では velocity は使用せず 0 扱い）
   * 格納: `push_note(notes, absTick, track, 0, d1, 0)`

3. **Set Tempo メタイベント (`0xFF 0x51 0x03 tt tt tt`)**
   * `tt tt tt` = 四分音符あたりマイクロ秒 (`tempoUsPerQN`)
   * 格納: `push_tempo(tempos, absTick, tempo)`
   * 一時保存先: `TempoVec tempos`（`tempos.a[tempos.n]`）

4. **End of Track (`0xFF 0x2F 0x00`)**
   * 検出後にトラック解析ループを `break`。

5. **SysEx (`0xF0`, `0xF7`)**
   * `read_vlq` で長さを取り、`skip_n` で内容を読み飛ばす。

6. **その他メタイベント**
   * 長さだけ読んでスキップ（テンポ以外は保持しない）。

#### (F) 一時配列から最終 `MidiSong` への反映

`load_midi_build_events` の後半で、抽出データを `MidiSong song` に確定します。

* `song.trackCount = ntr`
* `song.lengthTicks = endTickMax`
* `song.tpqn = div & 0x7FFF`
* テンポ:
  * `TempoVec tempos` → `build_tempo_segments` で `song.seg` / `song.segCount`
  * `fill_seg_startSample` で各セグメント `startSample` を計算
* ノート:
  * `NoteVec notes` を `tick` 順ソート
  * 各要素 `notes.a[i].tick` をテンポセグメントで `notes.a[i].sample` に変換
  * `sample` 順に再ソート
  * `song.ev = notes.a`, `song.evCount = notes.n`
* 最終長:
  * `song.lengthSamples = midi_tick_to_sample(&song, song.lengthTicks, sampleRate)`

#### (G) 変数対応のクイックリファレンス

* ファイル全体バッファ: `buf`, `fsz`
* 走査カーソル: `Cur c`（全体）, `Cur t`（各トラック）
* ヘッダ値: `fmt`, `ntr`, `div`, `hlen`
* 解析中tick: `absTick`, `delta`
* ランニングステータス: `running`, `status`
* 抽出ノート: `NoteVec notes` → `song.ev`
* 抽出テンポ: `TempoVec tempos` → `song.seg`
* 曲全体情報: `song.tpqn`, `song.lengthTicks`, `song.lengthSamples`, `song.trackCount`

この対応を押さえると、SMF(Type1) の「バイナリ上の意味」と `midi_smf.c` のメモリ上データ構造の対応を追いやすくなります。

---

## 3. 機能解説（musicEvent.c）

### 3.1 目的と全体構造
`musicEvent.c` は **音声再生とMIDI/tickイベントを同期的に生成し、メインループでイベントを処理する** モジュールです。  
全体の流れは以下です。  

1. `musicEventInit` で SDL音声デバイス・WAV・MIDI・イベント設定を初期化。  
2. `audio_cb`（SDL audio callback）内で  
   * 音声ミックス  
   * 拍イベント生成  
   * MIDIイベント生成  
   * フレームイベント生成  
3. `musicEventUpdate` でイベントキューを取り出し、各ハンドラを呼び出す。  

---

### 3.2 主要機能

#### (1) MIDIイベントのディスパッチ `dispatch_midi_note`
* `AppEvent.kind == EV_MIDI_NOTE` の場合のみ処理。  
* **デバウンス処理**により短時間連打を抑制。  
* `noteMap` に登録された `NoteHandler` を呼び出す。  

#### (2) TickLane管理
* `wrap_phase`, `frac01`, `rebase_lanes`, `sync_offset_from_beatAbs`  
  * `beatAbs` の進行に合わせて `nextBeat` を更新。  
  * phaseを考慮してメトロノームやオフビートを実現。  

#### (3) EventQueue
* `evq_push` / `evq_pop`  
  * SDL_SpinLock で排他し、リアルタイムスレッド⇔メインスレッドを接続。  

#### (4) BPM/位相操作
* `set_bpm_locked`  
  * BPM変更時に拍位置を保持し、laneを再調整。  
* `shift_phase_frames_locked` / `shift_phase_ms_locked`  
  * シフト分だけ拍位置を進めたり戻したりする。  
* `reset_transport_locked`  
  * 音楽位置・クリック・拍・イベント状態をリセット。  

#### (5) 音源準備
* `load_wav_as_f32`  
  * SDLの変換機構でWAVをターゲット形式へ。  
* `make_click_f32`  
  * 短いクリック音を合成してメトロノーム用に生成。  

#### (6) MIDIイベント生成
* `push_midi_range`  
  * `MidiSong.ev` から「指定サンプル範囲内のMIDIイベント」をキューへ格納。  

#### (7) オーディオコールバック `audio_cb`
* **MIDIイベントの生成**  
  * `musicPos` と `frames` から範囲を計算し `push_midi_range` を呼ぶ。  
  * ループ時は範囲を分割し、`nextEvIndex` をリセット。  
* **拍（Tick）イベント生成**  
  * `beatAbs` と `TickLane.nextBeat` を比較して `EV_TICK` を生成。  
* **Frameイベント生成**  
  * `FrameScheduler` で `musicPos` 到達時に `EV_FRAME` を生成。  
* **音声ミックス**  
  * `music` と `click` を加算し、クリッピングを抑制。  
* **位置更新**  
  * `musicPos` と `clickPos` を進める。  

#### (8) 初期化 `musicEventInit`
* SDL Audio デバイスを開く。  
* `sound/ss.wav` を読み込み、クリック音を合成。  
* MIDIを読み込み `MidiSong` に格納。  
* BPM/拍設定、`TickLane` 初期化。  
* `FrameScheduler` に特定フレームイベントを登録。  
* `noteMap` にMIDIノートのハンドラを登録。  

#### (9) 更新ループ `musicEventUpdate`
* キーボード操作でBPM調整や位相シフトを行う。  
* イベントキューを取り出し、  
  * MIDI → `dispatch_midi_note`  
  * Frame → `on_frame_event_*`  
  * Tick → `on_tick_*`  
  を呼び出す。  
* タイトル情報（BPM/Offset/Frameなど）を更新する。  

#### (10) 終了処理 `musicEventQuit`
* `free_midi_song` によりMIDIリソースを解放。  

---

## 4. 全体構造（機能間の関係）

**MIDI解析層（midi_smf.c）**  
→ `load_midi_build_events` が `MidiSong` を構築し、`MidiNoteEvent` をサンプル順配列として提供。  

**オーディオ再生・イベント生成層（musicEvent.c）**  
→ `musicEventInit` で MIDI を読み込み、`AppState.song` に保持。  
→ `audio_cb` で時間進行に合わせて `EventQueue` に `AppEvent` を投入。  

**メインスレッド処理層（musicEventUpdate）**  
→ `EventQueue` を消費し、MIDI/拍/フレームイベントごとに処理を振り分け。  

---

## 5. 使用時の一連の流れ（まとめ）

1. **初期化**: `musicEventInit`  
   * WAVとMIDIを読み込み、拍・イベントを構成。  
2. **再生/更新ループ**:  
   * `audio_cb` が音声再生とイベント生成を行う。  
   * `musicEventUpdate` がイベントを処理。  
3. **終了処理**: `musicEventQuit`  
   * MIDIメモリを解放。  

---

## 6. `musicEvent.c` Handler（Tick / Frame / MidiTrack）追加・削除手順

ここでは、実際にプログラマーがどの順番で設定すれば安全に動かせるかを、`musicEvent.h` の公開APIベースで説明します。

> 重要: `musicEvent.c` 側の登録/削除関数は内部で `SDL_LockAudioDevice` を使用し、オーディオコールバックとの競合を避ける設計です。`st` を直接編集せず、公開関数を経由して設定してください。

### 6.1 Tick Handler（拍イベント）

#### 役割
* `TickId` ごと（`TICK_BEAT` など）に、拍イベント発火時の処理を差し替えます。

#### 追加（登録）手順
1. `TickHandler` シグネチャ（`void (*)(AppState* st)`）の関数を作成する。
2. `musicEventRegisterTickHandler(id, handler)` を呼ぶ。
3. 必要なら `musicEventSetTickEnabled(id, true)` で lane を有効化する。

```c
static void onMyBeat(struct AppState* st) {
    (void)st;
    // 任意処理
}

musicEventRegisterTickHandler(TICK_BEAT, onMyBeat);
musicEventSetTickEnabled(TICK_BEAT, true);
```

#### 削除（解除）手順
* `musicEventUnregisterTickHandler(id)` を呼ぶ。
* 必要なら `musicEventSetTickEnabled(id, false)` も呼び、イベント生成自体を止める。

#### 注意点
* `id` が範囲外（`id < 0 || id >= TICK_MAX`）だと登録失敗または無視されます。
* Handlerを解除しても lane が有効のままだと、イベント生成自体は継続します（呼び先が `NULL` になるだけ）。

---

### 6.2 Frame Handler（特定フレーム到達イベント）

Frame 系は「いつ発火するか（FrameEvent）」と「発火時に何をするか（FrameHandler）」を分けて設定します。

#### 追加（登録）手順
1. `FrameHandler` 関数（`void (*)(AppState* st, int musicFrame)`）を作る。
2. `musicEventRegisterFrameHandler(id, handler)` で処理を登録する。
3. `musicEventAddFrameEvent(id, frame, enabled)` で発火タイミングを登録する。

```c
static void onBossWarning(struct AppState* st, int musicFrame) {
    (void)st;
    (void)musicFrame;
    // 任意処理
}

musicEventRegisterFrameHandler(10, onBossWarning);
musicEventAddFrameEvent(10, 44100 * 30, true); // 30秒地点
```

#### 更新・無効化・削除
* 同じ `id` に `musicEventAddFrameEvent` を再実行すると、既存イベントを更新できます。
* 一時停止したいだけなら `musicEventSetFrameEventEnabled(id, false)`。
* イベント定義自体を削除するなら `musicEventRemoveFrameEvent(id)`。
* 処理関数側も消すなら `musicEventUnregisterFrameHandler(id)`。

#### 注意点
* `id` は `0 <= id < FRAME_EV_MAX` が必須です。
* `musicEventRemoveFrameEvent(id)` は対象が無いと `false` を返します。
* FrameEventを削除しても、FrameHandlerの関数ポインタは残るため、不要なら明示的に unregister してください。

---

### 6.3 MidiTrack Handler（MIDIトラック単位）

#### 役割
* MIDIイベントを「ノート番号」ではなく「トラック番号」単位で処理します。

#### 追加（登録）手順
1. `MidiTrackHandler` 関数（`void (*)(AppState* st, uint8_t track, uint8_t note, uint8_t vel, bool on)`）を作る。
2. `musicEventRegisterMidiTrackHandler(track, handler)` を呼ぶ。
3. 必要なら `musicEventSetMidiTrackEnabled(track, true)` を呼ぶ（通常は初期状態で true）。

```c
static void onTrack0(struct AppState* st, uint8_t track, uint8_t note, uint8_t vel, bool on) {
    (void)st;
    (void)track;
    (void)note;
    (void)vel;
    (void)on;
    // 任意処理
}

musicEventRegisterMidiTrackHandler(0, onTrack0);
musicEventSetMidiTrackEnabled(0, true);
```

#### 削除（解除）手順
* `musicEventUnregisterMidiTrackHandler(track)` で関数解除。
* 必要なら `musicEventSetMidiTrackEnabled(track, false)` で対象トラックのディスパッチを停止する。

#### 注意点
* `track >= 128` は無効です。
* `st.song.trackCount > 0` のときは `track >= st.song.trackCount` でも登録失敗になります。
* `enabled=false` だとコールバックに届く前に破棄されるため、負荷を下げたい時は unregister だけでなく disable も有効です。

---

### 6.4 推奨セットアップ順（実運用）

1. `musicEventInit()` を実行して、音声/MIDI/内部状態を初期化する。
2. Tick / Frame / MidiTrack の各Handlerを register する。
3. FrameEvent（発火フレーム）を add する。
4. 必要な lane / frame event / track を enable する。
5. メインループ内で `musicEventUpdate()` を継続実行する。

この順序にしておくと、「処理関数未登録のままイベントだけ先に発火する」状態を避けやすくなります。
