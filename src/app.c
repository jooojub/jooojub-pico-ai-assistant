#include "app.h"
#include "DEV_Config.h"
#include "LCD_1in3.h"
#include "GUI_Paint.h"
#include "Infrared.h"
#include <stdlib.h>

#define BTN_UP    2
#define BTN_DOWN  18
#define BTN_LEFT  16
#define BTN_RIGHT 20

#define CHAR_R    22
#define SPEED     5

#define PINK      0xFD56

static void draw_character(int x, int y) {
    /* ears — drawn before body so body overlaps the base */
    Paint_DrawCircle(x - 14, y - 22, 9,  BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawCircle(x + 14, y - 22, 9,  BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawCircle(x - 14, y - 22, 5,  PINK,  DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawCircle(x + 14, y - 22, 5,  PINK,  DOT_PIXEL_1X1, DRAW_FILL_FULL);

    /* body */
    Paint_DrawCircle(x, y, CHAR_R, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);

    /* eyes */
    Paint_DrawCircle(x - 8, y - 6, 6, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawCircle(x + 8, y - 6, 6, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);

    /* pupils */
    Paint_DrawCircle(x - 7, y - 6, 3, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawCircle(x + 9, y - 6, 3, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);

    /* cheeks */
    Paint_DrawCircle(x - 14, y + 5, 4, PINK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawCircle(x + 14, y + 5, 4, PINK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}

void app_run(void) {
    DEV_Module_Init();
    DEV_SET_PWM(50);

    LCD_1IN3_Init(HORIZONTAL);
    LCD_1IN3_Clear(WHITE);

    UWORD *img = (UWORD *)malloc(LCD_1IN3_WIDTH * LCD_1IN3_HEIGHT * 2);
    if (!img) return;

    Paint_NewImage((UBYTE *)img, LCD_1IN3_WIDTH, LCD_1IN3_HEIGHT, ROTATE_0, WHITE);
    Paint_SetScale(65);
    Paint_Clear(WHITE);

    SET_Infrared_PIN(BTN_UP);
    SET_Infrared_PIN(BTN_DOWN);
    SET_Infrared_PIN(BTN_LEFT);
    SET_Infrared_PIN(BTN_RIGHT);

    int x = LCD_1IN3_WIDTH  / 2;
    int y = LCD_1IN3_HEIGHT / 2;

    draw_character(x, y);
    LCD_1IN3_Display(img);

    while (1) {
        int nx = x, ny = y;

        if (DEV_Digital_Read(BTN_LEFT)  == 0) nx -= SPEED;
        if (DEV_Digital_Read(BTN_RIGHT) == 0) nx += SPEED;
        if (DEV_Digital_Read(BTN_UP)    == 0) ny -= SPEED;
        if (DEV_Digital_Read(BTN_DOWN)  == 0) ny += SPEED;

        /* clamp to screen boundaries */
        if (nx < CHAR_R + 14)                  nx = CHAR_R + 14;
        if (nx > LCD_1IN3_WIDTH  - CHAR_R - 14) nx = LCD_1IN3_WIDTH  - CHAR_R - 14;
        if (ny < CHAR_R + 14)                  ny = CHAR_R + 14;
        if (ny > LCD_1IN3_HEIGHT - CHAR_R)      ny = LCD_1IN3_HEIGHT - CHAR_R;

        if (nx != x || ny != y) {
            x = nx;
            y = ny;
            Paint_Clear(WHITE);
            draw_character(x, y);
            LCD_1IN3_Display(img);
        }

        DEV_Delay_ms(16);
    }

    free(img);
}
