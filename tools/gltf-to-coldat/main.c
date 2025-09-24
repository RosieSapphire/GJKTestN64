/* #define DO_DEBUG_PRINT */

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "structs.c"
#include "collision_mesh.c"
#include "util.c"

enum {
        RET_GOOD,
        RET_FILE_NAME_NOT_SUPPLIED,
        RET_GLTF_LOAD_FAIL,
        RET_BIN_LOAD_FAIL,
        RET_CGLTF_FAIL,
        RET_COLMESH_FILE_READ_FAIL,
        RET_CODE_CNT
};

int main(const int argc, const char **argv)
{
        const char *obj_name = NULL, *out_path = NULL, *in_path = NULL;
        char *gltf_path = NULL, *bin_path = NULL,
             *gltf_buf = NULL, *bin_buf = NULL,
             *cm_path = NULL;
        size_t gltf_buf_sz = 0, bin_buf_sz = 0;
        cgltf_options gltf_opt = { 0 };
        cgltf_data *gltf_data = NULL;
        cgltf_result gltf_res = { 0 };
        struct collision_mesh cm;

        if (argc < 3) {
                printf("usage: %s in_dir out_dir obj_name\n", argv[0]);
                return RET_FILE_NAME_NOT_SUPPLIED;
        }

        /* Aquire file data */
        in_path = argv[1];
        out_path = argv[2];
        obj_name = argv[3];
        gltf_get_paths_from_name(&gltf_path, &bin_path,
                                 &cm_path, in_path, out_path, obj_name);
        if (!(gltf_buf = file_get_buffer(gltf_path, &gltf_buf_sz)))
                return RET_GLTF_LOAD_FAIL;

        if (!(bin_buf = file_get_buffer(bin_path, &bin_buf_sz)))
                return RET_BIN_LOAD_FAIL;

        /* Rip that shit! */
        gltf_res = cgltf_parse_file(&gltf_opt, gltf_path, &gltf_data);
        if (gltf_res != cgltf_result_success) {
                printf("CGLTF failed to parse file '%s'.\n", gltf_path);
                return RET_CGLTF_FAIL;
        }

        cm.tri_cnt = 0;
        cm.tris = NULL;

        gltf_data_to_collision_mesh(&cm, gltf_data, bin_buf);
        if (!collision_mesh_write_to_file(&cm, cm_path)) {
                printf("Failed to write to collision file '%s'.\n", cm_path);
                return RET_COLMESH_FILE_READ_FAIL;
        }

        collision_mesh_free(&cm);
        if (!collision_mesh_read_from_file(&cm, cm_path)) {
                printf("Failed to read from collision file '%s'.\n", cm_path);
                return RET_COLMESH_FILE_READ_FAIL;
        }

        collision_mesh_printf(&cm);

        /* Nuke it all */
        collision_mesh_free(&cm);
        cgltf_free(gltf_data);
        free(cm_path);
        free(bin_buf);
        free(gltf_buf);
        free(bin_path);
        free(gltf_path);

        return RET_GOOD;
}
