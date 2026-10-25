/* Native resource/PEL lifecycle from NROC030/C95C/BE44; no Forge services. */
typedef unsigned int u32;
typedef unsigned char u8;
typedef unsigned short u16;
#define U32(p,o) (*(volatile u32 *)((u8 *)(p)+(o)))
#define U8(p,o) (*(volatile u8 *)((u8 *)(p)+(o)))
#define U16(p,o) (*(volatile u16 *)((u8 *)(p)+(o)))
extern u32 iai_main_base(void);
extern void iai_quest_clear(void);
extern int iai_helm_active(void *);
extern void iai_log_span(const char *,u32);
#include "diagnostics.h"
u32 fx_native[4]; /* ready, main base, private shell resource, shell PEL handle */
static __attribute__((used)) struct { void *arc,*manager,*trail,*resources[3]; u32 failed,played; } fx;

/* ARM EABI structure copies emitted by -Oz; volatile stores prevent recursive lowering. */
void __aeabi_memcpy(void *dest,const void *source,u32 size) {
    u8 *d=dest;const u8 *s=source;
    if(!(((u32)d|(u32)s)&3))while(size>=4){*(volatile u32 *)d=*(const u32 *)s;d+=4;s+=4;size-=4;}
    while(size--)*((volatile u8 *)d++)=*s++;
}

static int fx_manager(void *p,u32 base) {
    return p && U32((void *)base,0x18509e8)==(u32)p &&
        U32((void *)base,0x188a0d4)==(u32)p && U32(p,0)==base+0x1731534;
}
void fx_reset(void) {
    u32 base=iai_main_base();fx_native[0]=0;
    if(fx_manager(fx.manager,base)){
        void *handles[]={fx.trail,(void *)fx_native[3]};
        for(u32 k=0;k<2;++k)if(handles[k] && U32(handles[k],4)<U32(fx.manager,0x5c))
            ((void (*)(void *,void *))(base+0x326c78))(fx.manager,handles[k]);
    }
    void *arc=fx.arc;fx.arc=fx.manager=fx.trail=0;
    for(u32 k=0;k<3;++k)fx.resources[k]=0;
    fx_native[2]=fx_native[3]=fx.failed=fx.played=0;
    if(arc)((void (*)(void *))(base+0x887660))(arc);
}
static int fx_name(const char *a,const char *b) {
    for(u32 k=0;k<64;++k){char c=a[k];if(c=='\\')c='/';if(c!=b[k])return 0;if(!c)return 1;}
    return 0;
}
void fx_tick(int load) {
    u32 base=iai_main_base();fx_native[1]=base;
    void *manager=(void *)U32((void *)base,0x18509e8);
    void *quest=(void *)U32((void *)base,0x188ad90);
    if(!quest || U32(quest,0)!=base+0x1753b30 || U8(quest,0x50)!=2 ||
       (fx.manager && fx.manager!=manager)){if(fx.arc)fx_reset();return;}
    if(!load || fx.failed || !fx_manager(manager,base))return;
    if(!fx.arc){
        void *loader=(void *)U32((void *)base,0x2126a64);if(!loader)return;
        fx.arc=((void *(*)(void *,void *,const char *,u32))U32((void *)U32(loader,0),0x30))
            (loader,(void *)(base+0x2124df0),"arc/forge/fix40_effects",1);
        if(!fx.arc){fx.failed=1;iai_log_span("MHGU_LSO_P13_FX_FAIL",20);return;}
        iai_log_span("MHGU_LSO_P13_FX_ARC",19);
    }
    u32 flags=U8(fx.arc,0x50);
    if(flags&0x50){fx_reset();fx.failed=1;iai_log_span("MHGU_LSO_P13_FX_FAIL",20);return;}
    if(!(flags&1) || fx_native[0])return;
    static const char *const names[]={"effect/pel/pl/w07_f40","effect/pel/pl/w04_f40","shell/pl/pl_w04_f40/pl_w04_f40"};
    u32 count=U32(fx.arc,0x68);void **list=(void **)U32(fx.arc,0x64);
    if(!list || !count || count>4096)return;
    for(u32 i=0;i<count;++i){
        void *r=list[i];if(!r)continue;
        for(u32 k=0;k<3;++k)if(!fx.resources[k] && U32(r,0)==base+(k==2?0x17632e0:0x175261c) &&
           fx_name((char *)r+8,names[k]))fx.resources[k]=r;
    }
    for(u32 k=0;k<3;++k){
        if(!fx.resources[k])return;
        flags=U8(fx.resources[k],0x50);
        if(flags&0x50){fx_reset();fx.failed=1;iai_log_span("MHGU_LSO_P13_FX_FAIL",20);return;}
        if(!(flags&1))return;
    }
    fx.manager=manager;
    for(u32 k=0;k<2;++k)if(!(k?fx_native[3]:(u32)fx.trail)){
        void *h=((void *(*)(void *,void *))(base+0x3268d8))(manager,fx.resources[k]);
        if(h && U32(h,4)<U32(manager,0x5c)){
            if(k)fx_native[3]=(u32)h;else fx.trail=h;
        }
    }
    if(fx.trail && fx_native[3]){
        fx_native[2]=(u32)fx.resources[2];fx_native[0]=1;
        iai_log_span("MHGU_LSO_P13_FX_READY",21);
    }
}
void fx_trail_reset(void) { fx.played=0; }
u32 fx_event(void *p,u32 slot,u32 *event,u32 index,void *context) {
    if(slot!=1 || !event || index!=1 || event[0]!=3 || U16(event,6)!=0x3f40 || event[0x12]!=2)
        return ((u32 (*)(void *,u32,u32 *,u32,void *))(iai_main_base()+0x2ae6b0))(p,slot,event,index,context);
    if(!fx_native[0] || fx.played || !iai_helm_active(p) || !fx_manager(fx.manager,iai_main_base()) ||
       U32(fx.trail,4)>=U32(fx.manager,0x5c))return 0;
    if(context)U32(context,8)=2;
    u32 result=((u32 (*)(void *,void *,u32,u32,void *,u32))U32((void *)U32(p,0),0x138))
        (p,fx.trail,1,0x3f40,context,U32(p,0x3310));
    if(result){fx.played=1;iai_log_span("MHGU_LSO_P13_TRAIL",18);}
    return result;
}
/* NROD684/C550: replay a missed PSL event after asynchronous loading, through its live controller. */
void fx_trail(void *p) {
    if(!fx_native[0] || fx.played || !iai_helm_active(p))return;
    u32 base=iai_main_base();void *registry=(void *)U32((void *)base,0x188a12c);
    u32 index=U32(p,0x1360);
    if(!registry || U32(registry,0)!=base+0x1752828 || U32(p,0x135c)!=base+0x1734428 || index>=64)return;
    void *ctl=(void *)U32(registry,0x1c+index*4);
    if(!ctl || U32(ctl,0)!=base+0x17314bc || U32(ctl,0xb4)!=(u32)p+0xfd8 ||
       U32(ctl,0x18)!=0x5f3 || (U8(ctl,0xc8)&14))return;
    void *psl=(void *)U32(ctl,0x40);
    if(!psl || U32(psl,0)!=base+0x17526c4)return;
    void *entry=((void *(*)(void *,u32))(base+0x324aa8))(psl,243);
    void *event=entry?(void *)U32(entry,8):0;
    if(event && (U8(event,0)&2) && U16(event,6)==0x3f40 && U32(event,0x48)==2)
        ((void (*)(void *,void *,u32))(base+0x31d140))(ctl,event,1);
}
void fx_particle_update(void *p) {
    extern void fx_particle_gateway(void *);
    fx_particle_gateway(p);
    u32 base=iai_main_base();
    if(!p || U32(p,0)!=base+0x17317dc)return;
    void *ctx=(void *)U32(p,0x380);if(!ctx || U32(ctx,0x1b4)!=0x3f40 || U8(ctx,0x30)!=1)return;
    void *flags=(void *)U32(ctx,0x194);
    if((!flags || !(U8(flags,0)&14)) && !U16(ctx,0x198) && U32(p,0x104)>0x40ffffff)U32(p,0xfc)=0;
}
void fx_scene(void *scene) {
    u32 base=iai_main_base();
    if(scene && U32(scene,0)==base+0x1753b30 && U8(scene,0x50)>=2 && U8(scene,0x50)<=9){
        fx_reset();iai_quest_clear();
    }
}
