#include "mjpegw.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"

// ffmpeg -v debug -i test.avi -f null /dev/null

#define MJPEGW_INITIAL_IDX_CAP (16)

#pragma pack(push, 1)

typedef struct
{
    char     riff[4];     // "RIFF"
    uint32_t size;        // file_size - 8  (PATCH LATER)
    char     avi[4];      // "AVI "
} riff_header;


typedef struct
{
    char     list[4];     // "LIST"
    uint32_t size;        // size of this LIST - 8  (PATCH LATER)
    char     type[4];     // "hdrl"
} list_header;

typedef struct
{
    char     id[4];       // "avih"
    uint32_t size;        // 56
    uint32_t microsec_per_frame;
    uint32_t max_bytes_per_sec;
    uint32_t padding_granularity;
    uint32_t flags;
    uint32_t total_frames;
    uint32_t initial_frames;
    uint32_t streams;
    uint32_t suggested_buffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t reserved[4];
} avih_chunk;

typedef struct
{
    char     list[4];     // "LIST"
    uint32_t size;        // size of this LIST - 8 (PATCH LATER)
    char     type[4];     // "strl"
} avi_stream_list;

typedef struct
{
    char     id[4];       // "strh"
    uint32_t size;        // 56
    char     type[4];     // "vids"
    char     handler[4];  // "MJPG"
    uint32_t flags;
    uint32_t priority;
    uint16_t language;
    uint16_t initial_frames;
    uint32_t scale;
    uint32_t rate;
    uint32_t start;
    uint32_t length;
    uint32_t suggested_buffer_size;
    uint32_t quality;
    uint32_t sample_size;
    struct
    {
        int16_t left;
        int16_t top;
        int16_t right;
        int16_t bottom;
    } frame;
} strh_chunk;

typedef struct
{
    char     id[4];       // "strf"
    uint32_t size;        // 40
    uint32_t bi_size;     // 40
    int32_t  bi_width;
    int32_t  bi_height;
    uint16_t bi_planes;
    uint16_t bi_bit_count;
    uint32_t bi_compression; // "MJPG" == 0x47504A4D
    uint32_t bi_image_size;  // 0 usually fine
    int32_t  bi_x_ppm;
    int32_t  bi_y_ppm;
    uint32_t bi_clr_used;
    uint32_t bi_clr_important;
} strf_chunk;

typedef struct
{
    char     list[4];     // "LIST"
    uint32_t size;        // PATCH LATER
    char     type[4];     // "movi"
} movi_header;

typedef struct
{
    char     id[4];       // "00dc"
    uint32_t size;        // jpeg_data_size
    // followed by jpeg_data[size]
} frame_chunk;

typedef struct
{
    char     id[4];       // "00dc"
    uint32_t flags;       // 0x10 = keyframe
    uint32_t offset;      // offset relative to start of 'movi' LIST DATA
    uint32_t size;        // jpeg_data_size
} idx1_entry;

typedef struct
{
    char     id[4];       // "idx1"
    uint32_t size;        // num_entries * 16
    // followed by avi_idx1_entry_t entries[num_frames]
} idx1_header;

#pragma pack(pop)


typedef struct mjpegw_context
{
    FILE* f;

    uint32_t width;
    uint32_t height;
    uint32_t fps;
    uint32_t frame_count;

    mjpegw_mem_interface mem;

    long riff_pos;
    long movi_pos;
    long frame_count_pos;
    long length_pos;

    // main avi structures
    riff_header riff;
    list_header hdrl;
    avih_chunk avih;
    avi_stream_list strl;
    strh_chunk strh;
    strf_chunk strf;
    movi_header movi;

    idx1_entry* idx;
    uint32_t idx_count;
    uint32_t idx_capacity;

    uint8_t* jpeg_data;
    uint32_t jpeg_size;
    uint32_t jpeg_capacity;
} mjpegw_context;


//-----------------------------------------------------------------------------------------------------------------------------
static inline void* malloc_wrapper(size_t size, void* user)
{
    (void)user; // unused
    return malloc(size);
}

//-----------------------------------------------------------------------------------------------------------------------------
static inline void* realloc_wrapper(void* old_ptr, size_t old_size, size_t new_size, void* user)
{
    (void)user;
    (void)old_size;
    return realloc(old_ptr, new_size);
}

//-----------------------------------------------------------------------------------------------------------------------------
static inline void free_wrapper(void* ptr, void* user)
{
    (void)user;
    free(ptr);
}

//-----------------------------------------------------------------------------------------------------------------------------
static inline mjpegw_mem_interface default_allocator(void) 
{
    return (mjpegw_mem_interface) 
    {
        .malloc_fn  = malloc_wrapper,
        .realloc_fn = realloc_wrapper,
        .free_fn    = free_wrapper,
        .user       = NULL
    };
}

//-----------------------------------------------------------------------------------------------------------------------------
mjpegw_context* mjpegw_open(const char *filename, uint32_t width, uint32_t height, uint32_t fps, mjpegw_mem_interface* mem)
{
    mjpegw_context* ctx = NULL;

    // use user allocator or default
    mjpegw_mem_interface allocator = mem ? *mem : default_allocator();

    ctx = allocator.malloc_fn(sizeof(mjpegw_context), allocator.user);
    if(!ctx)
        return NULL;

    *ctx = (mjpegw_context) {0};

    ctx->width  = width;
    ctx->height = height;
    ctx->fps = fps;
    ctx->frame_count = 0;
    ctx->mem = allocator;

    ctx->f = fopen(filename, "wb+");
    if(!ctx->f)
    {
        ctx->mem.free_fn(ctx, ctx->mem.user);
        return NULL;
    }

    memcpy(ctx->riff.riff, "RIFF", 4);
    ctx->riff.size = 0; // patch later
    memcpy(ctx->riff.avi, "AVI ", 4);
    ctx->riff_pos = ftell(ctx->f);
    fwrite(&ctx->riff, sizeof(ctx->riff), 1, ctx->f);

    memcpy(ctx->hdrl.list, "LIST", 4);
    ctx->hdrl.size = 188;
    memcpy(ctx->hdrl.type, "hdrl", 4);
    fwrite(&ctx->hdrl, sizeof(ctx->hdrl), 1, ctx->f);

    ctx->frame_count_pos = ftell(ctx->f) + 32;
    memcpy(ctx->avih.id, "avih", 4);
    ctx->avih.size = 56;
    ctx->avih.microsec_per_frame = 1000000 / fps;
    ctx->avih.max_bytes_per_sec = 0;
    ctx->avih.padding_granularity = 0;
    ctx->avih.flags = 0x10; // HAS_INDEX
    ctx->avih.total_frames = 0; // patch later
    ctx->avih.initial_frames = 0;
    ctx->avih.streams = 1;
    ctx->avih.suggested_buffer_size = ctx->width*ctx->height*3;
    ctx->avih.width = width;
    ctx->avih.height = height;
    fwrite(&ctx->avih, sizeof(ctx->avih), 1, ctx->f);

    memcpy(ctx->strl.list, "LIST", 4);
    ctx->strl.size = sizeof(strh_chunk) + sizeof(strf_chunk);
    assert(ctx->strl.size == 112);
    memcpy(ctx->strl.type, "strl", 4);
    fwrite(&ctx->strl, sizeof(ctx->strl), 1, ctx->f);

    ctx->length_pos = ftell(ctx->f) + 44;
    memcpy(ctx->strh.id, "strh", 4);
    ctx->strh.size = 56;
    memcpy(ctx->strh.type, "vids", 4);
    memcpy(ctx->strh.handler, "MJPG", 4);
    ctx->strh.flags = 0x10; // HAS_INDEX
    ctx->strh.priority = 0;
    ctx->strh.language = 0;
    ctx->strh.initial_frames = 0;
    ctx->strh.scale = 1;
    ctx->strh.rate = fps;
    ctx->strh.start = 0;
    ctx->strh.length = 0; // patch later
    ctx->strh.suggested_buffer_size = ctx->width*ctx->height*3;
    ctx->strh.quality = -1;
    ctx->strh.sample_size = 0;
    ctx->strh.frame.left = 0;
    ctx->strh.frame.top = 0;
    ctx->strh.frame.right = width;
    ctx->strh.frame.bottom = height;
    fwrite(&ctx->strh, sizeof(ctx->strh), 1, ctx->f);

    memcpy(ctx->strf.id, "strf", 4);
    ctx->strf.size = sizeof(ctx->strf);
    ctx->strf.bi_size = 40;
    ctx->strf.bi_width = width;
    ctx->strf.bi_height = height;
    ctx->strf.bi_planes = 1;
    ctx->strf.bi_bit_count = 24; // RGB24
    ctx->strf.bi_compression = 0x47504A4D; // 'MJPG'
    ctx->strf.bi_image_size = 0;
    ctx->strf.bi_x_ppm = 0;
    ctx->strf.bi_y_ppm = 0;
    ctx->strf.bi_clr_used = 0;
    ctx->strf.bi_clr_important = 0;
    fwrite(&ctx->strf, sizeof(ctx->strf), 1, ctx->f);

    memcpy(ctx->movi.list, "LIST", 4);
    ctx->movi.size = 0; // patch later
    memcpy(ctx->movi.type, "movi", 4);
    ctx->movi_pos = ftell(ctx->f);
    fwrite(&ctx->movi, sizeof(ctx->movi), 1, ctx->f);

    ctx->idx_capacity = MJPEGW_INITIAL_IDX_CAP;
    ctx->idx_count = 0;
    ctx->idx = ctx->mem.malloc_fn(sizeof(idx1_entry) * ctx->idx_capacity, ctx->mem.user);
    if (!ctx->idx)
    {
        fclose(ctx->f);
        ctx->mem.free_fn(ctx, ctx->mem.user);
        return NULL;
    }

    ctx->jpeg_capacity = width*height;
    ctx->jpeg_data = ctx->mem.malloc_fn(ctx->jpeg_capacity, ctx->mem.user);
    ctx->jpeg_size = 0;

    return ctx;
}

//-----------------------------------------------------------------------------------------------------------------------------
void jpeg_write_func(void* context, void* data, int size)
{
    mjpegw_context *ctx = (mjpegw_context*) context;

    if ((ctx->jpeg_size + size) >= ctx->jpeg_capacity)
    {
        ctx->jpeg_data = ctx->mem.realloc_fn(ctx->jpeg_data, ctx->jpeg_capacity, ctx->jpeg_size + size, ctx->mem.user);
        assert(ctx->jpeg_data);
        ctx->jpeg_capacity += size;
    }

    uint8_t* ptr = (uint8_t*) ctx->jpeg_data;
    ptr += ctx->jpeg_size;
    memcpy(ptr, data, size);

    ctx->jpeg_size += size;
}


//-----------------------------------------------------------------------------------------------------------------------------
void mjpegw_add_frame(mjpegw_context *ctx, const uint8_t* pixels, const int quality)
{
    ctx->jpeg_size = 0;

    tje_encode_with_func(jpeg_write_func, ctx, quality, ctx->width, ctx->height, 4, pixels);
    
    if(ctx->idx_count >= ctx->idx_capacity)
    {
        uint32_t new_capacity = ctx->idx_capacity ? ctx->idx_capacity * 2 : 16;
        idx1_entry* new_idx = ctx->mem.realloc_fn(ctx->idx, sizeof(idx1_entry) * ctx->idx_capacity,
                                                  sizeof(idx1_entry) * new_capacity, ctx->mem.user);

        assert(new_idx != NULL);
        ctx->idx = new_idx;
        ctx->idx_capacity = new_capacity;
    }

    long frame_pos = ftell(ctx->f);
    assert(frame_pos != -1L);

    uint32_t chunk_size = ctx->jpeg_size;
    if (ctx->jpeg_size & 1)
        chunk_size++;

    frame_chunk hdr = { .size = chunk_size };
    memcpy(hdr.id, "00dc", 4);
    fwrite(&hdr, sizeof(frame_chunk), 1, ctx->f);
    fwrite(ctx->jpeg_data, 1, ctx->jpeg_size, ctx->f);

    if (ctx->jpeg_size & 1)
    {
        uint8_t pad = 0;
        fwrite(&pad, 1, 1, ctx->f);
    }

    ctx->movi.size += sizeof(frame_chunk) + chunk_size;

    idx1_entry* entry = &ctx->idx[ctx->idx_count++];
    memcpy(entry->id, "00dc", 4);
    entry->flags = 0x10;
    entry->offset = (uint32_t)(frame_pos - ctx->movi_pos - 8); // relative to 'movi' data start
    entry->size = chunk_size;

    ctx->frame_count++;
}

//-----------------------------------------------------------------------------------------------------------------------------
void mjpegw_close(mjpegw_context *ctx)
{
    assert(ctx);

    // patch frame count
    fseek(ctx->f, ctx->frame_count_pos, SEEK_SET);
    fwrite(&ctx->frame_count, sizeof(uint32_t), 1, ctx->f);
    fseek(ctx->f, ctx->length_pos, SEEK_SET);
    fwrite(&ctx->frame_count, sizeof(uint32_t), 1, ctx->f);

    // write idx1 chunk
    fseek(ctx->f, 0, SEEK_END); 
    idx1_header idxh = {0};
    memcpy(idxh.id, "idx1", 4);
    idxh.size = ctx->idx_count * sizeof(idx1_entry);

    fwrite(&idxh, sizeof(idx1_header), 1, ctx->f);
    if (ctx->idx_count)
        fwrite(ctx->idx, sizeof(idx1_entry), ctx->idx_count, ctx->f);

    // patch movi LIST size
    long cur_pos = ftell(ctx->f);
    fseek(ctx->f, ctx->movi_pos + 4, SEEK_SET);
    uint32_t movi_size = (uint32_t)(cur_pos - (ctx->movi_pos + 8));
    fwrite(&movi_size, sizeof(uint32_t), 1, ctx->f);

    // patch RIFF size
    fseek(ctx->f, ctx->riff_pos + 4, SEEK_SET);
    uint32_t riff_size = (uint32_t)(cur_pos - (ctx->riff_pos + 8));
    fwrite(&riff_size, sizeof(uint32_t), 1, ctx->f);

    if (ctx->idx)
    {
        ctx->mem.free_fn(ctx->idx, ctx->mem.user);
        ctx->idx = NULL;
        ctx->idx_capacity = 0;
    }

    if (ctx->jpeg_data)
    {
        ctx->mem.free_fn(ctx->jpeg_data, ctx->mem.user);
        ctx->jpeg_data = NULL;
        ctx->jpeg_capacity = 0;
    }

    fclose(ctx->f);
    ctx->f = NULL;
}

