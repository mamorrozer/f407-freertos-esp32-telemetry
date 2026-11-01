#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "st7735.h"

typedef struct
{
    uint32_t command_count;
    uint32_t data_count;
    uint32_t delay_count;
    uint32_t address_window_commands;
    uint8_t current_dc;
    uint8_t current_mosi;
    uint8_t current_byte;
    uint8_t bit_count;
    uint8_t active_command;
    uint8_t pixel_capture;
    uint8_t saw_sleep_out;
    uint8_t saw_display_on;
    uint8_t saw_rgb565;
    uint8_t saw_pixel_byte;
    uint8_t first_pixel_high_byte;
    uint8_t first_pixel_low_byte;
} FakeBus;

static void fake_sck(void *context, uint8_t level)
{
    FakeBus *bus = (FakeBus *)context;
    if ((level != 0U) && (bus->bit_count < 8U))
    {
        bus->current_byte = (uint8_t)((bus->current_byte << 1U) |
                                      bus->current_mosi);
        bus->bit_count++;
        if (bus->bit_count == 8U)
        {
            if (bus->current_dc == 0U)
            {
                bus->command_count++;
                bus->active_command = bus->current_byte;
                if ((bus->active_command == 0x2AU) ||
                    (bus->active_command == 0x2BU) ||
                    (bus->active_command == 0x2CU))
                {
                    bus->address_window_commands++;
                }
                if (bus->active_command == 0x11U) bus->saw_sleep_out = 1U;
                if (bus->active_command == 0x29U) bus->saw_display_on = 1U;
                bus->pixel_capture = bus->active_command == 0x2CU ? 1U : 0U;
            }
            else
            {
                bus->data_count++;
                if ((bus->active_command == 0x3AU) &&
                    (bus->current_byte == 0x05U))
                {
                    bus->saw_rgb565 = 1U;
                }
                if ((bus->pixel_capture != 0U) && (bus->saw_pixel_byte < 2U))
                {
                    if (bus->saw_pixel_byte == 0U)
                    {
                        bus->first_pixel_high_byte = bus->current_byte;
                    }
                    else
                    {
                        bus->first_pixel_low_byte = bus->current_byte;
                    }
                    bus->saw_pixel_byte++;
                }
            }
            bus->current_byte = 0U;
            bus->bit_count = 0U;
        }
    }
}

static void fake_mosi(void *context, uint8_t level)
{
    FakeBus *bus = (FakeBus *)context;
    bus->current_mosi = level != 0U ? 1U : 0U;
}

static void fake_cs(void *context, uint8_t level)
{
    FakeBus *bus = (FakeBus *)context;
    if (level == 0U)
    {
        bus->current_byte = 0U;
        bus->bit_count = 0U;
    }
}

static void fake_dc(void *context, uint8_t level)
{
    FakeBus *bus = (FakeBus *)context;
    bus->current_dc = level;
}

static void fake_rst(void *context, uint8_t level)
{
    (void)context;
    (void)level;
}

static void fake_bl(void *context, uint8_t level)
{
    (void)context;
    (void)level;
}

static void fake_delay(void *context, uint32_t milliseconds)
{
    FakeBus *bus = (FakeBus *)context;
    (void)milliseconds;
    bus->delay_count++;
}

static int test_bus_activity(void)
{
    FakeBus bus = {0};
    ST7735_Bus io = {.write_sck = fake_sck,
                     .write_mosi = fake_mosi,
                     .write_cs = fake_cs,
                     .write_dc = fake_dc,
                     .write_rst = fake_rst,
                     .write_bl = fake_bl,
                     .delay_ms = fake_delay,
                     .context = &bus};
    ST7735 display = {0};
    uint32_t window_commands_before_text;

    if (st7735_init(&display, NULL, 0U, 0, 0) == 0)
    {
        return 1;
    }
    if (st7735_init(&display, &io, 0U, 2, 1) != 0 || bus.delay_count < 3U)
    {
        return 2;
    }
    if (st7735_set_rotation(&display, 1U) == 0)
    {
        return 3;
    }
    if (st7735_fill(&display, ST7735_RED) != 0 ||
        st7735_fill_rect(&display, 2U, 3U, 10U, 12U, ST7735_BLUE) != 0 ||
        st7735_fill_rect(&display, 10U, 12U, 2U, 3U, ST7735_BLUE) == 0 ||
        st7735_draw_pixel(&display, 127U, 159U, ST7735_GREEN) != 0 ||
        st7735_draw_pixel(&display, 128U, 0U, ST7735_BLUE) == 0)
    {
        return 4;
    }
    window_commands_before_text = bus.address_window_commands;
    if (st7735_draw_text(&display, 0U, 0U, "TEMP", ST7735_WHITE,
                         ST7735_BLACK) != 0 ||
        (bus.address_window_commands - window_commands_before_text) != 3U ||
        st7735_draw_text(&display, 124U, 156U, "TEMP", ST7735_WHITE,
                         ST7735_BLACK) != 0 ||
        st7735_draw_text(&display, 0U, 0U, NULL, ST7735_WHITE,
                         ST7735_BLACK) == 0)
    {
        return 5;
    }
    if (display.initialized != 1U || display.backlight_on != 1U ||
        bus.saw_sleep_out == 0U || bus.saw_display_on == 0U ||
        bus.saw_rgb565 == 0U || bus.command_count < 20U ||
        bus.data_count < 40U || bus.saw_pixel_byte != 2U ||
        bus.first_pixel_high_byte != 0xF8U ||
        bus.first_pixel_low_byte != 0x00U)
    {
        return 6;
    }
    return 0;
}

int main(void)
{
    if (test_bus_activity() != 0)
    {
        fprintf(stderr, "ST7735 callback, bounds, or text test failed\n");
        return 1;
    }
    puts("ST7735 tests passed");
    return 0;
}
