#ifndef TFT_FORMAT_H
#define TFT_FORMAT_H

#include <stddef.h>
#include <stdint.h>

void tft_format_temperature(char *buffer, size_t size, int32_t value_centi);

#endif
