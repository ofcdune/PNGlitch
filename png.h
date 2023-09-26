#ifndef PNGREADER_PNG_H
#define PNGREADER_PNG_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys\stat.h>

#include <unistd.h>
#include <fcntl.h>
#include <zlib.h>

#ifdef __unix__
    #define byteswap_ulong(x)
    (
    char temp, temp2;
    unsigned char *l = (unsigned char *) &x;

    temp = l[0];
    temp2 = l[3];

    l[0] = temp2;
    l[3] = temp;

    temp = l[1];
    temp2 = l[2];

    l[1] = temp2;
    l[2] = temp;
    )
#elif __WIN32__ || _MSC_VER
    #define byteswap_ulong(x) _byteswap_ulong(x)
#endif

#define CHALLOC(x) if (NULL == (x)) {fputs("Failed to init buffer\n", stderr);exit(1);}

#define CHUNK 32768

struct __attribute__((packed)) png_stats {
    uint32_t width;
    uint32_t height;
    unsigned char bit_depth;
    unsigned char color_type;
    unsigned char compression_method;
    unsigned char filter_method;
    unsigned char interlace_method;
};

struct png_chunk_hdr {
    uint32_t len;
    unsigned char type[4];
};

struct png {
    struct png_chunk_hdr *chunk_hdr;
    unsigned char *chunk_data;
    struct png *next;
};


struct png *png_extract_chunks(unsigned char *picture, uint32_t *crc_tbl);
struct png_chunk_hdr *png_chunk_hdr(unsigned char *picture);
unsigned char *png_chunk_data(uint32_t len, unsigned char *restrict picture);
size_t png_zlib_decompress(unsigned char *compressed, size_t strm_len, unsigned char **uncompressed);
_Bool png_validate_chunk(const unsigned char *picture, uint32_t from, uint32_t len, uint32_t *crc_tbl);
_Bool png_validate_signature(const unsigned char *picture);
_Bool png_validate_hdr(const struct png_chunk_hdr *restrict hdr, const unsigned char *compare);
unsigned char *png_filter_image_fixed(const unsigned char *restrict unfiltered, size_t unfiltered_size, const struct png_stats *restrict stats, unsigned char filter_method);
size_t png_zlib_compress(unsigned char *restrict uncompressed, size_t strm_len, unsigned char **restrict compressed);
struct png * png_inject_data(unsigned char *restrict compressed, size_t compressed_len, uint32_t max_len);
size_t png_recycle_chunks(struct png *restrict old_png_data, struct png *restrict idat_chunks);
unsigned char *png_flatten_image(struct png *restrict png_linked, size_t image_size, uint32_t *crc_table);
struct png_stats *png_extract_stats(const unsigned char *idat_chunk_data);
void png_validate_ihdr(const struct png_stats *stats);
size_t png_extract_data(struct png *restrict image_linked, unsigned char **restrict buffer);
unsigned char *png_reconstruct_image(const unsigned char *restrict uncompressed, size_t reconstructed_size,
                                     const struct png_stats *restrict stats);
unsigned char paeth(unsigned char a, unsigned char b, unsigned char c);
void zerr(int ret);
uint32_t crc(const unsigned char *data, uint32_t offset, uint32_t len, const uint32_t *tbl);
uint32_t *mk_crc_tbl();

#endif
