#include "png.h"

struct png_stats *png_extract_stats(const unsigned char *idat_chunk_data) {

    struct png_stats *stats = (struct png_stats *) idat_chunk_data;
    stats->width = byteswap_ulong(stats->width);
    stats->height = byteswap_ulong(stats->height);

    return stats;
}

_Bool png_validate_signature(const unsigned char *picture) {
    static const unsigned char signature[8] = "\x89PNG\r\n\x1a\n";
    return !(memcmp(picture, signature, 8));
}

_Bool png_validate_chunk(const unsigned char *picture, uint32_t from, uint32_t len, uint32_t *crc_tbl) {
    uint32_t chunk_crc = byteswap_ulong(crc(picture, from, len, crc_tbl));
    return !(memcmp((unsigned char *) &chunk_crc, picture+from+len, 4));
}

_Bool png_validate_hdr(const struct png_chunk_hdr *restrict hdr, const unsigned char *restrict compare) {
    return !(memcmp(hdr->type, compare, 4));
}
