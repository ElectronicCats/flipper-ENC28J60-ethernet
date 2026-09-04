#include "passive_details_text.h"

#include <stdio.h>
#include <string.h>

#define PASSIVE_DETAILS_GLYPHS_PER_PAGE                                       \
    ((PASSIVE_DETAILS_VALUE_WIDTH_PIXELS / PASSIVE_DETAILS_MAX_GLYPH_WIDTH) * \
     PASSIVE_DETAILS_VALUE_LINES)

static size_t passive_details_utf8_sequence_length(const char* text, size_t remaining) {
    const uint8_t first = (uint8_t)text[0];
    size_t length = 1;

    if((first & 0xE0U) == 0xC0U) {
        length = 2;
    } else if((first & 0xF0U) == 0xE0U) {
        length = 3;
    } else if((first & 0xF8U) == 0xF0U) {
        length = 4;
    }

    if(length > remaining) {
        return 1;
    }

    for(size_t index = 1; index < length; index++) {
        if(((uint8_t)text[index] & 0xC0U) != 0x80U) {
            return 1;
        }
    }

    return length;
}

static size_t passive_details_skip_spaces(const char* text, size_t offset) {
    while(text[offset] == ' ') {
        offset++;
    }
    return offset;
}

static size_t passive_details_page_end(const char* text, size_t start) {
    const size_t length = strlen(text);
    size_t offset = start;
    size_t last_space = SIZE_MAX;
    size_t glyphs = 0;

    while(offset < length && glyphs < PASSIVE_DETAILS_GLYPHS_PER_PAGE) {
        if(text[offset] == ' ') {
            last_space = offset;
        }

        size_t sequence_length =
            passive_details_utf8_sequence_length(text + offset, length - offset);
        offset += sequence_length;
        glyphs++;
    }

    if(offset < length && last_space != SIZE_MAX && last_space > start) {
        return last_space;
    }

    return offset;
}

uint8_t passive_details_text_page_count(const char* text) {
    if(!text || !text[0]) {
        return 1;
    }

    size_t offset = 0;
    uint8_t pages = 0;

    while(text[offset] && pages < UINT8_MAX) {
        size_t end = passive_details_page_end(text, offset);
        if(end <= offset) {
            break;
        }

        pages++;
        offset = passive_details_skip_spaces(text, end);
    }

    return pages ? pages : 1;
}

bool passive_details_text_get_page(
    const char* text,
    uint8_t page,
    char* output,
    size_t output_size) {
    if(!output || output_size == 0) {
        return false;
    }

    output[0] = '\0';
    if(!text || !text[0]) {
        return page == 0 && snprintf(output, output_size, "N/A") > 0;
    }

    size_t offset = 0;
    for(uint8_t current_page = 0; text[offset]; current_page++) {
        size_t end = passive_details_page_end(text, offset);
        if(end <= offset) {
            return false;
        }

        if(current_page == page) {
            size_t copy_length = end - offset;
            while(copy_length > 0 && text[offset + copy_length - 1] == ' ') {
                copy_length--;
            }
            if(copy_length >= output_size) {
                copy_length = output_size - 1;
            }
            memcpy(output, text + offset, copy_length);
            output[copy_length] = '\0';
            return true;
        }

        offset = passive_details_skip_spaces(text, end);
    }

    return false;
}

void passive_details_format_mac(const uint8_t mac[6], char* output, size_t output_size) {
    if(!mac || !output || output_size == 0) {
        return;
    }

    snprintf(
        output,
        output_size,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

void passive_details_append_label(char* output, size_t output_size, const char* label) {
    if(!output || output_size == 0 || !label || !label[0]) {
        return;
    }

    size_t used = strlen(output);
    if(used >= output_size - 1) {
        return;
    }

    snprintf(output + used, output_size - used, "%s%s", used ? ", " : "", label);
}
