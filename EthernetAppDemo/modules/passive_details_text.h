#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * FontSecondary is Flipper's proportional HaxrCorp 4089 font. Its glyph
 * bounding box is at most eight pixels wide. A long-value page reserves
 * 120 pixels for three lines, so 45 glyphs is a conservative page bound;
 * the Widget text box still performs the final pixel-accurate wrapping.
 */
#define PASSIVE_DETAILS_VALUE_WIDTH_PIXELS 120U
#define PASSIVE_DETAILS_VALUE_LINES        3U
#define PASSIVE_DETAILS_MAX_GLYPH_WIDTH    8U

uint8_t passive_details_text_page_count(const char* text);

bool passive_details_text_get_page(
    const char* text,
    uint8_t page,
    char* output,
    size_t output_size);

void passive_details_format_mac(const uint8_t mac[6], char* output, size_t output_size);

void passive_details_append_label(char* output, size_t output_size, const char* label);
