#include <stdio.h>
#include <stdint.h>

static void fwrite_ef32(const void *ptr, FILE *fo)
{
        uint32_t val;

        val = *((uint32_t *)ptr);
        val = ((val & 0x000000FF) << 24) | ((val & 0x0000FF00) << 8) |
              ((val & 0x00FF0000) >> 8) | ((val & 0xFF000000) >> 24);

        (void)fwrite(&val, 4, 1, fo);
}

static uint32_t fread_ef32(FILE *fi)
{
        uint32_t val;

        (void)fread(&val, 4, 1, fi);
        val = ((val & 0x000000FF) << 24) | ((val & 0x0000FF00) << 8) |
              ((val & 0x00FF0000) >> 8) | ((val & 0xFF000000) >> 24);

        return val;
}
