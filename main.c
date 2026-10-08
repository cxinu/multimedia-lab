#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Helper to read 16-bit Big-Endian integers
uint16_t read_u16_be(FILE *fp) {
    uint8_t buf[2];
    if (fread(buf, 1, 2, fp) != 2) return 0;
    return (uint16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
}

// Helper to read 32-bit Big-Endian integers
uint32_t read_u32_be(FILE *fp) {
    uint8_t buf[4];
    if (fread(buf, 1, 4, fp) != 4) return 0;
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  |
            (uint32_t)buf[3];
}

// Helper to read 16-bit Little-Endian integers
uint16_t read_u16_le(FILE *fp) {
    uint8_t buf[2];
    if (fread(buf, 1, 2, fp) != 2) return 0;
    return (uint16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
}

// Helper to read 32-bit Little-Endian integers
uint32_t read_u32_le(FILE *fp) {
    uint8_t buf[4];
    if (fread(buf, 1, 4, fp) != 4) return 0;
    return (uint32_t)buf[0]        |
          ((uint32_t)buf[1] << 8)  |
          ((uint32_t)buf[2] << 16) |
          ((uint32_t)buf[3] << 24);
}

// Helper to decode ID3v2 Synchsafe Integers (7 bits per byte)
uint32_t decode_synchsafe(const uint8_t bytes[4]) {
    return ((uint32_t)(bytes[0] & 0x7F) << 21) |
           ((uint32_t)(bytes[1] & 0x7F) << 14) |
           ((uint32_t)(bytes[2] & 0x7F) << 7)  |
            (uint32_t)(bytes[3] & 0x7F);
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


// JPEG image
// https://en.wikipedia.org/wiki/JPEG
// 2 Bytes - MAGIC NUMBER (0xFF 0xD8, Start of Image SOI)
// MARKERS [0xFF + Marker Byte + 2B Payload Length]
// [SOF0] (Start of Frame 0 - Baseline DCT)
// 1B - PRECISION (bits per sample, usually 8)
// 2B - HEIGHT
// 2B - WIDTH
// 1B - NUMBER OF COMPONENTS (1=Grayscale, 3=YCbCr, 4=CMYK)
// ...


// WAV audio
// https://en.wikipedia.org/wiki/WAV
// RIFF CONTAINER [4B "RIFF", 4B File Size, 4B "WAVE"]
// CHUNKS [fmt , data, ...]
// [fmt ] (16+ Bytes)
// 2B - AUDIO FORMAT (1=PCM uncompressed)
// 2B - NUM CHANNELS (1=Mono, 2=Stereo)
// 4B - SAMPLE RATE (e.g. 44100 Hz)
// 4B - BYTE RATE
// 2B - BLOCK ALIGN
// 2B - BITS PER SAMPLE (e.g. 16-bit)
// ...


// MP3 audio
// https://en.wikipedia.org/wiki/MP3
// https://id3.org/id3v2.3.0
// [OPTIONAL] ID3v2 TAG HEADER (10 Bytes)
// 3B - MAGIC NUMBER ("ID3")
// 2B - VERSION (Major, Revision e.g., 0x03 0x00 for v2.3)
// 1B - FLAGS (Unsynchronization, Extended Header, Experimental)
// 4B - TAG SIZE (Synchsafe integer - 7 bits used per byte)
// [ID3 FRAMES / PAYLOAD...]
// 
// AUDIO FRAMES (Raw Bitstream)
// [MPEG FRAME HEADER] (4 Bytes / 32 Bits)
// 11 bits - FRAME SYNC (All bits set to '1': 0xFFE0 mask)
//  2 bits - MPEG AUDIO VERSION (11=MPEG-1, 10=MPEG-2, 00=MPEG-2.5)ZAYN - Dusk Till Dawn.mp3
//  2 bits - LAYER DESCRIPTION (11=Layer I, 10=Layer II, 01=Layer III)
//  1 bit  - CRC PROTECTION (0=Protected, 1=Not Protected)
//  4 bits - BITRATE INDEX (Lookup table based on Version & Layer)
//  2 bits - SAMPLING RATE FREQUENCY INDEX
//  1 bit  - PADDING BIT (0=No padding, 1=Padded)
//  1 bit  - PRIVATE BIT
//  2 bits - CHANNEL MODE (00=Stereo, 01=Joint Stereo, 10=Dual Channel, 11=Mono)
// ...


// MP4 video / ISOBMFF
// https://en.wikipedia.org/wiki/ISO_base_media_file_format
// ATOMS / BOXES [ftyp, moov, mdat, ...]
// [HEADER] (8 Bytes)
// 4B - SIZE (Big-Endian atom length)
// 4B - TYPE (ASCII e.g. "ftyp", "moov", "trak")
// ...


// AVI video
// https://en.wikipedia.org/wiki/Audio_Video_Interleave
// RIFF CONTAINER [4B "RIFF", 4B File Size, 4B "AVI "]
// LISTS & CHUNKS [hdrl, movi, ...]
// ...

#pragma pack(push, 1)
typedef struct {
    uint16_t magic;
    uint32_t file_size;
    uint32_t reserved;
    uint32_t pixel_offset;
} BmpFileHeader;

typedef struct {
    uint32_t header_size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t image_size;
} BmpDibHeader;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint8_t  bit_depth;
    uint8_t  color_type;
    uint8_t  compression;
    uint8_t  filter;
    uint8_t  interlace;
} PngIhdr;

typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} WavFmtChunk;
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

        if (fread(type, 1, 4, fp) != 4) break;

        printf("-> Found Chunk: [%s] | Data Length: %u bytes\n", type, length);

        if (strcmp(type, "IHDR") == 0) {
            PngIhdr ihdr;
            if (fread(&ihdr, sizeof(PngIhdr), 1, fp) == 1) {
                uint32_t width = ((ihdr.width >> 24) & 0xFF) | ((ihdr.width >> 8) & 0xFF00) |
                                 ((ihdr.width << 8) & 0xFF0000) | ((ihdr.width << 24) & 0xFF000000);
                uint32_t height = ((ihdr.height >> 24) & 0xFF) | ((ihdr.height >> 8) & 0xFF00) |
                                  ((ihdr.height << 8) & 0xFF0000) | ((ihdr.height << 24) & 0xFF000000);

                printf("   [IHDR Meta] Dimensions: %u x %u | Bit Depth: %u | Color Type: %u\n",
                       width, height, ihdr.bit_depth, ihdr.color_type);
            }
            fseek(fp, 4, SEEK_CUR);
        } else if (strcmp(type, "IEND") == 0) {
            break;
        } else {
            fseek(fp, (long)length + 4, SEEK_CUR);
        }
    }
}

void parse_jpeg(FILE *fp) {
    fseek(fp, 2, SEEK_SET);

    printf("\n[Format: JPEG]\n");
    printf("Scanning Segments...\n");

    while (1) {
        uint8_t marker_prefix;
        if (fread(&marker_prefix, 1, 1, fp) != 1) break;

        if (marker_prefix != 0xFF) continue;

        uint8_t marker;
        if (fread(&marker, 1, 1, fp) != 1) break;
        if (marker == 0x00 || marker == 0xFF) continue;

        if (marker == 0xD9 || marker == 0xDA) {
            printf("-> Found Marker: [0xFF%02X] - Reached scan data or EOI\n", marker);
            break;
        }

        uint16_t length = read_u16_be(fp);

        if ((marker >= 0xC0 && marker <= 0xC3) || 
            (marker >= 0xC5 && marker <= 0xC7) || 
            (marker >= 0xC9 && marker <= 0xCB) || 
            (marker >= 0xCD && marker <= 0xCF)) {

            uint8_t precision;
            fread(&precision, 1, 1, fp);
            uint16_t height = read_u16_be(fp);
            uint16_t width = read_u16_be(fp);
            uint8_t components;
            fread(&components, 1, 1, fp);

            printf("-> Found Marker: [0xFF%02X] | Length: %u bytes\n", marker, length);
            printf("   [SOF Meta] Dimensions: %u x %u | Precision: %u-bit | Components: %u\n",
                   width, height, precision, components);

            fseek(fp, (long)length - 8, SEEK_CUR);
        } else {
            printf("-> Found Marker: [0xFF%02X] | Length: %u bytes\n", marker, length);
            fseek(fp, (long)length - 2, SEEK_CUR);
        }
    }
}

void parse_wav(FILE *fp) {
    fseek(fp, 4, SEEK_SET);
    uint32_t file_size = read_u32_le(fp) + 8;

    printf("\n[Format: WAV Audio]\n");
    printf("File Size:     %u bytes\n", file_size);
    printf("Scanning Chunks...\n");

    fseek(fp, 12, SEEK_SET);

    while (1) {
        char type[5] = {0};
        if (fread(type, 1, 4, fp) != 4) break;

        uint32_t length = read_u32_le(fp);
        printf("-> Found Chunk: [%s] | Data Length: %u bytes\n", type, length);

        if (memcmp(type, "fmt ", 4) == 0) {
            WavFmtChunk fmt;
            if (fread(&fmt, sizeof(WavFmtChunk), 1, fp) == 1) {
                printf("   [fmt Meta] Format: %u | Channels: %u | Sample Rate: %u Hz | Bit Depth: %u-bit\n",
                       fmt.audio_format, fmt.num_channels, fmt.sample_rate, fmt.bits_per_sample);
            }
            if (length > sizeof(WavFmtChunk)) {
                fseek(fp, (long)(length - sizeof(WavFmtChunk)), SEEK_CUR);
            }
        } else {
            fseek(fp, (long)length, SEEK_CUR);
        }

        if (length % 2 != 0) {
            fseek(fp, 1, SEEK_CUR);
        }
    }
}

void parse_mp3(FILE *fp) {
    fseek(fp, 0, SEEK_SET);
    uint8_t header[10];

    printf("\n[Format: MP3 Audio]\n");

    long id3v2_tag_size = 0;
    if (fread(header, 1, 10, fp) == 10 && memcmp(header, "ID3", 3) == 0) {
        uint8_t ver_major = header[3];
        uint8_t ver_minor = header[4];
        uint32_t tag_body_size = decode_synchsafe(&header[6]);
        id3v2_tag_size = (long)tag_body_size + 10;

        printf("-> Found ID3v2 Header: Version 2.%u.%u | Tag Size: %ld bytes\n",
               ver_major, ver_minor, id3v2_tag_size);
    }

    fseek(fp, id3v2_tag_size, SEEK_SET);

    // Look for first valid MPEG frame sync header (0xFFE0 mask)
    uint8_t sync_buf[4];
    int frame_found = 0;

    while (fread(sync_buf, 1, 1, fp) == 1) {
        if (sync_buf[0] == 0xFF) {
            if (fread(&sync_buf[1], 1, 3, fp) == 3) {
                if ((sync_buf[1] & 0xE0) == 0xE0) {
                    frame_found = 1;
                    break;
                }
                fseek(fp, -3, SEEK_CUR);
            }
        }
    }

    if (frame_found) {
        uint8_t mpeg_ver_bits = (sync_buf[1] >> 3) & 0x03;
        uint8_t layer_bits    = (sync_buf[1] >> 1) & 0x03;
        uint8_t bitrate_index = (sync_buf[2] >> 4) & 0x0F;
        uint8_t sample_index  = (sync_buf[2] >> 2) & 0x03;

        const char *mpeg_ver_str = (mpeg_ver_bits == 3) ? "MPEG Version 1" :
                                   (mpeg_ver_bits == 2) ? "MPEG Version 2" :
                                   (mpeg_ver_bits == 0) ? "MPEG Version 2.5" : "Reserved";

        const char *layer_str = (layer_bits == 3) ? "Layer I" :
                                (layer_bits == 2) ? "Layer II" :
                                (layer_bits == 1) ? "Layer III" : "Reserved";

        static const uint16_t bitrates_v1_l3[16] = {
            0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0
        };

        static const uint32_t sample_rates_v1[4] = {44100, 48000, 32000, 0};

        uint16_t bitrate = (mpeg_ver_bits == 3 && layer_bits == 1) ? bitrates_v1_l3[bitrate_index] : 0;
        uint32_t sample_rate = (mpeg_ver_bits == 3) ? sample_rates_v1[sample_index] : 0;

        printf("-> First MPEG Frame Synced\n");
        printf("   [Frame Meta] Standard: %s | %s\n", mpeg_ver_str, layer_str);
        if (bitrate > 0)     printf("   [Frame Meta] Bitrate: ~%u kbps\n", bitrate);
        if (sample_rate > 0) printf("   [Frame Meta] Sample Rate: %u Hz\n", sample_rate);
    } else {
        printf("-> No MPEG Frame Sync Header located.\n");
    }
}

void parse_mp4(FILE *fp) {
    fseek(fp, 0, SEEK_END);
    long file_len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    printf("\n[Format: MP4 Video]\n");
    printf("Scanning Atoms...\n");

    while (ftell(fp) < file_len) {
        uint32_t length = read_u32_be(fp);
        char type[5] = {0};

        if (fread(type, 1, 4, fp) != 4) break;

        uint64_t actual_length = length;
        if (length == 1) {
            uint32_t high = read_u32_be(fp);
            uint32_t low = read_u32_be(fp);
            actual_length = ((uint64_t)high << 32) | low;
        } else if (length == 0) {
            long current_pos = ftell(fp);
            if (current_pos >= 0 && file_len >= current_pos) {
                actual_length = (uint64_t)(file_len - current_pos) + 8;
            } else {
                break;
            }
        }

        printf("-> Found Atom: [%s] | Data Length: %llu bytes\n", type, (unsigned long long)actual_length);

        if (strcmp(type, "ftyp") == 0) {
            char major_brand[5] = {0};
            fread(major_brand, 1, 4, fp);
            uint32_t minor_version = read_u32_be(fp);

            printf("   [ftyp Meta] Major Brand: %s | Minor Version: %u\n", major_brand, minor_version);
            fseek(fp, (long)(actual_length - 16), SEEK_CUR);
        } else if (strcmp(type, "moov") == 0 || strcmp(type, "trak") == 0 || strcmp(type, "mdia") == 0) {
            continue;
        } else {
            uint64_t skip = (length == 1) ? actual_length - 16 : actual_length - 8;
            fseek(fp, (long)skip, SEEK_CUR);
        }
    }
}

void parse_avi(FILE *fp) {
    fseek(fp, 4, SEEK_SET);
    uint32_t file_size = read_u32_le(fp) + 8;

    printf("\n[Format: AVI Video]\n");
    printf("File Size:     %u bytes\n", file_size);
    printf("Scanning Chunks...\n");

    fseek(fp, 12, SEEK_SET);

    while (1) {
        char type[5] = {0};
        if (fread(type, 1, 4, fp) != 4) break;

        uint32_t length = read_u32_le(fp);

        if (memcmp(type, "LIST", 4) == 0) {
            char list_type[5] = {0};
            fread(list_type, 1, 4, fp);
            printf("-> Found LIST: [%s] | Data Length: %u bytes\n", list_type, length);
        } else {
            printf("-> Found Chunk: [%s] | Data Length: %u bytes\n", type, length);
            fseek(fp, (long)length, SEEK_CUR);
        }

        if (length % 2 != 0) {
            fseek(fp, 1, SEEK_CUR);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <media_file>", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filepath = argv[1];

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    uint8_t magic[12];
    if (fread(magic, 1, 12, fp) != 12) {
        fprintf(stderr, "Error reading file signature.\n");
        fclose(fp);
        return EXIT_FAILURE;
    }

    if (magic[0] == 'B' && magic[1] == 'M') {
        parse_bmp(fp);
    }
    else if (magic[0] == 0x89 && magic[1] == 'P' && magic[2] == 'N' && magic[3] == 'G') {
        parse_png(fp);
    }
    else if (magic[0] == 0xFF && magic[1] == 0xD8 && magic[2] == 0xFF) {
        parse_jpeg(fp);
    }
    else if (memcmp(magic, "RIFF", 4) == 0 && memcmp(magic + 8, "WAVE", 4) == 0) {
        parse_wav(fp);
    }
    else if (memcmp(magic, "RIFF", 4) == 0 && memcmp(magic + 8, "AVI ", 4) == 0) {
        parse_avi(fp);
    }
    else if (memcmp(magic + 4, "ftyp", 4) == 0) {
        parse_mp4(fp);
    }
    else if (memcmp(magic, "ID3", 3) == 0 || (magic[0] == 0xFF && (magic[1] & 0xE0) == 0xE0)) {
        parse_mp3(fp);
    }
    else {
        printf("Unsupported file format.\n");
    }

    fclose(fp);
    return EXIT_SUCCESS;
}
