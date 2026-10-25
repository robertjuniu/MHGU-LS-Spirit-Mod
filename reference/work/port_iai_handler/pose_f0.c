#include "pose_f0.h"

/* Reconstructed from CRO file offsets 7A60..7CD8, 7E64, 7EE4.
   Table values are extracted locally, never compiled into this source.
   F1 / attacks / damage / input / state transitions are deliberately absent. */
int iai_pose_f0(const float *tables, u32 table_bytes, float frame,
                u32 part, IaiPose *out) {
    if (!tables || !out || table_bytes != 2u * 131u * 48u ||
        (part != 9 && part != 10) || !__builtin_isfinite(frame)) return 0;
    if (frame < 0) frame = 0;
    if (frame > 130) frame = 130;
    /* CRO 7FE8 = 73.0. At this point the blade uses the sheath table,
       bone 8, selector 2 and uniform 0.8 scale (not its native scale). */
    u32 late = frame >= 73.0f;
    u32 table = part == 10 || late;
    u32 i = (u32)frame;
    float fraction = frame - (float)i;
    const float *a = tables + (table * 131u + i) * 12u;
    const float *b = i == 130 ? a : a + 12;
    IaiPose p;
    for (u32 k = 0; k < 12; ++k)
        p.transform[k] = a[k] + (b[k] - a[k]) * fraction;
    float *t = p.transform;
    float norm = __builtin_sqrtf(t[4]*t[4] + t[5]*t[5] +
                                t[6]*t[6] + t[7]*t[7]);
    if (!(norm > 0) || !__builtin_isfinite(norm)) return 0;
    for (u32 k = 4; k < 8; ++k) t[k] /= norm;
    if (late) t[8] = t[9] = t[10] = 0.8f;
    /* Original pivot x is 4 for early blade; 3.5 for sheath/late.
       z comes from interpolated transform[11], NOT constant 4.
       CRO loads only two floats from 7CDC/7CE8, then model+6C. */
    float x = (part == 9 && !late ? 4.0f : 3.5f) * t[8];
    float y = 0;
    float z = t[11] * t[10];
    float qx=t[4], qy=t[5], qz=t[6], qw=t[7];
    float cx=2*(qy*z-qz*y), cy=2*(qz*x-qx*z), cz=2*(qx*y-qy*x);
    t[0] -= x + qw*cx + qy*cz - qz*cy;
    t[1] -= y + qw*cy + qz*cx - qx*cz;
    t[2] -= z + qw*cz + qx*cy - qy*cx;
    t[11] = 0;
    p.bone = part == 10 || late ? 8 : 12;
    p.selector = late ? 2 : (part == 9 ? 19 : 1);
    /* Volatile stores keep this freestanding on both A32 and Wasm: no
       compiler-generated memcpy dependency for the fixed-size struct. */
    volatile float *dest = out->transform;
    for (u32 k = 0; k < 12; ++k) dest[k] = t[k];
    out->bone = p.bone;
    out->selector = p.selector;
    return 1;
}
