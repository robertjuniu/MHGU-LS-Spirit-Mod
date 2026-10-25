#include "weapon_settle.h"

static int unit(float *q) {
    float n=0;for(u32 i=0;i<4;++i)n+=q[i]*q[i];
    if (!__builtin_isfinite(n) || n<1e-12f) return 0;
    n=__builtin_sqrtf(n);for(u32 i=0;i<4;++i)q[i]/=n;return 1;
}
static int finite(const float *p,u32 n) { for(u32 i=0;i<n;++i)if(!__builtin_isfinite(p[i]))return 0;return 1; }
static void multiply(float *out,const float *a,const float *b) {
    for(u32 c=0;c<3;++c)for(u32 r=0;r<3;++r){float v=0;for(u32 k=0;k<3;++k)v+=a[k*3+r]*b[c*3+k];out[c*3+r]=v;}
}
/* Same column-major quaternion layout as native8A8384. */
static int compose(const float *v,float *r) {
    float q[4]={v[4],v[5],v[6],v[7]};if(!unit(q))return 0;
    float x=q[0],y=q[1],z=q[2],w=q[3];
    float a[9]={1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w),
        2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w),
        2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y)};
    for(u32 c=0;c<3;++c)for(u32 row=0;row<3;++row)r[c*3+row]=a[c*3+row]*v[8+c];return finite(r,9);
}
static int inverse(float *out,const float *m) {
    float a=m[0],b=m[3],c=m[6],d=m[1],e=m[4],f=m[7],g=m[2],h=m[5],i=m[8];
    float det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(!__builtin_isfinite(det) || __builtin_fabsf(det)<1e-8f)return 0;
    float v[9]={e*i-f*h,f*g-d*i,d*h-e*g,c*h-b*i,a*i-c*g,b*g-a*h,b*f-c*e,c*d-a*f,a*e-b*d};
    for(u32 k=0;k<9;++k)out[k]=v[k]/det;return finite(out,9);
}
static int decompose(const float *m,float *q,float *scale) {
    float r[9];
    for(u32 c=0;c<3;++c){float n=0;for(u32 k=0;k<3;++k)n+=m[c*3+k]*m[c*3+k];if(!__builtin_isfinite(n)||n<1e-12f)return 0;scale[c]=__builtin_sqrtf(n);}
    float det=m[0]*(m[4]*m[8]-m[7]*m[5])-m[3]*(m[1]*m[8]-m[7]*m[2])+m[6]*(m[1]*m[5]-m[4]*m[2]);
    if(det<0)scale[0]=-scale[0];
    for(u32 c=0;c<3;++c)for(u32 k=0;k<3;++k)r[c*3+k]=m[c*3+k]/scale[c];
    /* shortcut: reject shear rather than approximating it; expand only if a real weapon needs it. */
    for(u32 a=0;a<3;++a)for(u32 b=a+1;b<3;++b){float dot=0;for(u32 k=0;k<3;++k)dot+=r[a*3+k]*r[b*3+k];if(__builtin_fabsf(dot)>.001f)return 0;}
    float trace=r[0]+r[4]+r[8],s;
    if(trace>0){s=2*__builtin_sqrtf(trace+1);q[3]=s*.25f;q[0]=(r[5]-r[7])/s;q[1]=(r[6]-r[2])/s;q[2]=(r[1]-r[3])/s;}
    else if(r[0]>r[4] && r[0]>r[8]){s=2*__builtin_sqrtf(1+r[0]-r[4]-r[8]);q[3]=(r[5]-r[7])/s;q[0]=s*.25f;q[1]=(r[3]+r[1])/s;q[2]=(r[6]+r[2])/s;}
    else if(r[4]>r[8]){s=2*__builtin_sqrtf(1+r[4]-r[0]-r[8]);q[3]=(r[6]-r[2])/s;q[0]=(r[3]+r[1])/s;q[1]=s*.25f;q[2]=(r[7]+r[5])/s;}
    else{s=2*__builtin_sqrtf(1+r[8]-r[0]-r[4]);q[3]=(r[1]-r[3])/s;q[0]=(r[6]+r[2])/s;q[1]=(r[7]+r[5])/s;q[2]=s*.25f;}
    return unit(q);
}
int iai_settle_weapon(const IaiPose *pose,const float *native,const float *old_parent,const float *new_parent,float weight,IaiPose *out) {
    if(!pose||!native||!out||!__builtin_isfinite(weight)||weight<0||weight>1||!finite(pose->transform,12)||!finite(native,12))return 0;
    float target_q[4]={native[4],native[5],native[6],native[7]};if(!unit(target_q))return 0;
    IaiPose p=*pose;
    if(weight==1){for(u32 k=0;k<12;++k)p.transform[k]=native[k];*out=p;return 1;}
    float *v=p.transform;
    if(old_parent||new_parent){
        if(!old_parent||!new_parent||!finite(old_parent,16)||!finite(new_parent,16))return 0;
        for(u32 k=3;k<12;k+=4)if(__builtin_fabsf(old_parent[k])>.001f||__builtin_fabsf(new_parent[k])>.001f)return 0;
        if(__builtin_fabsf(old_parent[15]-1)>.001f||__builtin_fabsf(new_parent[15]-1)>.001f)return 0;
        float a[9],b[9],inv[9],relative[9],local[9],converted[9],position[3];
        for(u32 c=0;c<3;++c)for(u32 r=0;r<3;++r){a[c*3+r]=old_parent[c*4+r];b[c*3+r]=new_parent[c*4+r];}
        if(!inverse(inv,b)||!compose(v,local))return 0;
        multiply(relative,inv,a);multiply(converted,relative,local);
        for(u32 r=0;r<3;++r){position[r]=0;for(u32 k=0;k<3;++k)position[r]+=relative[k*3+r]*v[k]+inv[k*3+r]*(old_parent[12+k]-new_parent[12+k]);}
        if(!decompose(converted,v+4,v+8))return 0;
        for(u32 r=0;r<3;++r)v[r]=position[r];
    }
    if(!unit(v+4))return 0;
    float dot=0;for(u32 k=0;k<4;++k)dot+=v[4+k]*target_q[k];
    for(u32 k=0;k<4;++k)v[4+k]+=(target_q[k]*(dot<0?-1:1)-v[4+k])*weight;
    if(!unit(v+4))return 0;
    for(u32 k=0;k<3;++k){v[k]+=(native[k]-v[k])*weight;v[8+k]+=(native[8+k]-v[8+k])*weight;}
    v[3]=native[3];v[11]=native[11];
    if(!finite(v,12))return 0;
    *out=p;return 1;
}
