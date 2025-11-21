#include "../mjpegw.h"
#include <math.h>
#include <stdlib.h>


void fill_rgba_waves(uint8_t  *rgba, uint32_t width, uint32_t height, uint32_t frame_index)
{
    float t = (float)frame_index * 0.05f;   // animation phase
    for (uint32_t y = 0; y < height; y++)
    {
        for (uint32_t x = 0; x < width; x++)
        {
            uint8_t *p = rgba + (uint32_t)((y * width + x) * 4u);

            float u = (float)x / (float)width;
            float v = (float)y / (float)height;

            float w1 = sinf((u * 6.28318f * 3.0f) + t * 0.8f);
            float w2 = sinf((v * 6.28318f * 2.0f) - t * 1.1f);
            float w3 = sinf((u * 6.28318f * 1.2f) + (v * 6.28318f * 1.8f) + t * 0.6f);

            uint8_t r = (uint8_t)((w1 * 0.5f + 0.5f) * 255.0f);
            uint8_t g = (uint8_t)((w2 * 0.5f + 0.5f) * 255.0f);
            uint8_t b = (uint8_t)((w3 * 0.5f + 0.5f) * 255.0f);

            p[0] = r;
            p[1] = g;
            p[2] = b;
            p[3] = 255u;
        }
    }
}

#define WIDTH (800U)
#define HEIGHT (450U)

int main(void)
{
    struct mjpegw_context* avi = mjpegw_open("test.avi", WIDTH, HEIGHT, 60, NULL);
    if (avi == NULL)
        return -1;

    uint8_t* rgba = malloc(WIDTH*HEIGHT*4);

    for(uint32_t i=0; i<500; ++i)
    {
        fill_rgba_waves(rgba, WIDTH, HEIGHT, i);
        mjpegw_add_frame(avi, rgba, 2);
    }

    mjpegw_close(avi);

    free(rgba);

    return 0;
}