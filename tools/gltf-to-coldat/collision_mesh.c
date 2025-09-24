#ifndef COLLISION_MESH_C
#define COLLISION_MESH_C

#include <stdbool.h>

static void collision_mesh_from_gltf_prim(struct collision_mesh *cm,
                                          const cgltf_data *gltf,
                                          const void *bin,
                                          const cgltf_primitive *prim)
{
        cgltf_buffer_view *indi_bv, *pos_bv;
        uint16_t *indi_buf;
        float *pos_buf_raw, *pos_buf;
        int indi_cnt;

        /* Rip indices */
        indi_bv = prim->indices->buffer_view;
        if (!indi_bv) {
                printf("ERROR: Couldn't find indices buffer view in prim.\n");
                return;
        }

        indi_buf = malloc(indi_bv->size);
        memcpy(indi_buf, bin + indi_bv->offset, indi_bv->size);
        indi_cnt = indi_bv->size >> 1;

        /* Rip raw positions */
        pos_bv = NULL;
        for (size_t i = 0; i < prim->attributes_count; ++i) {
                cgltf_attribute_type at;

                at = prim->attributes[i].type;
                if (at == cgltf_attribute_type_position) {
                        pos_bv = gltf->buffer_views + i;
                        break;
                }
        }

        if (!pos_bv) {
                printf("ERROR: Couldn't find position buffer view in prim.\n");
                return;
        }

        pos_buf_raw = malloc(pos_bv->size);
        memcpy(pos_buf_raw, bin + pos_bv->offset, pos_bv->size);

        /* Unwrap/deindex vertices */
        pos_buf = malloc(indi_cnt * sizeof(float) * 3);
        for (int i = 0; i < indi_cnt; ++i) {
                float *src, *dst;

                src = pos_buf_raw + (indi_buf[i] * 3);
                dst = pos_buf + (i * 3);
                memcpy(dst, src, sizeof(float) * 3);
        }

        /* Populate collision mesh */
        cm->tri_cnt = indi_cnt / 3;
        cm->tris = malloc(sizeof(*cm->tris) * cm->tri_cnt);
        memcpy(cm->tris, pos_buf, sizeof(*cm->tris) * cm->tri_cnt);

        /* Exit */
        free(pos_buf);
        free(pos_buf_raw);
        free(indi_buf);
}

static void collision_mesh_free(struct collision_mesh *cm)
{
        cm->tri_cnt = 0;
        free(cm->tris);
        cm->tris = NULL;
}

static void collision_mesh_stitch(struct collision_mesh *dst,
                                  struct collision_mesh *src_arr,
                                  const int src_cnt)
{
        dst->tri_cnt = 0;
        dst->tris = NULL;

        for (int i = 0; i < src_cnt; ++i) {
                struct collision_mesh *src = src_arr + i;
                size_t sz;
                int off_cnt;

                off_cnt = dst->tri_cnt;
                dst->tri_cnt += src->tri_cnt;
                sz = sizeof(*dst->tris) * dst->tri_cnt;
                if (!dst->tris)
                        dst->tris = malloc(sz);
                else
                        dst->tris = realloc(dst->tris, sz);

                memcpy(dst->tris + off_cnt, src->tris,
                       src->tri_cnt * sizeof(*src->tris));
                collision_mesh_free(src);
        }
}

static void gltf_data_to_collision_mesh(struct collision_mesh *cm,
                                        const cgltf_data *gltf,
                                        const void *bin)
{
        cgltf_mesh *mesh_main;
        cgltf_primitive *prims;
        struct collision_mesh *cm_tmp;
        int cm_tmp_cnt;

        if (gltf->meshes_count < 1) {
                printf("NO MESHES FOUND IN FILE!\n");
                return;
        }

        if (gltf->meshes_count > 1) {
                printf("MORE THAN 1 MESH IN FILE! UNIMPLEMENTED!\n");
                return;
        }

        mesh_main = gltf->meshes + 0;
        if (!mesh_main) {
                printf("MAIN MESH POINTER IS INVALID!\n");
                return;
        }

        if (mesh_main->primitives_count < 1) {
                printf("NO PRIMITIVES IN MAIN MESH!\n");
                return;
        }

        cm_tmp_cnt = mesh_main->primitives_count;
        prims = mesh_main->primitives;
        if (!prims) {
                printf("ERROR: Prims pointer is still NULL somehow...\n");
                return;
        }

        cm_tmp = malloc(sizeof(*cm_tmp) * cm_tmp_cnt);
        for (int i = 0; i < cm_tmp_cnt; ++i)
                collision_mesh_from_gltf_prim(cm_tmp + i, gltf, bin,
                                              mesh_main->primitives + i);

        collision_mesh_stitch(cm, cm_tmp, cm_tmp_cnt);

        free(cm_tmp);
}

static void collision_mesh_printf(const struct collision_mesh *cm)
{
#ifndef DO_DEBUG_PRINT
        return;
#endif

        printf("Collision Mesh (%d Triangles):\n", cm->tri_cnt);
        for (size_t i = 0; i < cm->tri_cnt; ++i) {
                printf("\tTriangle %lu:\n", i);
                for (int j = 0; j < 3; ++j) {
                        struct point *p;

                        p = cm->tris[i].p + j;
                        printf("\t\t(%f, %f, %f)\n", p->v[0], p->v[1], p->v[2]);
                }

                printf("\n");
        }
}

#include "endian.c"

static bool collision_mesh_write_to_file(const struct collision_mesh *cm,
                                         const char *path)
{
        FILE *f;
        
        f = fopen(path, "wb");
        if (!f)
                return false;

        fwrite_ef32(&cm->tri_cnt, f);
        for (unsigned int i = 0; i < cm->tri_cnt; ++i)
                for (unsigned int j = 0; j < 3; ++j)
                        for (unsigned int k = 0; k < 3; ++k)
                                fwrite_ef32(cm->tris[i].p[j].v + k, f);

        fclose(f);

        return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
static bool collision_mesh_read_from_file(struct collision_mesh *cm,
                                          const char *path)
{
        FILE *f;

        if (!(f = fopen(path, "rb"))) {
                printf("Failed to open collision mesh file from '%s'\n", path);
                return false;
        }

        cm->tri_cnt = fread_ef32(f);
        cm->tris = malloc(sizeof(*cm->tris) * cm->tri_cnt);
        for (unsigned int i = 0; i < cm->tri_cnt; ++i) {
                for (unsigned int j = 0; j < 3; ++j) {
                        for (unsigned int k = 0; k < 3; ++k) {
                                uint32_t v;

                                v = fread_ef32(f);
                                cm->tris[i].p[j].v[k] = *((float *)(&v));
                        }
                }
        }

        fclose(f);

        return true;
}
#pragma GCC diagnostic pop

#endif /* COLLISION_MESH_C */
