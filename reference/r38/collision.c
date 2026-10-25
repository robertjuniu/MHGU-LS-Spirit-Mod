#include "collision.h"
_Static_assert(sizeof(IaiShape)==48,"Native HTS record extent");
int iai_collision_shape(const float *table,u32 bytes,float frame,IaiShape *out) {
    if (!table || !out || bytes!=17*8*4 || !__builtin_isfinite(frame) ||
        frame<12.0f || frame>=28.0f) return 0;
    float offset=frame-12.0f;u32 row=(u32)offset;
    float alpha=offset-(float)row;
    for (u32 k=0;k<8;++k) {
        float a=table[row*8+k],b=table[(row+1)*8+k];
        if (!__builtin_isfinite(a) || !__builtin_isfinite(b)) return 0;
    }
    out->bone0=out->bone1=12;out->type=out->flags=1;
    out->unused=0;out->radius=52.0f;
    for (u32 k=0;k<8;++k) {
        float a=table[row*8+k],b=table[(row+1)*8+k];
        out->endpoints[k]=a+(b-a)*alpha;
    }
    return 1;
}
