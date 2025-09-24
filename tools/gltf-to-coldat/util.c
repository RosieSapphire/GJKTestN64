#ifndef UTIL_C
#define UTIL_C

static char *file_get_buffer(const char *path, size_t *sz)
{
        FILE *f;
        char *buf;

        f = fopen(path, "rb");
        if (!f) {
                printf("Failed to load file from '%s'\n", path);
                return NULL;
        }

        fseek(f, 0, SEEK_END);
        *sz = ftell(f);
        rewind(f);
        buf = malloc(*sz + 1);
        fread(buf, 1, *sz, f);
        buf[*sz] = 0;
        fclose(f);

        return buf;
}

static void gltf_get_paths_from_name(char **gltf_path, char **bin_path,
                                     char **cm_path, const char *indir,
                                     const char *outdir, const char *obj_name)
{
#define GLTF_EXT_LEN 5
#define BIN_EXT_LEN 4
#define CM_EXT_LEN 3

        int obj_name_len, indir_len, outdir_len;
        size_t gltf_path_sz, bin_path_sz, cm_path_sz;

        obj_name_len = strlen(obj_name);

        indir_len = strlen(indir);
        while (indir[indir_len - 1] == '/')
                --indir_len;

        outdir_len = strlen(outdir);
        while (outdir[outdir_len - 1] == '/')
                --outdir_len;


        gltf_path_sz = indir_len + 1 + obj_name_len + GLTF_EXT_LEN + 1;
        bin_path_sz = indir_len + 1 + obj_name_len + BIN_EXT_LEN + 1;
        cm_path_sz = outdir_len + 1 + obj_name_len + CM_EXT_LEN + 1;
        *gltf_path = malloc(gltf_path_sz);
        *bin_path = malloc(bin_path_sz);
        *cm_path = malloc(cm_path_sz);

        snprintf(*gltf_path, gltf_path_sz, "%.*s/%s.gltf",
                 indir_len, indir, obj_name);
        snprintf(*bin_path, bin_path_sz, "%.*s/%s.bin",
                 indir_len, indir, obj_name);
        snprintf(*cm_path, cm_path_sz, "%.*s/%s.cm",
                 outdir_len, outdir, obj_name);

#undef CM_EXT_LEN
#undef BIN_EXT_LEN
#undef GLTF_EXT_LEN
}

#endif /* UTIL_C */
