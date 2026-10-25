/* Original-pose mathematics only. No game addresses, hooks or installation. */
#ifndef IAI_POSE_F0_H
#define IAI_POSE_F0_H
typedef unsigned int u32;
typedef struct { float transform[12]; u32 bone, selector; } IaiPose;
/* tables: the two unchanged 131x48-byte F0 tables from the original CRO.
   Caller has already selected original body motion 05F9, not a native pose.
   Returns zero without changing out for invalid input. */
int iai_pose_f0(const float *tables, u32 table_bytes, float frame,
                u32 part, IaiPose *out);
#endif
