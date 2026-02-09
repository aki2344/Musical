#include "dynamic_font_atlas.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>

/* =========================================================
   小さいユーティリティ
   ========================================================= */
static inline void dfa_rotate_point(float* x, float* y, float px, float py, float c, float s)
{
    float dx = *x - px;
    float dy = *y - py;
    float rx = dx * c - dy * s;
    float ry = dx * s + dy * c;
    *x = px + rx;
    *y = py + ry;
}

static uint32_t dfa_fnv1a32(const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

static uint32_t dfa_hash_str(const char* s) {
    return dfa_fnv1a32(s, strlen(s));
}

/* UTF-8 デコード（最低限、異常系は U+FFFD へ） */
static const char* dfa_utf8_next(const char* s, uint32_t* out_cp) {
    const uint8_t* p = (const uint8_t*)s;
    if (!p || !*p) { *out_cp = 0; return s; }

    uint32_t cp = 0;
    if (p[0] < 0x80) {
        cp = p[0];
        *out_cp = cp;
        return s + 1;
    }
    if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        if (cp < 0x80) cp = 0xFFFD;
        *out_cp = cp;
        return s + 2;
    }
    if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        if (cp < 0x800) cp = 0xFFFD;
        *out_cp = cp;
        return s + 3;
    }
    if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
        cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        if (cp < 0x10000 || cp > 0x10FFFF) cp = 0xFFFD;
        *out_cp = cp;
        return s + 4;
    }
    *out_cp = 0xFFFD;
    return s + 1;
}

/* =========================================================
   (2) アトラス詰め（Shelf packer）
   ========================================================= */

typedef struct DFA_ShelfPacker {
    int w, h;
    int x, y;
    int shelf_h;
} DFA_ShelfPacker;

static void dfa_packer_init(DFA_ShelfPacker* p, int w, int h) {
    p->w = w; p->h = h;
    p->x = 0; p->y = 0; p->shelf_h = 0;
}

static bool dfa_packer_alloc(DFA_ShelfPacker* p, int rw, int rh, int* out_x, int* out_y) {
    if (rw > p->w || rh > p->h) return false;

    if (p->x + rw > p->w) {
        p->x = 0;
        p->y += p->shelf_h;
        p->shelf_h = 0;
    }
    if (p->y + rh > p->h) return false;

    *out_x = p->x;
    *out_y = p->y;

    p->x += rw;
    if (rh > p->shelf_h) p->shelf_h = rh;
    return true;
}

/* =========================================================
   グリフキャッシュ（簡易ハッシュ）
   ========================================================= */

typedef enum DFA_GlyphState {
    DFA_GLYPH_EMPTY = 0,
    DFA_GLYPH_PENDING = 1,
    DFA_GLYPH_READY = 2,
    DFA_GLYPH_FAILED = 3
} DFA_GlyphState;

typedef struct DFA_Glyph {
    uint32_t codepoint;

    int page_index;
    SDL_Rect tex_rect;     /* 実際のグリフ領域（padding除外済み） */

    int minx, maxx, miny, maxy;
    int advance;

    DFA_GlyphState state;
} DFA_Glyph;

typedef struct DFA_MapEntry {
    uint32_t key;
    int value;      /* glyph index */
    bool used;
} DFA_MapEntry;

typedef struct DFA_GlyphMap {
    DFA_MapEntry* entries;
    int cap;
    int len;
} DFA_GlyphMap;

static void dfa_map_init(DFA_GlyphMap* m, int cap) {
    m->cap = cap;
    m->len = 0;
    m->entries = (DFA_MapEntry*)SDL_calloc((size_t)cap, sizeof(DFA_MapEntry));
}

static void dfa_map_destroy(DFA_GlyphMap* m) {
    SDL_free(m->entries);
    m->entries = NULL;
    m->cap = m->len = 0;
}

static int dfa_map_find_slot(DFA_GlyphMap* m, uint32_t key) {
    uint32_t h = dfa_fnv1a32(&key, sizeof(key));
    int mask = m->cap - 1;
    int idx = (int)(h & (uint32_t)mask);
    for (;;) {
        if (!m->entries[idx].used) return idx;
        if (m->entries[idx].key == key) return idx;
        idx = (idx + 1) & mask;
    }
}

static void dfa_map_grow(DFA_GlyphMap* m) {
    int oldcap = m->cap;
    DFA_MapEntry* old = m->entries;

    int newcap = oldcap * 2;
    DFA_MapEntry* ne = (DFA_MapEntry*)SDL_calloc((size_t)newcap, sizeof(DFA_MapEntry));

    m->entries = ne;
    m->cap = newcap;
    m->len = 0;

    for (int i = 0; i < oldcap; i++) {
        if (!old[i].used) continue;
        int slot = dfa_map_find_slot(m, old[i].key);
        m->entries[slot] = old[i];
        m->len++;
    }
    SDL_free(old);
}

static bool dfa_map_get(DFA_GlyphMap* m, uint32_t key, int* out_value) {
    int slot = dfa_map_find_slot(m, key);
    if (!m->entries[slot].used) return false;
    *out_value = m->entries[slot].value;
    return true;
}

static void dfa_map_put(DFA_GlyphMap* m, uint32_t key, int value) {
    if ((m->len + 1) * 10 >= m->cap * 7) { /* load factor ~0.7 */
        dfa_map_grow(m);
    }
    int slot = dfa_map_find_slot(m, key);
    if (!m->entries[slot].used) {
        m->entries[slot].used = true;
        m->entries[slot].key = key;
        m->entries[slot].value = value;
        m->len++;
    }
    else {
        m->entries[slot].value = value;
    }
}

/* =========================================================
   アトラスページ（GPU + CPUコピー）
   ========================================================= */

typedef struct DFA_AtlasPage {
    SDL_Texture* tex;
    uint8_t* pixels;     /* ARGB8888 のCPUコピー */
    int pitch;           /* bytes per row (w*4) */
    int w, h;
    DFA_ShelfPacker packer;
} DFA_AtlasPage;

/* =========================================================
   (1) フォント生成/グリフ生成スレッド
   ========================================================= */

typedef struct DFA_RasterResult {
    uint32_t codepoint;

    int minx, maxx, miny, maxy, advance;

    SDL_Surface* surface; /* ARGB8888（TTF_RenderGlyph_Blended）: メインスレッドがDestroyする */
    bool ok;
} DFA_RasterResult;

typedef struct DFA_U32Queue {
    uint32_t* buf;
    int cap, head, tail, count;
} DFA_U32Queue;

typedef struct DFA_ResultQueue {
    DFA_RasterResult* buf;
    int cap, head, tail, count;
} DFA_ResultQueue;

static void dfa_u32q_init(DFA_U32Queue* q, int cap) {
    q->cap = cap;
    q->head = q->tail = q->count = 0;
    q->buf = (uint32_t*)SDL_malloc((size_t)cap * sizeof(uint32_t));
}
static void dfa_u32q_destroy(DFA_U32Queue* q) {
    SDL_free(q->buf);
    memset(q, 0, sizeof(*q));
}
static bool dfa_u32q_push(DFA_U32Queue* q, uint32_t v) {
    if (q->count == q->cap) return false;
    q->buf[q->tail] = v;
    q->tail = (q->tail + 1) % q->cap;
    q->count++;
    return true;
}
static bool dfa_u32q_pop(DFA_U32Queue* q, uint32_t* out) {
    if (q->count == 0) return false;
    *out = q->buf[q->head];
    q->head = (q->head + 1) % q->cap;
    q->count--;
    return true;
}

static void dfa_rq_init(DFA_ResultQueue* q, int cap) {
    q->cap = cap;
    q->head = q->tail = q->count = 0;
    q->buf = (DFA_RasterResult*)SDL_calloc((size_t)cap, sizeof(DFA_RasterResult));
}
static void dfa_rq_destroy(DFA_ResultQueue* q) {
    /* surface はメインスレッドが所有するのでここでは触らない（stop時にdrainする） */
    SDL_free(q->buf);
    memset(q, 0, sizeof(*q));
}
static bool dfa_rq_push(DFA_ResultQueue* q, const DFA_RasterResult* v) {
    if (q->count == q->cap) return false;
    q->buf[q->tail] = *v;
    q->tail = (q->tail + 1) % q->cap;
    q->count++;
    return true;
}
static bool dfa_rq_pop(DFA_ResultQueue* q, DFA_RasterResult* out) {
    if (q->count == 0) return false;
    *out = q->buf[q->head];
    q->head = (q->head + 1) % q->cap;
    q->count--;
    return true;
}

typedef struct DFA_FontWorker {
    SDL_Thread* thread;

    SDL_mutex* mtx;
    SDL_cond* cv;

    bool quit;

    /* init handshake */
    bool init_done;
    bool init_ok;

    char* font_path;
    float ptsize;
    int style;

    /* worker-owned font */
    TTF_Font* font;
    int ascent, descent, lineskip;

    DFA_U32Queue req_q;
    DFA_ResultQueue res_q;
} DFA_FontWorker;

static int dfa_worker_main(void* ud) {
    DFA_FontWorker* w = (DFA_FontWorker*)ud;

    /* フォントをこのスレッドで生成 */
    w->font = TTF_OpenFont(w->font_path, w->ptsize);
    if (w->font) {
        TTF_SetFontStyle(w->font, w->style);
        w->ascent = TTF_FontAscent(w->font);
        w->descent = TTF_FontDescent(w->font);
        w->lineskip = TTF_FontLineSkip(w->font);
    }

    SDL_LockMutex(w->mtx);
    w->init_done = true;
    w->init_ok = (w->font != NULL);
    SDL_CondSignal(w->cv);
    SDL_UnlockMutex(w->mtx);

    if (!w->font) {
        return 0;
    }

    const SDL_Color white = { 255, 255, 255, 255 };

    for (;;) {
        SDL_LockMutex(w->mtx);
        while (!w->quit && w->req_q.count == 0) {
            SDL_CondWait(w->cv, w->mtx);
        }
        if (w->quit) {
            SDL_UnlockMutex(w->mtx);
            break;
        }

        uint32_t cp = 0;
        (void)dfa_u32q_pop(&w->req_q, &cp);
        SDL_UnlockMutex(w->mtx);

        DFA_RasterResult rr;
        memset(&rr, 0, sizeof(rr));
        rr.codepoint = cp;

        if (TTF_GlyphMetrics(w->font, cp, &rr.minx, &rr.maxx, &rr.miny, &rr.maxy, &rr.advance) != 0) {
            rr.ok = false;
            rr.surface = NULL;
        }
        else {
            //rr.surface = TTF_RenderGlyph_Blended(w->font, cp, white);
            //rr.ok = (rr.surface != NULL);
            /* ラスタライズ（このスレッドで） */
            SDL_Surface* s8 = TTF_RenderGlyph_Solid(w->font, cp, white);  /* ★Solid */
            if (!s8) {
                rr.ok = false;
                rr.surface = NULL;
            }
            else {
                /* ★8-bit(0=透過,1=白) → ARGB8888 に手動展開（滲み無し） */
                SDL_Surface* s32 = SDL_CreateRGBSurfaceWithFormat(0, s8->w, s8->h, 32, SDL_PIXELFORMAT_ARGB8888);
                if (!s32) {
                    SDL_FreeSurface(s8);
                    rr.ok = false;
                    rr.surface = NULL;
                }
                else {
                    const uint8_t* src = (const uint8_t*)s8->pixels;
                    uint32_t* dst = (uint32_t*)s32->pixels;
                    const int sp = s8->pitch;          /* bytes */
                    const int dp = s32->pitch / 4;     /* uint32_t */

                    for (int y = 0; y < s8->h; y++) {
                        const uint8_t* row = src + y * sp;
                        uint32_t* out = dst + y * dp;
                        for (int x = 0; x < s8->w; x++) {
                            out[x] = (row[x] == 0) ? 0x00000000u : 0xFFFFFFFFu; /* 透過 or 白 */
                        }
                    }

                    SDL_FreeSurface(s8);
                    rr.surface = s32;
                    rr.ok = true;
                }
            }

        }

        /* res_q が満杯なら surface をリークさせない */
        SDL_LockMutex(w->mtx);
        if (!dfa_rq_push(&w->res_q, &rr)) {
            SDL_UnlockMutex(w->mtx);
            if (rr.surface) SDL_FreeSurface(rr.surface);
        }
        else {
            SDL_CondSignal(w->cv);
            SDL_UnlockMutex(w->mtx);
        }
    }

    TTF_CloseFont(w->font);
    w->font = NULL;
    return 0;
}

static bool dfa_worker_start(DFA_FontWorker* w, const char* font_path, float ptsize, int style) {
    memset(w, 0, sizeof(*w));
    w->mtx = SDL_CreateMutex();
    w->cv = SDL_CreateCond();
    if (!w->mtx || !w->cv) return false;

    w->font_path = SDL_strdup(font_path);
    w->ptsize = ptsize;
    w->style = style;

    dfa_u32q_init(&w->req_q, 4096);
    dfa_rq_init(&w->res_q, 4096);

    w->thread = SDL_CreateThread(dfa_worker_main, "DFA_FontWorker", w);
    if (!w->thread) return false;

    SDL_LockMutex(w->mtx);
    while (!w->init_done) {
        SDL_CondWait(w->cv, w->mtx);
    }
    bool ok = w->init_ok;
    SDL_UnlockMutex(w->mtx);

    return ok;
}

static void dfa_worker_stop(DFA_FontWorker* w) {
    if (!w) return;

    SDL_LockMutex(w->mtx);
    w->quit = true;
    SDL_CondSignal(w->cv);
    SDL_UnlockMutex(w->mtx);

    if (w->thread) {
        SDL_WaitThread(w->thread, NULL);
        w->thread = NULL;
    }

    /* join後なら追加pushは来ないので残ったsurfaceを安全に破棄できる */
    {
        DFA_RasterResult rr;
        SDL_LockMutex(w->mtx);
        while (dfa_rq_pop(&w->res_q, &rr)) {
            if (rr.surface) SDL_FreeSurface(rr.surface);
        }
        SDL_UnlockMutex(w->mtx);
    }

    dfa_u32q_destroy(&w->req_q);
    dfa_rq_destroy(&w->res_q);

    SDL_free(w->font_path);
    w->font_path = NULL;

    if (w->cv) SDL_DestroyCond(w->cv);
    if (w->mtx) SDL_DestroyMutex(w->mtx);
    w->cv = NULL;
    w->mtx = NULL;
}

/* =========================================================
   (4) 永続キャッシュ
   ========================================================= */

#define DFA_META_MAGIC 0x31414644u /* 'DFA1' */
#define DFA_META_VERSION 1

typedef struct DFA_MetaHeader {
    uint32_t magic;
    uint32_t version;

    uint32_t font_path_hash;
    float ptsize;
    uint32_t style;

    int32_t atlas_w, atlas_h;
    int32_t padding_px;
    uint32_t extrude;

    int32_t ascent, descent, lineskip;

    uint32_t page_count;
    uint32_t glyph_count;
} DFA_MetaHeader;

typedef struct DFA_MetaPage {
    int32_t pack_x, pack_y, pack_shelf_h;
} DFA_MetaPage;

typedef struct DFA_MetaGlyph {
    uint32_t codepoint;
    int32_t page_index;
    int32_t x, y, w, h;
    int32_t minx, maxx, miny, maxy, advance;
    uint32_t state; /* READY だけ保存 */
} DFA_MetaGlyph;

/* =========================================================
   FontAtlas 本体（外部には晒さない）
   ========================================================= */

typedef struct DFA_FontAtlas {
    SDL_Renderer* renderer;

    char* font_path;
    float ptsize;
    int style;

    int atlas_w, atlas_h;
    int padding_px;
    bool extrude;

    char* cache_dir; /* NULL可 */
    uint32_t cache_key_hash;

    /* metrics */
    int ascent, descent, lineskip;

    DFA_FontWorker worker;

    DFA_AtlasPage* pages;
    int page_count;
    int page_cap;

    DFA_Glyph* glyphs;
    int glyph_count;
    int glyph_cap;

    DFA_GlyphMap map;
} DFA_FontAtlas;

/* =========================================================
   Atlasページ生成/破棄
   ========================================================= */

static bool dfa_page_create(DFA_FontAtlas* a, DFA_AtlasPage* p) {
    memset(p, 0, sizeof(*p));
    p->w = a->atlas_w;
    p->h = a->atlas_h;
    p->pitch = p->w * 4;

    p->pixels = (uint8_t*)SDL_calloc((size_t)p->pitch * (size_t)p->h, 1);
    if (!p->pixels) return false;

    p->tex = SDL_CreateTexture(a->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, p->w, p->h);
    if (!p->tex) return false;

    (void)SDL_SetTextureBlendMode(p->tex, SDL_BLENDMODE_BLEND);

    /* 重要：ここで ScaleMode を固定しない（呼び出し側の Default を生かす）
       (void)SDL_SetTextureScaleMode(p->tex, SDL_SCALEMODE_LINEAR);
     */

    dfa_packer_init(&p->packer, p->w, p->h);

    SDL_Rect r = { 0, 0, p->w, p->h };
    (void)SDL_UpdateTexture(p->tex, &r, p->pixels, p->pitch);

    return true;
}

static void dfa_page_destroy(DFA_AtlasPage* p) {
    if (p->tex) SDL_DestroyTexture(p->tex);
    SDL_free(p->pixels);
    memset(p, 0, sizeof(*p));
}

static DFA_AtlasPage* dfa_get_or_create_page_for(DFA_FontAtlas* a, int rw, int rh, int* out_px, int* out_py, int* out_page_index) {
    for (int i = 0; i < a->page_count; i++) {
        int x, y;
        if (dfa_packer_alloc(&a->pages[i].packer, rw, rh, &x, &y)) {
            *out_px = x; *out_py = y; *out_page_index = i;
            return &a->pages[i];
        }
    }

    if (a->page_count == a->page_cap) {
        int newcap = (a->page_cap == 0) ? 4 : a->page_cap * 2;
        DFA_AtlasPage* np = (DFA_AtlasPage*)SDL_realloc(a->pages, (size_t)newcap * sizeof(DFA_AtlasPage));
        if (!np) return NULL;
        a->pages = np;
        a->page_cap = newcap;
    }

    DFA_AtlasPage* p = &a->pages[a->page_count];
    if (!dfa_page_create(a, p)) return NULL;
    a->page_count++;

    int x, y;
    if (!dfa_packer_alloc(&p->packer, rw, rh, &x, &y)) return NULL;
    *out_px = x; *out_py = y; *out_page_index = a->page_count - 1;
    return p;
}

/* グリフ配列確保 */
static int dfa_glyph_new(DFA_FontAtlas* a, uint32_t cp) {
    if (a->glyph_count == a->glyph_cap) {
        int newcap = (a->glyph_cap == 0) ? 1024 : a->glyph_cap * 2;
        DFA_Glyph* ng = (DFA_Glyph*)SDL_realloc(a->glyphs, (size_t)newcap * sizeof(DFA_Glyph));
        if (!ng) return -1;
        a->glyphs = ng;
        a->glyph_cap = newcap;
    }
    int idx = a->glyph_count++;
    DFA_Glyph* g = &a->glyphs[idx];
    memset(g, 0, sizeof(*g));
    g->codepoint = cp;
    g->page_index = -1;
    g->state = DFA_GLYPH_EMPTY;
    return idx;
}

/* padding + extrude で CPUバッファへコピー */
static void dfa_blit_with_padding(
    DFA_FontAtlas* a,
    DFA_AtlasPage* p,
    int alloc_x, int alloc_y, int alloc_w, int alloc_h,
    SDL_Surface* surf
) {
    const int pad = a->padding_px;
    const int gx = alloc_x + pad;
    const int gy = alloc_y + pad;
    const int gw = surf->w;
    const int gh = surf->h;

    const uint8_t* src = (const uint8_t*)surf->pixels;
    uint8_t* dst = p->pixels;

    for (int y = 0; y < gh; y++) {
        memcpy(
            dst + (gy + y) * p->pitch + gx * 4,
            src + y * surf->pitch,
            (size_t)gw * 4
        );
    }

    if (!a->extrude || pad <= 0) return;

    const int ex = 1;

    if (gy - ex >= 0) {
        memcpy(dst + (gy - ex) * p->pitch + gx * 4,
            dst + (gy)*p->pitch + gx * 4,
            (size_t)gw * 4);
    }
    if (gy + gh < p->h) {
        memcpy(dst + (gy + gh) * p->pitch + gx * 4,
            dst + (gy + gh - 1) * p->pitch + gx * 4,
            (size_t)gw * 4);
    }

    for (int y = 0; y < gh; y++) {
        if (gx - ex >= 0) {
            memcpy(dst + (gy + y) * p->pitch + (gx - ex) * 4,
                dst + (gy + y) * p->pitch + (gx) * 4,
                4);
        }
        if (gx + gw < p->w) {
            memcpy(dst + (gy + y) * p->pitch + (gx + gw) * 4,
                dst + (gy + y) * p->pitch + (gx + gw - 1) * 4,
                4);
        }
    }

    if (gx - ex >= 0 && gy - ex >= 0) {
        memcpy(dst + (gy - ex) * p->pitch + (gx - ex) * 4,
            dst + (gy)*p->pitch + (gx) * 4, 4);
    }
    if (gx + gw < p->w && gy - ex >= 0) {
        memcpy(dst + (gy - ex) * p->pitch + (gx + gw) * 4,
            dst + (gy)*p->pitch + (gx + gw - 1) * 4, 4);
    }
    if (gx - ex >= 0 && gy + gh < p->h) {
        memcpy(dst + (gy + gh) * p->pitch + (gx - ex) * 4,
            dst + (gy + gh - 1) * p->pitch + (gx) * 4, 4);
    }
    if (gx + gw < p->w && gy + gh < p->h) {
        memcpy(dst + (gy + gh) * p->pitch + (gx + gw) * 4,
            dst + (gy + gh - 1) * p->pitch + (gx + gw - 1) * 4, 4);
    }

    (void)alloc_w; (void)alloc_h;
}

static bool dfa_try_load_cache(DFA_FontAtlas* a) {
    if (!a->cache_dir) return false;

    char meta_path[1024];
    SDL_snprintf(meta_path, sizeof(meta_path), "%s/meta.bin", a->cache_dir);

    SDL_RWops* io = SDL_RWFromFile(meta_path, "rb");
    if (!io) return false;

    DFA_MetaHeader mh;
    if (SDL_RWread(io, &mh, 1, sizeof(mh)) != sizeof(mh)) { SDL_RWclose(io); return false; }
    if (mh.magic != DFA_META_MAGIC || mh.version != DFA_META_VERSION) { SDL_RWclose(io); return false; }

    if (mh.font_path_hash != a->cache_key_hash ||
        mh.ptsize != a->ptsize ||
        mh.style != (uint32_t)a->style ||
        mh.atlas_w != a->atlas_w ||
        mh.atlas_h != a->atlas_h ||
        mh.padding_px != a->padding_px ||
        (mh.extrude != 0) != a->extrude) {
        SDL_RWclose(io);
        return false;
    }

    a->ascent = mh.ascent;
    a->descent = mh.descent;
    a->lineskip = mh.lineskip;

    a->pages = (DFA_AtlasPage*)SDL_calloc((size_t)mh.page_count, sizeof(DFA_AtlasPage));
    a->page_cap = (int)mh.page_count;
    a->page_count = 0;

    DFA_MetaPage* mp = (DFA_MetaPage*)SDL_malloc((size_t)mh.page_count * sizeof(DFA_MetaPage));
    if (!mp) { SDL_RWclose(io); return false; }
    if (SDL_RWread(io, mp, 1, (size_t)mh.page_count * sizeof(DFA_MetaPage)) != (size_t)mh.page_count * sizeof(DFA_MetaPage)) {
        SDL_free(mp); SDL_RWclose(io); return false;
    }

    for (uint32_t i = 0; i < mh.page_count; i++) {
        DFA_AtlasPage* p = &a->pages[i];
        if (!dfa_page_create(a, p)) { SDL_free(mp); SDL_RWclose(io); return false; }

        char png_path[1024];
        SDL_snprintf(png_path, sizeof(png_path), "%s/atlas_%03u.png", a->cache_dir, i);
        SDL_Surface* s = IMG_Load(png_path);
        if (!s) { SDL_free(mp); SDL_RWclose(io); return false; }

        SDL_Surface* cvt = s;
        if (s->format->format != SDL_PIXELFORMAT_ARGB8888) {
            cvt = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ARGB8888, 0);
            SDL_FreeSurface(s);
            if (!cvt) { SDL_free(mp); SDL_RWclose(io); return false; }
        }

        if (cvt->w != a->atlas_w || cvt->h != a->atlas_h) {
            SDL_FreeSurface(cvt);
            SDL_free(mp); SDL_RWclose(io);
            return false;
        }

        memcpy(p->pixels, cvt->pixels, (size_t)p->pitch * (size_t)p->h);
        SDL_FreeSurface(cvt);

        SDL_Rect r = { 0, 0, p->w, p->h };
        (void)SDL_UpdateTexture(p->tex, &r, p->pixels, p->pitch);

        p->packer.x = mp[i].pack_x;
        p->packer.y = mp[i].pack_y;
        p->packer.shelf_h = mp[i].pack_shelf_h;

        a->page_count++;
    }
    SDL_free(mp);

    dfa_map_init(&a->map, 2048);
    a->glyphs = NULL; a->glyph_cap = 0; a->glyph_count = 0;

    for (uint32_t gi = 0; gi < mh.glyph_count; gi++) {
        DFA_MetaGlyph mg;
        if (SDL_RWread(io, &mg, 1, sizeof(mg)) != sizeof(mg)) { SDL_RWclose(io); return false; }

        int idx = dfa_glyph_new(a, mg.codepoint);
        if (idx < 0) { SDL_RWclose(io); return false; }

        DFA_Glyph* g = &a->glyphs[idx];
        g->codepoint = mg.codepoint;
        g->page_index = mg.page_index;
        g->tex_rect.x = mg.x; g->tex_rect.y = mg.y; g->tex_rect.w = mg.w; g->tex_rect.h = mg.h;
        g->minx = mg.minx; g->maxx = mg.maxx; g->miny = mg.miny; g->maxy = mg.maxy; g->advance = mg.advance;
        g->state = (DFA_GlyphState)mg.state;

        dfa_map_put(&a->map, g->codepoint, idx);
    }

    SDL_RWclose(io);
    return true;
}

static bool dfa_save_cache(DFA_FontAtlas* a) {
    if (!a || !a->cache_dir) return false;

    for (int i = 0; i < a->page_count; i++) {
        char png_path[1024];
        SDL_snprintf(png_path, sizeof(png_path), "%s/atlas_%03d.png", a->cache_dir, i);

        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(
            a->pages[i].pixels,
            a->atlas_w,
            a->atlas_h,
            32,
            a->pages[i].pitch,
            SDL_PIXELFORMAT_ARGB8888);
        if (!s) return false;

        if (IMG_SavePNG(s, png_path) != 0) {
            SDL_FreeSurface(s);
            return false;
        }
        SDL_FreeSurface(s);
    }

    char meta_path[1024];
    SDL_snprintf(meta_path, sizeof(meta_path), "%s/meta.bin", a->cache_dir);

    SDL_RWops* io = SDL_RWFromFile(meta_path, "wb");
    if (!io) return false;

    DFA_MetaHeader mh;
    memset(&mh, 0, sizeof(mh));
    mh.magic = DFA_META_MAGIC;
    mh.version = DFA_META_VERSION;

    mh.font_path_hash = a->cache_key_hash;
    mh.ptsize = a->ptsize;
    mh.style = (uint32_t)a->style;

    mh.atlas_w = a->atlas_w;
    mh.atlas_h = a->atlas_h;
    mh.padding_px = a->padding_px;
    mh.extrude = a->extrude ? 1u : 0u;

    mh.ascent = a->ascent;
    mh.descent = a->descent;
    mh.lineskip = a->lineskip;

    mh.page_count = (uint32_t)a->page_count;

    uint32_t ready_count = 0;
    for (int i = 0; i < a->glyph_count; i++) {
        if (a->glyphs[i].state == DFA_GLYPH_READY) ready_count++;
    }
    mh.glyph_count = ready_count;

    if (SDL_RWwrite(io, &mh, 1, sizeof(mh)) != sizeof(mh)) { SDL_RWclose(io); return false; }

    for (int i = 0; i < a->page_count; i++) {
        DFA_MetaPage mp;
        mp.pack_x = a->pages[i].packer.x;
        mp.pack_y = a->pages[i].packer.y;
        mp.pack_shelf_h = a->pages[i].packer.shelf_h;
        if (SDL_RWwrite(io, &mp, 1, sizeof(mp)) != sizeof(mp)) { SDL_RWclose(io); return false; }
    }

    for (int i = 0; i < a->glyph_count; i++) {
        const DFA_Glyph* g = &a->glyphs[i];
        if (g->state != DFA_GLYPH_READY) continue;

        DFA_MetaGlyph mg;
        memset(&mg, 0, sizeof(mg));
        mg.codepoint = g->codepoint;
        mg.page_index = g->page_index;
        mg.x = g->tex_rect.x; mg.y = g->tex_rect.y; mg.w = g->tex_rect.w; mg.h = g->tex_rect.h;
        mg.minx = g->minx; mg.maxx = g->maxx; mg.miny = g->miny; mg.maxy = g->maxy; mg.advance = g->advance;
        mg.state = (uint32_t)g->state;

        if (SDL_RWwrite(io, &mg, 1, sizeof(mg)) != sizeof(mg)) { SDL_RWclose(io); return false; }
    }

    SDL_RWclose(io);
    return true;
}

/* =========================================================
   (3) DrawList（ページごとにbatch）
   ========================================================= */

typedef struct DFA_PageDraw {
    SDL_Vertex* v;
    int vlen, vcap;

    int* i;
    int ilen, icap;
} DFA_PageDraw;

typedef struct DFA_DrawList {
    DFA_PageDraw* pages;
    int page_count;
} DFA_DrawList;

static void dfa_pagedraw_reset(DFA_PageDraw* pd) {
    pd->vlen = 0;
    pd->ilen = 0;
}
static void dfa_pagedraw_reserve(DFA_PageDraw* pd, int add_v, int add_i) {
    if (pd->vlen + add_v > pd->vcap) {
        int nc = (pd->vcap == 0) ? 1024 : pd->vcap * 2;
        while (nc < pd->vlen + add_v) nc *= 2;
        pd->v = (SDL_Vertex*)SDL_realloc(pd->v, (size_t)nc * sizeof(SDL_Vertex));
        pd->vcap = nc;
    }
    if (pd->ilen + add_i > pd->icap) {
        int nc = (pd->icap == 0) ? 2048 : pd->icap * 2;
        while (nc < pd->ilen + add_i) nc *= 2;
        pd->i = (int*)SDL_realloc(pd->i, (size_t)nc * sizeof(int));
        pd->icap = nc;
    }
}

static bool dfa_drawlist_begin(DFA_DrawList** out_list, DFA_FontAtlas* atlas) {
    if (!out_list || !atlas) return false;
    DFA_DrawList* l = (DFA_DrawList*)SDL_calloc(1, sizeof(DFA_DrawList));
    if (!l) return false;

    l->pages = (DFA_PageDraw*)SDL_calloc((size_t)atlas->page_count, sizeof(DFA_PageDraw));
    l->page_count = atlas->page_count;

    *out_list = l;
    return true;
}

static void dfa_drawlist_reset(DFA_DrawList* list, DFA_FontAtlas* atlas) {
    if (!list || !atlas) return;

    if (list->page_count < atlas->page_count) {
        DFA_PageDraw* np = (DFA_PageDraw*)SDL_realloc(list->pages, (size_t)atlas->page_count * sizeof(DFA_PageDraw));
        if (!np) return;
        memset(np + list->page_count, 0, (size_t)(atlas->page_count - list->page_count) * sizeof(DFA_PageDraw));
        list->pages = np;
        list->page_count = atlas->page_count;
    }

    for (int i = 0; i < list->page_count; i++) {
        dfa_pagedraw_reset(&list->pages[i]);
    }
}

static void dfa_drawlist_flush(DFA_DrawList* list, DFA_FontAtlas* atlas) {
    if (!list || !atlas || !atlas->renderer) return;

    for (int i = 0; i < list->page_count && i < atlas->page_count; i++) {
        DFA_PageDraw* pd = &list->pages[i];
        if (pd->ilen == 0 || pd->vlen == 0) continue;

        (void)SDL_RenderGeometry(atlas->renderer, atlas->pages[i].tex, pd->v, pd->vlen, pd->i, pd->ilen);
    }
}

static void dfa_drawlist_destroy(DFA_DrawList* list) {
    if (!list) return;
    for (int i = 0; i < list->page_count; i++) {
        SDL_free(list->pages[i].v);
        SDL_free(list->pages[i].i);
    }
    SDL_free(list->pages);
    SDL_free(list);
}

/* =========================================================
   Atlas create/destroy/pump（旧公開APIを内部化）
   ========================================================= */

static void dfa_atlas_destroy(DFA_FontAtlas* a);

static bool dfa_atlas_create(
    DFA_FontAtlas** out_atlas,
    SDL_Renderer* renderer,
    const char* font_path,
    float ptsize,
    int style,
    int atlas_w,
    int atlas_h,
    int padding_px,
    bool extrude,
    const char* cache_dir
) {
    if (!out_atlas || !renderer || !font_path) return false;

    DFA_FontAtlas* a = (DFA_FontAtlas*)SDL_calloc(1, sizeof(DFA_FontAtlas));
    if (!a) return false;

    a->renderer = renderer;
    a->font_path = SDL_strdup(font_path);
    a->ptsize = ptsize;
    a->style = style;

    a->atlas_w = atlas_w;
    a->atlas_h = atlas_h;
    a->padding_px = padding_px;
    a->extrude = extrude;

    a->cache_dir = cache_dir ? SDL_strdup(cache_dir) : NULL;
    a->cache_key_hash = dfa_hash_str(font_path);

    if (!dfa_worker_start(&a->worker, font_path, ptsize, style)) {
        dfa_atlas_destroy(a);
        return false;
    }
    a->ascent = a->worker.ascent;
    a->descent = a->worker.descent;
    a->lineskip = a->worker.lineskip;

    bool loaded = dfa_try_load_cache(a);
    if (!loaded) {
        dfa_map_init(&a->map, 2048);

        a->pages = NULL; a->page_count = 0; a->page_cap = 0;
        a->glyphs = NULL; a->glyph_count = 0; a->glyph_cap = 0;

        a->page_cap = 4;
        a->pages = (DFA_AtlasPage*)SDL_calloc((size_t)a->page_cap, sizeof(DFA_AtlasPage));
        if (!a->pages || !dfa_page_create(a, &a->pages[0])) {
            dfa_atlas_destroy(a);
            return false;
        }
        a->page_count = 1;
    }

    *out_atlas = a;
    return true;
}

static void dfa_atlas_destroy(DFA_FontAtlas* a) {
    if (!a) return;

    dfa_worker_stop(&a->worker);

    for (int i = 0; i < a->page_count; i++) {
        dfa_page_destroy(&a->pages[i]);
    }
    SDL_free(a->pages);

    dfa_map_destroy(&a->map);
    SDL_free(a->glyphs);

    SDL_free(a->font_path);
    SDL_free(a->cache_dir);
    SDL_free(a);
}

/* worker結果を取り込み → packしてGPUへ */
static void dfa_atlas_pump(DFA_FontAtlas* a, int max_items) {
    if (!a) return;
    if (max_items <= 0) max_items = 999999;

    for (int n = 0; n < max_items; n++) {
        DFA_RasterResult rr;
        bool has = false;

        SDL_LockMutex(a->worker.mtx);
        has = dfa_rq_pop(&a->worker.res_q, &rr);
        SDL_UnlockMutex(a->worker.mtx);

        if (!has) break;

        int idx;
        if (!dfa_map_get(&a->map, rr.codepoint, &idx)) {
            if (rr.surface) SDL_FreeSurface(rr.surface);
            continue;
        }

        DFA_Glyph* g = &a->glyphs[idx];
        if (!rr.ok || !rr.surface) {
            g->state = DFA_GLYPH_FAILED;
            if (rr.surface) SDL_FreeSurface(rr.surface);
            continue;
        }

        const int pad = a->padding_px;
        const int rw = rr.surface->w + pad * 2;
        const int rh = rr.surface->h + pad * 2;

        int alloc_x, alloc_y, page_index;
        DFA_AtlasPage* page = dfa_get_or_create_page_for(a, rw, rh, &alloc_x, &alloc_y, &page_index);
        if (!page) {
            g->state = DFA_GLYPH_FAILED;
            SDL_FreeSurface(rr.surface);
            continue;
        }

        dfa_blit_with_padding(a, page, alloc_x, alloc_y, rw, rh, rr.surface);

        SDL_Rect upd = { alloc_x, alloc_y, rw, rh };
        const uint8_t* src = page->pixels + alloc_y * page->pitch + alloc_x * 4;
        (void)SDL_UpdateTexture(page->tex, &upd, src, page->pitch);

        g->page_index = page_index;
        g->tex_rect.x = alloc_x + pad;
        g->tex_rect.y = alloc_y + pad;
        g->tex_rect.w = rr.surface->w;
        g->tex_rect.h = rr.surface->h;

        g->minx = rr.minx; g->maxx = rr.maxx; g->miny = rr.miny; g->maxy = rr.maxy; g->advance = rr.advance;
        g->state = DFA_GLYPH_READY;

        SDL_FreeSurface(rr.surface);
    }
}

/* =========================================================
   文字要求/描画（va_list内部化）
   ========================================================= */

static void dfa_request_text_str(DFA_FontAtlas* atlas, const char* text)
{
    if (!atlas || !text || text[0] == '\0') return;

    uint32_t cp = 0;
    const char* p = text;
    while (*p) {
        p = dfa_utf8_next(p, &cp);
        if (cp == 0) break;
        if (cp == '\n' || cp == '\r' || cp == '\t') continue;

        int idx;
        if (!dfa_map_get(&atlas->map, cp, &idx)) {
            idx = dfa_glyph_new(atlas, cp);
            if (idx < 0) continue;

            atlas->glyphs[idx].state = DFA_GLYPH_PENDING;
            dfa_map_put(&atlas->map, cp, idx);

            SDL_LockMutex(atlas->worker.mtx);
            (void)dfa_u32q_push(&atlas->worker.req_q, cp);
            SDL_CondSignal(atlas->worker.cv);
            SDL_UnlockMutex(atlas->worker.mtx);
        }
    }
}

static bool dfa_draw_text_str(
    DFA_DrawList* list,
    DFA_FontAtlas* atlas,
    float x,
    float y_baseline,
    float scale,
    SDL_Color color,
    const char* text
) {
    if (!atlas || !list || !text || text[0] == '\0') return false;
    if (scale <= 0.0f) scale = 1.0f;

    bool complete = true;

    float pen_x = x;
    float pen_y = y_baseline;

    uint32_t cp = 0;
    uint32_t prev = 0;

    const char* p = text;
    while (*p) {
        p = dfa_utf8_next(p, &cp);
        if (cp == 0) break;

        if (cp == '\n') {
            pen_x = x;
            pen_y += (float)atlas->lineskip * scale;
            prev = 0;
            continue;
        }
        if (cp == '\r' || cp == '\t') continue;

        int idx;
        if (!dfa_map_get(&atlas->map, cp, &idx)) {
            complete = false;

            int ni = dfa_glyph_new(atlas, cp);
            if (ni >= 0) {
                atlas->glyphs[ni].state = DFA_GLYPH_PENDING;
                dfa_map_put(&atlas->map, cp, ni);

                SDL_LockMutex(atlas->worker.mtx);
                (void)dfa_u32q_push(&atlas->worker.req_q, cp);
                SDL_CondSignal(atlas->worker.cv);
                SDL_UnlockMutex(atlas->worker.mtx);
            }
            continue;
        }

        DFA_Glyph* g = &atlas->glyphs[idx];
        if (g->state != DFA_GLYPH_READY) {
            complete = false;
            continue;
        }

        /* kerningは安全側で0 */
        int kern = 0;
        pen_x += (float)kern * scale;

        const float bearingX = (float)g->minx * scale;
        float gx = pen_x/* + bearingX*/;

        /* 現状維持：gy = pen_y */
        float gy = pen_y;

        float gw = (float)g->tex_rect.w * scale;
        float gh = (float)g->tex_rect.h * scale;

        int page_index = g->page_index;
        if (page_index < 0 || page_index >= list->page_count) {
            complete = false;
            pen_x += (float)g->advance * scale;
            prev = cp;
            continue;
        }

        DFA_PageDraw* pd = &list->pages[page_index];
        dfa_pagedraw_reserve(pd, 4, 6);

        const float u0 = (float)g->tex_rect.x / (float)atlas->atlas_w;
        const float v0 = (float)g->tex_rect.y / (float)atlas->atlas_h;
        const float u1 = (float)(g->tex_rect.x + g->tex_rect.w) / (float)atlas->atlas_w;
        const float v1 = (float)(g->tex_rect.y + g->tex_rect.h) / (float)atlas->atlas_h;

        int base = pd->vlen;

        pd->v[pd->vlen++] = (SDL_Vertex){ .position = { gx,      gy      }, .color = color, .tex_coord = { u0, v0 } };
        pd->v[pd->vlen++] = (SDL_Vertex){ .position = { gx + gw, gy      }, .color = color, .tex_coord = { u1, v0 } };
        pd->v[pd->vlen++] = (SDL_Vertex){ .position = { gx + gw, gy + gh }, .color = color, .tex_coord = { u1, v1 } };
        pd->v[pd->vlen++] = (SDL_Vertex){ .position = { gx,      gy + gh }, .color = color, .tex_coord = { u0, v1 } };

        pd->i[pd->ilen++] = base + 0;
        pd->i[pd->ilen++] = base + 1;
        pd->i[pd->ilen++] = base + 2;
        pd->i[pd->ilen++] = base + 0;
        pd->i[pd->ilen++] = base + 2;
        pd->i[pd->ilen++] = base + 3;

        pen_x += (float)g->advance * scale;
        prev = cp;
    }

    return complete;
}


/* =========================================================
   フォント・マネージャ（staticグローバル）
   ========================================================= */


typedef struct DFA_LineInfo {
    int start;      /* byte index */
    int end;        /* byte index (exclusive) */
    float min_x;    /* 行の可視境界(min) */
    float max_x;    /* 行の可視境界(max) */
} DFA_LineInfo;


#define DFA_NAME_MAX 64

typedef struct DFA_FontEntry {
    bool used;
    DFA_FontID id;
    char name[DFA_NAME_MAX];

    DFA_FontAtlas* atlas;
    DFA_DrawList* list;

    /* ★追加：フォーマット済み文字列の再利用バッファ */
    char* fmt_buf;
    size_t fmt_cap;
    DFA_LineInfo* lines;
    int lines_cap;
} DFA_FontEntry;

static struct {
    DFA_FontEntry* items;
    int count;
    int cap;
    DFA_FontID next_id;
} g_dfa = { 0 };

static DFA_FontEntry* dfa_find_by_id(DFA_FontID id) {
    if (id == DFA_INVALID_FONT_ID) return NULL;
    for (int i = 0; i < g_dfa.count; i++) {
        if (g_dfa.items[i].used && g_dfa.items[i].id == id) return &g_dfa.items[i];
    }
    return NULL;
}

static DFA_FontEntry* dfa_find_by_name(const char* name) {
    if (!name || !name[0]) return NULL;
    for (int i = 0; i < g_dfa.count; i++) {
        if (!g_dfa.items[i].used) continue;
        if (SDL_strcmp(g_dfa.items[i].name, name) == 0) return &g_dfa.items[i];
    }
    return NULL;
}

static DFA_FontEntry* dfa_alloc_entry(void) {
    if (g_dfa.count == g_dfa.cap) {
        int newcap = (g_dfa.cap == 0) ? 8 : g_dfa.cap * 2;
        DFA_FontEntry* np = (DFA_FontEntry*)SDL_realloc(g_dfa.items, (size_t)newcap * sizeof(DFA_FontEntry));
        if (!np) return NULL;
        SDL_memset(np + g_dfa.cap, 0, (size_t)(newcap - g_dfa.cap) * sizeof(DFA_FontEntry));
        g_dfa.items = np;
        g_dfa.cap = newcap;
    }
    DFA_FontEntry* e = &g_dfa.items[g_dfa.count++];
    SDL_memset(e, 0, sizeof(*e));
    e->used = true;
    return e;
}

#ifndef va_copy
#define va_copy(dst, src) ((dst) = (src))
#endif

static const char* dfa_vformat_reuse(char** io_buf, size_t* io_cap, const char* fmt, va_list ap)
{
    if (!io_buf || !io_cap || !fmt) return NULL;

    /* まず必要サイズ計測（C99ならOK。古い実装は -1 を返す場合あり） */
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);

    if (needed >= 0) {
        size_t req = (size_t)needed + 1;

        if (*io_cap < req) {
            size_t ncap = (*io_cap > 0) ? *io_cap : 256;
            while (ncap < req) ncap *= 2;

            char* nb = (char*)SDL_realloc(*io_buf, ncap);
            if (!nb) return NULL;
            *io_buf = nb;
            *io_cap = ncap;
        }

        va_list ap3;
        va_copy(ap3, ap);
        vsnprintf(*io_buf, *io_cap, fmt, ap3);
        va_end(ap3);

        return *io_buf;
    }

    /* フォールバック：サイズ不明（needed<0）なら、試行で拡張 */
    size_t ncap = (*io_cap > 0) ? *io_cap : 256;

    for (int guard = 0; guard < 20; guard++) { /* 最大 256 * 2^20 ≒ 256MB まで（安全ガード） */
        char* nb = (char*)SDL_realloc(*io_buf, ncap);
        if (!nb) return NULL;
        *io_buf = nb;
        *io_cap = ncap;

        va_list ap4;
        va_copy(ap4, ap);
        int wrote = vsnprintf(*io_buf, *io_cap, fmt, ap4);
        va_end(ap4);

        if (wrote >= 0 && (size_t)wrote < *io_cap) {
            return *io_buf; /* 収まった */
        }

        /* 収まらない：必要サイズが分かるならそれに合わせて拡張、分からないなら倍々 */
        if (wrote >= 0) {
            ncap = (size_t)wrote + 1;
        }
        else {
            ncap *= 2;
        }
    }

    return NULL;
}
static inline float dfa_fallback_advance_px(const DFA_FontAtlas* a, float scale)
{
    /* 未生成グリフの幅推定（大雑把）。行折り返しの暴れを軽減する目的 */
    return (float)a->ptsize * 0.5f * scale;
}

static bool dfa_is_breakable_space(uint32_t cp)
{
    return (cp == 0x20u /* space */) || (cp == 0x3000u /* ideographic space */);
}

/* text: UTF-8（len bytes）。layoutに応じて行分割し、linesに結果を入れる。
   返り値: 行数。out_complete=false なら未生成グリフが含まれる等で“不完全”。 */
static int dfa_layout_lines(
    DFA_FontAtlas* atlas,
    const char* text,
    int len,
    float scale,
    const DFA_TextLayout* layout,
    DFA_LineInfo** io_lines,
    int* io_cap,
    bool* out_complete
) {
    if (out_complete) *out_complete = true;
    if (!atlas || !text || len <= 0) return 0;

    const float maxw = (layout && layout->max_width_px > 0.0f) ? layout->max_width_px : 0.0f;
    const DFA_WrapMode wrap = layout ? layout->wrap : DFA_WRAP_NONE;

    int line_cap = (*io_cap > 0) ? *io_cap : 16;
    if (!*io_lines) {
        *io_lines = (DFA_LineInfo*)SDL_malloc((size_t)line_cap * sizeof(DFA_LineInfo));
        if (!*io_lines) return 0;
        *io_cap = line_cap;
    }

    int line_count = 0;

    int line_start = 0;
    int cur = 0;

    /* 行の“可視境界”計測 */
    float pen = 0.0f;
    float min_x = 0.0f, max_x = 0.0f;
    bool has_any = false;

    /* word wrap 用 */
    int last_break_pos = -1;        /* 改行候補のbyte（break文字の開始） */
    float pen_at_break = 0.0f;
    float min_at_break = 0.0f, max_at_break = 0.0f;
    bool has_break = false;

    while (cur < len) {
        const char* p = text + cur;
        uint32_t cp = 0;
        const char* np = dfa_utf8_next(p, &cp);
        int next = (int)(np - text);
        if (next <= cur) break;

        /* 明示改行 */
        if (cp == '\n') {
            /* 行確定 */
            if (line_count == *io_cap) {
                int nc = (*io_cap) * 2;
                DFA_LineInfo* nl = (DFA_LineInfo*)SDL_realloc(*io_lines, (size_t)nc * sizeof(DFA_LineInfo));
                if (!nl) break;
                *io_lines = nl;
                *io_cap = nc;
            }
            (*io_lines)[line_count++] = (DFA_LineInfo){
                .start = line_start,
                .end = cur,
                .min_x = has_any ? min_x : 0.0f,
                .max_x = has_any ? max_x : 0.0f
            };

            /* 次行初期化 */
            line_start = next;
            cur = next;
            pen = 0.0f; min_x = 0.0f; max_x = 0.0f; has_any = false;
            last_break_pos = -1; has_break = false;
            continue;
        }

        if (cp == '\r' || cp == '\t') {
            cur = next;
            continue;
        }

        /* 幅計算：glyphがREADYなら advance/bounds、なければfallback */
        float adv = 0.0f;
        float gmin = 0.0f, gmax = 0.0f;

        int idx;
        if (dfa_map_get(&atlas->map, cp, &idx)) {
            DFA_Glyph* g = &atlas->glyphs[idx];
            if (g->state == DFA_GLYPH_READY) {
                adv = (float)g->advance * scale;
                gmin = pen + (float)g->minx * scale;
                gmax = pen + (float)g->maxx * scale;
            }
            else {
                if (out_complete) *out_complete = false;
                adv = dfa_fallback_advance_px(atlas, scale);
                gmin = pen;
                gmax = pen + adv;
            }
        }
        else {
            if (out_complete) *out_complete = false;
            adv = dfa_fallback_advance_px(atlas, scale);
            gmin = pen;
            gmax = pen + adv;
        }

        /* word wrap の“候補”更新（スペースで切る） */
        if (wrap == DFA_WRAP_WORD && dfa_is_breakable_space(cp)) {
            last_break_pos = cur; /* space開始 */
            pen_at_break = pen;   /* space手前までの幅 */
            min_at_break = min_x;
            max_at_break = max_x;
            has_break = true;
        }

        /* ここで追加すると超える？（maxw>0 のときのみ） */
        if (maxw > 0.0f && wrap != DFA_WRAP_NONE) {
            float new_min = has_any ? SDL_min(min_x, gmin) : gmin;
            float new_max = has_any ? SDL_max(max_x, gmax) : gmax;
            float vis_w = new_max - new_min;

            if (has_any && vis_w > maxw) {
                /* 改行 */
                int break_end = cur;   /* 既定は “今の文字の直前で切る” */

                if (wrap == DFA_WRAP_WORD && has_break && last_break_pos > line_start) {
                    /* space位置で切る */
                    break_end = last_break_pos;
                    pen = pen_at_break;
                    min_x = min_at_break;
                    max_x = max_at_break;
                }
                else {
                    /* CHAR wrap：今の文字は次行へ回すので、curは変えない */
                }

                if (line_count == *io_cap) {
                    int nc = (*io_cap) * 2;
                    DFA_LineInfo* nl = (DFA_LineInfo*)SDL_realloc(*io_lines, (size_t)nc * sizeof(DFA_LineInfo));
                    if (!nl) break;
                    *io_lines = nl;
                    *io_cap = nc;
                }

                (*io_lines)[line_count++] = (DFA_LineInfo){
                    .start = line_start,
                    .end = break_end,
                    .min_x = has_any ? min_x : 0.0f,
                    .max_x = has_any ? max_x : 0.0f
                };

                /* 次行開始位置 */
                if (wrap == DFA_WRAP_WORD && has_break && last_break_pos > line_start) {
                    /* space群をスキップして開始 */
                    int s = break_end;
                    while (s < len) {
                        uint32_t c2 = 0;
                        const char* np2 = dfa_utf8_next(text + s, &c2);
                        if (!dfa_is_breakable_space(c2)) break;
                        s = (int)(np2 - text);
                    }
                    line_start = s;
                    cur = s;
                }
                else {
                    line_start = cur; /* 今の文字から次行 */
                    /* curはそのまま */
                }

                /* 行初期化 */
                pen = 0.0f; min_x = 0.0f; max_x = 0.0f; has_any = false;
                last_break_pos = -1; has_break = false;
                continue; /* 今の文字を改めて処理するため cur は進めない */
            }
        }

        /* 行境界更新 */
        if (!has_any) {
            min_x = gmin;
            max_x = gmax;
            has_any = true;
        }
        else {
            min_x = SDL_min(min_x, gmin);
            max_x = SDL_max(max_x, gmax);
        }

        pen += adv;
        cur = next;
    }

    /* 末尾行 */
    if (line_start <= len) {
        if (line_count == *io_cap) {
            int nc = (*io_cap) * 2;
            DFA_LineInfo* nl = (DFA_LineInfo*)SDL_realloc(*io_lines, (size_t)nc * sizeof(DFA_LineInfo));
            if (nl) {
                *io_lines = nl;
                *io_cap = nc;
            }
        }
        if (line_count < *io_cap) {
            (*io_lines)[line_count++] = (DFA_LineInfo){
                .start = line_start,
                .end = len,
                .min_x = has_any ? min_x : 0.0f,
                .max_x = has_any ? max_x : 0.0f
            };
        }
    }

    return line_count;
}

static void dfa_message_rebuild_offsets(DFA_Message* m)
{
    if (!m || !m->text) return;

    const int len = (int)SDL_strlen(m->text);

    /* まず数える */
    int count = 0;
    int cur = 0;
    while (cur < len) {
        uint32_t cp = 0;
        const char* np = dfa_utf8_next(m->text + cur, &cp);
        int next = (int)(np - m->text);
        if (next <= cur) break;
        count++;
        cur = next;
    }

    SDL_free(m->offsets);
    m->offsets = (int*)SDL_malloc((size_t)(count + 1) * sizeof(int));
    m->count = count;

    cur = 0;
    m->offsets[0] = 0;
    for (int i = 0; i < count; i++) {
        uint32_t cp = 0;
        const char* np = dfa_utf8_next(m->text + cur, &cp);
        int next = (int)(np - m->text);
        if (next <= cur) { m->offsets[i + 1] = cur; break; }
        cur = next;
        m->offsets[i + 1] = cur;
    }
}

static bool dfa_draw_run_bytes(
    DFA_DrawList* list,
    DFA_FontAtlas* atlas,
    float pen_origin_x,
    float baseline_y,
    float scale,
    SDL_Color color,
    const char* text,
    int start,
    int end,
    bool use_true_baseline,
    float pivot_x,
    float pivot_y,
    float c,
    float s,
    bool do_rot
) {
    bool complete = true;
    float pen = 0.0f;

    int cur = start;
    while (cur < end) {
        const char* p = text + cur;
        uint32_t cp = 0;
        const char* np = dfa_utf8_next(p, &cp);
        int next = (int)(np - text);
        if (next <= cur || next > end) break;

        if (cp == '\r' || cp == '\t' || cp == '\n') { cur = next; continue; }

        int idx;
        if (!dfa_map_get(&atlas->map, cp, &idx)) {
            /* 未要求なら要求 */
            complete = false;
            int ni = dfa_glyph_new(atlas, cp);
            if (ni >= 0) {
                atlas->glyphs[ni].state = DFA_GLYPH_PENDING;
                dfa_map_put(&atlas->map, cp, ni);

                SDL_LockMutex(atlas->worker.mtx);
                (void)dfa_u32q_push(&atlas->worker.req_q, cp);
                SDL_CondSignal(atlas->worker.cv);
                SDL_UnlockMutex(atlas->worker.mtx);
            }
            pen += dfa_fallback_advance_px(atlas, scale);
            cur = next;
            continue;
        }

        DFA_Glyph* g = &atlas->glyphs[idx];
        if (g->state != DFA_GLYPH_READY) {
            complete = false;
            pen += dfa_fallback_advance_px(atlas, scale);
            cur = next;
            continue;
        }

        const float gx0 = pen_origin_x + pen;
        const float gy0 = baseline_y;

        const float gw = (float)g->tex_rect.w * scale;
        const float gh = (float)g->tex_rect.h * scale;

        int page_index = g->page_index;
        if (page_index < 0 || page_index >= list->page_count) {
            complete = false;
            pen += (float)g->advance * scale;
            cur = next;
            continue;
        }

        DFA_PageDraw* pd = &list->pages[page_index];
        dfa_pagedraw_reserve(pd, 4, 6);

        const float u0 = (float)g->tex_rect.x / (float)atlas->atlas_w;
        const float v0 = (float)g->tex_rect.y / (float)atlas->atlas_h;
        const float u1 = (float)(g->tex_rect.x + g->tex_rect.w) / (float)atlas->atlas_w;
        const float v1 = (float)(g->tex_rect.y + g->tex_rect.h) / (float)atlas->atlas_h;

        /* ★頂点座標（回転前） */
        float x0 = gx0, y0 = gy0;
        float x1 = gx0 + gw, y1 = gy0;
        float x2 = gx0 + gw, y2 = gy0 + gh;
        float x3 = gx0, y3 = gy0 + gh;

        /* ★文字列全体のピボット(pivot_x,pivot_y)で回転 */
        if (do_rot) {
            dfa_rotate_point(&x0, &y0, pivot_x, pivot_y, c, s);
            dfa_rotate_point(&x1, &y1, pivot_x, pivot_y, c, s);
            dfa_rotate_point(&x2, &y2, pivot_x, pivot_y, c, s);
            dfa_rotate_point(&x3, &y3, pivot_x, pivot_y, c, s);
        }

        int base = pd->vlen;

        pd->v[pd->vlen++] = (SDL_Vertex){ .position = { x0, y0 }, .color = color, .tex_coord = { u0, v0 } };
        pd->v[pd->vlen++] = (SDL_Vertex){ .position = { x1, y1 }, .color = color, .tex_coord = { u1, v0 } };
        pd->v[pd->vlen++] = (SDL_Vertex){ .position = { x2, y2 }, .color = color, .tex_coord = { u1, v1 } };
        pd->v[pd->vlen++] = (SDL_Vertex){ .position = { x3, y3 }, .color = color, .tex_coord = { u0, v1 } };

        pd->i[pd->ilen++] = base + 0;
        pd->i[pd->ilen++] = base + 1;
        pd->i[pd->ilen++] = base + 2;
        pd->i[pd->ilen++] = base + 0;
        pd->i[pd->ilen++] = base + 2;
        pd->i[pd->ilen++] = base + 3;

        pen += (float)g->advance * scale;
        cur = next;
    }

    return complete;
}

static bool dfa_draw_layout_bytes(
    DFA_FontEntry* e,
    float x,
    float y,
    float scale,
    float angle_rad,
    SDL_Color color,
    const DFA_TextLayout* layout_in,
    const char* text,
    int len
) {
    if (!e || !e->atlas || !e->list || !text) return false;
    if (scale <= 0.0f) scale = 1.0f;

    DFA_TextLayout layout = layout_in ? *layout_in : DFA_TextLayoutDefault();
    if (layout.line_spacing <= 0.0f) layout.line_spacing = 1.0f;

    /* TOP/MIDDLE/BOTTOM で“ぴったり”合わせたいなら true baseline 推奨 */
    if (layout.vanchor != DFA_VANCHOR_BASELINE) layout.use_true_baseline = true;

    bool complete_layout = true;
    int line_count = dfa_layout_lines(
        e->atlas, text, len, scale, &layout,
        &e->lines, &e->lines_cap,
        &complete_layout
    );
    if (line_count <= 0) return false;

    /* baseline0 計算（あなたが以前入れた “tight anchor” 版を使っている想定）
       ここは既存のものをそのまま使ってOKです。 */
    const float L = (float)e->atlas->lineskip * scale * layout.line_spacing;

    /* ここは既存実装のままでも動きます（ピボットは x,y）
       “TOP=上端ぴったり / MIDDLE=中心ぴったり” を入れているならそれを維持してください。 */
    float baseline0 = y;
    if (layout.vanchor == DFA_VANCHOR_TOP) {
        baseline0 = y;
    }
    else if (layout.vanchor == DFA_VANCHOR_MIDDLE) {
        float block_h = (float)line_count * L;
        baseline0 = y - block_h * 0.5f;
    }
    else if (layout.vanchor == DFA_VANCHOR_BOTTOM) {
        float block_h = (float)line_count * L;
        baseline0 = y - block_h;
    }

    const bool do_rot = (fabsf(angle_rad) > 0.000001f);
    const float c = do_rot ? cosf(angle_rad) : 1.0f;
    const float s = do_rot ? sinf(angle_rad) : 0.0f;

    /* ★ピボットは “align/vanchor で決まる基準点＝(x,y)” */
    const float pivot_x = x;
    const float pivot_y = y;

    bool complete_draw = true;

    for (int i = 0; i < line_count; i++) {
        const DFA_LineInfo* ln = &e->lines[i];
        const float minx = ln->min_x;
        const float maxx = ln->max_x;

        float pen_origin = x;
        if (layout.align == DFA_ALIGN_LEFT) {
            pen_origin = x - minx;
        }
        else if (layout.align == DFA_ALIGN_CENTER) {
            pen_origin = x - (minx + maxx) * 0.5f;
        }
        else { /* RIGHT */
            pen_origin = x - maxx;
        }

        float baseline = baseline0 + (float)i * L;

        bool ok = dfa_draw_run_bytes(
            e->list, e->atlas,
            pen_origin, baseline, scale, color,
            text, ln->start, ln->end,
            layout.use_true_baseline,
            pivot_x, pivot_y,
            c, s, do_rot
        );
        if (!ok) complete_draw = false;
    }

    return complete_layout && complete_draw;
}


/* =========================================================
   公開API（新設計）
   ========================================================= */

DFA_FontID DFA_Init(
    SDL_Renderer* renderer,
    const char* name,
    const char* font_path,
    float ptsize,
    int style,
    int atlas_w,
    int atlas_h,
    int padding_px,
    bool extrude,
    const char* cache_dir
) {
    if (!renderer || !font_path) return DFA_INVALID_FONT_ID;

    if (name && name[0]) {
        DFA_FontEntry* ex = dfa_find_by_name(name);
        if (ex) return ex->id;
    }

    DFA_FontEntry* e = dfa_alloc_entry();
    if (!e) return DFA_INVALID_FONT_ID;

    if (g_dfa.next_id == 0) g_dfa.next_id = 1;
    e->id = g_dfa.next_id++;

    if (name && name[0]) {
        SDL_strlcpy(e->name, name, DFA_NAME_MAX);
    }
    else {
        SDL_snprintf(e->name, DFA_NAME_MAX, "font_%u", (unsigned)e->id);
    }

    DFA_FontAtlas* atlas = NULL;
    if (!dfa_atlas_create(&atlas, renderer, font_path, ptsize, style, atlas_w, atlas_h, padding_px, extrude, cache_dir)) {
        e->used = false;
        return DFA_INVALID_FONT_ID;
    }

    DFA_DrawList* list = NULL;
    if (!dfa_drawlist_begin(&list, atlas)) {
        dfa_atlas_destroy(atlas);
        e->used = false;
        return DFA_INVALID_FONT_ID;
    }

    dfa_drawlist_reset(list, atlas);

    e->atlas = atlas;
    e->list = list;

    return e->id;
}

void DFA_RequestText(DFA_FontID font, const char* fmt, ...)
{
    DFA_FontEntry* e = dfa_find_by_id(font);
    if (!e || !e->atlas || !fmt) return;

    va_list ap;
    va_start(ap, fmt);
    const char* text = dfa_vformat_reuse(&e->fmt_buf, &e->fmt_cap, fmt, ap);
    va_end(ap);

    if (text) dfa_request_text_str(e->atlas, text);
}

void DFA_Update(int max_pump_per_font) {
    if (max_pump_per_font <= 0) max_pump_per_font = 64;

    for (int i = 0; i < g_dfa.count; i++) {
        DFA_FontEntry* e = &g_dfa.items[i];
        if (!e->used || !e->atlas || !e->list) continue;

        dfa_atlas_pump(e->atlas, max_pump_per_font);

        /* ここでまとめて描画 */
        dfa_drawlist_flush(e->list, e->atlas);

        /* 次フレーム用にリセット */
        dfa_drawlist_reset(e->list, e->atlas);
    }
}

void DFA_Quit(void) {
    for (int i = 0; i < g_dfa.count; i++) {
        DFA_FontEntry* e = &g_dfa.items[i];
        if (!e->used) continue;

        /* 必要ならキャッシュ保存（cache_dirがある場合のみ） */
        if (e->atlas && e->atlas->cache_dir) {
            (void)dfa_save_cache(e->atlas);
        }

        if (e->list) {
            dfa_drawlist_destroy(e->list);
            e->list = NULL;
        }
        if (e->atlas) {
            dfa_atlas_destroy(e->atlas);
            e->atlas = NULL;
        }
        if (e->fmt_buf) {
            SDL_free(e->fmt_buf);
            e->fmt_buf = NULL;
            e->fmt_cap = 0;
        }
        if (e->lines) {
            SDL_free(e->lines);
            e->lines = NULL;
            e->lines_cap = 0;
        }

        e->used = false;
    }

    SDL_free(g_dfa.items);
    g_dfa.items = NULL;
    g_dfa.count = 0;
    g_dfa.cap = 0;
    g_dfa.next_id = 1;

}

DFA_TextLayout DFA_TextLayoutDefault(void)
{
    DFA_TextLayout t;
    t.align = DFA_ALIGN_LEFT;
    t.vanchor = DFA_VANCHOR_BASELINE;
    t.max_width_px = 0.0f;
    t.wrap = DFA_WRAP_NONE;
    t.line_spacing = 1.0f;
    t.use_true_baseline = true;
    return t;
}

bool DFA_DrawText(
    DFA_FontID font,
    float x,
    float y,
    float scale,
    float angle,
    SDL_Color color,
    const DFA_TextLayout* layout,
    const char* fmt,
    ...
) {
    DFA_FontEntry* e = dfa_find_by_id(font);
    if (!e || !e->atlas || !e->list || !fmt) return false;

    va_list ap;
    va_start(ap, fmt);
    const char* text = dfa_vformat_reuse(&e->fmt_buf, &e->fmt_cap, fmt, ap);
    va_end(ap);

    if (!text) return false;
    return dfa_draw_layout_bytes(e, x, y, scale, angle, color, layout, text, (int)SDL_strlen(text));
}

bool DFA_MeasureText(
    DFA_FontID font,
    float scale,
    const DFA_TextLayout* layout_in,
    float* out_w,
    float* out_h,
    const char* fmt,
    ...
) {
    DFA_FontEntry* e = dfa_find_by_id(font);
    if (!e || !e->atlas || !fmt || !out_w || !out_h) return false;
    if (scale <= 0.0f) scale = 1.0f;

    DFA_TextLayout layout = layout_in ? *layout_in : DFA_TextLayoutDefault();
    if (layout.line_spacing <= 0.0f) layout.line_spacing = 1.0f;

    va_list ap;
    va_start(ap, fmt);
    const char* text = dfa_vformat_reuse(&e->fmt_buf, &e->fmt_cap, fmt, ap);
    va_end(ap);
    if (!text) return false;

    bool complete_layout = true;
    int line_count = dfa_layout_lines(
        e->atlas, text, (int)SDL_strlen(text), scale, &layout,
        &e->lines, &e->lines_cap,
        &complete_layout
    );
    if (line_count <= 0) { *out_w = 0; *out_h = 0; return false; }

    float w = 0.0f;
    for (int i = 0; i < line_count; i++) {
        float lw = e->lines[i].max_x - e->lines[i].min_x;
        if (lw > w) w = lw;
    }

    const float line_advance = (float)e->atlas->lineskip * scale * layout.line_spacing;
    float h = (float)line_count * line_advance;

    *out_w = w;
    *out_h = h;
    return complete_layout;
}


void DFA_MessageInit(DFA_Message* m)
{
    if (!m) return;
    SDL_memset(m, 0, sizeof(*m));
    m->layout = DFA_TextLayoutDefault();
}

void DFA_MessageDestroy(DFA_Message* m)
{
    if (!m) return;
    SDL_free(m->text);
    SDL_free(m->offsets);
    SDL_memset(m, 0, sizeof(*m));
}

bool DFA_MessageSet(
    DFA_Message* m,
    DFA_FontID font,
    float x,
    float y,
    float scale,
    float angle,
    SDL_Color color,
    float interval_sec,
    const DFA_TextLayout* layout,
    const char* fmt,
    ...
) {
    if (!m || !fmt) return false;

    DFA_FontEntry* e = dfa_find_by_id(font);
    if (!e) return false;

    va_list ap;
    va_start(ap, fmt);
    const char* text = dfa_vformat_reuse(&e->fmt_buf, &e->fmt_cap, fmt, ap);
    va_end(ap);
    if (!text) return false;

    SDL_free(m->text);
    m->text = SDL_strdup(text);
    if (!m->text) return false;

    m->font = font;
    m->x = x; m->y = y; m->scale = scale;
    m->angle = angle;
    m->color = color;
    m->interval_sec = interval_sec;
    m->accum_sec = 0.0f;
    m->visible = 0;
    m->playing = true;
    m->layout = layout ? *layout : DFA_TextLayoutDefault();

    dfa_message_rebuild_offsets(m);
    return true;
}

void DFA_MessageStart(DFA_Message* m, bool reset)
{
    if (!m) return;
    m->playing = true;
    m->accum_sec = 0.0f;
    m->last_ticks = 0;
    if (reset) m->visible = 0;
}

void DFA_MessageUpdate(DFA_Message* m, float dt_sec)
{
    if (!m || !m->playing) return;
    if (m->count <= 0) return;

    if (m->interval_sec <= 0.0f) {
        m->visible = m->count;
        return;
    }

    m->accum_sec += dt_sec;
    while (m->accum_sec >= m->interval_sec && m->visible < m->count) {
        m->accum_sec -= m->interval_sec;
        m->visible++;
    }
}

void DFA_MessageUpdateAuto(DFA_Message* m)
{
    if (!m || !m->playing) return;
    uint64_t now = SDL_GetTicks();
    if (m->last_ticks == 0) {
        m->last_ticks = now;
        return;
    }
    float dt = (float)(now - m->last_ticks) / 1000.0f;
    m->last_ticks = now;
    DFA_MessageUpdate(m, dt);
}

void DFA_MessageUpdateSfx(DFA_Message* m, float dt_sec, Mix_Chunk* sfx_track)
{
    if (!m || !m->playing) return;
    if (m->count <= 0) return;

    if (m->interval_sec <= 0.0f) {
        /* 即時全表示（この場合は“1文字ごと”ではないので鳴らさない方が自然） */
        m->visible = m->count;
        return;
    }

    m->accum_sec += dt_sec;

    while (m->accum_sec >= m->interval_sec && m->visible < m->count) {
        m->accum_sec -= m->interval_sec;
        m->visible++;

        /* ★ここで1文字ごとに効果音（同じtrackは再スタートされる） */
        if (sfx_track) {
            (void)Mix_PlayChannel(-1, sfx_track, 0);
        }
    }
}

void DFA_MessageUpdateAutoSfx(DFA_Message* m, Mix_Chunk* sfx_track)
{
    if (!m || !m->playing) return;

    uint64_t now = SDL_GetTicks();

    if (m->last_ticks == 0) {
        m->last_ticks = now;
        return;
    }

    float dt = (float)(now - m->last_ticks) / 1000.0f;
    m->last_ticks = now;

    DFA_MessageUpdateSfx(m, dt, sfx_track);
}


bool DFA_MessageDraw(const DFA_Message* m)
{
    if (!m || !m->text || !m->offsets) return false;

    DFA_FontEntry* e = dfa_find_by_id(m->font);
    if (!e || !e->atlas || !e->list) return false;

    int vis = m->visible;
    if (vis < 0) vis = 0;
    if (vis > m->count) vis = m->count;

    int len = m->offsets[vis];
    return dfa_draw_layout_bytes(
        e,
        m->x, m->y,
        m->scale,
        m->angle,
        m->color,
        &m->layout,
        m->text,
        len
    );
}
