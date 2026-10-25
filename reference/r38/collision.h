#ifndef IAI_COLLISION_H
#define IAI_COLLISION_H
#include "../port_iai_handler/pose_f0.h"
typedef struct {
    unsigned short bone0,bone1,type,flags;
    u32 unused;
    float radius,endpoints[8];
} IaiShape;
/* Original CRO 15860..15A80; 17 frames x 8 float components.
   The last row is the interpolation endpoint for frame just below 28. */
int iai_collision_shape(const float *table,u32 bytes,float frame,IaiShape *out);
#endif
