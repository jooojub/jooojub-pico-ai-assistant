#include "app.h"
#include "DEV_Config.h"
#include "LCD_1in3.h"
#include "GUI_Paint.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"

/*
 * Face proportions ported from StackChan (320x240) to our 240x240 LCD.
 * Horizontal scale: 0.75  |  Vertical scale: 1.0 (same height)
 * Origin: display center (120, 120)
 */

/* Buttons */
#define BTN_A   15   /* KEY_A  — talking      */
#define BTN_B   17   /* KEY_B  — shy / love   */
#define BTN_X   19   /* KEY_X  — happy / scroll */

/* Eyes: StackChan offset (±70, -16) scaled × 0.75 */
#define EYE_LEFT_X      68
#define EYE_RIGHT_X     172
#define EYE_Y           104
#define EYE_R            7

/* Mouth neutral: StackChan weight=0, 90×6 scaled × 0.75 */
#define MOUTH_X1        86
#define MOUTH_X2        154
#define MOUTH_Y1        143
#define MOUTH_Y2        149
/* Mouth open: same width as closed, extends down, bottom corners r=12 */
#define MOUTH_OPEN_Y2   165
#define MOUTH_OPEN_R    12

/* Shy decorator: StackChan offset (±108, 28) scaled × 0.75 */
#define SHY_LEFT_X      39
#define SHY_RIGHT_X     201
#define SHY_Y           148

/* Heart decorator: StackChan offset (108, -70) scaled × 0.75 */
#define HEART_X         201
#define HEART_Y          50
#define HEART_S          12

/* Colors */
#define PINK    0xFD56   /* blush pink  */
#define RED     0xF800   /* heart red   */

/* Speech bubble — StackChan style: pill shape + upward arrow */
#define BUBBLE_X1        10
#define BUBBLE_X2       230
#define BUBBLE_Y1       182   /* top of rounded rect */
#define BUBBLE_Y2       232   /* 50 px tall, matching StackChan 52 px */
#define BUBBLE_R        12    /* corner radius */
#define BUBBLE_ARROW_CX 150   /* arrow x: proportional to StackChan's 200/320 × 240 */
#define BUBBLE_ARROW_H   10   /* arrow height (apex to base) */
#define BUBBLE_ARROW_HW   9   /* arrow half-width at base */
#define SCROLL_PX         3   /* pixels scrolled per TALK_PHASE */

/* Timing */
#define BLINK_INTERVAL_MS   3000
#define BLINK_CLOSE_MS       150
#define TALK_CYCLES            5
#define TALK_PHASE_MS        120
#define LOVE_BLINK_MS        400
#define LOVE_BLINKS            3
#define HAPPY_DURATION_MS   8000
#define LOOP_DELAY_MS         16

typedef enum { STATE_IDLE, STATE_TALKING, STATE_LOVE, STATE_HAPPY } AppState;

static UWORD *g_img;
static char g_talk_text[256]  = "Hello world";
static char g_happy_text[256] = "Hello! I am your AI assistant. Nice to meet you today!";

/* Serial command state — shared between parser and main loop */
typedef enum { CMD_NONE, CMD_TALK, CMD_LOVE, CMD_HAPPY, CMD_IDLE } PendingCmd;
static volatile PendingCmd g_pending_cmd = CMD_NONE;
static char g_pending_arg[256] = {0};

/* ------------------------------------------------------------------ */
/* Integer sqrt (Babylonian)                                           */
/* ------------------------------------------------------------------ */
static int isqrt_i(int n) {
    if (n <= 0) return 0;
    int x = n, y = (x + 1) / 2;
    while (y < x) { x = y; y = (x + n / x) / 2; }
    return x;
}

/* ------------------------------------------------------------------ */
/* Heart: two circles for bumps (V-notch gap at top) + V tip.        */
/* V starts at dy_vs (before circle bottom) to avoid abrupt          */
/* narrowing at the circle edge. dy/8 tilt leans it clockwise.       */
/* ------------------------------------------------------------------ */
static void fill_heart(int cx, int cy, int s, UWORD color) {
    int r      = s / 2;
    int dy_vs  = -(r / 3);
    int ly_vs  = dy_vs + r;
    int sq_vs  = r * r - ly_vs * ly_vs;
    int hw_vs  = r + (sq_vs > 0 ? isqrt_i(sq_vs) : 0);
    int v_span = s - dy_vs;

    for (int dy = -s; dy <= s; dy++) {
        int ccx = cx + dy / 4;
        int ly  = dy + r;
        if (dy < -r) {
            int sq = r * r - ly * ly;
            if (sq < 0) continue;
            int dx = isqrt_i(sq);
            Paint_DrawLine(ccx - r - dx, cy + dy, ccx - r + dx, cy + dy,
                           color, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
            Paint_DrawLine(ccx + r - dx, cy + dy, ccx + r + dx, cy + dy,
                           color, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        } else if (dy <= dy_vs) {
            int sq = r * r - ly * ly;
            if (sq < 0) continue;
            int dx = isqrt_i(sq);
            Paint_DrawLine(ccx - r - dx, cy + dy, ccx + r + dx, cy + dy,
                           color, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        } else {
            int w = hw_vs * (s - dy) / v_span;
            if (w <= 0) break;
            Paint_DrawLine(ccx - w, cy + dy, ccx + w, cy + dy,
                           color, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Rounded-rectangle fill (scanline, inclusive y bounds).             */
/* Fixes Paint_DrawRectangle's exclusive-Yend issue for corners.      */
/* ------------------------------------------------------------------ */
static void draw_rounded_rect_filled(int x1, int y1, int x2, int y2, int r, UWORD color) {
    for (int y = y1; y <= y2; y++) {
        int xl, xr;
        if (y < y1 + r) {
            int dy = y1 + r - y;
            int dx = isqrt_i(r * r - dy * dy);
            xl = x1 + r - dx;  xr = x2 - r + dx;
        } else if (y > y2 - r) {
            int dy = y - (y2 - r);
            int dx = isqrt_i(r * r - dy * dy);
            xl = x1 + r - dx;  xr = x2 - r + dx;
        } else {
            xl = x1;  xr = x2;
        }
        if (xl <= xr)
            Paint_DrawLine(xl, y, xr, y, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    }
}

/* ------------------------------------------------------------------ */
/* Eyes                                                                */
/* ------------------------------------------------------------------ */
static void draw_eyes_open(void) {
    Paint_DrawCircle(EYE_LEFT_X,  EYE_Y, EYE_R, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawCircle(EYE_RIGHT_X, EYE_Y, EYE_R, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}

/* Blink: eyelid closes from top (thin line = fully closed) */
static void draw_eyes_closed(void) {
    Paint_DrawLine(EYE_LEFT_X  - EYE_R, EYE_Y, EYE_LEFT_X  + EYE_R, EYE_Y,
                   WHITE, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(EYE_RIGHT_X - EYE_R, EYE_Y, EYE_RIGHT_X + EYE_R, EYE_Y,
                   WHITE, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
}

/*
 * Happy: keep top 70 % of eye circle.
 * StackChan Happy → weight 72, rotation 155° → eyelid covers ~28 % from top,
 * container rotated so "top" becomes lower edge → net effect: bottom 30 % erased.
 */
static void draw_eyes_happy(void) {
    draw_eyes_open();
    /* erase bottom 30 % (= 6 px for r=10) */
    int erase_y = EYE_Y + EYE_R - (EYE_R * 2 * 30) / 100;
    Paint_DrawRectangle(EYE_LEFT_X  - EYE_R - 1, erase_y,
                        EYE_LEFT_X  + EYE_R + 1, EYE_Y + EYE_R + 1,
                        BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawRectangle(EYE_RIGHT_X - EYE_R - 1, erase_y,
                        EYE_RIGHT_X + EYE_R + 1, EYE_Y + EYE_R + 1,
                        BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}

/* ------------------------------------------------------------------ */
/* Mouth                                                               */
/* ------------------------------------------------------------------ */
static void draw_mouth_closed(void) {
    /* StackChan weight=0: 90×6 flat bar */
    for (int y = MOUTH_Y1; y <= MOUTH_Y2; y++)
        Paint_DrawLine(MOUTH_X1, y, MOUTH_X2, y, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
}

static void draw_mouth_open(void) {
    /* Same width as closed, rounded all four corners */
    draw_rounded_rect_filled(MOUTH_X1, MOUTH_Y1, MOUTH_X2, MOUTH_OPEN_Y2, MOUTH_OPEN_R, WHITE);
}

/* ------------------------------------------------------------------ */
/* Shy decorator: 3 diagonal brush-stroke lines per cheek             */
/* Approximates StackChan's decorator_shy (39×18 px brush marks)      */
/* ------------------------------------------------------------------ */
static void draw_shy(void) {
    for (int i = 0; i < 3; i++) {
        int y = SHY_Y - 4 + i * 5;
        /* left cheek: strokes slant toward center (/ direction) */
        Paint_DrawLine(SHY_LEFT_X - 7, y + 3,
                       SHY_LEFT_X + 7, y - 3,
                       PINK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        /* right cheek: mirror (\ direction) */
        Paint_DrawLine(SHY_RIGHT_X + 7, y + 3,
                       SHY_RIGHT_X - 7, y - 3,
                       PINK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    }
}

/* ------------------------------------------------------------------ */
/* Speech bubble: pill-shaped white box + upward triangle arrow       */
/* Inspired by StackChan's DefaultSpeechBubble (LV_RADIUS_CIRCLE +   */
/* default_bubble_arrow image at offset +40 right of center).         */
/* ------------------------------------------------------------------ */
static void draw_speech_bubble(void) {
    /* Arrow: filled triangle pointing up at BUBBLE_ARROW_CX */
    for (int i = 0; i <= BUBBLE_ARROW_H; i++) {
        int w = i * BUBBLE_ARROW_HW / BUBBLE_ARROW_H;
        int y = BUBBLE_Y1 - BUBBLE_ARROW_H + i;
        Paint_DrawLine(BUBBLE_ARROW_CX - w, y, BUBBLE_ARROW_CX + w, y,
                       WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    }
    /* Pill: scanline-based rounded rect (avoids Rectangle exclusive-Yend bug) */
    draw_rounded_rect_filled(BUBBLE_X1, BUBBLE_Y1, BUBBLE_X2, BUBBLE_Y2, BUBBLE_R, WHITE);
}

/* ------------------------------------------------------------------ */
/* Redraw helpers                                                      */
/* ------------------------------------------------------------------ */
static void redraw_idle(int eyes_open) {
    Paint_Clear(BLACK);
    if (eyes_open) draw_eyes_open(); else draw_eyes_closed();
    draw_mouth_closed();
    LCD_1IN3_Display(g_img);
}

static void redraw_talk(int open, int scroll_off) {
    Paint_Clear(BLACK);
    draw_eyes_open();
    if (open) draw_mouth_open(); else draw_mouth_closed();

    draw_speech_bubble();

    /* Text: black on white, clipped to bubble interior */
    int text_x1 = BUBBLE_X1 + BUBBLE_R + 4;
    int text_x2 = BUBBLE_X2 - BUBBLE_R - 4;
    int ty = BUBBLE_Y1 + ((BUBBLE_Y2 - BUBBLE_Y1) - Font16.Height) / 2;
    int tx = text_x1 - scroll_off;
    const char *ptr = g_talk_text;
    /* Skip chars scrolled off the left */
    while (tx < text_x1 && *ptr) { tx += Font16.Width; ptr++; }
    if (*ptr && tx < text_x2) {
        /* Limit to chars that fit before the right edge */
        int vis = (text_x2 - tx) / (int)Font16.Width + 1;
        int slen = (int)strlen(ptr);
        if (vis > slen) vis = slen;
        char buf[64];
        if (vis > 63) vis = 63;
        memcpy(buf, ptr, (size_t)vis);
        buf[vis] = '\0';
        Paint_DrawString_EN((UWORD)tx, (UWORD)ty, buf, &Font16, BLACK, WHITE);
    }

    LCD_1IN3_Display(g_img);
}

static void redraw_love(int heart_on) {
    Paint_Clear(BLACK);
    draw_eyes_open();
    draw_mouth_closed();
    draw_shy();
    if (heart_on) fill_heart(HEART_X, HEART_Y, HEART_S, RED);
    LCD_1IN3_Display(g_img);
}

static void redraw_happy(int scroll_off) {
    Paint_Clear(BLACK);
    draw_eyes_happy();
    draw_mouth_closed();

    draw_speech_bubble();

    int text_x1 = BUBBLE_X1 + BUBBLE_R + 4;
    int text_x2 = BUBBLE_X2 - BUBBLE_R - 4;
    int ty  = BUBBLE_Y1 + ((BUBBLE_Y2 - BUBBLE_Y1) - Font16.Height) / 2;
    int tx  = text_x1 - scroll_off;
    const char *ptr = g_happy_text;
    while (tx < text_x1 && *ptr) { tx += Font16.Width; ptr++; }
    if (*ptr && tx < text_x2) {
        int vis = (text_x2 - tx) / (int)Font16.Width + 1;
        int slen = (int)strlen(ptr);
        if (vis > slen) vis = slen;
        char buf[64];
        if (vis > 63) vis = 63;
        memcpy(buf, ptr, (size_t)vis);
        buf[vis] = '\0';
        Paint_DrawString_EN((UWORD)tx, (UWORD)ty, buf, &Font16, BLACK, WHITE);
    }

    LCD_1IN3_Display(g_img);
}

/* ------------------------------------------------------------------ */
/* Serial command parser (non-blocking, called every loop iteration)  */
/* Protocol: "<CMD> [arg]\n"                                          */
/*   TALK <text>   – talk animation with custom text                  */
/*   LOVE          – shy/love animation                               */
/*   HAPPY <text>  – happy animation with scrolling text              */
/*   IDLE          – return to idle                                   */
/* ------------------------------------------------------------------ */
static void poll_serial(void) {
    static char buf[258];
    static int  len = 0;

    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (c == '\n' || c == '\r') {
            if (len == 0) continue;
            buf[len] = '\0';
            len = 0;

            if (strncmp(buf, "TALK ", 5) == 0) {
                strncpy(g_pending_arg, buf + 5, sizeof(g_pending_arg) - 1);
                g_pending_cmd = CMD_TALK;
            } else if (strcmp(buf, "LOVE") == 0) {
                g_pending_cmd = CMD_LOVE;
            } else if (strncmp(buf, "HAPPY ", 6) == 0) {
                strncpy(g_pending_arg, buf + 6, sizeof(g_pending_arg) - 1);
                g_pending_cmd = CMD_HAPPY;
            } else if (strcmp(buf, "IDLE") == 0) {
                g_pending_cmd = CMD_IDLE;
            }
            printf("OK\n");
        } else if (len < (int)(sizeof(buf) - 2)) {
            buf[len++] = (char)c;
        }
    }
}

/* ------------------------------------------------------------------ */
/* app_run                                                             */
/* ------------------------------------------------------------------ */
void app_run(void) {
    DEV_Module_Init();
    DEV_SET_PWM(50);
    LCD_1IN3_Init(HORIZONTAL);
    LCD_1IN3_Clear(BLACK);

    g_img = (UWORD *)malloc(LCD_1IN3_WIDTH * LCD_1IN3_HEIGHT * 2);
    if (!g_img) return;

    Paint_NewImage((UBYTE *)g_img, LCD_1IN3_WIDTH, LCD_1IN3_HEIGHT, ROTATE_0, BLACK);
    Paint_SetScale(65);
    Paint_Clear(BLACK);

    DEV_KEY_Config(BTN_A);
    DEV_KEY_Config(BTN_B);
    DEV_KEY_Config(BTN_X);

    draw_eyes_open();
    draw_mouth_closed();
    LCD_1IN3_Display(g_img);
    printf("READY\n");

    AppState state       = STATE_IDLE;
    int idle_ms          = 0;
    int blinking         = 0;
    int blink_ms         = 0;
    int talk_phase_ms    = 0;
    int talk_open        = 0;
    int talk_count       = 0;
    int scroll_off       = 0;
    int love_phase_ms    = 0;
    int love_hearts_on   = 1;
    int love_blink_count = 0;
    int happy_ms         = 0;
    int happy_scroll_off = 0;

    while (1) {
        if (state == STATE_IDLE) {

            if (DEV_Digital_Read(BTN_A) == 0) {
                state = STATE_TALKING;
                talk_phase_ms = 0; talk_open = 1; talk_count = 0;
                scroll_off = 0;
                blinking = idle_ms = 0;
                redraw_talk(1, 0);

            } else if (DEV_Digital_Read(BTN_B) == 0) {
                state = STATE_LOVE;
                love_phase_ms = 0; love_hearts_on = 1; love_blink_count = 0;
                blinking = idle_ms = 0;
                redraw_love(1);

            } else if (DEV_Digital_Read(BTN_X) == 0) {
                state = STATE_HAPPY;
                happy_ms = 0; happy_scroll_off = 0;
                blinking = idle_ms = 0;
                redraw_happy(0);

            } else if (!blinking) {
                idle_ms += LOOP_DELAY_MS;
                if (idle_ms >= BLINK_INTERVAL_MS) {
                    redraw_idle(0);
                    blinking = 1; blink_ms = 0; idle_ms = 0;
                }
            } else {
                blink_ms += LOOP_DELAY_MS;
                if (blink_ms >= BLINK_CLOSE_MS) {
                    redraw_idle(1);
                    blinking = 0;
                }
            }

        } else if (state == STATE_TALKING) {

            talk_phase_ms += LOOP_DELAY_MS;
            if (talk_phase_ms >= TALK_PHASE_MS) {
                talk_phase_ms = 0; talk_open = !talk_open; talk_count++;
                /* Advance scroll if text is wider than bubble */
                int vis_w  = BUBBLE_X2 - BUBBLE_X1 - 10;
                int text_w = (int)strlen(g_talk_text) * (int)Font16.Width;
                if (text_w > vis_w) {
                    scroll_off += SCROLL_PX;
                    if (scroll_off > text_w) scroll_off = 0;
                }
                if (talk_count >= TALK_CYCLES * 2) {
                    state = STATE_IDLE;
                    scroll_off = 0;
                    redraw_idle(1);
                } else {
                    redraw_talk(talk_open, scroll_off);
                }
            }

        } else if (state == STATE_LOVE) {

            love_phase_ms += LOOP_DELAY_MS;
            if (love_phase_ms >= LOVE_BLINK_MS) {
                love_phase_ms = 0;
                love_hearts_on = !love_hearts_on;
                love_blink_count++;
                if (love_blink_count >= LOVE_BLINKS * 2) {
                    state = STATE_IDLE;
                    redraw_idle(1);
                } else {
                    redraw_love(love_hearts_on);
                }
            }

        } else { /* STATE_HAPPY */

            happy_ms += LOOP_DELAY_MS;

            int h_text_w = (int)strlen(g_happy_text) * (int)Font16.Width;
            int h_vis_w  = BUBBLE_X2 - BUBBLE_X1 - 10;
            if (h_text_w > h_vis_w) {
                happy_scroll_off += 4;
                if (happy_scroll_off > h_text_w) happy_scroll_off = 0;
            }
            if (happy_ms >= HAPPY_DURATION_MS) {
                state = STATE_IDLE;
                happy_scroll_off = 0;
                redraw_idle(1);
            } else {
                redraw_happy(happy_scroll_off);
            }
        }

        /* Process any pending serial command */
        poll_serial();
        if (g_pending_cmd != CMD_NONE) {
            PendingCmd cmd = g_pending_cmd;
            g_pending_cmd = CMD_NONE;

            if (cmd == CMD_TALK) {
                strncpy(g_talk_text, g_pending_arg, sizeof(g_talk_text) - 1);
                state = STATE_TALKING;
                talk_phase_ms = 0; talk_open = 1; talk_count = 0; scroll_off = 0;
                blinking = idle_ms = 0;
                redraw_talk(1, 0);
            } else if (cmd == CMD_LOVE) {
                state = STATE_LOVE;
                love_phase_ms = 0; love_hearts_on = 1; love_blink_count = 0;
                blinking = idle_ms = 0;
                redraw_love(1);
            } else if (cmd == CMD_HAPPY) {
                strncpy(g_happy_text, g_pending_arg, sizeof(g_happy_text) - 1);
                state = STATE_HAPPY;
                happy_ms = 0; happy_scroll_off = 0;
                blinking = idle_ms = 0;
                redraw_happy(0);
            } else if (cmd == CMD_IDLE) {
                state = STATE_IDLE;
                blinking = idle_ms = 0;
                redraw_idle(1);
            }
        }

        DEV_Delay_ms(LOOP_DELAY_MS);
    }

    free(g_img);
}
