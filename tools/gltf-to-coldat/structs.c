#ifndef STRUCTS_C
#define STRUCTS_C

#include <stdint.h>

struct point {
        float v[3];
};

struct triangle {
        struct point p[3];
};

struct collision_mesh {
        uint32_t tri_cnt;
        struct triangle *tris;
};

#endif /* STRUCTS_C */
