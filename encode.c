#include "png.h"

#define CON_A(h, c) ((c) >= bpp ? unfiltered[(h) * stride + (c) - bpp] : 0)
#define CON_B(h, c) ((h) > 0 ? unfiltered[((h) - 1) * stride + (c)] : 0)
#define CON_C(h, c) ((h > 0 && c >= bpp) ? unfiltered[((h) - 1) * stride + c - bpp] : 0)

#define PNG_IDAT_REMAINING (compressed_len - offset)

unsigned char *png_filter_image_fixed(const unsigned char *restrict unfiltered, size_t unfiltered_size, const struct png_stats *restrict stats, unsigned char filter_method) {

    if (filter_method > 4) {
        fputs("Unknown filter method!\n", stderr);
        exit(1);
    }

    unsigned char *filtered = (unsigned char *) calloc(unfiltered_size + stats->height, sizeof(*filtered));
    CHALLOC(filtered)

    unsigned char bpp = stats->bit_depth;
    uint32_t c, h, offset = 0, stride = unfiltered_size / stats->height;
    size_t i = 0;

    for (h = 0; h < stats->height; h++) {
        filtered[offset++] = filter_method;

        for (c = 0; c < stride; c++) {
            switch (filter_method) {
                case 0:
                    filtered[offset++] = unfiltered[i++];
                    break;
                case 1:
                    filtered[offset++] = unfiltered[i++] - CON_A(h, c);
                    break;
                case 2:
                    filtered[offset++] = unfiltered[i++] - CON_B(h, c);
                    break;
                case 3:
                    filtered[offset++] = unfiltered[i++] - ((CON_A(h, c) + CON_B(h, c)) / 2);
                    break;
                case 4:
                    filtered[offset++] = unfiltered[i++] - paeth(CON_A(h, c), CON_B(h, c), CON_C(h, c));
                    break;
            }
        }
    }

    return filtered;
}

size_t png_zlib_compress(unsigned char *restrict uncompressed, size_t strm_len, unsigned char **restrict compressed) {

    unsigned char *temp;
    compressed[0] = (unsigned char *) calloc(1, sizeof(*compressed[0]));
    CHALLOC(compressed[0])

    size_t offset = 0, compressed_len = 0;

    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    int ret, flush;

    unsigned char input[CHUNK];
    unsigned char output[CHUNK];

    deflateInit(&stream, Z_DEFAULT_COMPRESSION);

    do {

        if (strm_len >= CHUNK) {
            stream.avail_in = CHUNK;
            strm_len -= CHUNK;
            memcpy(input, uncompressed + offset, CHUNK);
            stream.next_in = input;

            flush = Z_NO_FLUSH;
        } else {
            stream.avail_in = strm_len;
            stream.next_in = uncompressed + offset;

            flush = Z_FINISH;
        }

        do {
            stream.avail_out = CHUNK;
            stream.next_out = output;

            if (Z_STREAM_ERROR == (ret = deflate(&stream, flush))) {
                zerr(ret);
                exit(1);
            }

            switch (ret) {
                case Z_NEED_DICT:
                    ret = Z_DATA_ERROR;
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                    zerr(ret);
                    exit(1);
                default:
                    break;
            }

            temp = (unsigned char *) realloc(compressed[0], compressed_len + CHUNK - stream.avail_out);
            CHALLOC(temp)
            compressed[0] = temp;

            memcpy(compressed[0] + compressed_len, output, CHUNK - stream.avail_out);
            compressed_len += CHUNK - stream.avail_out;

        } while (stream.avail_out == 0);
        offset += CHUNK;

    } while (Z_FINISH != flush);

    if (ret != Z_STREAM_END) {
        zerr(ret);
        exit(1);
    }

    deflateEnd(&stream);
    return compressed_len;
}

struct png * png_inject_data(unsigned char *restrict compressed, size_t compressed_len, uint32_t max_len) {
    struct png *idat_linked = (struct png *) calloc(1, sizeof(*idat_linked));
    CHALLOC(idat_linked)

    size_t offset = 0;
    uint32_t chunk_len;
    struct png *ptr = idat_linked;

    ptr->chunk_hdr = (struct png_chunk_hdr *) calloc(1, sizeof(*ptr->chunk_hdr));
    CHALLOC(ptr->chunk_hdr)

    memcpy(ptr->chunk_hdr->type, (unsigned char *) "IDAT", 4);

    if (max_len > PNG_IDAT_REMAINING) {
        chunk_len = PNG_IDAT_REMAINING;
    } else {
        chunk_len = max_len;
    }

    ptr->chunk_hdr->len = chunk_len;
    ptr->chunk_data = (unsigned char *) calloc(chunk_len, sizeof(*ptr->chunk_data));
    CHALLOC(ptr->chunk_data)

    memcpy(ptr->chunk_data, compressed + offset, chunk_len);

    offset += chunk_len;

    while (PNG_IDAT_REMAINING > 0) {

        ptr->next = (struct png *) calloc(1, sizeof(*ptr->next));
        CHALLOC(ptr->next)
        ptr = ptr->next;

        ptr->chunk_hdr = (struct png_chunk_hdr *) calloc(1, sizeof(*ptr->chunk_hdr));
        CHALLOC(ptr->chunk_hdr)

        memcpy(ptr->chunk_hdr->type, (unsigned char *) "IDAT", 4);

        if (max_len > PNG_IDAT_REMAINING) {
            chunk_len = PNG_IDAT_REMAINING;
        } else {
            chunk_len = max_len;
        }

        ptr->chunk_hdr->len = chunk_len;
        ptr->chunk_data = (unsigned char *) calloc(chunk_len, sizeof(*ptr->chunk_data));
        CHALLOC(ptr->chunk_data)

        memcpy(ptr->chunk_data, compressed + offset, chunk_len);

        offset += chunk_len;
    }

    return idat_linked;
}

size_t png_recycle_chunks(struct png *restrict old_png_data, struct png *restrict idat_chunks) {
    size_t size_total = 8;

    struct png *ptr = old_png_data;
    struct png *ptr2, *remove;

    size_total += ptr->chunk_hdr->len + 12;

    while (!png_validate_hdr(ptr->next->chunk_hdr, (const unsigned char *) "IDAT")) {
        ptr = ptr->next;
        size_total += ptr->chunk_hdr->len + 12;
    }

    ptr2 = ptr->next->next;
    remove = ptr->next;

    while (NULL != ptr2) {
        free(remove->chunk_data);
        free(remove->chunk_hdr);
        free(remove);

        ptr->next = ptr2;
        remove = ptr2;
        ptr2 = ptr2->next;
    }

    ptr->next = idat_chunks;
    while (NULL != ptr->next) {
        ptr = ptr->next;
        size_total += ptr->chunk_hdr->len + 12;
    }

    ptr->next = remove;
    while (NULL != ptr->next) {
        ptr = ptr->next;
        size_total += ptr->chunk_hdr->len + 12;
    }

    return size_total;
}

unsigned char *png_flatten_image(struct png *restrict png_linked, size_t image_size, uint32_t *crc_table) {
    unsigned char *image = (unsigned char *) calloc(8 + image_size, sizeof(*image));
    CHALLOC(image)

    unsigned char *tmp = image;
    tmp = mempcpy(tmp, "\x89PNG\r\n\x1a\n", 8);
    uint32_t len_buffer, checksum;

    struct png *linked = png_linked;
    while (NULL != linked->next) {

        len_buffer = linked->chunk_hdr->len;
        linked->chunk_hdr->len = byteswap_ulong(linked->chunk_hdr->len);

        tmp = mempcpy(tmp, (unsigned char *) &linked->chunk_hdr->len, 4);
        tmp = mempcpy(tmp, linked->chunk_hdr->type, 4);
        tmp = mempcpy(tmp, linked->chunk_data, len_buffer);

        checksum = crc(linked->chunk_data, 0, len_buffer, crc_table);
        tmp = mempcpy(tmp, (unsigned char *) &checksum, 4);

        linked = linked->next;
    }

    return image;
}
