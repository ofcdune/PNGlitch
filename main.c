#include "png.h"

int main(int argc, char *argv[]) {

    if (argc != 3) {
        printf("Usage: %s [INPUT] [OUTPUT]\n", argv[0]);
        exit(1);
    }

    uint32_t *crc_tbl = mk_crc_tbl();

    int fd;
    if ((fd = open(argv[1], O_RDONLY | O_BINARY)) == -1) {
        printf("Failed to open file '%s'\n", argv[1]);
        exit(1);
    }

    size_t file_size = lseek(fd, 0L, SEEK_END);
    lseek(fd, 0L, SEEK_SET);

    unsigned char *picture = (unsigned char *) calloc(file_size, sizeof(*picture));
    CHALLOC(picture)

    if (read(fd, picture, file_size) == -1) {
        printf("Failed to read file '%s'\n", argv[1]);
        close(fd);
        exit(1);
    }
    close(fd);

    if (!png_validate_signature(picture)) {
        fputs("The image does not have a valid png signature, aborting!\n", stderr);
        exit(1);
    }

    struct png *start = png_extract_chunks(picture, crc_tbl);
    free(picture);

    if (!png_validate_hdr(start->chunk_hdr, (const unsigned char *) "IHDR")) {
        fputs("IHDR chunk is not the first chunk of the image, aborting!", stderr);
        exit(1);
    }

    struct png_stats *image_info = png_extract_stats(start->chunk_data);
    png_validate_ihdr(image_info);

    unsigned char *compressed_buffer;
    size_t compressed_size = png_extract_data(start, &compressed_buffer);

    unsigned char *decompressed_buffer;
    size_t decompressed_size = png_zlib_decompress(compressed_buffer, compressed_size, &decompressed_buffer);
    free(compressed_buffer);

    size_t reconstructed_size = decompressed_size - image_info->height;
    unsigned char *reconstructed_image = png_reconstruct_image(decompressed_buffer, reconstructed_size, image_info);
    free(decompressed_buffer);

    unsigned char *filtered_ = png_filter_image_fixed(reconstructed_image, reconstructed_size, image_info, 4);
    unsigned char *compressed;

    compressed_size = png_zlib_compress(filtered_, image_info->height + reconstructed_size, &compressed);

    image_info->width = byteswap_ulong(image_info->width);
    image_info->height = byteswap_ulong(image_info->height);

    struct png *idat = png_inject_data(compressed, compressed_size, 1 << 16);

    file_size = png_recycle_chunks(start, idat);
    picture = png_flatten_image(start, file_size, crc_tbl);

    if ((fd = open(argv[2], O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, S_IREAD | S_IWRITE)) == -1) {
        fprintf(stderr, "Failed to open file '%s'\n", argv[2]);
        exit(1);
    }

    if (write(fd, picture, file_size) == -1) {
        fprintf(stderr, "Failed to open file '%s'\n", argv[2]);
        close(fd);
        exit(1);
    }
    close(fd);

    free(crc_tbl);

    return 0;
}
