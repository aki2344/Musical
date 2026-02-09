#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef uint32_t DFA_FontID;
#define DFA_INVALID_FONT_ID ((DFA_FontID)0)

/* 追加したフォントは内部で保持され、DFA_Quit()で一括破棄されます */
DFA_FontID DFA_Init(
    SDL_Renderer* renderer,
    const char* name,          /* ユニーク名推奨。NULL/空なら自動名 */
    const char* font_path,
    float ptsize,
    int style,
    int atlas_w,
    int atlas_h,
    int padding_px,
    bool extrude,
    const char* cache_dir      /* NULL可 */
);

/* 毎フレームの最後（または適切なタイミング）に1回呼ぶ */
void DFA_Update(int max_pump_per_font);

/* プリウォーム（任意。呼ばなくてもDrawが自動要求します） */
void DFA_RequestText(DFA_FontID font, const char* fmt, ...);

/* 全フォントの後始末（必要ならキャッシュ保存も内部で実行） */
void DFA_Quit(void);

/* ===========================
   追加: レイアウト（Align / Wrap / Vertical anchor）
   =========================== */

typedef enum DFA_TextAlign {
    DFA_ALIGN_LEFT = 0,
    DFA_ALIGN_CENTER = 1,
    DFA_ALIGN_RIGHT = 2,
} DFA_TextAlign;

typedef enum DFA_TextVAnchor {
    DFA_VANCHOR_BASELINE = 0, /* y をベースラインとして扱う */
    DFA_VANCHOR_TOP = 1,      /* y を上端（テキストブロックの上）として扱う */
    DFA_VANCHOR_MIDDLE = 2,   /* y を中央として扱う */
    DFA_VANCHOR_BOTTOM = 3,   /* y を下端として扱う */
} DFA_TextVAnchor;

typedef enum DFA_WrapMode {
    DFA_WRAP_NONE = 0,
    DFA_WRAP_CHAR = 1,  /* 幅超えで強制改行 */
    DFA_WRAP_WORD = 2,  /* 空白で改行（無理ならCHARへフォールバック） */
} DFA_WrapMode;

typedef struct DFA_TextLayout {
    DFA_TextAlign align;
    DFA_TextVAnchor vanchor;

    float max_width_px;     /* <=0 ならwrap無効 */
    DFA_WrapMode wrap;

    float line_spacing;     /* 1.0 = 通常、1.2などで行間増加 */
    bool use_true_baseline; /* 未使用 */
} DFA_TextLayout;

/* デフォルト値（LEFT + BASELINE + wrapなし）を返す */
DFA_TextLayout DFA_TextLayoutDefault(void);

/* 追加: レイアウト対応（fmt版） */
bool DFA_DrawText(
    DFA_FontID font,
    float x,
    float y,
    float scale, 
    float angle,
    SDL_Color color,
    const DFA_TextLayout* layout, /* NULLならDefault */
    const char* fmt,
    ...
);

/* 追加: サイズ計測（wrap/align/anchorも反映） */
bool DFA_MeasureText(
    DFA_FontID font,
    float scale,
    const DFA_TextLayout* layout, /* NULLならDefault */
    float* out_w,
    float* out_h,
    const char* fmt,
    ...
);

/* ===========================
   追加: 1文字ずつ表示するメッセージ
   =========================== */

typedef struct DFA_Message {
    DFA_FontID font;
    float x, y, scale, angle;
    SDL_Color color;
    DFA_TextLayout layout; /* コピー保持 */

    char* text;            /* UTF-8 */
    int* offsets;          /* codepoint i の開始byte。offsets[0]=0, offsets[count]=strlen */
    int count;             /* 総コードポイント数 */
    int visible;           /* 現在表示するコードポイント数 */

    float interval_sec;    /* 1文字あたりの秒（<=0なら即時全表示） */
    float accum_sec;

    uint64_t last_ticks;   /* DFA_MessageUpdateAuto 用 */
    bool playing;
} DFA_Message;

void DFA_MessageInit(DFA_Message* m);
void DFA_MessageDestroy(DFA_Message* m);

bool DFA_MessageSet(
    DFA_Message* m,
    DFA_FontID font,
    float x,
    float y,
    float scale,
    float angle,
    SDL_Color color,
    float interval_sec,
    const DFA_TextLayout* layout, /* NULLならDefault */
    const char* fmt,
    ...
);

/* 再生開始（reset=trueで先頭から） */
void DFA_MessageStart(DFA_Message* m, bool reset);

/* dtを渡す更新（秒） */
void DFA_MessageUpdate(DFA_Message* m, float dt_sec);

/* 自動更新（SDL_GetTicks で内部dt算出） */
void DFA_MessageUpdateAuto(DFA_Message* m);

/* 描画（内部で DFA_DrawTextEx 相当を呼ぶ） */
bool DFA_MessageDraw(const DFA_Message* m);

/* SDL2_mixer を使う場合だけリンクしてください */
typedef struct Mix_Chunk Mix_Chunk;

/* 1文字進むたびに効果音を鳴らす版 */
void DFA_MessageUpdateSfx(DFA_Message* m, float dt_sec, Mix_Chunk* sfx_track);
void DFA_MessageUpdateAutoSfx(DFA_Message* m, Mix_Chunk* sfx_track);
