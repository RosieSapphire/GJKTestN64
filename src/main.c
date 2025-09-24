#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3ddebug.h>
#include <t3d/tpx.h>

#define VIEWPORT_NEAR (.25f * MODEL_SCALE)
#define VIEWPORT_FAR (10.f * MODEL_SCALE)
#define VIEWPORT_FOV_DEG 75.f

#define OBSERVER_TURN_SPEED_SLOW 3.f
#define OBSERVER_TURN_SPEED_FAST 16.f
#define OBSERVER_PITCH_LIMIT 85.f
#define OBSERVER_NOCLIP_SPEED_SLOW 4.2f
#define OBSERVER_NOCLIP_SPEED_FAST 12.2f

#define OBJECT_MOVE_SPEED 16.f

#define JOYSTICK_MAG_MAX 60
#define JOYSTICK_MAG_MIN 6

#define LERP(A, B, T) ((A) + ((B) - (A)) * (T))

enum mode : int8_t {
        MODE_INVALID = -1,
        MODE_OBSERVER,
        MODE_MOVE_OBJ_A,
        MODE_MOVE_OBJ_B,
        MODE_COUNT
};

enum { OBJ_A, OBJ_B, OBJ_COUNT };

/* Returns stick's magnitude */
static float get_normalized_stick(float *out, const int8_t stick_in_x,
                                  const int8_t stick_in_y)
{
        float v[2], mag;

        v[0] = (float)stick_in_x / JOYSTICK_MAG_MAX;
        v[1] = (float)stick_in_y / JOYSTICK_MAG_MAX;
        mag = sqrtf(v[0] * v[0] + v[1] * v[1]);
        if (!mag) {
                out[0] = 0.f;
                out[1] = 0.f;
                return 0.f;
        }

        if (mag < ((float)JOYSTICK_MAG_MIN / (float)JOYSTICK_MAG_MAX)) {
                out[0] = 0.f;
                out[1] = 0.f;
                return 0.f;
        }

        if (mag >= 1.f) {
                v[0] /= mag;
                v[1] /= mag;
                mag = 1.f;
        }

        out[0] = v[0];
        out[1] = v[1];

        return mag;
}

struct collision_triangle {
        T3DVec3 pos[3];
};

struct collision_data {
        uint32_t tri_cnt;
        struct collision_triangle *tris;
};

struct object {
        struct collision_data col_dat;
        T3DModel *mdl;
        T3DMat4FP *mtx;
        rspq_block_t *dl;
        T3DVec3 pos_a;
        T3DVec3 pos_b;
};

struct collision_data object_get_collision_data(const char *path)
{
        struct collision_data cd;
        FILE *f;
        
        f = asset_fopen(path, NULL);

        fread(&cd.tri_cnt, 4, 1, f);
        cd.tris = malloc(sizeof(*cd.tris) * cd.tri_cnt);
        for (uint32_t i = 0; i < cd.tri_cnt; ++i) {
                struct collision_triangle *t;

                t = cd.tris + i;
                for (int j = 0; j < 3; ++j)
                        for (int k = 0; k < 3; ++k)
                                fread(t->pos[j].v + k, 4, 1, f);
        }

        fclose(f);

        return cd;
}

static char *path_replace_extension(const char *in, const char *new_ext)
{
        char *out;
        int out_len_old, out_len;
        int new_ext_len;

        out_len = (int)(strrchr(in, '.') - in);
        out = malloc(out_len + 1);
        out[out_len] = 0;
        memcpy(out, in, out_len);
        new_ext_len = strlen(new_ext);
        out_len_old = out_len;
        out_len += new_ext_len;
        out = realloc(out, out_len + 1);
        out[out_len] = 0;
        snprintf(out + out_len_old, out_len, ".%s", new_ext);

        return out;
}

static struct object object_create(const char *path, const T3DVec3 *start_pos)
{
        struct object o;
        char *path_cm;

        path_cm = path_replace_extension(path, "cm");
        o.col_dat = object_get_collision_data(path_cm);
        free(path_cm);

        o.mdl = t3d_model_load(path);
        o.mtx = malloc_uncached(sizeof(*o.mtx));

        rspq_block_begin();
        t3d_matrix_push(o.mtx);
        t3d_model_draw(o.mdl);
        t3d_matrix_pop(1);
        o.dl = rspq_block_end();

        o.pos_a = (start_pos) ? *start_pos : (T3DVec3){{0.f, 0.f, 0.f}};
        o.pos_b = o.pos_a;

        return o;
}

static void object_destroy(struct object *o)
{
        rspq_block_free(o->dl);
        free_uncached(o->mtx);
        t3d_model_free(o->mdl);
        if (o->col_dat.tri_cnt || o->col_dat.tris) {
                o->col_dat.tri_cnt = 0;
                free(o->col_dat.tris);
        }
}

static void object_render(const struct object *o, const float st)
{
        T3DVec3 pos;

        t3d_vec3_lerp(&pos, &o->pos_a, &o->pos_b, st);
        t3d_vec3_scale(&pos, &pos, MODEL_SCALE);
        t3d_mat4fp_from_srt_euler(o->mtx, (float[3]){1.f, 1.f, 1.f},
                                  (float[3]){0.f, 0.f, 0.f}, pos.v);
        rspq_block_run(o->dl);
}

struct observer {
        T3DVec3 pos_a;
        T3DVec3 pos_b;
        T3DVec3 up;
        float yaw_a;
        float yaw_b;
        float pitch_a;
        float pitch_b;
};

static struct observer observer_init(void)
{
        struct observer o;

        o.pos_a = (T3DVec3){{0.f, -5.f, 1.f}};
        o.pos_b = o.pos_a;
        o.up = (T3DVec3){{0.f, 0.f, 1.f}};
        o.yaw_a = M_PI * .5f;
        o.yaw_b = o.yaw_a;
        o.pitch_a = 0.f;
        o.pitch_b = o.pitch_a;

        return o;
}

static T3DVec3 observer_get_forward_dir(const struct observer *o,
                                        const float st)
{
        float yaw_a, yaw_b, yaw, pitch_a, pitch_b, pitch, cos_pitch;

        /* Decide whether or not to get the offset ones for visual effects. */
        yaw_a = o->yaw_a;
        yaw_b = o->yaw_b;
        pitch_a = o->pitch_a;
        pitch_b = o->pitch_b;

        yaw = LERP(yaw_a, yaw_b, st);
        pitch = LERP(pitch_a, pitch_b, st);
        cos_pitch = cosf(pitch);

        return (T3DVec3){{cosf(yaw) * cos_pitch,
                sinf(yaw) * cos_pitch,
                sinf(pitch)}};
}

static T3DVec3 observer_get_right_dir(const struct observer *o,
                                      const T3DVec3 *forw_dir)
{
        T3DVec3 right_dir;

        t3d_vec3_cross(&right_dir, forw_dir, &o->up);

        return right_dir;
}

static void observer_update(struct observer *o,
                            const joypad_inputs_t *inp,
                            const float ft)
{
        /* Looking */
        {
                float stick[2];
                float turn_speed;

                turn_speed = OBSERVER_TURN_SPEED_SLOW;
                if (inp->btn.z)
                        turn_speed = OBSERVER_TURN_SPEED_FAST;

                get_normalized_stick(stick, inp->stick_x, inp->stick_y);

                o->yaw_a = o->yaw_b;
                o->pitch_a = o->pitch_b;
                o->yaw_b -= stick[0] * turn_speed * ft;
                o->pitch_b -= stick[1] * turn_speed * ft;

                {
                        float pitch_limit;

                        pitch_limit = T3D_DEG_TO_RAD(OBSERVER_PITCH_LIMIT);
                        if (o->pitch_b > pitch_limit)
                                o->pitch_b = pitch_limit;

                        if (o->pitch_b < -pitch_limit)
                                o->pitch_b = -pitch_limit;
                }
        }

        /* Movement */
        {
                T3DVec3 forw_move, right_move, move_vec;
                float input_dir[2], speed;

                input_dir[0] = inp->btn.c_right - inp->btn.c_left;
                input_dir[1] = inp->btn.c_up - inp->btn.c_down;

                if (!input_dir[0] && !input_dir[1]) {
                        o->pos_a = o->pos_b;
                        return;
                }

                {
                        float mag;

                        mag = sqrtf(input_dir[0] * input_dir[0] +
                                    input_dir[1] * input_dir[1]);
                        if (mag) {
                                input_dir[0] /= mag;
                                input_dir[1] /= mag;
                        }
                }

                forw_move = observer_get_forward_dir(o, 1.f);
                right_move = observer_get_right_dir(o, &forw_move);

                right_move.v[2] = 0.f;
                t3d_vec3_norm(&right_move);

                t3d_vec3_scale(&forw_move, &forw_move, input_dir[1]);
                t3d_vec3_scale(&right_move, &right_move, input_dir[0]);

                t3d_vec3_add(&move_vec, &forw_move, &right_move);
                t3d_vec3_norm(&move_vec);

                speed = (inp->btn.r) ? OBSERVER_NOCLIP_SPEED_FAST :
                        OBSERVER_NOCLIP_SPEED_SLOW;
                t3d_vec3_scale(&move_vec, &move_vec, speed * ft);

                o->pos_a = o->pos_b;
                t3d_vec3_add(&o->pos_b, &o->pos_b, &move_vec);
        }
}

static void observer_to_view_matrix(const struct observer *o,
                                    T3DViewport *vp, const float st)
{
        T3DVec3 eye, forw_dir, focus;

        /* Eye */
        t3d_vec3_lerp(&eye, &o->pos_a, &o->pos_b, st);
        t3d_vec3_scale(&eye, &eye, MODEL_SCALE);

        /* Focus */
        forw_dir = observer_get_forward_dir(o, st);
        t3d_vec3_add(&focus, &eye, &forw_dir);

        t3d_viewport_look_at(vp, &eye, &focus, &o->up);
}

static const char *mode_enum_to_string(const enum mode m)
{
        switch (m) {
                case MODE_OBSERVER:
                        return "MODE_OBSERVER";

                case MODE_MOVE_OBJ_A:
                        return "MODE_MOVE_OBJ_A";

                case MODE_MOVE_OBJ_B:
                        return "MODE_MOVE_OBJ_B";

                default:
                        return NULL;
        }
}

static void object_move(struct object *o,
                        const joypad_inputs_t *inp,
                        const float ft)
{
        T3DVec3 move;
        float stick[2];

        get_normalized_stick(stick, inp->stick_x, inp->stick_y);
        move = (T3DVec3){{ stick[0], stick[1],
                           inp->btn.c_up - inp->btn.c_down }};
        t3d_vec3_scale(&move, &move, OBJECT_MOVE_SPEED * ft);

        o->pos_a = o->pos_b;
        t3d_vec3_add(&o->pos_b, &o->pos_b, &move);
}

static enum mode update_depending_on_mode(enum mode m,
                                          struct observer *obs,
                                          struct object *objs,
                                          const joypad_inputs_t *inp_new,
                                          const joypad_inputs_t *inp_old,
                                          const float ft)
{
        if (inp_new->btn.a && !inp_old->btn.a)
                if (++m >= MODE_COUNT)
                        m = 0;

        if (inp_new->btn.b && !inp_old->btn.b)
                if (--m < 0)
                        m = MODE_COUNT - 1;

        if (m < MODE_MOVE_OBJ_A) 
                observer_update(obs, inp_new, ft);
        else
                object_move(objs + m - 1, inp_new, ft);

        return m;
}

#define DBG_Y_POS (32 + (line++ * 10))
static void render_debug_info(const enum mode mode)
{
        int line;

        line = 0;
        t3d_debug_print_start();
        t3d_debug_printf(32, DBG_Y_POS, "Mode: %s (%d)",
                         mode_enum_to_string(mode), mode);
        if (mode < MODE_MOVE_OBJ_A)
                return;

        t3d_debug_printf(32, DBG_Y_POS, "Mesh %d", mode - 1);
}
#undef DBG_Y_POS

static void particles_update_from_objs(const int part_cnt, TPXParticle *parts,
                                       struct object *objs)
{
        T3DVec3 *diffs;

        diffs = malloc(sizeof(*diffs) * part_cnt);
        for (uint32_t y = 0; y < objs[OBJ_A].col_dat.tri_cnt; ++y) {
                for (uint32_t x = 0; x < objs[OBJ_B].col_dat.tri_cnt; ++x) {
                        T3DVec3 *a, *b;

                        a = objs[OBJ_A].col_dat.tris[y].pos;
                        b = objs[OBJ_B].col_dat.tris[x].pos;
                        for (int i = 0; i < 3; ++i) {
                                T3DVec3 *v;
                                T3DVec3 at, bt;

                                v = diffs + (y * (objs[OBJ_B].col_dat.tri_cnt *
                                                  3) + x) + i;
                                t3d_vec3_add(&at, a + i, &objs[OBJ_A].pos_b);
                                t3d_vec3_add(&bt, b + i, &objs[OBJ_B].pos_b);
                                t3d_vec3_diff(v, &bt, &at);
                                t3d_vec3_scale(v, v, 16);
                        }
                }
        }

        for (int i = 0; i < part_cnt; i += 2) {
                TPXParticle *p;

                p = parts + (i >> 1);
                p->posA[0] = diffs[i].v[0];
                p->posA[1] = diffs[i].v[1];
                p->posA[2] = diffs[i].v[2];
                p->sizeA = 4;
                p->colorA[0] = 0xFF;
                p->colorA[1] = 0xFF;
                p->colorA[2] = 0xFF;
                p->colorA[3] = 0xFF;

                p->posB[0] = diffs[i + 1].v[0];
                p->posB[1] = diffs[i + 1].v[1];
                p->posB[2] = diffs[i + 1].v[2];
                p->sizeB = 4;
                p->colorB[0] = 0xFF;
                p->colorB[1] = 0xFF;
                p->colorB[2] = 0xFF;
                p->colorB[3] = 0xFF;
        }

        free(diffs);
}

int main(void)
{
        T3DViewport viewport;
        int dfs_handle;
        float time_accumulated;
        joypad_inputs_t inp_old, inp_new;

        T3DMat4FP *particle_mtx;
        TPXParticle *particles;
        uint32_t particle_count;

        struct observer observer;
        struct object objs[OBJ_COUNT];
        enum mode mode;

        /* Initialize Libdragon. */
        display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3,
                     GAMMA_NONE, FILTERS_RESAMPLE);
        rdpq_init();
        joypad_init();
#ifdef DEBUG
        debug_init_usblog();
        debug_init_isviewer();
        rdpq_debug_start();
#endif
        asset_init_compression(COMPRESS_LEVEL);
        dfs_handle = dfs_init(DFS_DEFAULT_LOCATION);

        /* Initialize Tiny3D. */
        t3d_init((T3DInitParams){});

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        t3d_debug_print_init();
#pragma GCC diagnostic pop

        viewport = t3d_viewport_create();

        /* Initialize Simulation */
        observer = observer_init();
        objs[OBJ_A] = object_create("rom:/obj_a.t3dm",
                                    &(T3DVec3){{1.f, 0.f, 0.f}});
        objs[OBJ_B] = object_create("rom:/obj_b.t3dm",
                                    &(T3DVec3){{-1.f, 0.f, 0.f}});
        mode = MODE_OBSERVER;

        /* Initialize TPX (particles) */
        tpx_init((TPXInitParams){});
        particle_mtx = malloc_uncached(sizeof(*particle_mtx));
        particle_count = (objs[OBJ_A].col_dat.tri_cnt * 3) *
                         (objs[OBJ_B].col_dat.tri_cnt * 3);
        debugf("particle_count: %lu\n", particle_count);
        particles = malloc_uncached(sizeof(*particles) * (particle_count >> 1));

        /* Main loop. */
        time_accumulated = 0.f;

        for (;;) {
                static const float fixed_time = 1.f / TICKRATE;
                float subtick;

                /* Updating */
                for (time_accumulated += display_get_delta_time();
                     time_accumulated >= fixed_time;
                     time_accumulated -= fixed_time) {
                        joypad_poll();
                        inp_old = inp_new;
                        inp_new = joypad_get_inputs(JOYPAD_PORT_1);

                        mode = update_depending_on_mode(mode, &observer, objs,
                                                        &inp_new, &inp_old,
                                                        fixed_time);
                }

                /* Rendering Setup */
                subtick = time_accumulated / fixed_time;
                t3d_viewport_set_projection(&viewport,
                                            T3D_DEG_TO_RAD(VIEWPORT_FOV_DEG),
                                            VIEWPORT_NEAR, VIEWPORT_FAR);
                observer_to_view_matrix(&observer, &viewport, subtick);
                t3d_mat4fp_from_srt(particle_mtx,
                                    (float[3]){1.f, 1.f, 1.f},
                                    (float[4]){0.f, 0.f, 0.f, 1.f},
                                    (float[3]){0.f, 0.f, 0.f});

                /* 3D Rendering */
                rdpq_attach(display_get(), display_get_zbuf());
                t3d_frame_start();
                rdpq_mode_dithering(DITHER_NOISE_NONE);
                rdpq_mode_antialias(AA_NONE);

                t3d_viewport_attach(&viewport);
                t3d_screen_clear_color(color_from_packed32(0x0));
                t3d_screen_clear_depth();

                t3d_light_set_ambient((uint8_t[4]){0xFF, 0xFF, 0xFF, 0xFF});
                t3d_light_set_count(0);
                for (int i = 0; i < OBJ_COUNT; ++i)
                        object_render(objs + i, subtick);

                /* Particle Rendering */
                rdpq_sync_pipe();
                rdpq_sync_tile();
                rdpq_set_mode_standard();
                rdpq_mode_zbuf(true, true);
                rdpq_mode_zoverride(true, 0, 0);
                rdpq_mode_combiner(RDPQ_COMBINER1((PRIM, 0, ENV, 0),
                                                  (0, 0, 0, 1)));
                tpx_state_from_t3d();
                tpx_matrix_push(particle_mtx);
                tpx_state_set_scale(1.f, 1.f);
                particles_update_from_objs(particle_count, particles, objs);
                tpx_particle_draw(particles, particle_count);
                tpx_matrix_pop(1);

                /* UI Rendering */
                render_debug_info(mode);
                rdpq_detach_show();
        }

        /* Terminate Simulation */
        for (int i = 0; i < OBJ_COUNT; ++i)
                object_destroy(objs + i);

        /* Terminate TPX */
        free_uncached(particles);
        free_uncached(particle_mtx);
        tpx_destroy();

        /* Terminate Tiny3D. */
        t3d_destroy();

        /* Terminate Libdragon. */
        dfs_close(dfs_handle);
        joypad_close();
        rdpq_close();
#ifdef DEBUG
        rdpq_debug_stop();
#endif
        display_close();
}
