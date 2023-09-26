#include "png.h"

uint32_t crc(const unsigned char *data, uint32_t offset, uint32_t len, const uint32_t *tbl) {
    uint32_t i, c, crc = 0;

    c = crc ^ 0xffffffff;
    uint32_t offset_end = offset + len;
    for (i = offset; i < offset_end; i++) {
        c = tbl[(c ^ data[i]) & 255] ^ ((c >> 8) & 0xffffff);
    }

    return c ^ 0xffffffff;
}

uint32_t *mk_crc_tbl() {

    uint32_t c;
    uint16_t i, j;
    uint32_t *crc_table = (uint32_t *) calloc(256, sizeof(*crc_table));
    CHALLOC(crc_table)

    for (i = 0; i <= 255; i++) {
        c = i;
        for (j = 0; j < 8; j++) {
            if ((c & 1) == 1) {
                c = 0xedb88320 ^ ((c>>1) & 0x7FFFFFFF);
            } else {
                c = ((c >> 1) & 0x7FFFFFFF);
            }
        }
        crc_table[i] = c;
    }

    return crc_table;
}

void print_hex(const unsigned char *restrict ptr, size_t size) {

    const unsigned char hex[] = "0123456789ABCDEF";

    unsigned char mask = 0xf0;
    size_t i;
    for (i = 0; i < size; i++) {
        putchar(hex[(ptr[i] & mask) >> 4]);
        putchar(hex[ptr[i] & (mask >> 4)]);
        putchar(' ');
    }
    putchar('\n');
}
