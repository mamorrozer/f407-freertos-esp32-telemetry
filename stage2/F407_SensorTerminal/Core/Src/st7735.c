#include "st7735.h"

#include <stddef.h>

#define ST7735_CMD_SWRESET 0x01U
#define ST7735_CMD_SLPOUT 0x11U
#define ST7735_CMD_FRMCTR1 0xB1U
#define ST7735_CMD_FRMCTR2 0xB2U
#define ST7735_CMD_FRMCTR3 0xB3U
#define ST7735_CMD_INVCTR 0xB4U
#define ST7735_CMD_PWCTR1 0xC0U
#define ST7735_CMD_PWCTR2 0xC1U
#define ST7735_CMD_PWCTR3 0xC2U
#define ST7735_CMD_PWCTR4 0xC3U
#define ST7735_CMD_PWCTR5 0xC4U
#define ST7735_CMD_VMCTR1 0xC5U
#define ST7735_CMD_MADCTL 0x36U
#define ST7735_CMD_COLMOD 0x3AU
#define ST7735_CMD_CASET 0x2AU
#define ST7735_CMD_RASET 0x2BU
#define ST7735_CMD_RAMWR 0x2CU
#define ST7735_CMD_GMCTRP1 0xE0U
#define ST7735_CMD_GMCTRN1 0xE1U
#define ST7735_CMD_DISPON 0x29U

static const uint8_t glyph_blank[5] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U};

static const uint8_t *glyph_for(char character)
{
    static const uint8_t digits[10][5] = {
        {0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU},
        {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U},
        {0x42U, 0x61U, 0x51U, 0x49U, 0x46U},
        {0x21U, 0x41U, 0x45U, 0x4BU, 0x31U},
        {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U},
        {0x27U, 0x45U, 0x45U, 0x45U, 0x39U},
        {0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U},
        {0x01U, 0x71U, 0x09U, 0x05U, 0x03U},
        {0x36U, 0x49U, 0x49U, 0x49U, 0x36U},
        {0x06U, 0x49U, 0x49U, 0x29U, 0x1EU},
    };
    static const uint8_t letters[26][5] = {
        {0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU},
        {0x7FU, 0x49U, 0x49U, 0x49U, 0x36U},
        {0x3EU, 0x41U, 0x41U, 0x41U, 0x22U},
        {0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU},
        {0x7FU, 0x49U, 0x49U, 0x49U, 0x41U},
        {0x7FU, 0x09U, 0x09U, 0x09U, 0x01U},
        {0x3EU, 0x41U, 0x49U, 0x49U, 0x7AU},
        {0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU},
        {0x00U, 0x41U, 0x7FU, 0x41U, 0x00U},
        {0x20U, 0x40U, 0x41U, 0x3FU, 0x01U},
        {0x7FU, 0x08U, 0x14U, 0x22U, 0x41U},
        {0x7FU, 0x40U, 0x40U, 0x40U, 0x40U},
        {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU},
        {0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU},
        {0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU},
        {0x7FU, 0x09U, 0x09U, 0x09U, 0x06U},
        {0x3EU, 0x41U, 0x51U, 0x21U, 0x5EU},
        {0x7FU, 0x09U, 0x19U, 0x29U, 0x46U},
        {0x46U, 0x49U, 0x49U, 0x49U, 0x31U},
        {0x01U, 0x01U, 0x7FU, 0x01U, 0x01U},
        {0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU},
        {0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU},
        {0x7FU, 0x20U, 0x18U, 0x20U, 0x7FU},
        {0x63U, 0x14U, 0x08U, 0x14U, 0x63U},
        {0x07U, 0x08U, 0x70U, 0x08U, 0x07U},
        {0x61U, 0x51U, 0x49U, 0x45U, 0x43U},
    };
    static const uint8_t colon[5] = {0x00U, 0x36U, 0x36U, 0x00U, 0x00U};
    static const uint8_t period[5] = {0x00U, 0x60U, 0x60U, 0x00U, 0x00U};
    static const uint8_t percent[5] = {0x63U, 0x13U, 0x08U, 0x64U, 0x63U};
    static const uint8_t minus[5] = {0x08U, 0x08U, 0x08U, 0x08U, 0x08U};
    static const uint8_t slash[5] = {0x20U, 0x10U, 0x08U, 0x04U, 0x02U};
    static const uint8_t underscore[5] = {0x40U, 0x40U, 0x40U, 0x40U, 0x40U};

    if ((character >= '0') && (character <= '9'))
    {
        return digits[(uint8_t)(character - '0')];
    }
    if ((character >= 'A') && (character <= 'Z'))
    {
        return letters[(uint8_t)(character - 'A')];
    }
    switch (character)
    {
    case ':': return colon;
    case '.': return period;
    case '%': return percent;
    case '-': return minus;
    case '/': return slash;
    case '_': return underscore;
    case ' ': return glyph_blank;
    default: return glyph_blank;
    }
}

static int bus_valid(const ST7735_Bus *bus)
{
    return (bus != NULL) &&
           ((bus->write_bytes != NULL) ||
            ((bus->write_sck != NULL) && (bus->write_mosi != NULL))) &&
           (bus->write_cs != NULL) &&
           (bus->write_dc != NULL) && (bus->write_rst != NULL) &&
           (bus->write_bl != NULL) && (bus->delay_ms != NULL);
}

static void write_byte(ST7735 *display, uint8_t value)
{
    if (display->bus.write_bytes != NULL)
    {
        if (display->bus.write_bytes(display->bus.context, &value, 1U) != 0)
        {
            display->transfer_error = 1U;
        }
        return;
    }
    for (uint8_t bit = 0U; bit < 8U; ++bit)
    {
        display->bus.write_sck(display->bus.context, 0U);
        display->bus.write_mosi(display->bus.context,
                                (uint8_t)((value & 0x80U) != 0U));
        display->bus.write_sck(display->bus.context, 1U);
        value <<= 1U;
    }
    display->bus.write_sck(display->bus.context, 0U);
}

static void begin_transfer(ST7735 *display, uint8_t data_mode)
{
    display->bus.write_cs(display->bus.context, 0U);
    display->bus.write_dc(display->bus.context, data_mode);
}

static void end_transfer(ST7735 *display)
{
    display->bus.write_cs(display->bus.context, 1U);
}

static void write_command(ST7735 *display, uint8_t command)
{
    begin_transfer(display, 0U);
    write_byte(display, command);
    end_transfer(display);
}

static void write_data(ST7735 *display, const uint8_t *data, uint16_t length)
{
    if ((data == NULL) || (length == 0U))
    {
        return;
    }
    begin_transfer(display, 1U);
    if (display->bus.write_bytes != NULL)
    {
        if (display->bus.write_bytes(display->bus.context, data, length) != 0)
        {
            display->transfer_error = 1U;
        }
    }
    else
    {
        for (uint16_t index = 0U; index < length; ++index)
        {
            write_byte(display, data[index]);
        }
    }
    end_transfer(display);
}

static void write_color(ST7735 *display, uint16_t color)
{
    write_byte(display, (uint8_t)(color >> 8U));
    write_byte(display, (uint8_t)color);
}

static int write_madctl(ST7735 *display, uint8_t rotation)
{
    static const uint8_t madctl[4] = {0xC0U, 0xA0U, 0x00U, 0x60U};
    if (rotation != ST7735_ROTATION_PORTRAIT)
    {
        return -2;
    }
    write_command(display, ST7735_CMD_MADCTL);
    write_data(display, &madctl[rotation], 1U);
    return 0;
}

static int set_window(ST7735 *display, uint16_t x0, uint16_t y0,
                      uint16_t x1, uint16_t y1)
{
    int32_t column_start;
    int32_t column_end;
    int32_t row_start;
    int32_t row_end;
    uint8_t address[4];

    if ((display == NULL) || (display->initialized == 0U) ||
        (x0 > x1) || (y0 > y1) || (x1 >= ST7735_WIDTH) ||
        (y1 >= ST7735_HEIGHT))
    {
        return -2;
    }
    column_start = (int32_t)x0 + display->x_offset;
    column_end = (int32_t)x1 + display->x_offset;
    row_start = (int32_t)y0 + display->y_offset;
    row_end = (int32_t)y1 + display->y_offset;
    if ((column_start < 0) || (column_end > 65535) || (row_start < 0) ||
        (row_end > 65535))
    {
        return -2;
    }

    write_command(display, ST7735_CMD_CASET);
    address[0] = (uint8_t)((uint16_t)column_start >> 8U);
    address[1] = (uint8_t)column_start;
    address[2] = (uint8_t)((uint16_t)column_end >> 8U);
    address[3] = (uint8_t)column_end;
    write_data(display, address, sizeof(address));

    write_command(display, ST7735_CMD_RASET);
    address[0] = (uint8_t)((uint16_t)row_start >> 8U);
    address[1] = (uint8_t)row_start;
    address[2] = (uint8_t)((uint16_t)row_end >> 8U);
    address[3] = (uint8_t)row_end;
    write_data(display, address, sizeof(address));
    write_command(display, ST7735_CMD_RAMWR);
    return 0;
}

static void begin_pixels(ST7735 *display)
{
    begin_transfer(display, 1U);
}

static void end_pixels(ST7735 *display)
{
    end_transfer(display);
}

static void write_pixels(ST7735 *display, uint32_t count, uint16_t color)
{
    if (display->bus.write_bytes != NULL)
    {
        uint8_t chunk[256U];
        const uint8_t high = (uint8_t)(color >> 8U);
        const uint8_t low = (uint8_t)color;
        for (uint16_t index = 0U; index < sizeof(chunk); index += 2U)
        {
            chunk[index] = high;
            chunk[index + 1U] = low;
        }
        while (count != 0U)
        {
            uint16_t pixels = count > (sizeof(chunk) / 2U)
                                  ? (uint16_t)(sizeof(chunk) / 2U)
                                  : (uint16_t)count;
            if (display->bus.write_bytes(display->bus.context, chunk,
                                         (uint16_t)(pixels * 2U)) != 0)
            {
                display->transfer_error = 1U;
                return;
            }
            count -= pixels;
        }
        return;
    }
    for (uint32_t index = 0U; index < count; ++index)
    {
        write_color(display, color);
    }
}

static void write_pixel_row(ST7735 *display, const uint8_t *pixels,
                            uint16_t count)
{
    if (display->bus.write_bytes != NULL)
    {
        if (display->bus.write_bytes(display->bus.context, pixels,
                                     (uint16_t)(count * 2U)) != 0)
        {
            display->transfer_error = 1U;
        }
    }
    else
    {
        for (uint16_t index = 0U; index < count; ++index)
        {
            uint16_t color = (uint16_t)(((uint16_t)pixels[index * 2U] << 8U) |
                                        pixels[index * 2U + 1U]);
            write_color(display, color);
        }
    }
}

int st7735_init(ST7735 *display, const ST7735_Bus *bus,
                uint8_t rotation, int16_t x_offset, int16_t y_offset)
{
    static const uint8_t frmctr1[] = {0x05U, 0x3CU, 0x3CU};
    static const uint8_t frmctr3[] = {0x05U, 0x3CU, 0x3CU,
                                      0x05U, 0x3CU, 0x3CU};
    static const uint8_t pwctr1[] = {0x28U, 0x08U, 0x04U};
    static const uint8_t pwctr2[] = {0xC0U};
    static const uint8_t pwctr3[] = {0x0DU, 0x00U};
    static const uint8_t pwctr4[] = {0x8DU, 0x2AU};
    static const uint8_t pwctr5[] = {0x8DU, 0xEEU};
    static const uint8_t gamma_positive[] = {
        0x04U, 0x22U, 0x07U, 0x0AU, 0x2EU, 0x30U, 0x25U, 0x2AU,
        0x28U, 0x26U, 0x2EU, 0x3AU, 0x00U, 0x01U, 0x03U, 0x13U};
    static const uint8_t gamma_negative[] = {
        0x04U, 0x16U, 0x06U, 0x0DU, 0x2DU, 0x26U, 0x23U, 0x27U,
        0x27U, 0x25U, 0x2DU, 0x3BU, 0x00U, 0x01U, 0x04U, 0x13U};
    static const uint8_t one = 0x03U;
    static const uint8_t color_mode = 0x05U;

    if ((display == NULL) || !bus_valid(bus) ||
        (rotation != ST7735_ROTATION_PORTRAIT))
    {
        return -1;
    }
    *display = (ST7735){.bus = *bus,
                         .rotation = rotation,
                         .x_offset = x_offset,
                         .y_offset = y_offset,
                         .initialized = 0U,
                          .backlight_on = 0U,
                          .transfer_error = 0U};

    display->bus.write_cs(display->bus.context, 1U);
    if (display->bus.write_bytes == NULL)
    {
        display->bus.write_sck(display->bus.context, 0U);
        display->bus.write_mosi(display->bus.context, 0U);
    }
    display->bus.write_dc(display->bus.context, 1U);
    display->bus.write_bl(display->bus.context, 0U);
    display->bus.write_rst(display->bus.context, 1U);
    display->bus.delay_ms(display->bus.context, 1U);
    display->bus.write_rst(display->bus.context, 0U);
    display->bus.delay_ms(display->bus.context, 1U);
    display->bus.write_rst(display->bus.context, 1U);
    display->bus.delay_ms(display->bus.context, 120U);

    write_command(display, ST7735_CMD_SWRESET);
    display->bus.delay_ms(display->bus.context, 120U);
    write_command(display, ST7735_CMD_SLPOUT);
    display->bus.delay_ms(display->bus.context, 120U);
    write_command(display, ST7735_CMD_FRMCTR1);
    write_data(display, frmctr1, sizeof(frmctr1));
    write_command(display, ST7735_CMD_FRMCTR2);
    write_data(display, frmctr1, sizeof(frmctr1));
    write_command(display, ST7735_CMD_FRMCTR3);
    write_data(display, frmctr3, sizeof(frmctr3));
    write_command(display, ST7735_CMD_INVCTR);
    write_data(display, &one, 1U);
    write_command(display, ST7735_CMD_PWCTR1);
    write_data(display, pwctr1, sizeof(pwctr1));
    write_command(display, ST7735_CMD_PWCTR2);
    write_data(display, pwctr2, sizeof(pwctr2));
    write_command(display, ST7735_CMD_PWCTR3);
    write_data(display, pwctr3, sizeof(pwctr3));
    write_command(display, ST7735_CMD_PWCTR4);
    write_data(display, pwctr4, sizeof(pwctr4));
    write_command(display, ST7735_CMD_PWCTR5);
    write_data(display, pwctr5, sizeof(pwctr5));
    write_command(display, ST7735_CMD_VMCTR1);
    write_data(display, (const uint8_t[]){0x1AU}, 1U);
    if (write_madctl(display, rotation) != 0)
    {
        return -2;
    }
    write_command(display, ST7735_CMD_GMCTRP1);
    write_data(display, gamma_positive, sizeof(gamma_positive));
    write_command(display, ST7735_CMD_GMCTRN1);
    write_data(display, gamma_negative, sizeof(gamma_negative));
    write_command(display, ST7735_CMD_COLMOD);
    write_data(display, &color_mode, 1U);
    write_command(display, ST7735_CMD_DISPON);
    if (display->transfer_error != 0U)
    {
        display->bus.write_cs(display->bus.context, 1U);
        return -3;
    }
    display->initialized = 1U;
    return st7735_set_backlight(display, 1U);
}

int st7735_set_rotation(ST7735 *display, uint8_t rotation)
{
    if ((display == NULL) || (display->initialized == 0U) ||
        (rotation != ST7735_ROTATION_PORTRAIT))
    {
        return -2;
    }
    display->transfer_error = 0U;
    if ((write_madctl(display, rotation) != 0) ||
        (display->transfer_error != 0U))
    {
        return -2;
    }
    display->rotation = rotation;
    return 0;
}

int st7735_set_backlight(ST7735 *display, uint8_t on)
{
    if ((display == NULL) || !bus_valid(&display->bus))
    {
        return -1;
    }
    display->bus.write_bl(display->bus.context, on != 0U ? 1U : 0U);
    display->backlight_on = on != 0U ? 1U : 0U;
    return 0;
}

int st7735_fill_rect(ST7735 *display, uint16_t x0, uint16_t y0,
                     uint16_t x1, uint16_t y1, uint16_t color)
{
    if (display != NULL)
    {
        display->transfer_error = 0U;
    }
    if (set_window(display, x0, y0, x1, y1) != 0)
    {
        return -2;
    }
    begin_pixels(display);
    write_pixels(display, (uint32_t)(x1 - x0 + 1U) * (y1 - y0 + 1U), color);
    end_pixels(display);
    return display->transfer_error != 0U ? -3 : 0;
}

int st7735_fill(ST7735 *display, uint16_t color)
{
    return st7735_fill_rect(display, 0U, 0U, ST7735_WIDTH - 1U,
                            ST7735_HEIGHT - 1U, color);
}

int st7735_draw_pixel(ST7735 *display, uint16_t x, uint16_t y,
                      uint16_t color)
{
    if (display != NULL)
    {
        display->transfer_error = 0U;
    }
    if (set_window(display, x, y, x, y) != 0)
    {
        return -2;
    }
    begin_pixels(display);
    write_color(display, color);
    end_pixels(display);
    return display->transfer_error != 0U ? -3 : 0;
}

int st7735_draw_text(ST7735 *display, uint16_t x, uint16_t y,
                     const char *text, uint16_t foreground,
                     uint16_t background)
{
    uint16_t available_width;
    uint16_t width;
    uint16_t height;
    uint16_t character_count = 0U;

    if ((display == NULL) || (text == NULL) || (display->initialized == 0U))
    {
        return -1;
    }
    if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT) || (*text == '\0'))
    {
        return 0;
    }
    display->transfer_error = 0U;

    available_width = (uint16_t)(ST7735_WIDTH - x);
    while ((text[character_count] != '\0') &&
           ((uint16_t)(character_count * 6U) < available_width))
    {
        character_count++;
    }
    width = (uint16_t)(character_count * 6U);
    if (width > available_width)
    {
        width = available_width;
    }
    height = (uint16_t)(ST7735_HEIGHT - y);
    if (height > 8U)
    {
        height = 8U;
    }
    if (set_window(display, x, y, (uint16_t)(x + width - 1U),
                   (uint16_t)(y + height - 1U)) != 0)
    {
        return -2;
    }

    begin_pixels(display);
    for (uint16_t row = 0U; row < height; ++row)
    {
        uint8_t pixels[ST7735_WIDTH * 2U];
        for (uint16_t column = 0U; column < width; ++column)
        {
            uint16_t character_index = (uint16_t)(column / 6U);
            uint16_t glyph_column = (uint16_t)(column % 6U);
            const uint8_t *glyph = glyph_for(text[character_index]);
            uint8_t lit = (glyph_column < 5U) && (row < 7U) &&
                          ((glyph[glyph_column] &
                            (uint8_t)(1U << row)) != 0U);
            uint16_t color = lit != 0U ? foreground : background;
            pixels[column * 2U] = (uint8_t)(color >> 8U);
            pixels[column * 2U + 1U] = (uint8_t)color;
        }
        write_pixel_row(display, pixels, width);
    }
    end_pixels(display);
    return display->transfer_error != 0U ? -3 : 0;
}

int st7735_draw_text_scaled_ratio(ST7735 *display, uint16_t x, uint16_t y,
                                  const char *text, uint8_t numerator,
                                  uint8_t denominator, uint16_t foreground,
                                  uint16_t background)
{
    uint16_t cell_width;
    uint16_t cell_height;
    uint16_t available_width;
    uint16_t available_height;
    uint16_t character_count = 0U;
    uint16_t width;
    uint16_t height;

    if ((display == NULL) || (text == NULL) ||
        (display->initialized == 0U) || (numerator == 0U) ||
        (denominator == 0U))
    {
        return -1;
    }
    if ((*text == '\0') || (x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT))
    {
        return 0;
    }
    display->transfer_error = 0U;

    cell_width = (uint16_t)((6U * numerator + denominator - 1U) /
                            denominator);
    cell_height = (uint16_t)((8U * numerator + denominator - 1U) /
                             denominator);
    available_width = (uint16_t)(ST7735_WIDTH - x);
    available_height = (uint16_t)(ST7735_HEIGHT - y);
    while ((text[character_count] != '\0') &&
           ((uint32_t)character_count * cell_width < available_width))
    {
        character_count++;
    }
    if (character_count == 0U)
    {
        return 0;
    }

    width = (uint16_t)(character_count * cell_width);
    if (width > available_width)
    {
        width = available_width;
    }
    height = cell_height < available_height ? cell_height : available_height;
    if (set_window(display, x, y, (uint16_t)(x + width - 1U),
                   (uint16_t)(y + height - 1U)) != 0)
    {
        return -2;
    }

    begin_pixels(display);
    for (uint16_t row = 0U; row < height; ++row)
    {
        uint8_t pixels[ST7735_WIDTH * 2U];
        uint16_t glyph_row = (uint16_t)((row * denominator) / numerator);
        for (uint16_t column = 0U; column < width; ++column)
        {
            uint16_t character_index = (uint16_t)(column / cell_width);
            uint16_t glyph_column = (uint16_t)(
                ((column % cell_width) * denominator) / numerator);
            const uint8_t *glyph = glyph_for(text[character_index]);
            uint8_t lit = (glyph_column < 5U) && (glyph_row < 7U) &&
                          ((glyph[glyph_column] &
                            (uint8_t)(1U << glyph_row)) != 0U);
            uint16_t color = lit != 0U ? foreground : background;
            pixels[column * 2U] = (uint8_t)(color >> 8U);
            pixels[column * 2U + 1U] = (uint8_t)color;
        }
        write_pixel_row(display, pixels, width);
    }
    end_pixels(display);
    return display->transfer_error != 0U ? -3 : 0;
}

int st7735_draw_text_scaled(ST7735 *display, uint16_t x, uint16_t y,
                            const char *text, uint8_t scale,
                            uint16_t foreground, uint16_t background)
{
    return st7735_draw_text_scaled_ratio(display, x, y, text, scale, 1U,
                                         foreground, background);
}
