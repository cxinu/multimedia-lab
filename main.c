#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

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

// Helper to read 64-bit Big-Endian integers
uint64_t read_u64_be(FILE *fp) {
    uint32_t hi = read_u32_be(fp);
    uint32_t lo = read_u32_be(fp);
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

// Helper to read 24-bit Big-Endian integers (FullBox flags field)
uint32_t read_u24_be(FILE *fp) {
    uint8_t buf[3];
    if (fread(buf, 1, 3, fp) != 3) return 0;
    return ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | (uint32_t)buf[2];
}

// Helper to read 32-bit Little-Endian signed integers
int32_t read_i32_le(FILE *fp) {
    uint8_t buf[4];
    if (fread(buf, 1, 4, fp) != 4) return 0;
    uint32_t u = (uint32_t)buf[0]        |
                ((uint32_t)buf[1] << 8)  |
                ((uint32_t)buf[2] << 16) |
                ((uint32_t)buf[3] << 24);
    int32_t v;
    memcpy(&v, &u, sizeof(v));
    return v;
}

// Helper to read 16-bit Little-Endian signed integers
int16_t read_i16_le(FILE *fp) {
    uint8_t buf[2];
    if (fread(buf, 1, 2, fp) != 2) return 0;
    uint16_t u = (uint16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
    int16_t v;
    memcpy(&v, &u, sizeof(v));
    return v;
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

// MP4: container boxes hold nested boxes; everything else is a leaf.
// (meta is a FullBox: 4 version/flags bytes precede its children.
//  ilst children are tag boxes like (c)nam whose payload is data boxes.)
#define MP4_MAX_DEPTH 8
#define MAC_TO_UNIX_EPOCH 2082844800ULL

// mvhd timescale, remembered for the mvex/mehd fragment-duration readout.
static uint32_t g_mvhd_timescale = 0;

static int is_container_box(const char *type) {
    static const char *names[] = {
        "moov", "trak", "mdia", "minf", "stbl", "edts",
        "udta", "meta", "dinf", "strk", "iprp", "mvex", "moof", "traf",
        "ilst", NULL
    };
    for (int i = 0; names[i] != NULL; i++) {
        if (strcmp(type, names[i]) == 0) return 1;
    }
    return 0;
}

static void print_indent(int depth) {
    printf("%*s", depth * 2, "");
}

// FourCCs may contain non-printables (e.g. 0xA9 in (c)nam); dots keep output sane.
static void print_fourcc(const char *fcc) {
    for (int i = 0; i < 4; i++) {
        unsigned char c = (unsigned char)fcc[i];
        putchar(isprint(c) ? (char)c : '.');
    }
}

// MP4 timestamps count seconds since 1904-01-01 (Mac epoch).
static void print_mac_time(uint64_t mac, const char *label, int depth) {
    print_indent(depth);
    if (mac == 0) {
        printf("[Meta] %s: N/A\n", label);
        return;
    }
    time_t t = (time_t)(mac - MAC_TO_UNIX_EPOCH);
    struct tm *tm = gmtime(&t);
    if (tm == NULL) {
        printf("[Meta] %s: raw %llu\n", label, (unsigned long long)mac);
        return;
    }
    char buf[32];
    if (strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", tm) == 0) {
        printf("[Meta] %s: raw %llu\n", label, (unsigned long long)mac);
        return;
    }
    printf("[Meta] %s: %s\n", label, buf);
}

// mdhd language: 1 pad bit + 3 x 5-bit codes, each + 0x60 = ASCII letter.
static void print_iso639_lang(uint16_t packed, int depth) {
    char lang[4];
    lang[0] = (char)(((packed >> 10) & 0x1F) + 0x60);
    lang[1] = (char)(((packed >> 5) & 0x1F) + 0x60);
    lang[2] = (char)(((packed & 0x1F) & 0x1F) + 0x60);
    lang[3] = '\0';
    print_indent(depth);
    printf("[mdhd Meta] Language: %s\n", lang);
}

static void decode_ftyp(FILE *fp, uint64_t payload, int depth) {
    if (payload < 8) return;
    char brand[5] = {0};
    if (fread(brand, 1, 4, fp) != 4) return;
    uint32_t minor = read_u32_be(fp);
    print_indent(depth);
    printf("[ftyp Meta] Major Brand: %.4s | Minor Version: %u", brand, minor);
    uint64_t n = (payload - 8) / 4;
    if (n > 0) {
        printf(" | Compatible: ");
        uint64_t show = n > 16 ? 16 : n;
        for (uint64_t i = 0; i < show; i++) {
            char compat[5] = {0};
            if (fread(compat, 1, 4, fp) != 4) break;
            printf("%.4s%s", compat, (i + 1 < show) ? ", " : "");
        }
        if (n > 16) printf(", ...");
    }
    putchar('\n');
}

static void decode_mvhd(FILE *fp, uint64_t payload, int depth) {
    if (payload < 4) return;
    int version = fgetc(fp);
    (void)read_u24_be(fp); // flags
    uint64_t creation = 0, modification = 0, duration = 0;
    uint32_t timescale = 0;
    if (version == 1) {
        if (payload < 36) return;
        creation = read_u64_be(fp);
        modification = read_u64_be(fp);
        timescale = read_u32_be(fp);
        duration = read_u64_be(fp);
    } else {
        if (payload < 24) return;
        creation = read_u32_be(fp);
        modification = read_u32_be(fp);
        timescale = read_u32_be(fp);
        duration = read_u32_be(fp);
    }
    g_mvhd_timescale = timescale;
    print_mac_time(creation, "Created", depth);
    print_mac_time(modification, "Modified", depth);
    print_indent(depth);
    if (timescale > 0 && duration > 0) {
        printf("[mvhd Meta] Timescale: %u | Duration: %llu units (%.2f sec)\n",
               timescale, (unsigned long long)duration,
               (double)duration / (double)timescale);
    } else if (timescale > 0) {
        printf("[mvhd Meta] Timescale: %u | Duration: 0 (fragmented file? "
               "see mvex/mehd)\n", timescale);
    } else {
        printf("[mvhd Meta] Timescale: %u | Duration: %llu units\n",
               timescale, (unsigned long long)duration);
    }
}

// mehd: movie extends header — total fragment duration for fragmented MP4s
// (mvhd/mdhd durations are 0 there; divide by the mvhd timescale).
static void decode_mehd(FILE *fp, uint64_t payload, int depth) {
    if (payload < 4) return;
    int version = fgetc(fp);
    (void)read_u24_be(fp); // flags
    uint64_t frag_duration = 0;
    if (version == 1) {
        if (payload < 16) return;
        frag_duration = read_u64_be(fp);
    } else {
        if (payload < 8) return;
        frag_duration = read_u32_be(fp);
    }
    print_indent(depth);
    if (g_mvhd_timescale > 0) {
        printf("[mehd Meta] Fragment Duration: %llu units (%.2f sec)\n",
               (unsigned long long)frag_duration,
               (double)frag_duration / (double)g_mvhd_timescale);
    } else {
        printf("[mehd Meta] Fragment Duration: %llu units\n",
               (unsigned long long)frag_duration);
    }
}

static void decode_tkhd(FILE *fp, uint64_t payload, int depth) {
    if (payload < 4) return;
    int version = fgetc(fp);
    uint32_t flags = read_u24_be(fp);
    // v1 header: creation(8) modification(8) id(4) reserved(4) duration(8) = 32
    // v0 header: creation(4) modification(4) id(4) reserved(4) duration(4) = 20
    // then: reserved(8) layer(2) alt_group(2) volume(2) reserved(2) matrix(36) = 52
    // then: width(4) height(4) = 8
    uint64_t need = (version == 1) ? 96u : 84u;
    if (payload < need) return;
    uint32_t track_id = 0;
    uint64_t duration = 0;
    if (version == 1) {
        (void)read_u64_be(fp);
        (void)read_u64_be(fp);
        track_id = read_u32_be(fp);
        (void)read_u32_be(fp);
        duration = read_u64_be(fp);
    } else {
        (void)read_u32_be(fp);
        (void)read_u32_be(fp);
        track_id = read_u32_be(fp);
        (void)read_u32_be(fp);
        duration = read_u32_be(fp);
    }
    if (fseek(fp, 52L, SEEK_CUR) != 0) return;
    uint32_t w_fixed = read_u32_be(fp); // 16.16 fixed point
    uint32_t h_fixed = read_u32_be(fp);
    print_indent(depth);
    printf("[tkhd Meta] Track ID: %u | %s | Duration: %llu units | "
           "Dimensions: %.2f x %.2f\n",
           track_id, (flags & 0x1u) ? "enabled" : "disabled",
           (unsigned long long)duration,
           (double)w_fixed / 65536.0, (double)h_fixed / 65536.0);
}

static void decode_mdhd(FILE *fp, uint64_t payload, int depth) {
    if (payload < 4) return;
    int version = fgetc(fp);
    (void)read_u24_be(fp); // flags
    uint32_t timescale = 0;
    uint64_t duration = 0;
    if (version == 1) {
        if (payload < 36) return;
        (void)read_u64_be(fp); // creation
        (void)read_u64_be(fp); // modification
        timescale = read_u32_be(fp);
        duration = read_u64_be(fp);
    } else {
        if (payload < 24) return;
        (void)read_u32_be(fp); // creation
        (void)read_u32_be(fp); // modification
        timescale = read_u32_be(fp);
        duration = read_u32_be(fp);
    }
    // Language is 2 bytes tucked right after duration; read raw bytes.
    uint8_t lang_buf[2];
    if (fread(lang_buf, 1, 2, fp) != 2) return;
    uint16_t lang = (uint16_t)(((uint16_t)lang_buf[0] << 8) | (uint16_t)lang_buf[1]);
    print_iso639_lang(lang, depth);
    print_indent(depth);
    if (timescale > 0) {
        printf("[mdhd Meta] Timescale: %u | Duration: %llu units (%.2f sec)\n",
               timescale, (unsigned long long)duration,
               (double)duration / (double)timescale);
    } else {
        printf("[mdhd Meta] Timescale: %u | Duration: %llu units\n",
               timescale, (unsigned long long)duration);
    }
}

static void decode_hdlr(FILE *fp, uint64_t payload, int depth) {
    // version/flags(4) pre_defined(4) handler(4) reserved(12) name(rest)
    if (payload < 24) return;
    (void)fgetc(fp);
    (void)read_u24_be(fp);
    (void)read_u32_be(fp); // pre_defined
    char handler[5] = {0};
    if (fread(handler, 1, 4, fp) != 4) return;
    if (fseek(fp, 12L, SEEK_CUR) != 0) return; // reserved
    uint64_t name_len = payload - 24;
    uint64_t show = name_len > 128 ? 128 : name_len;
    char *name = (char *)malloc((size_t)show + 1);
    if (name == NULL) return;
    size_t got = fread(name, 1, (size_t)show, fp);
    name[got] = '\0';
    const char *kind = "other";
    if (strcmp(handler, "vide") == 0) kind = "video";
    else if (strcmp(handler, "soun") == 0) kind = "audio";
    else if (strcmp(handler, "hint") == 0) kind = "hint";
    else if (strcmp(handler, "meta") == 0) kind = "metadata";
    print_indent(depth);
    printf("[hdlr Meta] Handler: %.4s (%s) | Name: %s\n", handler, kind, name);
    free(name);
}

static int is_video_entry(const char *t) {
    static const char *v[] = {
        "avc1", "avc2", "avc3", "avc4", "hev1", "hev2", "hvc1", "hvc2",
        "mp4v", "av01", "vp09", "encv", NULL
    };
    for (int i = 0; v[i] != NULL; i++) {
        if (strcmp(t, v[i]) == 0) return 1;
    }
    return 0;
}

static int is_audio_entry(const char *t) {
    static const char *a[] = {
        "mp4a", "ac-3", "ec-3", "alac", "sowt", "raw ", "twos",
        "flac", "Opus", "enca", NULL
    };
    for (int i = 0; a[i] != NULL; i++) {
        if (strcmp(t, a[i]) == 0) return 1;
    }
    return 0;
}

static void decode_stsd(FILE *fp, uint64_t payload, long box_start, int depth) {
    if (payload < 8) return;
    (void)fgetc(fp);
    (void)read_u24_be(fp); // version + flags
    uint32_t entries = read_u32_be(fp);
    print_indent(depth);
    printf("[stsd Meta] Sample Entries: %u\n", entries);
    uint32_t show = entries > 64 ? 64 : entries;
    for (uint32_t i = 0; i < show; i++) {
        long entry_start = ftell(fp);
        if (entry_start < 0) break;
        // Stop if fewer than 8 bytes remain in this box.
        long box_end = box_start + 8 + (long)payload;
        if ((uint64_t)(box_end - entry_start) < 8) break;
        uint32_t entry_size = read_u32_be(fp);
        char entry_type[5] = {0};
        if (fread(entry_type, 1, 4, fp) != 4) break;
        if (entry_size < 8) break;
        long entry_end = entry_start + (long)entry_size;
        if (entry_end > box_end) break;
        print_indent(depth);
        printf("[stsd Entry %u] [", i + 1);
        print_fourcc(entry_type);
        printf("] Size: %u bytes", entry_size);
        if (is_video_entry(entry_type)) {
            // reserved(6) data_ref(2) pre_defined(16) width(2) height(2) ...
            if ((uint64_t)(entry_end - entry_start) >= 8 + 28) {
                if (fseek(fp, entry_start + 8 + 24, SEEK_SET) == 0) {
                    uint16_t w = read_u16_be(fp);
                    uint16_t h = read_u16_be(fp);
                    printf(" | Video: %u x %u", w, h);
                }
            }
        } else if (is_audio_entry(entry_type)) {
            // reserved(6) data_ref(2) reserved(8) channels(2) size(2) ... rate(4 @ +24)
            if ((uint64_t)(entry_end - entry_start) >= 8 + 28) {
                if (fseek(fp, entry_start + 8 + 16, SEEK_SET) == 0) {
                    uint16_t ch = read_u16_be(fp);
                    (void)read_u16_be(fp); // sample size
                    (void)read_u16_be(fp); // pre_defined
                    (void)read_u16_be(fp); // reserved
                    uint32_t rate_fixed = read_u32_be(fp); // 16.16
                    printf(" | Audio: %u ch | %.0f Hz", ch,
                           (double)rate_fixed / 65536.0);
                }
            }
        }
        putchar('\n');
        if (fseek(fp, entry_end, SEEK_SET) != 0) break;
    }
}

// ilst/data: type_indicator 1 = UTF-8 text (title, artist, GPS (c)xyz, ...).
// Layout after the box header is version/flags(4) + indicator(4), then the
// payload — but spec-strict writers insert a locale(4) field first, while
// common writers (e.g. Lavf) omit it. Both are accepted: the 12-byte
// (spec) split wins when it looks like text, else the 8-byte split.
// Bytes >= 0x80 pass through so UTF-8 text stays readable; other controls -> '.'.
static int buf_is_text(const uint8_t *buf, uint64_t len) {
    uint64_t check = len > 160 ? 160 : len;
    if (check == 0) return 0;
    for (uint64_t i = 0; i < check; i++) {
        unsigned char uc = buf[i];
        if (uc == '\0') return i > 0; // NUL-terminated string (non-empty)
        if (!(uc >= 0x20 || uc == '\t' || uc == '\n' || uc == '\r' || uc >= 0x80))
            return 0;
    }
    return 1;
}

static void print_text_bytes(const uint8_t *buf, uint64_t len) {
    uint64_t show = len > 160 ? 160 : len;
    putchar('"');
    for (uint64_t i = 0; i < show; i++) {
        unsigned char uc = buf[i];
        if (uc == '\0') break;
        putchar((uc >= 0x20 || uc >= 0x80) ? (char)uc : '.');
    }
    if (len > 160) printf("...");
    putchar('"');
}

static void decode_data(FILE *fp, uint64_t payload, int depth) {
    if (payload < 8) return;
    uint64_t cap = payload > 4096 ? 4096 : payload;
    uint8_t *buf = (uint8_t *)malloc((size_t)cap);
    if (buf == NULL) return;
    size_t got = fread(buf, 1, (size_t)cap, fp);
    uint32_t indicator = 0;
    if (got >= 8) {
        indicator = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
                    ((uint32_t)buf[6] << 8) | (uint32_t)buf[7];
    }
    print_indent(depth);
    // Indicator 0 (locale-less writers like Lavf) skips 8; spec boxes
    // with a real type indicator skip 12 (locale field). Fall back to
    // whichever split reads as text.
    int t8 = (got > 8) && buf_is_text(buf + 8, (uint64_t)got - 8);
    int t12 = (got > 12) && buf_is_text(buf + 12, (uint64_t)got - 12);
    long off = -1;
    if (indicator == 0) {
        off = t8 ? 8L : (t12 ? 12L : -1L);
    } else {
        off = t12 ? 12L : (t8 ? 8L : -1L);
    }
    if (off < 0) {
        printf("[data Meta] type=%u, %llu bytes binary: ", indicator,
               (unsigned long long)payload);
        uint64_t dump = got > 16 ? 16 : got;
        for (uint64_t i = 0; i < dump; i++) printf("%02X ", buf[i]);
        printf("\n");
        free(buf);
        return;
    }
    printf("[data Meta] text: ");
    print_text_bytes(buf + (uint64_t)off, (uint64_t)got - (uint64_t)off);
    if (payload > (uint64_t)got) printf("... (%llu bytes total)", (unsigned long long)payload);
    printf("\n");
    free(buf);
}

static void decode_mp4_leaf(FILE *fp, const char *type, uint64_t payload,
                            long box_start, int depth) {
    if (strcmp(type, "ftyp") == 0) {
        decode_ftyp(fp, payload, depth + 1);
    } else if (strcmp(type, "mvhd") == 0) {
        decode_mvhd(fp, payload, depth + 1);
    } else if (strcmp(type, "mehd") == 0) {
        decode_mehd(fp, payload, depth + 1);
    } else if (strcmp(type, "tkhd") == 0) {
        decode_tkhd(fp, payload, depth + 1);
    } else if (strcmp(type, "mdhd") == 0) {
        decode_mdhd(fp, payload, depth + 1);
    } else if (strcmp(type, "hdlr") == 0) {
        decode_hdlr(fp, payload, depth + 1);
    } else if (strcmp(type, "stsd") == 0) {
        decode_stsd(fp, payload, box_start, depth + 1);
    } else if (strcmp(type, "data") == 0) {
        decode_data(fp, payload, depth + 1);
    } else if (strcmp(type, "mean") == 0 || strcmp(type, "name") == 0) {
        // Reverse-DNS tag domain/key: FullBox prefix + UTF-8 string.
        if (payload > 4) {
            uint64_t show = payload - 4 > 128 ? 128 : payload - 4;
            (void)fgetc(fp);
            (void)read_u24_be(fp);
            print_indent(depth + 1);
            printf("[%s Meta] \"", type);
            for (uint64_t i = 0; i < show; i++) {
                int c = fgetc(fp);
                if (c == EOF || c == '\0') break;
                unsigned char uc = (unsigned char)c;
                putchar((uc >= 0x20 || uc >= 0x80) ? (char)uc : '.');
            }
            printf("\"\n");
        }
    }
}

// in_ilst: ilst children are tag boxes (e.g. (c)nam) wrapping data boxes,
// so unknown types there descend instead of being skipped.
static void parse_mp4_boxes(FILE *fp, long end, int depth, int in_ilst) {
    while (ftell(fp) < end) {
        long box_start = ftell(fp);
        if (box_start < 0) break;
        uint32_t length = read_u32_be(fp);
        char type[5] = {0};
        if (fread(type, 1, 4, fp) != 4) break;

        uint64_t box_len = length;
        long header = 8;
        if (length == 1) {
            box_len = read_u64_be(fp);
            header = 16;
        } else if (length == 0) {
            box_len = (uint64_t)(end - box_start);
        }
        if (box_len < (uint64_t)header || box_start > end - (long)header ||
            box_len > (uint64_t)(end - box_start)) {
            print_indent(depth);
            printf("-> [");
            print_fourcc(type);
            printf("] corrupt length %llu, stopping\n",
                   (unsigned long long)box_len);
            break;
        }
        long box_end = box_start + (long)box_len;
        uint64_t payload = box_len - (uint64_t)header;

        print_indent(depth);
        printf("-> Found Box: [");
        print_fourcc(type);
        printf("] | Size: %llu bytes\n", (unsigned long long)box_len);

        if (strcmp(type, "uuid") == 0) {
            // 16-byte user type follows the header; content opaque.
        } else if (depth < MP4_MAX_DEPTH && is_container_box(type)) {
            long content = box_start + header;
            if (strcmp(type, "meta") == 0) content += 4; // FullBox prefix
            if (content < box_end) {
                fseek(fp, content, SEEK_SET);
                parse_mp4_boxes(fp, box_end, depth + 1,
                                 strcmp(type, "ilst") == 0);
            }
        } else if (in_ilst && payload >= 8 && strcmp(type, "data") != 0 &&
                   strcmp(type, "mean") != 0 && strcmp(type, "name") != 0) {
            // Tag box: descend to find its data child.
            fseek(fp, box_start + header, SEEK_SET);
            parse_mp4_boxes(fp, box_end, depth + 1, 1);
        } else {
            fseek(fp, box_start + header, SEEK_SET);
            decode_mp4_leaf(fp, type, payload, box_start, depth);
        }
        fseek(fp, box_end, SEEK_SET);
    }
}

void parse_mp4(FILE *fp) {
    fseek(fp, 0, SEEK_END);
    long file_len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    printf("\n[Format: MP4 Video]\n");
    printf("File Size:     %ld bytes\n", file_len);
    printf("Scanning Boxes...\n");

    g_mvhd_timescale = 0;
    parse_mp4_boxes(fp, file_len, 0, 0);
}

// AVI: MainAVIHeader + per-stream headers. strf layout depends on the
// stream type set by the preceding strh, tracked in AviCtx.
typedef struct {
    char stream_type[5]; // "vids", "auds", ... ("" until first strh)
} AviCtx;

static const char *audio_tag_name(uint16_t tag) {
    switch (tag) {
        case 0x0001: return "PCM";
        case 0x0003: return "IEEE float";
        case 0x0006: return "ALAW";
        case 0x0007: return "MULAW";
        case 0x0011: return "IMA ADPCM";
        case 0x0055: return "MP3";
        case 0x2000: return "AC3";
        case 0xFFFE: return "WAVE_FORMAT_EXTENSIBLE";
        default: return "unknown";
    }
}

// avih: MainAVIHeader (56 bytes)
static void decode_avih(FILE *fp, uint32_t size, int depth) {
    if (size < 40) return;
    uint32_t micro_sec = read_u32_le(fp);
    (void)read_u32_le(fp); // max bytes/sec
    (void)read_u32_le(fp); // padding
    (void)read_u32_le(fp); // flags
    uint32_t total_frames = read_u32_le(fp);
    (void)read_u32_le(fp); // initial frames
    uint32_t streams = read_u32_le(fp);
    (void)read_u32_le(fp); // suggested buffer size
    uint32_t width = read_u32_le(fp);
    uint32_t height = read_u32_le(fp);
    print_indent(depth);
    if (micro_sec > 0) {
        double fps = 1000000.0 / (double)micro_sec;
        printf("[avih Meta] %u x %u | %.2f fps | %u frames (%.2f sec) | %u streams\n",
               width, height, fps, total_frames,
               (double)total_frames / fps, streams);
    } else {
        printf("[avih Meta] %u x %u | fps N/A | %u frames | %u streams\n",
               width, height, total_frames, streams);
    }
}

// strh: AVIStreamHeader (56 bytes); records the stream type for strf.
static void decode_strh(FILE *fp, uint32_t size, int depth, AviCtx *ctx) {
    if (size < 44) return;
    char fcc_type[5] = {0}, handler[5] = {0};
    if (fread(fcc_type, 1, 4, fp) != 4) return;
    if (fread(handler, 1, 4, fp) != 4) return;
    memcpy(ctx->stream_type, fcc_type, 5);
    (void)read_u32_le(fp); // flags
    (void)read_i16_le(fp); // priority
    (void)read_i16_le(fp); // language
    (void)read_u32_le(fp); // initial frames
    uint32_t scale = read_u32_le(fp);
    uint32_t rate = read_u32_le(fp);
    (void)read_u32_le(fp); // start
    uint32_t length = read_u32_le(fp);
    print_indent(depth);
    printf("[strh Meta] Stream: %.4s | Codec/Handler: ", fcc_type);
    print_fourcc(handler);
    if (scale > 0) {
        printf(" | %.2f fps | Length: %u units (%.2f sec)\n",
               (double)rate / (double)scale, length,
               (double)length * (double)scale / (double)rate);
    } else {
        printf(" | rate %u/scale %u | Length: %u units\n", rate, scale, length);
    }
}

// strf: BITMAPINFOHEADER (video) or WAVEFORMATEX (audio).
static void decode_strf(FILE *fp, uint32_t size, int depth, const AviCtx *ctx) {
    print_indent(depth);
    if (strcmp(ctx->stream_type, "vids") == 0) {
        if (size < 40) {
            printf("[strf Meta] video: truncated (%u bytes)\n", size);
            return;
        }
        (void)read_u32_le(fp); // biSize
        int32_t w = read_i32_le(fp);
        int32_t h = read_i32_le(fp);
        (void)read_i16_le(fp); // planes
        int16_t bits = read_i16_le(fp);
        char comp[5] = {0};
        if (fread(comp, 1, 4, fp) != 4) return;
        printf("[strf Meta] Video: %d x %d | %d-bit | Compression: ", w, h, bits);
        print_fourcc(comp);
        putchar('\n');
    } else if (strcmp(ctx->stream_type, "auds") == 0) {
        if (size < 16) {
            printf("[strf Meta] audio: truncated (%u bytes)\n", size);
            return;
        }
        uint16_t tag = read_u16_le(fp);
        uint16_t ch = read_u16_le(fp);
        uint32_t rate = read_u32_le(fp);
        printf("[strf Meta] Audio: %s (tag 0x%04X) | %u ch | %u Hz\n",
               audio_tag_name(tag), tag, ch, rate);
    } else {
        printf("[strf Meta] (stream type '%s' unknown yet, %u bytes)\n",
               ctx->stream_type[0] ? ctx->stream_type : "none", size);
    }
}

// Only header lists are walked; movi/rec/idx1 media bodies are skipped
// (they can be gigabytes — never iterate their contents).
static int is_header_list(const char *list_type) {
    return strcmp(list_type, "hdrl") == 0 ||
           strcmp(list_type, "strl") == 0 ||
           strcmp(list_type, "INFO") == 0;
}

static void parse_avi_chunks(FILE *fp, long end, int depth, AviCtx *ctx) {
    while (1) {
        long chunk_start = ftell(fp);
        if (chunk_start < 0 || chunk_start + 8 > end) break;
        char id[5] = {0};
        if (fread(id, 1, 4, fp) != 4) break;
        uint32_t size = read_u32_le(fp);
        if ((uint64_t)size > (uint64_t)(end - chunk_start - 8)) break; // corrupt
        long data_start = chunk_start + 8;
        long data_end = data_start + (long)size;

        if (memcmp(id, "LIST", 4) == 0) {
            if (size < 4) break;
            char list_type[5] = {0};
            if (fread(list_type, 1, 4, fp) != 4) break;
            print_indent(depth);
            printf("-> Found LIST: [%.4s] | Size: %u bytes\n", list_type, size);
            if (is_header_list(list_type)) {
                AviCtx child = *ctx;
                if (strcmp(list_type, "strl") == 0) child.stream_type[0] = '\0';
                parse_avi_chunks(fp, data_start + (long)size - 4, depth + 1, &child);
                if (strcmp(list_type, "strl") == 0) {
                    // Carry the stream type up so later code could use it.
                    memcpy(ctx->stream_type, child.stream_type, 5);
                }
            }
        } else {
            if (strcmp(id, "avih") == 0 || strcmp(id, "strh") == 0 ||
                strcmp(id, "strf") == 0) {
                print_indent(depth);
                printf("-> Found Chunk: [%.4s] | Size: %u bytes\n", id, size);
                if (strcmp(id, "avih") == 0) {
                    decode_avih(fp, size, depth + 1);
                } else if (strcmp(id, "strh") == 0) {
                    decode_strh(fp, size, depth + 1, ctx);
                } else {
                    decode_strf(fp, size, depth + 1, ctx);
                }
            } else {
                // Data/tag chunks (movi frames live in LIST movi, skipped above).
                print_indent(depth);
                printf("-> Found Chunk: [");
                print_fourcc(id);
                printf("] | Size: %u bytes\n", size);
            }
        }
        // Chunks are word-aligned: odd sizes carry one pad byte.
        long next = data_end + ((size % 2 != 0) ? 1 : 0);
        if (next > end) break;
        fseek(fp, next, SEEK_SET);
    }
}

void parse_avi(FILE *fp) {
    fseek(fp, 0, SEEK_END);
    long file_len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char riff[5] = {0}, avi[5] = {0};
    if (fread(riff, 1, 4, fp) != 4) return;
    uint32_t riff_size = read_u32_le(fp);
    if (fread(avi, 1, 4, fp) != 4) return;

    printf("\n[Format: AVI Video]\n");
    printf("File Size:     %u bytes\n", riff_size + 8);
    printf("Scanning Chunks...\n");

    AviCtx ctx;
    ctx.stream_type[0] = '\0';
    parse_avi_chunks(fp, file_len, 0, &ctx);
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
