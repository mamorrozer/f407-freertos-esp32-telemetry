#include "tft_format.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void assert_temperature(int32_t value, const char *expected)
{
    char text[16];

    memset(text, 0xA5, sizeof(text));
    tft_format_temperature(text, sizeof(text), value);
    assert(strcmp(text, expected) == 0);
}

int main(void)
{
    assert_temperature(2500, "25.0");
    assert_temperature(2660, "26.6");
    assert_temperature(2665, "26.7");
    assert_temperature(-125, "-1.3");
    assert_temperature(INT32_MIN, "-21474836.5");

    puts("tft_format_test: PASS");
    return 0;
}
