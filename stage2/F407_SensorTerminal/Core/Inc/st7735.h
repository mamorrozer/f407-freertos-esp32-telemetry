#ifndef ST7735_H
#define ST7735_H

#include <stdint.h>

#define ST7735_WIDTH 128U
#define ST7735_HEIGHT 160U

#define ST7735_BLACK 0x0000U
#define ST7735_WHITE 0xFFFFU
#define ST7735_RED 0xF800U
#define ST7735_GREEN 0x07E0U
#define ST7735_BLUE 0x001FU
#define ST7735_YELLOW 0xFFE0U
#define ST7735_CYAN 0x07FFU
#define ST7735_MAGENTA 0xF81FU
#define ST7735_ROTATION_PORTRAIT 0U

typedef void (*st7735_pin_write_fn)(void *context, uint8_t level);
typedef void (*st7735_delay_fn)(void *context, uint32_t milliseconds);
typedef int (*st7735_write_bytes_fn)(void *context, const uint8_t *bytes,
                                     uint16_t length);

typedef struct
{
    st7735_pin_write_fn write_sck;
    st7735_pin_write_fn write_mosi;
    st7735_pin_write_fn write_cs;
    st7735_pin_write_fn write_dc;
    st7735_pin_write_fn write_rst;
    st7735_pin_write_fn write_bl;
    st7735_delay_fn delay_ms;
    st7735_write_bytes_fn write_bytes;
    void *context;
} ST7735_Bus;

typedef struct
{
    ST7735_Bus bus;
    uint8_t rotation;
    int16_t x_offset;
    int16_t y_offset;
    uint8_t initialized;
    uint8_t backlight_on;
    uint8_t transfer_error;
} ST7735;

int st7735_init(ST7735 *display, const ST7735_Bus *bus,
                uint8_t rotation, int16_t x_offset, int16_t y_offset);
int st7735_set_rotation(ST7735 *display, uint8_t rotation);
int st7735_set_backlight(ST7735 *display, uint8_t on);
int st7735_fill(ST7735 *display, uint16_t color);
int st7735_fill_rect(ST7735 *display, uint16_t x0, uint16_t y0,
                     uint16_t x1, uint16_t y1, uint16_t color);
int st7735_draw_pixel(ST7735 *display, uint16_t x, uint16_t y,
                      uint16_t color);
int st7735_draw_text(ST7735 *display, uint16_t x, uint16_t y,
                     const char *text, uint16_t foreground,
                     uint16_t background);
int st7735_draw_text_scaled(ST7735 *display, uint16_t x, uint16_t y,
                            const char *text, uint8_t scale,
                            uint16_t foreground, uint16_t background);
int st7735_draw_text_scaled_ratio(ST7735 *display, uint16_t x, uint16_t y,
                                  const char *text, uint8_t numerator,
                                  uint8_t denominator, uint16_t foreground,
                                  uint16_t background);

#endif
