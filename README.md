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
