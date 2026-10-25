#include "../port_iai_handler/pose_f0.h"

/* Original CRO 7A8C..7AB8 and 7AEC..7B18: four 177-row tables,
   early blade/sheath, late blade/sheath. F1 switches at frame 45.
   Unlike F0, blade selector remains 19 and no forced 0.8 scale is used.
   Tables are read-only original data, extracted locally by the auditor. */
int iai_pose_f1(const float *tables,u32 bytes,float frame,u32 part,IaiPose *out) {
    if (!tables || !out || bytes!=4u*177u*48u || (part!=9 && part!=10) ||
        !__builtin_isfinite(frame)) return 0;
    if (frame<0) frame=0;
    if (frame>176) frame=176;
    u32 late=frame>=45.0f;
    u32 table=late*2u+(part==10);
    u32 i=(u32)frame;
    float fraction=frame-(float)i;
    const float *a=tables+(table*177u+i)*12u;
    const float *b=i==176?a:a+12;
    IaiPose p;
    for(u32 k=0;k<12;++k) p.transform[k]=a[k]+(b[k]-a[k])*fraction;
    float *t=p.transform;
    float norm=__builtin_sqrtf(t[4]*t[4]+t[5]*t[5]+t[6]*t[6]+t[7]*t[7]);
    if (!(norm>0) || !__builtin_isfinite(norm)) return 0;
    for(u32 k=4;k<8;++k) t[k]/=norm;
    /* CRO 7E64: in F1 the pivot branch compares part==9, even after 45. */
    float x=(part==9?4.0f:3.5f)*t[8],z=t[11]*t[10];
    float qx=t[4],qy=t[5],qz=t[6],qw=t[7];
    float cx=2*qy*z,cy=2*(qz*x-qx*z),cz=-2*qy*x;
    t[0]-=x+qw*cx+qy*cz-qz*cy;
    t[1]-=qw*cy+qz*cx-qx*cz;
    t[2]-=z+qw*cz+qx*cy-qy*cx;
    t[11]=0;
    p.bone=part==9?(late?8:12):(late?2:8);
    p.selector=part==9?19:(late?2:1);
    volatile float *dest=out->transform;
    for(u32 k=0;k<12;++k) dest[k]=t[k];
    out->bone=p.bone;out->selector=p.selector;
    return 1;
}
