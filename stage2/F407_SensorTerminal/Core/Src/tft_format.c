#include "tft_format.h"

#include <stdio.h>

void tft_format_temperature(char *buffer, size_t size, int32_t value_centi)
{
    uint32_t magnitude;
    uint32_t tenths;

    if ((buffer == NULL) || (size == 0U))
    {
        return;
    }

    magnitude = value_centi < 0
                    ? (uint32_t)(-(int64_t)value_centi)
                    : (uint32_t)value_centi;
    tenths = (magnitude + 5U) / 10U;
    (void)snprintf(buffer, size, "%s%lu.%01lu",
                   value_centi < 0 ? "-" : "",
                   (unsigned long)(tenths / 10U),
                   (unsigned long)(tenths % 10U));
}
