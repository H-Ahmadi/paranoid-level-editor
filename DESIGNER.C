/*
 *  DESIGNER.C  -  Level Designer for Paranoid v1.8
 *
 *  A level editor for the 1993 shareware game Paranoid.  It runs in
 *  EGA mode 10h (640x350x16) and draws everything with the game's own
 *  data files (BITMAPS.PAR, FONTS.PAR), so it looks like part of the
 *  original package.  Levels are read from and written to LEVELS.PAR.
 *
 *  Build with Open Watcom 2.0 - see BUILD.BAT.  Note: do not use -ox;
 *  its loop optimiser (-ol) miscompiles the far-pointer loops below.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <bios.h>
#include <process.h>
#include <io.h>

typedef unsigned char u8;
typedef unsigned int  u16;

/* ------------------------------------------------------------------ */
/*  Level data                                                          */
/* ------------------------------------------------------------------ */

#define NLEVELS   30
#define COLS      14
#define ROWS      19
#define CELLS     (COLS*ROWS)          /* 266, stored column by column */
#define HALF      (NLEVELS*CELLS)      /* 7980 bytes per file half    */
#define NOICON    0xFF
#define SOLID     0x1C

static u8 lv_b[NLEVELS][CELLS];        /* brick codes                  */
static u8 lv_i[NLEVELS][CELLS];        /* hidden icon codes (FF none)  */
static u8 tmp_b[NLEVELS][CELLS];       /* scratch for test-play file   */
static u8 tmp_i[NLEVELS][CELLS];

/* key for reading and writing LEVELS.PAR */
static const u8 key[23] = {
    0x50,0x53,0x51,0x52,0x8B,0x04,0x05,0x30,0x00,0x8B,0x1D,0x83,
    0xC3,0x30,0x01,0xD8,0xB9,0xF5,0x00,0x8A,0x04,0x46,0x88 };

static void crypt(u8 *a, u8 *b, int encode)
{
    int blk, half, i, k;
    u8 n, *p;
    for (blk = 0; blk < 3; blk++) {
        k = 0; n = 0;
        for (half = 0; half < 2; half++) {
            p = half ? b : a;
            for (i = blk * 2660; i < (blk + 1) * 2660; i++) {
                if (encode) p[i] = (u8)((u8)(p[i] + n) ^ key[k]);
                else        p[i] = (u8)((u8)(p[i] ^ key[k]) - n);
                if (++k == 23) k = 0;
                n++;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Graphics data from the game                                         */
/* ------------------------------------------------------------------ */

#define BMP_SIZE  11568
#define BRK_OFF   (4*0x84)             /* bricks: 33 x (32x12), planar */
#define BRK_PLANE 0x630
#define ICN_OFF   (BRK_OFF+4*0x630)    /* icons: 49 frames x (16x12)   */
#define ICN_PLANE 0x498

static u8 __far bmp[BMP_SIZE];
static u8 font[2920];
#define FONT_S  (font)                 /* 8x8,  64 chars (small caps)  */
#define FONT_L  (font+512)             /* 8x14, 64 chars (capitals)    */
#define FONT_LOGO (font+1408)          /* 14 glyphs of 24x36           */

/* glyphs that spell the "PARANOID" logo, one every 24 pixels */
static const u8 logo_glyphs[8] = { 0x0B,0x04,0x0A,0x04,0x08,0x06,0x0C,0x0D };

/* first animation frame of each icon, plus end marker */
static const u8 icon_frame[13] = { 0,5,10,15,18,22,27,31,35,36,41,45,49 };

/* ------------------------------------------------------------------ */
/*  Names                                                               */
/* ------------------------------------------------------------------ */

static const char *brick_name[32] = {
    "Empty",          "Union Jack",     "Telly",          "BBB",
    "Double II",      "Double II",      "Double stripe",  "Double stripe",
    "Weak brick",     "Grey plain",     "Bonus B",        "Bonus O",
    "Bonus N",        "Bonus U",        "Bonus S",        "Grey patterned",
    "Red patterned",  "Blue patterned", "Green patterned","Cyan patterned",
    "Quad brick",     "Quad brick",     "Quad brick",     "Quad brick",
    "Red plain",      "Blue plain",     "Green plain",    "Cyan plain",
    "Solid block",    "Smiley",         "Frowny",         "Bomb" };

static const char *brick_info[32] = {
    "Rubs out",       "Picture 20 pts", "Picture 20 pts", "Picture 20 pts",
    "2 hits  25 pts", "1 hit left",     "2 hits  25 pts", "1 hit left",
    "5 points",       "10 points",      "Letter  10 pts", "Letter  10 pts",
    "Letter  10 pts", "Letter  10 pts", "Letter  10 pts", "15 points",
    "15 points",      "15 points",      "15 points",      "15 points",
    "4 hits  70 pts", "3 hits left",    "2 hits left",    "1 hit left",
    "10 points",      "10 points",      "10 points",      "10 points",
    "Indestructible", "Picture 20 pts", "Picture 20 pts", "Picture 20 pts" };

static const char *icon_name[12] = {
    "Expand", "Shrink", "Catch", "500 points", "Laser", "Split",
    "Extra ball", "Barrier", "Next level", "Speed up", "Slow down",
    "Thru-ball" };

static const char *icon_info[12] = {
    "Bigger bat", "Smaller bat", "Sticky bat", "Bonus score",
    "Bat fires", "Ball splits", "One more life", "Safety net",
    "Skip level", "Faster ball", "Slower ball", "Smashes thru" };

/* palette order for the brick selector (3 per row) */
#define NPAL_B 32
static const u8 pal_b[NPAL_B] = {
    0x00, 0x18, 0x19,   0x1A, 0x1B, 0x09,   0x10, 0x11, 0x12,
    0x13, 0x0F, 0x08,   0x1C, 0x04, 0x05,   0x06, 0x07, 0x14,
    0x15, 0x16, 0x17,   0x01, 0x02, 0x03,   0x1D, 0x1E, 0x1F,
    0x0A, 0x0B, 0x0C,   0x0D, 0x0E };
#define NPAL_I 13                     /* "no icon" + 12 icons          */

/* ------------------------------------------------------------------ */
/*  EGA low level                                                       */
/* ------------------------------------------------------------------ */

#define VGA ((u8 __far *)0xA0000000UL)
#define TICKS (*(volatile unsigned long __far *)0x0040006CUL)
#define SHIFTS (*(volatile u8 __far *)0x00400017UL)

enum { BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, GREY, DGREY,
       LBLUE, LGREEN, LCYAN, LRED, LMAGENTA, YELLOW, WHITE };

static void gc(u8 i, u8 v) { outp(0x3CE, i); outp(0x3CF, v); }
static void planes(u8 m)   { outp(0x3C4, 2); outp(0x3C5, m); }

static void set_mode(u8 m)
{
    union REGS r;
    r.h.ah = 0; r.h.al = m;
    int86(0x10, &r, &r);
}

static void ega_reset(void)
{
    gc(0, 0); gc(1, 0); gc(3, 0); gc(5, 0); gc(8, 0xFF); planes(0x0F);
}

/* filled rectangle, inclusive coordinates; xor!=0 XORs the colour */
static void rect_x(int x0, int y0, int x1, int y1, u8 c, int xor)
{
    int y, b, b0, b1;
    u8 lm, rm;
    u8 __far *p;
    volatile u8 t;

    if (x0 > x1 || y0 > y1) return;
    gc(0, c); gc(1, 0x0F); gc(3, xor ? 0x18 : 0); planes(0x0F);
    b0 = x0 >> 3; b1 = x1 >> 3;
    lm = (u8)(0xFF >> (x0 & 7));
    rm = (u8)(0xFF << (7 - (x1 & 7)));
    for (y = y0; y <= y1; y++) {
        p = VGA + y * 80 + b0;
        if (b0 == b1) { gc(8, lm & rm); t = *p; *p = 0xFF; continue; }
        gc(8, lm); t = *p; *p++ = 0xFF;
        if (b1 - b0 > 1) {
            gc(8, 0xFF);
            for (b = b0 + 1; b < b1; b++) { t = *p; *p++ = 0xFF; }
        }
        gc(8, rm); t = *p; *p = 0xFF;
    }
    gc(8, 0xFF); gc(1, 0); gc(3, 0);
}

#define rect(x0,y0,x1,y1,c) rect_x(x0,y0,x1,y1,c,0)

/* 1-pixel outline; with xor, corners are not drawn twice */
static void frame_x(int x0, int y0, int x1, int y1, u8 c, int xor)
{
    rect_x(x0, y0, x1, y0, c, xor);
    rect_x(x0, y1, x1, y1, c, xor);
    rect_x(x0, y0 + 1, x0, y1 - 1, c, xor);
    rect_x(x1, y0 + 1, x1, y1 - 1, c, xor);
}

/* monochrome bitmap at any x, drawn transparently in colour c */
static void mono(int x, int y, const u8 *src, int wb, int h, u8 c, int xor)
{
    int r, i, sh = x & 7;
    u8 b, m;
    u8 __far *p;
    volatile u8 t;

    gc(0, c); gc(1, 0x0F); gc(3, xor ? 0x18 : 0); planes(0x0F);
    for (r = 0; r < h; r++) {
        p = VGA + (y + r) * 80 + (x >> 3);
        for (i = 0; i < wb; i++, p++) {
            b = *src++;
            if (!b) continue;
            m = (u8)(b >> sh);
            if (m) { gc(8, m); t = *p; *p = 0xFF; }
            if (sh) {
                m = (u8)(b << (8 - sh));
                if (m) { gc(8, m); t = p[1]; p[1] = 0xFF; }
            }
        }
    }
    gc(8, 0xFF); gc(1, 0); gc(3, 0);
}

/* opaque planar sprite at a byte-aligned x */
static void sprite(int x, int y, const u8 __far *src, u16 stride, int wb, int h)
{
    int p, r, i;
    const u8 __far *s;
    u8 __far *d;

    ega_reset();
    for (p = 0; p < 4; p++) {
        planes((u8)(1 << p));
        s = src + p * stride;
        for (r = 0; r < h; r++) {
            d = VGA + (y + r) * 80 + (x >> 3);
            for (i = 0; i < wb; i++) *d++ = *s++;
        }
    }
    planes(0x0F);
}

static void draw_brick(int x, int y, u8 code)
{
    sprite(x, y, bmp + BRK_OFF + code * 48, BRK_PLANE, 4, 12);
}

static void draw_icon(int x, int y, int frame)
{
    sprite(x, y, bmp + ICN_OFF + frame * 24, ICN_PLANE, 2, 12);
}

/* ------------------------------------------------------------------ */
/*  Text, in the game's "small capitals" style                          */
/*  \1 switches to the highlight colour, \2 back to the normal one.     */
/* ------------------------------------------------------------------ */

static void glyph(int x, int y, char ch, u8 c, int small_only)
{
    int up = (ch >= 'a' && ch <= 'z');
    int n;
    if (up) ch -= 32;
    n = (u8)ch - 32;
    if (n <= 0 || n >= 64) return;
    if (up || small_only) mono(x, y + (small_only ? 0 : 6), FONT_S + n * 8, 1, 8, c, 0);
    else                  mono(x, y, FONT_L + n * 14, 1, 14, c, 0);
}

static int text_len(const char *s)
{
    int n = 0;
    for (; *s; s++) if (*s > 2) n++;
    return n * 8;
}

static void text2(int x, int y, const char *s, u8 c, u8 hi)
{
    u8 cur = c;
    for (; *s; s++) {
        if (*s == 1) { cur = hi; continue; }
        if (*s == 2) { cur = c;  continue; }
        glyph(x, y, *s, cur, 0);
        x += 8;
    }
}

#define text(x,y,s,c) text2(x,y,s,c,LRED)

static void ctext2(int cx, int y, const char *s, u8 c, u8 hi)
{
    text2(cx - text_len(s) / 2, y, s, c, hi);
}

#define ctext(cx,y,s,c) ctext2(cx,y,s,c,LRED)

static void small_text(int x, int y, const char *s, u8 c)
{
    for (; *s; s++, x += 8) glyph(x, y, *s, c, 1);
}

static void draw_logo(int x, int y, u8 c)
{
    int k;
    for (k = 0; k < 8; k++)
        mono(x + k * 24, y, FONT_LOGO + logo_glyphs[k] * 108, 3, 36, c, 0);
}

/* ------------------------------------------------------------------ */
/*  Bevelled boxes, exactly as the game draws them                      */
/* ------------------------------------------------------------------ */

static void raised(int x0, int y0, int x1, int y1)
{
    rect(x0 + 2, y0 + 2, x1 - 2, y1 - 2, GREY);
    rect(x0, y0, x1, y0 + 1, WHITE);
    rect(x0, y1 - 1, x1, y1, DGREY);
    rect(x0, y0 + 2, x0 + 1, y1 - 2, DGREY);
    rect(x1 - 1, y0 + 2, x1, y1 - 2, WHITE);
}

static void sunken(int x0, int y0, int x1, int y1, u8 fill)
{
    rect(x0 + 2, y0 + 2, x1 - 2, y1 - 2, fill);
    rect(x0, y0, x1, y0 + 1, DGREY);
    rect(x0, y1 - 1, x1, y1, WHITE);
    rect(x0, y0 + 2, x0 + 1, y1 - 2, WHITE);
    rect(x1 - 1, y0 + 2, x1, y1 - 2, DGREY);
}

/* ------------------------------------------------------------------ */
/*  Mouse                                                               */
/* ------------------------------------------------------------------ */

static int mouse_ok;

static void mouse_call(u16 ax, u16 *b, u16 *x, u16 *y)
{
    union REGS r;
    r.x.ax = ax;
    int86(0x33, &r, &r);
    if (b) *b = r.x.bx;
    if (x) *x = r.x.cx;
    if (y) *y = r.x.dx;
}

static void mouse_init(void)
{
    union REGS r;
    r.x.ax = 0;
    int86(0x33, &r, &r);
    mouse_ok = (r.x.ax == 0xFFFF);
    if (mouse_ok) mouse_call(1, 0, 0, 0);
}

static void MH(void) { if (mouse_ok) mouse_call(2, 0, 0, 0); }
static void MS(void) { if (mouse_ok) mouse_call(1, 0, 0, 0); }

/* ------------------------------------------------------------------ */
/*  Editor state                                                        */
/* ------------------------------------------------------------------ */

static int lv;                 /* current level 0..29          */
static int cx, cy;             /* cursor cell                  */
static int mode;               /* 0 = bricks, 1 = icons        */
static int sel_b = 1;          /* palette index, bricks        */
static int sel_i = 1;          /* palette index, icons         */
static int show_grid = 1, show_icons = 1;
static int dirty;
static int cur_vis;            /* cursor currently drawn       */
static int anim;               /* icon animation counter       */
static char msg[64];
static unsigned long msg_until;

static u8 clip_b[CELLS], clip_i[CELLS];   /* stored like a level */
static int have_clip, clip_w, clip_h;

static int block_on, bx0, by0;             /* block anchor cell    */

#define NUNDO 24
static u8 __far undo_b[NUNDO][CELLS];
static u8 __far undo_i[NUNDO][CELLS];
static int undo_lv[NUNDO];
static int undo_top, undo_n;

#define FX 16                  /* play area origin             */
#define FY 12

/* ------------------------------------------------------------------ */
/*  Files                                                               */
/* ------------------------------------------------------------------ */

static int file_exists(const char *n) { return access(n, 0) == 0; }

static int load_file(const char *name, u8 __far *dst, u16 len)
{
    int h;
    unsigned got;
    if (_dos_open(name, 0, &h) != 0) return 0;
    if (_dos_read(h, dst, len, &got) != 0 || got != len) { _dos_close(h); return 0; }
    _dos_close(h);
    return 1;
}

static int load_levels(const char *name)
{
    FILE *f = fopen(name, "rb");
    int ok;
    if (!f) return 0;
    ok = fread(lv_b, 1, HALF, f) == HALF && fread(lv_i, 1, HALF, f) == HALF;
    fclose(f);
    if (ok) crypt(&lv_b[0][0], &lv_i[0][0], 0);
    return ok;
}

static int write_levels(const char *name, u8 *a, u8 *b)
{
    FILE *f;
    int ok;
    crypt(a, b, 1);
    f = fopen(name, "wb");
    ok = f && fwrite(a, 1, HALF, f) == HALF && fwrite(b, 1, HALF, f) == HALF;
    if (f && fclose(f) != 0) ok = 0;
    crypt(a, b, 0);
    return ok;
}

static int copy_file(const char *from, const char *to)
{
    FILE *a, *b;
    static char buf[512];
    size_t n;
    int ok = 1;
    if (!(a = fopen(from, "rb"))) return 0;
    if (!(b = fopen(to, "wb"))) { fclose(a); return 0; }
    while ((n = fread(buf, 1, sizeof buf, a)) > 0)
        if (fwrite(buf, 1, n, b) != n) { ok = 0; break; }
    fclose(a);
    if (fclose(b) != 0) ok = 0;
    return ok;
}

/* puts things right if a save or a test-play was interrupted */
static void recover_test_files(void)
{
    /* a save writes LEVELS.$NW first and only renames it when it is   */
    /* complete, so a lone $NW is good and one next to a .PAR is not   */
    if (file_exists("LEVELS.$NW")) {
        if (file_exists("LEVELS.PAR")) remove("LEVELS.$NW");
        else rename("LEVELS.$NW", "LEVELS.PAR");
    }
    if (file_exists("LEVELS.$$$")) {
        remove("LEVELS.PAR");
        rename("LEVELS.$$$", "LEVELS.PAR");
    }
    if (file_exists("HISCORES.$$$")) {
        remove("HISCORES.PAR");
        rename("HISCORES.$$$", "HISCORES.PAR");
    }
}

/* ------------------------------------------------------------------ */
/*  Drawing the editor screen                                           */
/* ------------------------------------------------------------------ */

static void set_msg(const char *s)
{
    strcpy(msg, s);
    msg_until = TICKS + 55;
}

static void cursor_xor(void)
{
    int x = FX + cx * 32, y = FY + cy * 12;
    MH();
    frame_x(x, y, x + 31, y + 11, WHITE, 1);
    frame_x(x + 1, y + 1, x + 30, y + 10, WHITE, 1);
    MS();
    cur_vis = !cur_vis;
}

static void cursor_hide(void) { if (cur_vis) cursor_xor(); }

static void draw_cell(int c, int r)
{
    int x = FX + c * 32, y = FY + r * 12, i = c * ROWS + r;
    u8 b = lv_b[lv][i], ic = lv_i[lv][i];

    MH();
    if (b == 0) {
        rect(x, y, x + 31, y + 11, BLACK);
        if (show_grid) rect(x, y, x, y, DGREY);
    } else {
        draw_brick(x, y, b);
    }
    if (show_icons && ic != NOICON && ic < 12)
        draw_icon(x + 8, y, icon_frame[ic]);
    if (c == cx && r == cy && cur_vis) {
        cur_vis = 0;
        cursor_xor();
    }
    MS();
}

static void draw_status(void)
{
    char s[80];
    int i = cx * ROWS + cy;
    u8 b = lv_b[lv][i], ic = lv_i[lv][i];

    MH();
    rect(16, 296, 463, 337, BLACK);
    sprintf(s, "Level %02d   Column %02d   Row %02d", lv + 1, cx + 1, cy + 1);
    ctext(240, 296, s, LGREEN);
    if (b == 0) strcpy(s, "Empty");
    else {
        strcpy(s, brick_name[b]);
        if (ic != NOICON && ic < 12) { strcat(s, "  holding  "); strcat(s, icon_name[ic]); }
    }
    ctext(240, 310, s, LGREEN);
    if (msg[0])     ctext(240, 324, msg, YELLOW);
    else if (block_on)
        ctext2(240, 324, "Block:  \1Space\2 fills  \1Del\2 empties  \1C\2 copies", YELLOW, LRED);
    else if (dirty) ctext(240, 324, "Changes not saved yet", LRED);
    MS();
}

/* the marked block spans the anchor and the cursor */
static void block_rect(int *c0, int *r0, int *c1, int *r1)
{
    *c0 = bx0 < cx ? bx0 : cx;  *c1 = bx0 < cx ? cx : bx0;
    *r0 = by0 < cy ? by0 : cy;  *r1 = by0 < cy ? cy : by0;
}

static void block_frame(void)
{
    int c0, r0, c1, r1;
    if (!block_on) return;
    block_rect(&c0, &r0, &c1, &r1);
    MH();
    frame_x(FX + c0 * 32, FY + r0 * 12, FX + c1 * 32 + 31, FY + r1 * 12 + 11, YELLOW, 0);
    MS();
}

/* redraws the cells under the block outline, which removes it */
static void block_unframe(void)
{
    int c0, r0, c1, r1, c, r;
    if (!block_on) return;
    block_rect(&c0, &r0, &c1, &r1);
    for (c = c0; c <= c1; c++) { draw_cell(c, r0); draw_cell(c, r1); }
    for (r = r0; r <= r1; r++) { draw_cell(c0, r); draw_cell(c1, r); }
}

static void draw_field(void)
{
    int c, r;
    cursor_hide();
    MH();
    rect(15, 11, 463, 337, BLACK);
    draw_logo(147, 255, DGREY);              /* where the game puts it */
    for (c = 0; c < COLS; c++)
        for (r = 0; r < ROWS; r++)
            draw_cell(c, r);
    block_frame();
    draw_status();
    MS();
}

/* palette tile positions */
static void pal_pos(int i, int *x, int *y)
{
    if (mode == 0) { *x = 512 + (i % 3) * 40; *y = 96 + (i / 3) * 14; }
    else           { *x = 520 + (i % 3) * 40; *y = 98 + (i / 3) * 18; }
}

static void draw_pal_item(int i)
{
    int x, y, sel = (mode == 0 ? sel_b : sel_i) == i;
    int w = mode == 0 ? 32 : 16;
    pal_pos(i, &x, &y);
    MH();
    frame_x(x - 2, y - 1, x + w + 1, y + 12, sel ? WHITE : GREY, 0);
    frame_x(x - 3, y - 2, x + w + 2, y + 13, sel ? BLACK : GREY, 0);
    if (mode == 0) {
        if (pal_b[i] == 0) { rect(x, y, x + 31, y + 11, BLACK); frame_x(x, y, x + 31, y + 11, DGREY, 0); }
        else draw_brick(x, y, pal_b[i]);
    } else {
        if (i == 0) { rect(x, y, x + 15, y + 11, BLACK); frame_x(x, y, x + 15, y + 11, DGREY, 0); }
        else {
            int ic = i - 1, n = icon_frame[ic + 1] - icon_frame[ic];
            draw_icon(x, y, icon_frame[ic] + (anim % n));
        }
    }
    MS();
}

/* keyboard focus: the play area, the brick/icon panel or the level box */
enum { F_FIELD, F_PANEL, F_LEVEL, F_COUNT };
static int focus;
static int in_list;                    /* the level list is on screen  */
static int list_sel;

/* one-step undo for changes to the level order */
static u8 __far set_b[NLEVELS][CELLS];
static u8 __far set_i[NLEVELS][CELLS];
static int set_saved;

static void draw_selected(void)
{
    MH();
    rect(508, 256, 627, 283, GREY);
    if (mode == 0) {
        u8 b = pal_b[sel_b];
        ctext(568, 256, brick_name[b], BLACK);
        ctext(568, 270, brick_info[b], BLUE);
    } else if (sel_i == 0) {
        ctext(568, 256, "No icon", BLACK);
        ctext(568, 270, "Rubs out", BLUE);
    } else {
        ctext(568, 256, icon_name[sel_i - 1], BLACK);
        ctext(568, 270, icon_info[sel_i - 1], BLUE);
    }
    MS();
}

static void draw_palette(void)
{
    int i, n = mode == 0 ? NPAL_B : NPAL_I;
    MH();
    rect(508, 76, 627, 249, GREY);
    text(516, 78, "Bricks", mode == 0 ? LRED : BLACK);
    text(580, 78, "Icons",  mode == 1 ? LRED : BLACK);
    rect(516, 92, 563, 92, mode == 0 ? LRED : GREY);
    rect(580, 92, 619, 92, mode == 1 ? LRED : GREY);
    for (i = 0; i < n; i++) draw_pal_item(i);
    draw_selected();
    MS();
}

static void draw_level_box(void)
{
    char s[32];
    MH();
    rect(508, 52, 627, 67, GREY);
    sprintf(s, "\1<\2 Level %02d \1>\2", lv + 1);
    ctext2(568, 53, s, BLACK, LRED);
    MS();
}

/* a red outline round the panel box that has the keyboard */
static void focus_frame(void)
{
    MH();
    frame_x(504, 48, 631, 71,  focus == F_LEVEL ? LRED : GREY, 0);
    frame_x(504, 72, 631, 253, focus == F_PANEL ? LRED : GREY, 0);
    MS();
}

static void draw_panel(void)
{
    MH();
    raised(496, 0, 639, 349);
    raised(506, 6, 629, 45);
    ctext(568, 10, "Paranoid", BLUE);
    ctext(568, 26, "Level Designer", BLACK);
    raised(506, 50, 629, 69);
    draw_level_box();
    raised(506, 74, 629, 251);
    raised(506, 254, 629, 285);
    draw_palette();
    raised(506, 289, 629, 345);
    text(512, 292, "\1F1\2 Help", BLACK);
    text(572, 292, "\1F2\2 Save", BLACK);
    text(512, 304, "\1F3\2 Load", BLACK);
    text(572, 304, "\1F4\2 New",  BLACK);
    text(512, 316, "\1F5\2 Test", BLACK);
    text(572, 316, "\1F6\2 List", BLACK);
    ctext(568, 328, "\1Esc\2 Quit", BLACK);
    focus_frame();
    MS();
}

static void draw_all(void)
{
    MH();
    ega_reset();
    raised(0, 0, 479, 349);
    sunken(13, 9, 465, 339, BLACK);
    rect(480, 0, 495, 349, BLACK);
    draw_panel();
    cur_vis = 0;
    draw_field();
    MS();
}

/* ------------------------------------------------------------------ */
/*  Dialogs                                                             */
/* ------------------------------------------------------------------ */

static int get_key(void) { return _bios_keybrd(_NKEYBRD_READ); }

static int key_ready(void) { return _bios_keybrd(_NKEYBRD_READY) != 0; }

/* after a click opens something, wait until the button is let go */
static void mouse_release(void)
{
    u16 b;
    if (!mouse_ok) return;
    do mouse_call(3, &b, 0, 0); while (b & 3);
}

static void draw_list(void);

/* shows a box in the middle of the screen; returns the key pressed */
static int dialog(const char *l1, const char *l2, const char *l3)
{
    int k, c = in_list ? 320 : 240;
    cursor_hide();
    MH();
    raised(c - 160, 110, c + 159, 195);
    sunken(c - 148, 120, c + 147, 185, GREY);
    ctext(c, 128, l1, BLACK);
    ctext(c, 145, l2, BLACK);
    ctext2(c, 164, l3, BLUE, LRED);
    MS();
    while (key_ready()) get_key();
    k = get_key();
    if (in_list) draw_list();
    else draw_field();
    return k & 0xFF;
}

static int ask_yes(const char *l1, const char *l2)
{
    int k = dialog(l1, l2, "\1Y\2es  or  \1N\2o");
    return k == 'y' || k == 'Y';
}

/* ------------------------------------------------------------------ */
/*  Help, styled after the game's own information pages                 */
/* ------------------------------------------------------------------ */

static const char *help1[] = {
    "  Welcome to the Paranoid level designer.  With this program you",
    "can build thirty screens of your very own ball-bouncing fun.",
    "",
    "\1Cursor keys\2   move around (hold \1Shift\2 to draw as you go)",
    "\1Space\2 \1Delete\2  put down or rub out the chosen brick or icon",
    "\1Tab\2           move between the play area, bricks and level box",
    "\1Ctrl+cursor\2   choose a brick or icon without leaving the play area",
    "\1I\2             change between bricks and icons",
    "\1P\2             pick up the brick under the cursor",
    "\1B\2             mark a block, then \1Space\2 fills it or \1Delete\2 empties it",
    "\1C\2 and \1V\2       copy the block (or the whole level) and paste it",
    "\1M\2 \1U\2           mirror the level, undo the last change",
    "\1N\2             clear this level",
    "\1PgUp PgDn\2     go to the previous or next level",
    "\1F4\2 \1F6\2         add a new level, show the list of all levels",
    "\1G\2 and \1H\2       show or hide the grid and the icons",
    "",
    "  The mouse may be used too - the left button draws, the right",
    "button rubs out, and bricks are chosen by clicking on them.",
    0 };

static const char *help2[] = {
    "  Each level is fourteen bricks across and nineteen bricks deep.",
    "A level is finished when every brick except the solid blocks has",
    "been knocked out, so make sure each screen has at least one brick",
    "that can be broken!",
    "",
    "  Icons are hidden inside bricks, and float down when the brick",
    "is destroyed.  They cannot be hidden inside solid blocks.",
    "",
    "  \1F6\2 shows all thirty levels in order.  There you can add new",
    "levels anywhere, delete them, and move them about.  Paranoid",
    "always plays thirty levels, and an empty level is passed over",
    "almost at once - so keep your own levels at the front.",
    "",
    "  \1F2\2 saves all thirty levels into Levels.PAR.  The very first",
    "time, the original levels are put safely away in Levels.ORG, which",
    "is never written over, and each save keeps the version before it",
    "in Levels.BAK.  \1F3\2 can load the last save, the backup or the",
    "original levels.",
    "  \1F5\2 tries the current level in Paranoid - press Escape and then",
    "F10 there to come back.",
    0 };

static void help_page(const char **lines, const char *footer)
{
    int i;
    MH();
    ega_reset();
    raised(0, 0, 639, 349);
    ctext(320, 3, "Paranoid - Level Designer", BLACK);
    sunken(26, 19, 613, 330, BLACK);
    for (i = 0; lines[i]; i++)
        text2(56, 35 + i * 14, lines[i], LGREEN, YELLOW);
    ctext(320, 333, footer, BLACK);
    MS();
    mouse_release();
    while (key_ready()) get_key();
    get_key();
}

static void show_help(void)
{
    cursor_hide();
    help_page(help1, "Press a key for the next page");
    help_page(help2, "Press a key to return to the designer");
    draw_all();
}

/* ------------------------------------------------------------------ */
/*  Editing a level                                                     */
/* ------------------------------------------------------------------ */

static void push_undo(void)
{
    _fmemcpy(undo_b[undo_top], lv_b[lv], CELLS);
    _fmemcpy(undo_i[undo_top], lv_i[lv], CELLS);
    undo_lv[undo_top] = lv;
    undo_top = (undo_top + 1) % NUNDO;
    if (undo_n < NUNDO) undo_n++;
}

static void block_cancel(void)
{
    if (!block_on) return;
    cursor_hide();
    block_unframe();
    block_on = 0;
    draw_status();
}

static void undo(void)
{
    if (!undo_n) { set_msg("Nothing to undo"); draw_status(); return; }
    block_cancel();
    undo_top = (undo_top + NUNDO - 1) % NUNDO;
    undo_n--;
    lv = undo_lv[undo_top];
    _fmemcpy(lv_b[lv], undo_b[undo_top], CELLS);
    _fmemcpy(lv_i[lv], undo_i[undo_top], CELLS);
    dirty = 1;
    set_msg("Undone");
    draw_level_box();
    draw_field();
}

/* applies the current tool to one cell and redraws it;               */
/* returns 1 if it changed, -1 if an icon may not go there            */
static int apply_cell(int c, int r, int erase)
{
    int i = c * ROWS + r;
    u8 *b = &lv_b[lv][i], *ic = &lv_i[lv][i];

    if (mode == 0) {
        u8 nb = erase ? 0 : pal_b[sel_b];
        u8 ni = (nb == 0 || nb == SOLID) ? NOICON : *ic;
        if (*b == nb && *ic == ni) return 0;
        *b = nb; *ic = ni;
    } else {
        u8 ni = (erase || sel_i == 0) ? NOICON : (u8)(sel_i - 1);
        if (ni != NOICON && (*b == 0 || *b == SOLID)) return -1;
        if (*ic == ni) return 0;
        *ic = ni;
    }
    dirty = 1;
    draw_cell(c, r);
    return 1;
}

static void apply(int c, int r, int erase)
{
    int res = apply_cell(c, r, erase);
    if (res < 0) set_msg("Icons must go inside a breakable brick");
    if (res) draw_status();
}

static void block_start(void)
{
    if (block_on) { block_cancel(); return; }
    cursor_hide();
    block_on = 1;
    bx0 = cx; by0 = cy;
    block_frame();
    draw_status();
}

static void block_fill(int erase)
{
    int c0, r0, c1, r1, c, r;
    block_rect(&c0, &r0, &c1, &r1);
    block_cancel();
    push_undo();
    for (c = c0; c <= c1; c++)
        for (r = r0; r <= r1; r++)
            apply_cell(c, r, erase);
    set_msg(erase ? "Block emptied" : "Block filled");
    draw_status();
}

static void move_cursor(int nx, int ny)
{
    if (nx < 0) nx = 0;
    if (nx >= COLS) nx = COLS - 1;
    if (ny < 0) ny = 0;
    if (ny >= ROWS) ny = ROWS - 1;
    if (nx == cx && ny == cy) return;
    cursor_hide();
    if (block_on) block_unframe();
    cx = nx; cy = ny;
    if (block_on) block_frame();
    if (focus == F_FIELD) cursor_xor();
    draw_status();
}

static void go_level(int n)
{
    if (n < 0 || n >= NLEVELS || n == lv) return;
    block_cancel();
    lv = n;
    draw_level_box();
    draw_field();
}

static void select_item(int i)
{
    int old = mode == 0 ? sel_b : sel_i, n = mode == 0 ? NPAL_B : NPAL_I;
    if (i < 0) i = n - 1;
    if (i >= n) i = 0;
    if (mode == 0) sel_b = i; else sel_i = i;
    draw_pal_item(old);
    draw_pal_item(i);
    draw_selected();
}

/* steps through the panel: d is +-1 along a row, +-3 up and down */
static void pal_move(int d)
{
    int cur = mode == 0 ? sel_b : sel_i, n = mode == 0 ? NPAL_B : NPAL_I;
    if (cur + d < 0 || cur + d >= n) return;
    select_item(cur + d);
}

static void set_mode_tab(int m)
{
    if (m == mode) return;
    mode = m;
    draw_palette();
}

static void set_focus(int f)
{
    if (f == focus) return;
    if (focus == F_FIELD) cursor_hide();
    focus = f;
    focus_frame();
}

static void pick_up(void)
{
    int i, idx = cx * ROWS + cy;
    if (mode == 0) {
        for (i = 0; i < NPAL_B; i++)
            if (pal_b[i] == lv_b[lv][idx]) { select_item(i); break; }
    } else {
        u8 ic = lv_i[lv][idx];
        select_item(ic == NOICON ? 0 : ic + 1);
    }
}

static void mirror(void)
{
    int c, r;
    u8 t;
    block_cancel();
    push_undo();
    for (c = 0; c < COLS / 2; c++)
        for (r = 0; r < ROWS; r++) {
            int a = c * ROWS + r, b = (COLS - 1 - c) * ROWS + r;
            t = lv_b[lv][a]; lv_b[lv][a] = lv_b[lv][b]; lv_b[lv][b] = t;
            t = lv_i[lv][a]; lv_i[lv][a] = lv_i[lv][b]; lv_i[lv][b] = t;
        }
    dirty = 1;
    set_msg("Level mirrored");
    draw_field();
}

/* copies the marked block, or the whole level when nothing is marked */
static void copy_clip(int l)
{
    int c0 = 0, r0 = 0, c1 = COLS - 1, r1 = ROWS - 1, c, r, was_block = block_on;
    if (block_on) block_rect(&c0, &r0, &c1, &r1);
    clip_w = c1 - c0 + 1;
    clip_h = r1 - r0 + 1;
    for (c = c0; c <= c1; c++)
        for (r = r0; r <= r1; r++) {
            clip_b[(c - c0) * ROWS + (r - r0)] = lv_b[l][c * ROWS + r];
            clip_i[(c - c0) * ROWS + (r - r0)] = lv_i[l][c * ROWS + r];
        }
    have_clip = 1;
    block_cancel();
    set_msg(was_block ? "Block copied - V pastes it at the cursor" : "Level copied");
}

static int clip_is_level(void) { return have_clip && clip_w == COLS && clip_h == ROWS; }

/* a whole level is pasted over the level, a block at the cursor */
static void paste_clip(void)
{
    int c, r, ox, oy;
    if (!have_clip) { set_msg("Copy something with C first"); draw_status(); return; }
    block_cancel();
    ox = clip_is_level() ? 0 : cx;
    oy = clip_is_level() ? 0 : cy;
    push_undo();
    for (c = 0; c < clip_w && ox + c < COLS; c++)
        for (r = 0; r < clip_h && oy + r < ROWS; r++) {
            lv_b[lv][(ox + c) * ROWS + oy + r] = clip_b[c * ROWS + r];
            lv_i[lv][(ox + c) * ROWS + oy + r] = clip_i[c * ROWS + r];
        }
    dirty = 1;
    set_msg(clip_is_level() ? "Level pasted" : "Block pasted");
    draw_field();
}

static void clear_level(void)
{
    block_cancel();
    if (!ask_yes("Clear this whole level?", "")) return;
    push_undo();
    memset(lv_b[lv], 0, CELLS);
    memset(lv_i[lv], NOICON, CELLS);
    dirty = 1;
    set_msg("Level cleared");
    draw_field();
}

static int breakable_count(int l)
{
    int i, n = 0;
    for (i = 0; i < CELLS; i++)
        if (lv_b[l][i] && lv_b[l][i] != SOLID) n++;
    return n;
}

static int level_empty(int l)
{
    int i;
    for (i = 0; i < CELLS; i++) if (lv_b[l][i]) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Changing the order of the levels                                    */
/*  The game always reads exactly thirty, so a new level pushes the     */
/*  last one off the end and a deleted one leaves an empty level there. */
/* ------------------------------------------------------------------ */

static void save_set(void)
{
    _fmemcpy(set_b, lv_b, sizeof lv_b);
    _fmemcpy(set_i, lv_i, sizeof lv_i);
    set_saved = 1;
    undo_n = 0;                        /* level numbers have moved */
    dirty = 1;
}

static void copy_level(int to, int from)
{
    memcpy(lv_b[to], lv_b[from], CELLS);
    memcpy(lv_i[to], lv_i[from], CELLS);
}

static void empty_level(int l)
{
    memset(lv_b[l], 0, CELLS);
    memset(lv_i[l], NOICON, CELLS);
}

/* opens a gap at p, filled with an empty level or a copy of 'from' */
static int insert_level(int p, int from)
{
    int i;
    if (p >= NLEVELS) return 0;
    if (!level_empty(NLEVELS - 1) &&
        !ask_yes("Level 30 is not empty, and will be", "pushed off the end.  Carry on?"))
        return 0;
    save_set();
    if (from >= 0) {
        memcpy(tmp_b[0], lv_b[from], CELLS);
        memcpy(tmp_i[0], lv_i[from], CELLS);
    }
    for (i = NLEVELS - 1; i > p; i--) copy_level(i, i - 1);
    if (from >= 0) {
        memcpy(lv_b[p], tmp_b[0], CELLS);
        memcpy(lv_i[p], tmp_i[0], CELLS);
    } else {
        empty_level(p);
    }
    return 1;
}

static void delete_level(int p)
{
    int i;
    save_set();
    for (i = p; i < NLEVELS - 1; i++) copy_level(i, i + 1);
    empty_level(NLEVELS - 1);
}

static void swap_levels(int a, int b)
{
    save_set();
    memcpy(tmp_b[0], lv_b[a], CELLS); memcpy(tmp_i[0], lv_i[a], CELLS);
    copy_level(a, b);
    memcpy(lv_b[b], tmp_b[0], CELLS); memcpy(lv_i[b], tmp_i[0], CELLS);
}

/* swaps the level set with the saved copy, so U undoes and redoes */
static void undo_set(void)
{
    int l;
    for (l = 0; l < NLEVELS; l++) {
        memcpy(tmp_b[0], lv_b[l], CELLS);
        memcpy(tmp_i[0], lv_i[l], CELLS);
        _fmemcpy(lv_b[l], set_b[l], CELLS);
        _fmemcpy(lv_i[l], set_i[l], CELLS);
        _fmemcpy(set_b[l], tmp_b[0], CELLS);
        _fmemcpy(set_i[l], tmp_i[0], CELLS);
    }
    undo_n = 0;
    dirty = 1;
}

/* F4 in the designer: a new empty level straight after this one */
static void new_level_after(void)
{
    char s[64];
    if (lv + 1 >= NLEVELS) {
        set_msg("Level 30 is the last - use F6 to make room");
        draw_status();
        return;
    }
    block_cancel();
    if (!insert_level(lv + 1, -1)) return;
    lv++;
    sprintf(s, "New empty level made - it is level %02d", lv + 1);
    set_msg(s);
    draw_level_box();
    draw_field();
}

/* ------------------------------------------------------------------ */
/*  The level list (F6)                                                 */
/* ------------------------------------------------------------------ */

#define LX0 36                 /* first slot                   */
#define LY0 26
#define LW  95                 /* slot size, 6 x 5 slots       */
#define LH  48

/* the main colour of each brick, for the small pictures */
static const u8 thumb_col[32] = {
    BLACK, RED,   GREEN, GREEN, LMAGENTA, LMAGENTA, LBLUE, LBLUE,
    RED,   GREY,  WHITE, WHITE, WHITE,    WHITE,    WHITE, GREY,
    RED,   BLUE,  GREEN, CYAN,  LRED,     LRED,     LRED,  LRED,
    RED,   BLUE,  GREEN, CYAN,  DGREY,    GREEN,    GREEN, GREY };

static void draw_slot(int i)
{
    int sx = LX0 + (i % 6) * LW, sy = LY0 + (i / 6) * LH;
    int tx = sx + 22, ty = sy + 4, c, r, sel = (i == list_sel);
    char s[4];
    u8 b;

    MH();
    rect(sx, sy, sx + LW - 1, sy + LH - 1, BLACK);
    frame_x(tx - 3, ty - 3, tx + 58, ty + 40, sel ? YELLOW : BLACK, 0);
    frame_x(tx - 2, ty - 2, tx + 57, ty + 39, sel ? YELLOW : BLACK, 0);
    frame_x(tx - 1, ty - 1, tx + 56, ty + 38, DGREY, 0);
    sprintf(s, "%02d", i + 1);
    text(sx + 2, sy + 12, s, sel ? YELLOW : LGREEN);
    if (level_empty(i))
        small_text(tx + 8, ty + 15, "EMPTY", DGREY);
    else
        for (c = 0; c < COLS; c++)
            for (r = 0; r < ROWS; r++)
                if ((b = lv_b[i][c * ROWS + r]) != 0)
                    rect(tx + c * 4, ty + r * 2, tx + c * 4 + 2, ty + r * 2 + 1, thumb_col[b & 31]);
    MS();
}

static void list_msg(const char *s)
{
    MH();
    rect(28, 314, 611, 327, BLACK);
    if (s) ctext(320, 314, s, YELLOW);
    MS();
}

static void draw_list(void)
{
    int i;
    MH();
    ega_reset();
    raised(0, 0, 639, 349);
    ctext(320, 3, "Paranoid - The Thirty Levels", BLACK);
    sunken(26, 19, 613, 330, BLACK);
    for (i = 0; i < NLEVELS; i++) draw_slot(i);
    ctext2(320, 272, "\1Enter\2 edit   \1F4\2 or \1Ins\2 new level   \1Del\2 delete   \1D\2 duplicate",
           LGREEN, YELLOW);
    ctext2(320, 286, "\1<\2 \1>\2 move the level earlier or later   \1C\2 copy  \1V\2 paste  \1U\2 undo",
           LGREEN, YELLOW);
    ctext2(320, 300, "\1W\2 wipe all levels   \1Esc\2 back to the designer", LGREEN, YELLOW);
    ctext(320, 333, "Paranoid plays all thirty in order - empty ones are skipped", BLACK);
    MS();
}

static void list_select(int i)
{
    int old = list_sel;
    if (i < 0 || i >= NLEVELS || i == old) return;
    list_sel = i;
    draw_slot(old);
    draw_slot(i);
}

static int slot_hit(int mx, int my)
{
    int c, r;
    if (mx < LX0 || my < LY0) return -1;
    c = (mx - LX0) / LW;
    r = (my - LY0) / LH;
    return (c < 6 && r < 5) ? r * 6 + c : -1;
}

static void level_list(void)
{
    static u16 pmb;
    u16 mb, mx, my;
    int k, scan, ch, ext, shift, hit, done = 0;
    char s[64];

    block_cancel();
    cursor_hide();
    in_list = 1;
    list_sel = lv;
    draw_list();
    mouse_release();
    pmb = 0;

    while (!done) {
        k = 0;
        while (!key_ready() && !k) {
            if (!mouse_ok) continue;
            mouse_call(3, &mb, &mx, &my);
            if ((mb & 1) && !(pmb & 1) && (hit = slot_hit(mx, my)) >= 0) {
                if (hit == list_sel) k = 13;      /* second click edits */
                else list_select(hit);
            }
            pmb = mb;
        }
        if (!k) k = get_key();
        scan = k >> 8; ch = k & 0xFF; shift = SHIFTS & 3;
        ext = (ch == 0 || ch == 0xE0);
        if (!ext && shift && (scan == 0x4B || scan == 0x4D)) ext = 1;
        list_msg(0);

        if (ext && shift && (scan == 0x4B || scan == 0x4D)) {
            ch = scan == 0x4B ? '<' : '>';
            ext = 0;
        }
        if (ext) {
            switch (scan) {
            case 0x4B: list_select(list_sel - 1); break;
            case 0x4D: list_select(list_sel + 1); break;
            case 0x48: list_select(list_sel - 6); break;
            case 0x50: list_select(list_sel + 6); break;
            case 0x47: list_select(0); break;
            case 0x4F: list_select(NLEVELS - 1); break;
            case 0x3E: case 0x52: ch = 'n'; break;          /* F4, Ins */
            case 0x53: ch = 'x'; break;                     /* Del     */
            case 0x40: done = 1; break;                     /* F6      */
            }
        }
        switch (ch) {
        case 13: case ' ': case 27: case 9: done = 1; break;
        case 'n': case 'N':
            if (insert_level(list_sel, -1)) {
                draw_list();
                sprintf(s, "New empty level made at %02d", list_sel + 1);
                list_msg(s);
            }
            break;
        case 'x':
            if (!level_empty(list_sel)) {
                sprintf(s, "Delete level %02d?", list_sel + 1);
                if (!ask_yes(s, "Later levels move up one place")) break;
            }
            delete_level(list_sel);
            draw_list();
            list_msg("Level deleted");
            break;
        case 'd': case 'D':
            if (list_sel + 1 >= NLEVELS) { list_msg("There is no room after level 30"); break; }
            if (insert_level(list_sel + 1, list_sel)) {
                list_sel++;
                draw_list();
                list_msg("Level duplicated");
            }
            break;
        case '<': case ',':
            if (list_sel == 0) break;
            swap_levels(list_sel, list_sel - 1);
            list_sel--;
            draw_slot(list_sel + 1);
            draw_slot(list_sel);
            break;
        case '>': case '.':
            if (list_sel + 1 >= NLEVELS) break;
            swap_levels(list_sel, list_sel + 1);
            list_sel++;
            draw_slot(list_sel - 1);
            draw_slot(list_sel);
            break;
        case 'c': case 'C':
            block_on = 0;
            copy_clip(list_sel);
            list_msg("Level copied");
            break;
        case 'v': case 'V':
            if (!clip_is_level()) { list_msg("Copy a whole level with C first"); break; }
            if (!level_empty(list_sel)) {
                sprintf(s, "Paste over level %02d?", list_sel + 1);
                if (!ask_yes(s, "")) break;
            }
            save_set();
            memcpy(lv_b[list_sel], clip_b, CELLS);
            memcpy(lv_i[list_sel], clip_i, CELLS);
            draw_slot(list_sel);
            list_msg("Level pasted");
            break;
        case 'u': case 'U':
            if (!set_saved) { list_msg("Nothing to undo"); break; }
            undo_set();
            draw_list();
            list_msg("Undone - press U again to redo");
            break;
        case 'w': case 'W':
            if (!ask_yes("Wipe all thirty levels?", "(U will bring them back)")) break;
            save_set();
            for (k = 0; k < NLEVELS; k++) empty_level(k);
            draw_list();
            list_msg("All levels wiped - a clean start");
            break;
        }
    }

    in_list = 0;
    lv = list_sel;
    msg[0] = 0;
    mouse_release();
    draw_all();
}

/* ------------------------------------------------------------------ */
/*  Files                                                               */
/* ------------------------------------------------------------------ */

static void save(void)
{
    int i;
    char s[64];
    block_cancel();
    for (i = 0; i < NLEVELS; i++)
        if (!breakable_count(i) && !level_empty(i)) {
            sprintf(s, "Level %d has no breakable bricks", i + 1);
            if (!ask_yes(s, "Save anyway?")) return;
            break;
        }
    /* Backups come first, and nothing is saved if they fail:          */
    /*   LEVELS.ORG  the levels as they were before the first save -    */
    /*               made once and never written over                   */
    /*   LEVELS.BAK  the levels as they were before this save           */
    if (!file_exists("LEVELS.ORG")) {
        /* version 1.0 kept the original in LEVELS.BAK */
        const char *from = file_exists("LEVELS.BAK") ? "LEVELS.BAK" : "LEVELS.PAR";
        if (!copy_file(from, "LEVELS.ORG")) {
            remove("LEVELS.ORG");
            dialog("Could not make a copy of the", "original levels - nothing saved", "Press a key");
            return;
        }
    }
    remove("LEVELS.BAK");
    if (!copy_file("LEVELS.PAR", "LEVELS.BAK")) {
        dialog("Could not make a backup copy", "of Levels.PAR - nothing saved", "Press a key");
        return;
    }
    /* write a new file first, so a failed save cannot spoil the old one */
    remove("LEVELS.$NW");
    if (write_levels("LEVELS.$NW", &lv_b[0][0], &lv_i[0][0]) &&
        remove("LEVELS.PAR") == 0 && rename("LEVELS.$NW", "LEVELS.PAR") == 0) {
        dirty = 0;
        set_msg("All thirty levels saved");
    } else {
        recover_test_files();
        dialog("Could not write Levels.PAR", "Check for a full or protected disk", "Press a key");
    }
    draw_status();
}

static void reload(void)
{
    int k;
    const char *name;
    block_cancel();
    k = dialog("Load which levels?", "Your changes so far will be lost",
               "\1L\2ast save  \1B\2ackup  \1O\2riginal  \1Esc\2");
    switch (k) {
    case 'l': case 'L': name = "LEVELS.PAR"; break;
    case 'b': case 'B': name = "LEVELS.BAK"; break;
    case 'o': case 'O': name = file_exists("LEVELS.ORG") ? "LEVELS.ORG" : "LEVELS.PAR"; break;
    default: return;
    }
    if (!file_exists(name) || !load_levels(name)) {
        dialog("There is no backup to load yet", "", "Press a key");
        return;
    }
    /* anything but the last save still has to be saved to be played */
    dirty = (name[7] != 'P');
    undo_n = 0;
    set_saved = 0;
    set_msg(name[7] == 'P' ? "Last saved levels loaded" :
            name[7] == 'B' ? "Backup loaded - F2 saves it for the game" :
                             "Original levels loaded - F2 saves them for the game");
    draw_level_box();
    draw_field();
}

/* run Paranoid with the current level moved to the front */
static void test_play(void)
{
    int i, ok, had_scores;

    block_cancel();
    if (!breakable_count(lv)) {
        dialog("This level has no breakable bricks", "so there is nothing to play!", "Press a key");
        return;
    }
    if (!file_exists("PARANOID.COM")) {
        dialog("Cannot find Paranoid.COM", "in this directory", "Press a key");
        return;
    }
    for (i = 0; i < NLEVELS; i++) {
        memcpy(tmp_b[i], lv_b[(lv + i) % NLEVELS], CELLS);
        memcpy(tmp_i[i], lv_i[(lv + i) % NLEVELS], CELLS);
    }
    remove("LEVELS.$$$");
    if (rename("LEVELS.PAR", "LEVELS.$$$") != 0 ||
        !write_levels("LEVELS.PAR", &tmp_b[0][0], &tmp_i[0][0])) {
        recover_test_files();
        dialog("Could not prepare the test", "Check for a full or protected disk", "Press a key");
        return;
    }
    had_scores = file_exists("HISCORES.PAR");
    if (had_scores) copy_file("HISCORES.PAR", "HISCORES.$$$");

    MH();
    set_mode(3);
    printf("Starting Paranoid with level %d as level one...\n", lv + 1);
    ok = spawnl(P_WAIT, "PARANOID.COM", "PARANOID.COM", NULL) != -1;

    remove("LEVELS.PAR");
    rename("LEVELS.$$$", "LEVELS.PAR");
    if (had_scores) {
        remove("HISCORES.PAR");
        rename("HISCORES.$$$", "HISCORES.PAR");
    } else {
        remove("HISCORES.PAR");
    }

    set_mode(0x10);
    mouse_init();
    MH();
    set_msg(ok ? "Welcome back to the designer" : "Paranoid could not be started");
    draw_all();
    MS();
}

static void quit_check(void)
{
    int k;
    if (!dirty) goto leave;
    k = dialog("Your levels have been changed.", "Save them before leaving?",
               "\1Y\2es  \1N\2o  \1Esc\2 stay");
    if (k == 27) return;
    if (k == 'y' || k == 'Y') {
        save();
        if (dirty) return;
    } else if (k != 'n' && k != 'N') return;
leave:
    MH();
    set_mode(3);
    printf("Paranoid Level Designer - thanks for playing!\n");
    exit(0);
}

/* ------------------------------------------------------------------ */
/*  Title screen, in the style of the game's attract mode               */
/* ------------------------------------------------------------------ */

static const u8 ball[6] = { 0x3C, 0x7E, 0xFF, 0xFF, 0x7E, 0x3C };

static const char scroll_text[] =
    "                        "
    "Welcome to the Paranoid level designer ...  "
    "Build thirty screens of your very own ball-bouncing fun ...  "
    "Press F1 at any time for help ...  "
    "Press F6 to put your levels in any order you like ...  "
    "Press F5 to try out your level in Paranoid ...  "
    "Happy designing!  ...  ";

static void vsync(void)
{
    while (inp(0x3DA) & 8);
    while (!(inp(0x3DA) & 8));
}

static void title_screen(void)
{
    int bx[6] = { 80, 340, 500, 90, 330, 520 };
    int by[6] = { 60, 20,  30,  210, 250, 140 };
    int dx[6] = { 2, -2, 2, -2, 2, -2 };
    int dy[6] = { 1, 1, -1, -1, 1, 1 };
    int i, frame = 0, pos = 0, len = strlen(scroll_text);
    char win[25];

    ega_reset();
    rect(0, 0, 639, 349, BLACK);
    raised(0, 0, 639, 290);
    sunken(14, 10, 625, 280, BLACK);
    raised(182, 90, 457, 197);
    raised(198, 102, 441, 185);
    draw_logo(229, 114, BLUE);
    ctext(320, 160, "Level Designer", BLACK);
    raised(208, 302, 431, 349);
    sunken(222, 312, 417, 339, BLACK);
    small_text(553, 306, "VERSION 1.1", WHITE);
    small_text(549, 333, "PRESS SPACE", WHITE);
    small_text(573, 341, "TO START", WHITE);
    small_text(1, 341, "LEVEL DESIGNER", WHITE);
    for (i = 0; i < 6; i++) mono(bx[i], by[i], ball, 1, 6, WHITE, 1);

    while (!key_ready()) {
        vsync();
        for (i = 0; i < 6; i++) {
            int nx = bx[i] + dx[i], ny = by[i] + dy[i];
            mono(bx[i], by[i], ball, 1, 6, WHITE, 1);
            if (nx < 16 || nx > 615) { dx[i] = -dx[i]; nx = bx[i] + dx[i]; }
            if (ny < 12 || ny > 272) { dy[i] = -dy[i]; ny = by[i] + dy[i]; }
            if (nx + 7 >= 182 && nx <= 457 && ny + 5 >= 90 && ny <= 197) {
                if (bx[i] + 7 < 182 || bx[i] > 457) dx[i] = -dx[i];
                else dy[i] = -dy[i];
                nx = bx[i] + dx[i]; ny = by[i] + dy[i];
            }
            bx[i] = nx; by[i] = ny;
            mono(bx[i], by[i], ball, 1, 6, WHITE, 1);
        }
        if (++frame % 9 == 0) {
            for (i = 0; i < 24; i++) win[i] = scroll_text[(pos + i) % len];
            win[24] = 0;
            rect(224, 314, 415, 337, BLACK);
            text(224, 318, win, LGREEN);
            pos = (pos + 1) % len;
        }
    }
    get_key();
}

/* ------------------------------------------------------------------ */
/*  Main loop                                                           */
/* ------------------------------------------------------------------ */

static int pal_hit(int mx, int my)
{
    int i, x, y, n = mode == 0 ? NPAL_B : NPAL_I, w = mode == 0 ? 32 : 16;
    for (i = 0; i < n; i++) {
        pal_pos(i, &x, &y);
        if (mx >= x - 3 && mx <= x + w + 2 && my >= y - 1 && my <= y + 12) return i;
    }
    return -1;
}

static int is_arrow(int scan) { return scan == 0x48 || scan == 0x50 || scan == 0x4B || scan == 0x4D; }

static void handle_key(int k)
{
    int scan = k >> 8, ch = k & 0xFF;
    int shift = SHIFTS & 3;
    int ext = (ch == 0 || ch == 0xE0);

    /* shifted arrows on the numeric keypad arrive as digits */
    if (!ext && shift && is_arrow(scan)) { ext = 1; ch = 0; }

    /* keys that work wherever the focus is */
    if (ext) {
        switch (scan) {
        case 0x3B: show_help(); return;
        case 0x3C: save(); return;
        case 0x3D: reload(); return;
        case 0x3E: new_level_after(); return;              /* F4 */
        case 0x3F: test_play(); return;
        case 0x40: level_list(); return;                   /* F6 */
        case 0x49: go_level(lv - 1); return;
        case 0x51: go_level(lv + 1); return;
        case 0x0F: set_focus((focus + F_COUNT - 1) % F_COUNT); return;
        case 0x73: pal_move(-1); return;                   /* ctrl-arrows */
        case 0x74: pal_move(1); return;
        case 0x8D: pal_move(-3); return;
        case 0x91: pal_move(3); return;
        }
    } else {
        switch (ch) {
        case 9:  set_focus((focus + 1) % F_COUNT); return;
        case 27:
            if (block_on) block_cancel();
            else if (focus != F_FIELD) set_focus(F_FIELD);
            else quit_check();
            return;
        case 'i': case 'I': set_mode_tab(!mode); return;
        case ',': case '<': case '[': case '-': pal_move(-1); return;
        case '.': case '>': case ']': case '+': case '=': pal_move(1); return;
        case 'g': case 'G': show_grid = !show_grid; draw_field(); return;
        case 'h': case 'H': show_icons = !show_icons; draw_field(); return;
        case 8: case 'u': case 'U': case 26: undo(); return;
        }
    }

    if (focus == F_PANEL) {
        if (ext) {
            switch (scan) {
            case 0x4B: pal_move(-1); break;
            case 0x4D: pal_move(1); break;
            case 0x48: pal_move(-3); break;
            case 0x50: pal_move(3); break;
            }
        } else if (ch == 13 || ch == ' ') {
            set_focus(F_FIELD);
        }
        return;
    }

    if (focus == F_LEVEL) {
        if (ext && (scan == 0x4B || scan == 0x48)) go_level(lv - 1);
        else if (ext && (scan == 0x4D || scan == 0x50)) go_level(lv + 1);
        else if (ch == 13 || ch == ' ') level_list();
        return;
    }

    /* the play area */
    if (ext) {
        switch (scan) {
        case 0x48: move_cursor(cx, cy - 1); break;
        case 0x50: move_cursor(cx, cy + 1); break;
        case 0x4B: move_cursor(cx - 1, cy); break;
        case 0x4D: move_cursor(cx + 1, cy); break;
        case 0x47: move_cursor(0, cy); break;
        case 0x4F: move_cursor(COLS - 1, cy); break;
        case 0x53:
            if (block_on) block_fill(1);
            else { push_undo(); apply(cx, cy, 1); }
            break;
        case 0x52: push_undo(); apply(cx, cy, 0); break;
        }
        if (shift && is_arrow(scan) && !block_on) {
            push_undo();
            apply(cx, cy, 0);
        }
        return;
    }
    switch (ch) {
    case ' ': case 13:
        if (block_on) block_fill(0);
        else { push_undo(); apply(cx, cy, 0); }
        break;
    case 'b': case 'B': block_start(); break;
    case 'p': case 'P': pick_up(); break;
    case 'm': case 'M': mirror(); break;
    case 'c': case 'C': copy_clip(lv); draw_status(); break;
    case 'v': case 'V': paste_clip(); break;
    case 'n': case 'N': clear_level(); break;
    }
}

/* mouse clicks outside the play area */
static void panel_click(int mx, int my)
{
    int hit = pal_hit(mx, my);
    if (hit >= 0) { select_item(hit); return; }
    if (my >= 76 && my <= 92 && mx >= 512 && mx <= 567) { set_mode_tab(0); return; }
    if (my >= 76 && my <= 92 && mx >= 576 && mx <= 623) { set_mode_tab(1); return; }
    if (my >= 50 && my <= 69 && mx >= 506 && mx <= 629) {
        if (mx <= 545)      go_level(lv - 1);
        else if (mx >= 590) go_level(lv + 1);
        else                level_list();
        return;
    }
    if (my >= 292 && my <= 343 && mx >= 506 && mx <= 629) {
        int row = (my - 292) / 12;
        if (row > 3) row = 3;
        mouse_release();
        switch (row * 2 + (mx >= 568)) {
        case 0: show_help(); break;
        case 1: save(); break;
        case 2: reload(); break;
        case 3: new_level_after(); break;
        case 4: test_play(); break;
        case 5: level_list(); break;
        default: quit_check(); break;
        }
    }
}

int main(void)
{
    static const char *need[3] = { "BITMAPS.PAR", "FONTS.PAR", "LEVELS.PAR" };
    unsigned long last_blink = 0, last_anim = 0;
    u16 mb = 0, mx, my, prev_mb = 0;
    int i, missing = 0, stroke_c = -1, stroke_r = -1;

    recover_test_files();
    for (i = 0; i < 3; i++)
        if (!file_exists(need[i])) {
            if (!missing) printf("\r\nThe following files are missing :\r\n\r\n");
            printf("     %s\r\n", need[i]);
            missing = 1;
        }
    if (missing) {
        printf("\r\nPlease run the level designer from the Paranoid directory.\r\n");
        return 1;
    }
    if (!load_file("BITMAPS.PAR", bmp, BMP_SIZE) ||
        !load_file("FONTS.PAR", (u8 __far *)font, sizeof font) ||
        !load_levels("LEVELS.PAR")) {
        printf("\r\nCorrupt data file - please re-copy Paranoid from the release disk.\r\n");
        return 1;
    }

    set_mode(0x10);
    title_screen();
    mouse_init();
    draw_all();
    cursor_xor();

    for (;;) {
        unsigned long t = TICKS;

        if (key_ready()) {
            handle_key(get_key());
            last_blink = t;
            if (focus == F_FIELD && !cur_vis) cursor_xor();
        }

        if (mouse_ok) {
            mouse_call(3, &mb, &mx, &my);
            if (mb & 3) {
                if (mx >= FX && mx < FX + COLS * 32 && my >= FY && my < FY + ROWS * 12) {
                    int c = (mx - FX) / 32, r = (my - FY) / 12;
                    if (!(prev_mb & 3)) {
                        block_cancel();
                        set_focus(F_FIELD);
                        push_undo();
                    }
                    if (c != stroke_c || r != stroke_r || !(prev_mb & 3)) {
                        move_cursor(c, r);
                        apply(c, r, (mb & 2) != 0);
                        stroke_c = c; stroke_r = r;
                    }
                } else if (!(prev_mb & 3)) {
                    panel_click(mx, my);
                    mouse_call(3, &mb, 0, 0);
                }
            } else {
                stroke_c = stroke_r = -1;
            }
            prev_mb = mb;
        }

        if (focus == F_FIELD && t - last_blink >= 5) {
            last_blink = t;
            cursor_xor();
        }
        if (t - last_anim >= 3) {
            last_anim = t;
            anim++;
            if (mode == 1)
                for (i = 1; i < NPAL_I; i++) draw_pal_item(i);
        }
        if (msg[0] && t > msg_until) {
            msg[0] = 0;
            draw_status();
        }
    }
}
