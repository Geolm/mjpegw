#ifndef __MJPEGW_H__
#define __MJPEGW_H__

#include <stddef.h>
#include <stdint.h>

typedef struct mjpegw_mem_interface
{
    void*  (*malloc_fn)(size_t size, void* user);
    void*  (*realloc_fn)(void* old_ptr, size_t old_size, size_t new_size, void* user);
    void   (*free_fn)(void* ptr, void* user);
    void*   user;
} mjpegw_mem_interface;

struct mjpegw_context;

#ifdef __cplusplus
extern "C" {
#endif

struct mjpegw_context* mjpegw_open(const char *filename, uint32_t width, uint32_t height, uint32_t fps, mjpegw_mem_interface* mem);
void mjpegw_add_frame(struct mjpegw_context *ctx, const uint8_t* pixels, const int quality);
void mjpegw_close(struct mjpegw_context *ctx);


#ifdef __cplusplus
}
#endif


#endif