#include "st7735.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CAPTURE_BYTES 4096U

typedef struct
{
    uint8_t sck;
    uint8_t mosi;
    uint8_t dc;
    uint8_t byte_value;
    uint8_t bit_count;
    uint8_t bytes[CAPTURE_BYTES];
    uint8_t modes[CAPTURE_BYTES];
    size_t byte_count;
    size_t block_calls;
    uint16_t largest_block;
    uint8_t cs;
    uint8_t fail_next_block;
} FakeBus;

static void fake_sck(void *context, uint8_t level)
{
    FakeBus *bus = context;
    if ((bus->sck == 0U) && (level != 0U))
    {
        bus->byte_value = (uint8_t)((bus->byte_value << 1U) | bus->mosi);
        bus->bit_count++;
        if (bus->bit_count == 8U)
        {
            assert(bus->byte_count < CAPTURE_BYTES);
            bus->bytes[bus->byte_count] = bus->byte_value;
            bus->modes[bus->byte_count] = bus->dc;
            bus->byte_count++;
            bus->byte_value = 0U;
            bus->bit_count = 0U;
        }
    }
    bus->sck = level;
}

static void fake_mosi(void *context, uint8_t level)
{
    ((FakeBus *)context)->mosi = level;
}

static void fake_dc(void *context, uint8_t level)
{
    ((FakeBus *)context)->dc = level;
}

static void fake_pin(void *context, uint8_t level)
{
    (void)context;
    (void)level;
}

static void fake_cs(void *context, uint8_t level)
{
    ((FakeBus *)context)->cs = level;
}

static void fake_delay(void *context, uint32_t milliseconds)
{
    (void)context;
    (void)milliseconds;
}

static int fake_write_bytes(void *context, const uint8_t *bytes,
                            uint16_t length)
{
    FakeBus *bus = context;
    assert((bytes != NULL) || (length == 0U));
    bus->block_calls++;
    if (length > bus->largest_block)
    {
        bus->largest_block = length;
    }
    if (bus->fail_next_block != 0U)
    {
        bus->fail_next_block = 0U;
        return -1;
    }
    for (uint16_t i = 0U; i < length; ++i)
    {
        assert(bus->byte_count < CAPTURE_BYTES);
        bus->bytes[bus->byte_count] = bytes[i];
        bus->modes[bus->byte_count] = bus->dc;
        bus->byte_count++;
    }
    return 0;
}

static void reset_capture(FakeBus *bus)
{
    bus->byte_value = 0U;
    bus->bit_count = 0U;
    bus->byte_count = 0U;
    bus->block_calls = 0U;
    bus->largest_block = 0U;
}

static size_t find_command(const FakeBus *bus, uint8_t command)
{
    for (size_t index = 0U; index < bus->byte_count; ++index)
    {
        if ((bus->modes[index] == 0U) && (bus->bytes[index] == command))
        {
            return index;
        }
    }
    return bus->byte_count;
}

static uint16_t address_end(const FakeBus *bus, uint8_t command)
{
    size_t index = find_command(bus, command);
    assert(index + 4U < bus->byte_count);
    assert(bus->modes[index + 1U] == 1U);
    return (uint16_t)(((uint16_t)bus->bytes[index + 3U] << 8U) |
                      bus->bytes[index + 4U]);
}

static uint16_t address_start(const FakeBus *bus, uint8_t command)
{
    size_t index = find_command(bus, command);
    assert(index + 2U < bus->byte_count);
    assert(bus->modes[index + 1U] == 1U);
    return (uint16_t)(((uint16_t)bus->bytes[index + 1U] << 8U) |
                      bus->bytes[index + 2U]);
}

static void assert_uniform_pixel_count(const FakeBus *bus,
                                      size_t first_pixel_byte,
                                      uint16_t foreground,
                                      uint16_t background,
                                      size_t expected_fg,
                                      size_t expected_bg)
{
    size_t foreground_count = 0U;
    size_t background_count = 0U;
    for (size_t index = first_pixel_byte; index + 1U < bus->byte_count;
         index += 2U)
    {
        uint16_t color = (uint16_t)(((uint16_t)bus->bytes[index] << 8U) |
                                    bus->bytes[index + 1U]);
        if (color == foreground)
        {
            foreground_count++;
        }
        else
        {
            assert(color == background);
            background_count++;
        }
    }
    assert(foreground_count == expected_fg);
    assert(background_count == expected_bg);
}

int main(void)
{
    FakeBus fake = {0};
    ST7735 display;
    ST7735_Bus bus = {
        .write_sck = fake_sck,
        .write_mosi = fake_mosi,
        .write_cs = fake_cs,
        .write_dc = fake_dc,
        .write_rst = fake_pin,
        .write_bl = fake_pin,
        .delay_ms = fake_delay,
        .context = &fake,
    };

    assert(st7735_init(&display, &bus, ST7735_ROTATION_PORTRAIT, 0, 0) == 0);

    reset_capture(&fake);
    assert(st7735_draw_pixel(&display, 0U, 0U, ST7735_WHITE) == 0);
    assert(address_start(&fake, 0x2AU) == 0U);
    assert(address_start(&fake, 0x2BU) == 0U);

    reset_capture(&fake);
    assert(st7735_draw_text_scaled(&display, 10U, 20U, "1", 2U,
                                   ST7735_WHITE, ST7735_BLACK) == 0);
    assert(address_end(&fake, 0x2AU) == 21U);
    assert(address_end(&fake, 0x2BU) == 35U);
    assert(fake.byte_count == 11U + (12U * 16U * 2U));
    assert_uniform_pixel_count(&fake, 11U, ST7735_WHITE, ST7735_BLACK,
                               40U, 152U);

    reset_capture(&fake);
    assert(st7735_draw_text_scaled(&display, 0U, 0U, "1", 1U,
                                   ST7735_WHITE, ST7735_BLACK) == 0);
    assert(address_end(&fake, 0x2AU) == 5U);
    assert(address_end(&fake, 0x2BU) == 7U);
    assert(fake.byte_count == 11U + (6U * 8U * 2U));
    assert_uniform_pixel_count(&fake, 11U, ST7735_WHITE, ST7735_BLACK,
                               10U, 38U);

    reset_capture(&fake);
    assert(st7735_draw_text_scaled(&display, 124U, 157U, "1", 2U,
                                   ST7735_WHITE, ST7735_BLACK) == 0);
    assert(address_end(&fake, 0x2AU) == 127U);
    assert(address_end(&fake, 0x2BU) == 159U);
    assert(fake.byte_count == 11U + (4U * 3U * 2U));

    reset_capture(&fake);
    assert(st7735_draw_text_scaled(&display, 0U, 0U, "1", 0U,
                                   ST7735_WHITE, ST7735_BLACK) != 0);
    assert(fake.byte_count == 0U);

    assert(st7735_draw_text_scaled(NULL, 0U, 0U, "1", 2U,
                                   ST7735_WHITE, ST7735_BLACK) != 0);
    assert(st7735_draw_text_scaled(&display, 0U, 0U, NULL, 2U,
                                   ST7735_WHITE, ST7735_BLACK) != 0);
    assert(fake.byte_count == 0U);

    reset_capture(&fake);
    assert(st7735_draw_text_scaled(&display, ST7735_WIDTH, 0U, "1", 2U,
                                   ST7735_WHITE, ST7735_BLACK) == 0);
    assert(st7735_draw_text_scaled(&display, 0U, ST7735_HEIGHT, "1", 2U,
                                   ST7735_WHITE, ST7735_BLACK) == 0);
    assert(st7735_draw_text_scaled(&display, 0U, 0U, "", 2U,
                                   ST7735_WHITE, ST7735_BLACK) == 0);
    assert(fake.byte_count == 0U);

    {
        ST7735 fast_display;
        ST7735_Bus fast_bus = bus;
        fast_bus.write_bytes = fake_write_bytes;
        fast_bus.write_sck = NULL;
        fast_bus.write_mosi = NULL;
        assert(st7735_init(&fast_display, &fast_bus,
                           ST7735_ROTATION_PORTRAIT, 0, 0) == 0);

        reset_capture(&fake);
        assert(st7735_fill_rect(&fast_display, 0U, 0U, 1U, 1U,
                                ST7735_RED) == 0);
        assert(fake.block_calls > 0U);
        assert(fake.largest_block <= 256U);
        assert(fake.bytes[fake.byte_count - 8U] == 0xF8U);
        assert(fake.bytes[fake.byte_count - 7U] == 0x00U);
        assert(fake.bytes[fake.byte_count - 6U] == 0xF8U);
        assert(fake.bytes[fake.byte_count - 5U] == 0x00U);

        reset_capture(&fake);
        assert(st7735_draw_text_scaled(&fast_display, 8U, 72U, "100.0%", 2U,
                                       ST7735_WHITE, ST7735_BLACK) == 0);
        assert(fake.block_calls >= 16U);
        assert(fake.largest_block <= 256U);
        assert(fake.byte_count == 11U + (72U * 16U * 2U));

        reset_capture(&fake);
        assert(st7735_draw_text_scaled_ratio(
                   &fast_display, 8U, 72U, "100.0%", 3U, 2U,
                   ST7735_CYAN, ST7735_BLACK) == 0);
        assert(address_start(&fake, 0x2AU) == 8U);
        assert(address_end(&fake, 0x2AU) == 61U);
        assert(address_start(&fake, 0x2BU) == 72U);
        assert(address_end(&fake, 0x2BU) == 83U);
        assert(fake.byte_count == 11U + (54U * 12U * 2U));

        reset_capture(&fake);
        fake.fail_next_block = 1U;
        assert(st7735_draw_pixel(&fast_display, 1U, 1U,
                                 ST7735_GREEN) != 0);
        assert(fake.cs == 1U);
    }

    puts("st7735_scaled_text_test: PASS");
    return 0;
}
