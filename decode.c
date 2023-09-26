#include "png.h"

#define RECON_A(h, c) ((c) >= bpp ? reconstructed[(h) * stride + (c) - bpp] : 0)
#define RECON_B(h, c) ((h) > 0 ? reconstructed[((h) - 1) * stride + (c)] : 0)
#define RECON_C(h, c) ((h > 0 && c >= bpp) ? reconstructed[((h) - 1) * stride + c - bpp] : 0)

struct png_chunk_hdr *png_chunk_hdr(unsigned char *picture) {
    unsigned char *buffer = (unsigned char *) calloc(8, sizeof(*buffer));
    CHALLOC(buffer)
    memcpy(buffer, picture, 8);

    struct png_chunk_hdr *hdr = (struct png_chunk_hdr *) buffer;
    hdr->len = byteswap_ulong(hdr->len);

    return hdr;
}

unsigned char *png_chunk_data(uint32_t len, unsigned char *restrict picture) {
    if (len == 0) {
        return NULL;
    }

    unsigned char *buffer = (unsigned char *) calloc(len, sizeof(*buffer));
    CHALLOC(buffer)

    memcpy(buffer, picture, len);

    return buffer;
}

void png_validate_ihdr(const struct png_stats *stats) {

    if ((stats->width * stats->height) == 0) {
        fputs("Width and height cannot be 0\n", stderr);
        exit(1);
    }

    char mask;
    switch (stats->color_type) {
        case 0:
            mask = 0b00011111;
            break;
        case 3:
            mask = 0b00001111;
            break;
        case 2:
        case 4:
        case 6:
            mask = 0b00011000;
            break;

        default:
            fprintf(stderr, "Invalid color type: %d\n", stats->color_type);
            exit(1);
    }

    if (!(mask & stats->bit_depth)) {
        fprintf(stderr, "Invalid bit depth: (%d for color type %d)\n", stats->bit_depth, stats->color_type);
        exit(1);
    }

    if ((stats->compression_method * stats->filter_method) != 0) {
        fputs("Unknown compression/filter method\n", stderr);
        exit(1);
    }

    if (stats->interlace_method > 1) {
        fputs("Unknown interlace method\n", stderr);
        exit(1);
    }
}

struct png *png_extract_chunks(unsigned char *picture, uint32_t *crc_tbl) {
    size_t offset = 8;

    struct png *png_linked, *start;
    struct png_chunk_hdr *ptr;

    png_linked = (struct png *) calloc(1, sizeof(*png_linked));
    CHALLOC(png_linked)
    start = png_linked;

    do {
        png_linked->next = (struct png *) calloc(1, sizeof(*png_linked));
        CHALLOC(png_linked->next)
        png_linked = png_linked->next;

        ptr = png_chunk_hdr(picture + offset);
        png_linked->chunk_hdr = ptr;

        offset += 4;
        png_linked->chunk_data = png_chunk_data(ptr->len, picture + offset + 4);
        if (!png_validate_chunk(picture, offset, ptr->len + 4, crc_tbl)) {
            fprintf(stderr, "Checksum failed for the %s header!", ptr->type);
            exit(1);
        }

        offset += 4 + ptr->len + 4;
    } while (memcmp(ptr->type, "IEND", 4) != 0);

    if (!png_validate_hdr(png_linked->chunk_hdr, (const unsigned char *) "IEND")) {
        fputs("IEND chunk is not the last chunk of the image, aborting!\n", stderr);
        exit(1);
    }

    png_linked = start->next;
    free(start);
    start = png_linked;

    return start;
}

size_t png_extract_data(struct png *restrict image_linked, unsigned char **restrict buffer) {

    struct png *png_linked = image_linked;
    size_t buffsize = 0;

    buffer[0] = (unsigned char *) calloc(1, sizeof(*buffer[0]));
    unsigned char *temp;

    while (!png_validate_hdr(png_linked->chunk_hdr, "IDAT")) {
        png_linked = png_linked->next;
        if (NULL == png_linked) {
            fputs("Image does not contain any IDAT chunk, aborting!", stderr);
            exit(1);
        }
    }


    while (png_validate_hdr(png_linked->chunk_hdr, "IDAT")) {
        if (NULL == png_linked->chunk_data) {
            continue;
        }

        temp = realloc(*buffer, buffsize + png_linked->chunk_hdr->len);
        CHALLOC(temp)
        *buffer = temp;

        memcpy(*buffer + buffsize, png_linked->chunk_data, png_linked->chunk_hdr->len);
        buffsize += png_linked->chunk_hdr->len;

        png_linked = png_linked->next;
    }

    return buffsize;
}

void zerr(int ret) {
    fputs("zpipe: ", stderr);
    switch (ret) {
        case Z_ERRNO:
            if (ferror(stdin))
                fputs("error reading stdin\n", stderr);
            if (ferror(stdout))
                fputs("error writing stdout\n", stderr);
            break;
        case Z_STREAM_ERROR:
            fputs("invalid compression level\n", stderr);
            break;
        case Z_DATA_ERROR:
            fputs("invalid or incomplete deflate data\n", stderr);
            break;
        case Z_MEM_ERROR:
            fputs("out of memory\n", stderr);
            break;
        case Z_VERSION_ERROR:
            fputs("zlib version mismatch!\n", stderr);
            break;
    }
}

size_t png_zlib_decompress(unsigned char *restrict compressed, size_t strm_len, unsigned char **restrict uncompressed) {

    unsigned char *temp;
    uncompressed[0] = (unsigned char *) calloc(1, sizeof(*uncompressed[0]));
    CHALLOC(uncompressed[0])

    size_t offset = 0, uncompressed_len = 0;

    z_stream stream;
    stream.avail_in = 0;
    stream.zfree = Z_NULL;
    stream.zalloc = Z_NULL;
    stream.opaque = Z_NULL;
    stream.next_in = Z_NULL;

    int ret;

    unsigned char input[CHUNK];
    unsigned char output[CHUNK];

    inflateInit(&stream);

    do {

        if (strm_len >= CHUNK) {
            stream.avail_in = CHUNK;
            strm_len -= CHUNK;
            memcpy(input, compressed + offset, CHUNK);
            stream.next_in = input;
        } else {
            stream.avail_in = strm_len;
            stream.next_in = compressed + offset;
        }

        do {
            stream.avail_out = CHUNK;
            stream.next_out = output;

            if (Z_STREAM_ERROR == (ret = inflate(&stream, Z_NO_FLUSH))) {
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

            temp = (unsigned char *) realloc(uncompressed[0], uncompressed_len + CHUNK - stream.avail_out);
            CHALLOC(temp)
            uncompressed[0] = temp;

            memcpy(uncompressed[0] + uncompressed_len, output, CHUNK - stream.avail_out);
            uncompressed_len += CHUNK - stream.avail_out;

        } while (stream.avail_out == 0);
        offset += CHUNK;

    } while (Z_STREAM_END != ret);

    inflateEnd(&stream);
    return uncompressed_len;
}

unsigned char paeth(unsigned char a, unsigned char b, unsigned char c) {
    unsigned char p = a + b - c;
    unsigned char pa = abs(p - a);
    unsigned char pb = abs(p - b);
    unsigned char pc = abs(p - c);

    if (pa <= pb && pa <= pc) {
        return a;
    } else if (pb <= pc) {
        return b;
    } else {
        return c;
    }
}

unsigned char *png_reconstruct_image(const unsigned char *restrict uncompressed, size_t reconstructed_size, const struct png_stats *restrict stats) {

    uint32_t h, c;
    size_t i = 0, offset = 0;

    unsigned char filter_type, filtered_byte;
    unsigned char bpp = stats->bit_depth;

    uint32_t stride = reconstructed_size / stats->height;

    unsigned char *reconstructed = (unsigned char *) calloc(reconstructed_size, sizeof(*reconstructed));
    CHALLOC(reconstructed)

    for (h = 0; h < stats->height; h++) {
        filter_type = uncompressed[i++];

        for (c = 0; c < stride; c++) {
            filtered_byte = uncompressed[i++];

            switch (filter_type) {
                case 0:
                    reconstructed[offset++] = filtered_byte;
                    break;
                case 1:
                    reconstructed[offset++] = filtered_byte + RECON_A(h, c);
                    break;
                case 2:
                    reconstructed[offset++] = filtered_byte + RECON_B(h, c);
                    break;
                case 3:
                    reconstructed[offset++] = filtered_byte + ((RECON_A(h, c) + RECON_B(h, c)) / 2);
                    break;
                case 4:
                    reconstructed[offset++] = filtered_byte + paeth(RECON_A(h, c), RECON_B(h, c), RECON_C(h, c));
                    break;
                default:
                    fputs("Invalid filter byte, aborting!\n", stderr);
                    exit(1);
            }
        }
    }
    return reconstructed;
}
