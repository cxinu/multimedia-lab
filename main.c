#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Helper to read 32-bit Big-Endian integers (PNG format)
uint32_t read_u32_be(FILE *fp) {
    uint8_t buf[4];
    if (fread(buf, 1, 4, fp) != 4) return 0;
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  |
            (uint32_t)buf[3];
}

// BMP image
// FILE HEADER - 14 bytes
// HEADER INFO - 40+ bytes, SIZE DEPENDS ON HEADER VARIANT
// COLOR TABLE - variable, optional (for low color imgs)
// RAW DATA - variable, pixel color data
//
// FILE HEADER (14 bytes)
// 2 bytes - MAGIC NUMBER (0x42 0x4d, "BM")
// 4 bytes - FILE SIZE
// 4 bytes - RESERVED (00 00 00 00, unused) [legacy slop]
// 4 bytes - PIXEL OFFSET (which bytes pixel data begins)
//
// DIB (Device-Independent Bitmap) HEADER / INFO HEADER (~40+ bytes)
// 4 bytes - HEADER SIZE (<HEADER INFO> size)
// 4 bytes - WIDTH (in pixels)
// 4 bytes - HEIGHT (in pixels)
// 2 bytes - PLANES (Always 1, legacy)
// 2 bytes - BITS PER PIXEL (bpp) COLOR DEPTH
// 4 bytes - COMPRESSION (0=BI_RGB=uncompressed)
// 4 bytes - IMAGE SIZE (Raw pixel array)
// ...


// PNG image
// https://en.wikipedia.org/wiki/PNG
// 8 Bytes - MAGIC NUMBER (0x89 P N G 0D 0A 1A 0A)
// CHUNKS [IHDR, PLTE, IDAT, IEND]
// [IHDR] (13 Bytes)
// 4B - WIDTH
// 4B - HEIGHT
// 1B - BIT DEPTH
// 1B - COLOR TYPE
// 1B - COMPRESSION METHOD
// 1B - FILTER METHOD
// 1B - INTERLACE METHOD
// ...


#pragma pack(push, 1)
// BMP Headers
typedef struct {
    uint16_t magic;         // Must be 0x4D42 ("BM" in Little-Endian)
    uint32_t file_size;     // Size of the file in bytes
    uint32_t reserved;      // Reserved (0)
    uint32_t pixel_offset;  // Start offset of pixel data
} BmpFileHeader;

typedef struct {
    uint32_t header_size;   // Size of this DIB header
    int32_t  width;         // Image width in pixels
    int32_t  height;        // Image height in pixels
    uint16_t planes;        // Always 1
    uint16_t bpp;           // Bits per pixel (color depth)
    uint32_t compression;   // Compression method (0 = uncompressed BI_RGB)
    uint32_t image_size;    // Image size (can be 0 for uncompressed)
} BmpDibHeader;

// PNG IHDR (Big Endian)
typedef struct {
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t  color_type;
    uint8_t  compression;
    uint8_t  filter;
    uint8_t  interlace;
} PngIhdr;
#pragma pack(pop)

void parse_bmp(FILE *fp) {
    fseek(fp, 0, SEEK_SET);
    BmpFileHeader file_header;
    BmpDibHeader dib_header;

    if (fread(&file_header, sizeof(BmpFileHeader), 1, fp) != 1 ||
        fread(&dib_header, sizeof(BmpDibHeader), 1, fp) != 1) {
        fprintf(stderr, "Error reading BMP headers.\n");
        return;
    }

    printf("\n[Format: BMP]\n");
    printf("File Size:     %u bytes\n", file_header.file_size);
    printf("Pixel Offset:  Byte %u\n", file_header.pixel_offset);
    printf("DIB Size:      %u bytes\n", dib_header.header_size);
    printf("Dimensions:    %d x %d pixels\n", dib_header.width, dib_header.height);
    printf("Color Depth:   %u bpp\n", dib_header.bpp);
}

void parse_png(FILE *fp) {
    fseek(fp, 8, SEEK_SET);

    printf("\n[Format: PNG]\n");
    printf("Scanning Chunks...\n");

    while (1) {
        uint32_t length = read_u32_be(fp);
        char type[5] = {0};

        if (fread(type, 1, 4, fp) != 4) break; // EOF or read error

        printf("-> Found Chunk: [%s] | Data Length: %u bytes\n", type, length);

        if (strcmp(type, "IHDR") == 0) {
            PngIhdr ihdr;
            if (fread(&ihdr, sizeof(PngIhdr), 1, fp) == 1) {
                // Byte swap Big-Endian dimensions to host CPU endianness
                uint32_t width = ((ihdr.width >> 24) & 0xFF) | ((ihdr.width >> 8) & 0xFF00) |
                                 ((ihdr.width << 8) & 0xFF0000) | ((ihdr.width << 24) & 0xFF000000);
                uint32_t height = ((ihdr.height >> 24) & 0xFF) | ((ihdr.height >> 8) & 0xFF00) |
                                  ((ihdr.height << 8) & 0xFF0000) | ((ihdr.height << 24) & 0xFF000000);

                printf("   [IHDR Meta] Dimensions: %u x %u | Bit Depth: %u | Color Type: %u\n",
                       width, height, ihdr.bit_depth, ihdr.color_type);
            }
            fseek(fp, 4, SEEK_CUR); // Skip 4-byte CRC
        } else if (strcmp(type, "IEND") == 0) {
            break; // End of PNG stream
        } else {
            // Skip chunk payload + 4-byte CRC
            fseek(fp, length + 4, SEEK_CUR);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <image.bmp>", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filepath = argv[1];

    FILE *fp;
    fp = fopen(filepath, "rb");
    if (!fp) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    uint8_t magic[8];
    if (fread(magic, 1, 8, fp) != 8) {
        fprintf(stderr, "Error reading file signature.\n");
        fclose(fp);
        return EXIT_FAILURE;
    }

    // BMP magic (2 bytes: B M)
    if (magic[0] == 'B' && magic[1] == 'M') {
        parse_bmp(fp);
    }
    // PNG magic (4 bytes: 0x89 P N G, (89 50 4E 47 0D 0A 1A 0A))
    else if (magic[0] == 0x89 && magic[1] == 'P' && magic[2] == 'N' && magic[3] == 'G') {
        parse_png(fp);
    }
    else {
        printf("Unsupported file format.\n");
    }

    fclose(fp);
    return EXIT_SUCCESS;
}
