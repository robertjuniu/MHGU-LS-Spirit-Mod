/* Experimental original-pose integration. All game fields are version-bound.
   No game state/action ID is added: the private state belongs to this module.
   Only the explicitly requested stance suppresses the native idle callback. */
#include "pose_f0.h"
#include "collision.h"
#include "weapon_settle.h"
typedef unsigned short u16;
typedef unsigned char u8;
typedef unsigned long uptr;
#define U8(p,o) (*(volatile u8 *)((u8 *)(p)+(o)))
#define U16(p,o) (*(volatile u16 *)((u8 *)(p)+(o)))
#define U32(p,o) (*(volatile u32 *)((u8 *)(p)+(o)))
#define F32(p,o) (*(volatile float *)((u8 *)(p)+(o)))
extern uptr iai_main_base(void);
extern void iai_log_gp(void);
extern void iai_log_cuts(void);
extern void iai_log_combo_first(void);
extern void iai_log_combo_second(void);
extern void iai_log_reward_level(void);
extern void iai_log_reward_valor(void);
extern void iai_log_reward_delay(void);
extern void iai_log_yaw_exit(void);
extern void iai_log_alt_request(void);
extern void iai_log_alt_accepted(void);
extern void iai_log_blend_skip(void);
extern void iai_log_chain_buffer(void);
extern void iai_log_chain_commit(void);
extern void iai_log_window_cue(void);
extern void iai_log_roll_entry(void);
extern void iai_log_audio_sheath(void);
extern void iai_log_audio_draw(void);
extern void iai_log_audio_cut(void);
extern void iai_log_audio_missing(void);
extern void iai_log_native_end(void);
extern void iai_log_cancel7(void);
extern void iai_log_span(const char *,u32);
#include "diagnostics.h"
extern const float iai_tables[];
extern const float iai_f1_tables[];
extern const float iai_collision_tables[];
extern const float iai_combo_yaw_tables[];
extern int iai_pose_f1(const float *,u32,float,u32,IaiPose *);

typedef struct {
    void *player;
    u32 active, started, released, reset_idle, slash, zr_released, exiting, suppress_chord;
    u32 hit_attempted, hit_registered, hit_ticket, parried, counter_attempted;
    u32 combo_ready, combo_buffer, yaw_done, rechain_buffer, window_cued, audio_mask, blade_log_mask;
    float clock;
    u32 hit_motion;
} State;
static State state;
static __attribute__((used)) struct { void *player; u32 owner,actor,evaded,hit,buffer,yaw,protect,round,round_before; } foresight;
/* slash=1 retains R38; 2=small Iai; 3=thrust; 4=takeoff; 5=airborne; 6=dive; 7=landing. */
static struct { void *player; u32 owner,actor,active,mode,flag,gravity,in_floor,veto; } air;
static struct { u32 launched,frozen,level,speed_owned,speed0,speed1,point_valid,captured,sent,started,target,part,identity,target_vt,owner,actor; float local[3],start_frame; u32 detached; float elapsed; } helm;
static struct { void *player; u32 active,owner,actor; } helm_move;
static u32 helm_natural_landing;
static __attribute__((used)) struct {
    void *player,*manager,*record;
    u32 owner,actor,ticket,stage,valid,count,serial,epoch,busy;
    float previous[9],extra[12][9];
} sweep;
static u32 sweep_logged;
static struct { void *player; u32 owner,actor; float remaining,carry,clock; } small_regen;
static void small_regen_start(void *p,float duration) {
    u32 owner=U32(p,0x13c4),actor=U8(p,0x1054);
    if(small_regen.player!=p || small_regen.owner!=owner || small_regen.actor!=actor)small_regen.remaining=0;
    small_regen.player=p;small_regen.owner=owner;small_regen.actor=actor;
    if(small_regen.remaining<duration)small_regen.remaining=duration;
    small_regen.carry=0;small_regen.clock=F32(p,0x2534);
}
extern int small_pose_raw(void *,u32,float *,u32 *,u32 *);
extern int helm_point_local(const float *,const float *,float *);
#ifndef IAI_HOST_TEST
extern u32 helm_sound_base;
extern void helm_sound_raw(void *);
extern void fx_tick(int);
extern void fx_trail(void *);
extern void fx_trail_reset(void);
extern u32 fx_shell_raw(void *);
#ifndef IAI_RELEASE
static void helm_phase_trace(const char *tag,void *p);
static void log_words(const char *,const u32 *,u32);
#else
#define helm_phase_trace(...) ((void)0)
#define log_words(prefix,words,count) ((void)(words))
#endif
#else
extern void test_helm_sound(void *);
#endif
static u32 body_motion(void) { return state.exiting?5251:state.slash==8?5246:state.slash==9?5254:(state.slash==7?5244:(state.slash==6?5243:(state.slash==5?5226:(state.slash==4?5241:(state.slash==3?5240:(state.slash==2?5245:(state.slash?5250:5249))))))); }
static u32 body_clip(void) { return 0x500+body_motion()-5000; }
static int helm_motion_start(void *p);
static const u32 *clip(void *,u32);
static int foresight_commit(void *);
static int foresight_start(void *,u32,u32,u32);
static void foresight_tick(void *);
static void foresight_release(void *);
static void small_contact(void *);
#ifndef IAI_HOST_TEST
#include "foresight_data.h"
extern const float *small_tables_cell;
#endif
static int authored_pose(void *p,u32 part,IaiPose *out) {
    if (!p || !out || (part!=9 && part!=10) ||
        (state.slash<2 || state.slash>9) || U16(p,0x4b4)!=body_clip() ||
        !__builtin_isfinite(F32(p,0x500)) || F32(p,0x500)<0 || F32(p,0x500)>10000) return 0;
#ifndef IAI_HOST_TEST
    if(state.slash==4 || state.slash==5 || state.slash>=8){
        const u32 *header=clip(p,0x5f6);if(!header || header[2]!=141)return 0;
        const u32 *data=(const u32 *)((const u8 *)header+FORESIGHT_DATA_DELTA);
        if(data[0]!=0x46534f50 || data[1]!=44352 || data[2]!=141 || data[3]!=140)return 0;
        const float *saved=small_tables_cell;small_tables_cell=(const float *)(data+4);
        int result=small_pose_raw(p,part,out->transform,&out->bone,&out->selector);
        small_tables_cell=saved;return result;
    }
#endif
    return small_pose_raw(p,part,out->transform,&out->bone,&out->selector);
}
const float *small_pose_bone(void *p,int bone) {
    if(!p || bone<0 || bone>254)return 0;
    return ((const float *(*)(void *,u32))(iai_main_base()+0x27c40c))(p,(u32)bone);
}
/* A split B-then-ZR cancels normally, but cannot immediately restart F0. */
static struct { void *player; u32 quiet,reported; } reentry;
/* Bounded one-shot for the next native body transition after root bake. */
static struct { void *player; u32 active,budget; } blend;
/* A token for our native derivative only; never intercept an unrelated
   action116/135 or retain a player for later dereferencing. */
static struct { void *player; u32 action, active, buffer; float clock; } combo;
static IaiShape collision_shape;
/* Native cSetupParamPl_w07_100 methods, with its ABI prefix intact.
   Unique vptr scopes damage adjustment without retaining an object pointer. */
static u32 counter_vtable[8];
/* Flags live only inside the native guard evaluation at 2EE5AC..2EE5B0.
   Every native return passes our end hook; no persistent invulnerability.
   Only bits absent before this call are owned. Nested evaluations delegate. */
static struct { void *player; u32 depth, active, add_first, add_second; } guard;

/* An attack is witnessed at the native successful hit-data setup tail,
   not guessed from a weapon action ID. Scope every token to that action. */
static struct { void *player; u32 group, action, marked, pending, queued, budget; float clock; } follow;
/* Each snapshot belongs only to writes made by this module. Never dereference
   a remembered object until the engine itself supplies that object again. */
enum { WORDS=17 };
static const u32 fields[WORDS]={0x40,0x44,0x48,0x4c,0x50,0x54,0x58,0x5c,
    0x60,0x64,0x68,0x6c,0x34,0x13c8,0x13cc,0x13d4,0x1408};
typedef struct { void *part,*parent; u32 applied,before[WORDS],after[WORDS],flag_mask,flag_before,flag_after; } Saved;
static Saved saved[2];
#ifdef IAI_HOST_TEST
extern const u32 *test_clip(void *,u32);
extern void test_motion(void *,u32,float,float);
extern void test_helm_init(void *);
extern void test_idle_flags(void *,u32);
extern void test_attach(void *,void *,u32);
extern const float *test_bone_matrix(void *,u32);
extern void test_part_motion(void *,void *,u32,u32,u32,u32,float);
extern void test_part_disable(void *,void *,u32);
extern void test_part_blend(void *,u32,u32,u32,float,float,float);
extern void test_finish(void *,u32,u32,u32,u32);
extern u32 test_stick_yaw(void *);
extern const u8 *test_hit_entry(void *,u32);
extern void *test_hit_group(void *,u32);
extern void test_hit_prepare(void *,u32,u32,u32);
extern void test_guard_event(void *,u32);
extern void test_audio(void *,u32,int,int,u32,int);
extern void test_guard_protect(void *,u32,float);
extern u32 test_native_floor(void *,u32,u32,u32);
extern u32 test_spirit_level(void *);
extern void *test_target_actor(u32);
extern const float *test_target_matrix(void *,int);
extern u32 test_helm_queue(void *,const float *,u32,u32,u32);
extern void test_combo_request(void *,u32,u32,u32);
extern void test_level_add(void *,int);
extern void test_small_gauge(void *,u32);
extern u32 test_valor_active(void *);
extern void test_valor_threshold(void *,int);
extern float test_float_param(void *,u32);
extern u32 test_counter_queue(void *,const float *,u32,u32);
#endif

static const u32 *clip(void *p,u32 id) {
#ifdef IAI_HOST_TEST
    return test_clip(p,id);
#else
    return ((const u32 *(*)(void *,u32))(iai_main_base()+0x953b10))(p,id);
#endif
}
static int player_gate(void *p) {
    if (!p || U32(p,0)!=iai_main_base()+0x174cf64 || !(U8(p,0x13cc)&1)) return 0;
    void *owner=(void *)(uptr)U32(p,0x13c4);
    return owner && U8(owner,0x4d9)<=5 && U32(p,0x1470)==0;
}
static int idle(void *p) {
    return U16(p,0x2506)==1 && U16(p,0x250a)==0;
}
/* No remembered player is dereferenced: p must come from a current engine
   callback. Native 168994 assigns a per-initialization ticket; 1696C4 binds
   motion and 16919C invalidates on motion change. Keep a ticket tombstone
   after cancellation to suppress a geometry call already queued this frame. */
static int owns_hit(void *p) {
    return state.player==p && state.hit_registered && p &&
        U32(p,0x3020)==iai_main_base()+0x173eb2c &&
        U32(p,0x302c)==(uptr)p && U32(p,0x3094)==(state.hit_motion?state.hit_motion:5250u) &&
        U16(p,0x3054)==state.hit_ticket;
}
static void clear_hit(void *p) {
    if (owns_hit(p)) U8(p,0x3025)=0;
}
static void air_release(void *p) {
    if(!air.active || air.player!=p)return;
    air.active=0;
    if(U32(p,0x13c4)!=air.owner || U8(p,0x1054)!=air.actor){helm.speed_owned=0;return;}
    if(helm.speed_owned){
        u32 speed=U32(p,0x14b0);
        if(speed==0 || speed==0x40000000 || speed==0x404aaaab || speed==0x3f68ba2f)U32(p,0x14b0)=helm.speed0;
        speed=U32(p,0x50c);
        if(speed==0 || speed==0x40000000 || speed==0x404aaaab || speed==0x3f68ba2f)U32(p,0x50c)=helm.speed1;
        helm.speed_owned=0;
    }
    float y=F32(p,0x44),floor=F32(p,0x10f0),velocity=F32(p,0x1014);
    int grounded=__builtin_isfinite(y) && __builtin_isfinite(floor) &&
        __builtin_isfinite(velocity) && y-floor<=2 && velocity<=0;
    if(U8(p,0x2505)==2)U8(p,0x2505)=grounded?air.mode:2;
    if(U8(p,0x2ed3)==1)U8(p,0x2ed3)=grounded?air.flag:1;
    if(U32(p,0x1024)==0 || U32(p,0x1024)==0xbe68ba2fu){
        u32 gravity=air.gravity;
        if((gravity&0x7f800000)==0x7f800000 || (!grounded && !(gravity&0x7fffffff)))gravity=0xbe68ba2f;
        U32(p,0x1024)=gravity;
    }
    if(grounded || !__builtin_isfinite(velocity))F32(p,0x1014)=0;
    for(u32 k=0;k<4;++k)U32(p,0x1100+k*4)=0;
}
static void exit_turn(void *p);
static void stop(void *p) {
    foresight_release(p);
    if(sweep.player==p){sweep.valid=sweep.count=0;++sweep.epoch;}
#ifndef IAI_HOST_TEST
    if(state.active && state.player==p && state.slash>=3){
        union { float value; u32 bits; } clock={state.clock};
        u32 words[]={state.slash,U16(p,0x2506),U16(p,0x250a),U16(p,0x4b4),body_clip(),U32(p,0x2500),U32(p,0x500),U32(p,0x2534),U8(p,5),clock.bits,helm.launched};
        log_words("MHGU_LSO_P17_HELM_RELEASE",words,11);
    }
#endif
    if(state.active && state.player==p && !state.slash){reentry.player=p;reentry.quiet=18;reentry.reported=0;}
    clear_hit(p);air_release(p);
    if(state.slash==7 && helm.point_valid && helm.started && helm.sent>0 && helm.sent<7){
        helm.detached=1;helm.elapsed=F32(p,0x500);
    } else if(!helm.detached)helm.point_valid=0;
    state.active=0;state.reset_idle=1;state.suppress_chord=0;
    state.combo_ready=state.combo_buffer=state.rechain_buffer=0;
}
static int current(void *p) {
    if (!state.active || state.player!=p) return 0;
    if(state.slash>=8 && (U32(p,0x13c4)!=foresight.owner || U8(p,0x1054)!=foresight.actor)){
        stop(p);state.reset_idle=0;return 0;
    }
    if(state.slash==7 && (U32(p,0x13c4)!=helm.owner || U8(p,0x1054)!=helm.actor)){
        stop(p);state.reset_idle=0;return 0;
    }
    if(air.active && air.player==p && state.slash>=4 &&
       (U32(p,0x13c4)!=air.owner || U8(p,0x1054)!=air.actor)){
        stop(p);state.reset_idle=0;return 0;
    }
    if (!player_gate(p) || !idle(p) || (U32(p,0x2500)&1) ||
        (state.started && U16(p,0x4b4)!=body_clip()) ||
        !__builtin_isfinite(F32(p,0x2534)) || F32(p,0x2534)<state.clock) {
        stop(p); return 0;
    }
    return 1;
}
static void air_apply(void *p) {
    F32(p,0x1010)=F32(p,0x1018)=F32(p,0x1020)=F32(p,0x1028)=0;
    U8(p,0x2505)=2;U8(p,0x2ed3)=1;
    U32(p,0x1024)=state.slash==4?0:0xbe68ba2f;
    if(state.slash==4)F32(p,0x1014)=0;
    if(state.slash==5 && F32(p,0x1014)<=0){
        union { u32 bits; float value; } gravity={air.gravity};
        if(__builtin_isfinite(gravity.value) && gravity.value<0)U32(p,0x1024)=air.gravity;
    }
    if(state.slash==6){
        float frame=state.started?F32(p,0x500):0;
        U32(p,0x1024)=0;
        if(!helm.launched && frame>=8){
            /* Requested slight slowdown: stretch the approved19 updates by10%. */
            float speed=(F32(p,0x44)-F32(p,0x10f0))/((27-8)*1.1f);
            if(speed<6)speed=6;if(speed>96)speed=96;
          /* Charge only when the cut launches; a rejected/cancelled opening costs nothing. */
          if(helm.level){
#ifdef IAI_HOST_TEST
              test_level_add(p,-1);
#else
              ((void (*)(void *,int))(iai_main_base()+0x119ed98))(p,-1);
#endif
          }
            F32(p,0x1014)=-speed;helm.launched=1;
#ifndef IAI_HOST_TEST
            helm_phase_trace("MHGU_LSO_P17_DROP_LAUNCH",p);
#endif
        } else if(!helm.launched)F32(p,0x1014)=0;
        u32 speed=helm.launched?0x3f68ba2f:0x40000000; /* 1/1.1, same stretch as the travel. */
        if(frame>=27){
            F32(p,0x500)=27;speed=0;
#ifndef IAI_HOST_TEST
            if(!helm.frozen)helm_phase_trace("MHGU_LSO_P17_DROP_FREEZE",p);
#endif
            helm.frozen=1;
        }
        U32(p,0x14b0)=speed;U32(p,0x50c)=speed;
    }
}
static u32 native_floor(void *p,u32 a,u32 b,u32 c) {
#ifdef IAI_HOST_TEST
    return test_native_floor(p,a,b,c);
#else
    return ((u32 (*)(void *,u32,u32,u32))(iai_main_base()+0x2e846c))(p,a,b,c);
#endif
}
/* Author75C4 preserves velocities around the native terrain/collision query. */
u32 iai_air_physics(void *p,u32 a,u32 b,u32 c) {
    if(air.in_floor || !air.active || air.player!=p ||
       (state.slash<4 || state.slash>6) || !current(p))return native_floor(p,a,b,c);
    if(U32(p,0x13c4)!=air.owner || U8(p,0x1054)!=air.actor){stop(p);return native_floor(p,a,b,c);}
    air_apply(p);
    u32 velocity[6],pending=U32(p,0x2500);
    for(u32 k=0;k<6;++k)velocity[k]=U32(p,0x1010+k*4);
    air.in_floor=1;air.veto=0;
    u32 result=native_floor(p,a,b,c);air.in_floor=0;
    if(air.active && state.active && state.player==p && idle(p) &&
       U32(p,0x13c4)==air.owner && U8(p,0x1054)==air.actor && state.slash>=4 && state.slash<=6){
        if(air.veto)U32(p,0x2500)=(U32(p,0x2500)&~1u)|(pending&1);
        for(u32 k=0;k<6;++k)U32(p,0x1010+k*4)=velocity[k];
        air_apply(p);return 1;
    }
    air_release(p);return result;
}
static int guard_window(void *p) {
    return current(p) && state.started && (state.slash==1 || state.slash==2) && !state.parried &&
        F32(p,0x2534)-state.clock<300.0f &&
        !(U8(p,0x4b6)&4) && __builtin_isfinite(F32(p,0x500)) &&
        F32(p,0x500)>=0.0f && F32(p,0x500)<12.0f;
}
/* Payload BF2CD8..2D14 -> BF2F68 emits native event8 once on opening
   0<=frame<12, separate from success9. Switch119E394..3B8 independently
   confirms index8, cache2550, flag1 through virtual39C/API282624. */
static void cue_guard_window(void *p) {
    if (state.window_cued || !guard_window(p)) return;
    state.window_cued=1; /* Original marks before calling; never retry/loop. */
#ifdef IAI_HOST_TEST
    test_guard_event(p,8);
#else
    ((u32 (*)(void *,u32,void *,u32))(iai_main_base()+0x282624))
        (p,8,(u8 *)p+0x2550,1);
    iai_log_window_cue();
#endif
}
void iai_guard_begin(void *p) {
    ++guard.depth;
    if (guard.depth!=1 || !guard_window(p)) return;
    /* Never claim a native guard or another provider's second flag. */
    if ((U32(p,0x2510)&16) || (U32(p,0x2514)&2)) return;
    guard.player=p;guard.active=1;guard.add_first=16;guard.add_second=2;
    U32(p,0x2510)|=16;U32(p,0x2514)|=2;
}
void iai_guard_end(void *p) {
    if (!guard.depth) return;
    if (guard.depth==1 && guard.active && guard.player==p) {
        U32(p,0x2510)&=~guard.add_first;U32(p,0x2514)&=~guard.add_second;
        guard.active=0;guard.player=0;
    }
    --guard.depth;
}
/* CRO15AE8..15BE0 rewards. Switch fields/API independently verified by
   native119ED98,269338,2A33A8,29E70C. Never copy 3DS field offsets or
   virtual804: on Switch804 returns a float,810 is level-add. Called once
   after the scoped GP latch, never on ordinary draw/contact/extra cuts. */
static void reward_gp(void *p) {
    void *owner=(void *)(uptr)U32(p,0x13c4);
    if (U8(owner,0x4d9)!=5) {
#ifdef IAI_HOST_TEST
        test_level_add(p,1);
#else
        ((void (*)(void *,int))(iai_main_base()+0x119ed98))(p,1);
        iai_log_reward_level();
#endif
        return;
    }
#ifdef IAI_HOST_TEST
    u32 active=test_valor_active(p);
#else
    /* AAPCS r1 is unused by native269338; r2/r3 query its two flag words.
       r3 mask40000 reads player2844, NOT guard2514 nor 3DS23A8. */
    u32 active=((u32 (*)(void *,u32,u32,u32))(iai_main_base()+0x269338))(p,0,0,0x40000);
#endif
    int gauge=(int)U32(p,0x2790);
    if (!active) {
        /* Flat +50 as creator; passing +50 to native2A33A8 would apply
           skills/multipliers and reset27DC, unlike the creator's branch. */
        if (gauge<1000) {
            if (gauge<0) gauge=0;
            gauge+=50;if (gauge>1000) gauge=1000;
            U32(p,0x2790)=(u32)gauge;
        }
#ifdef IAI_HOST_TEST
        test_valor_threshold(p,0);
#else
        ((void (*)(void *,int))(iai_main_base()+0x2a33a8))(p,0);
        iai_log_reward_valor();
#endif
        return;
    }
    if (gauge<=0) return;
#ifdef IAI_HOST_TEST
    float param=test_float_param(p,79);
#else
    float param=((float (*)(void *,u32))(iai_main_base()+0x282828))(p,79);
#endif
    if (!__builtin_isfinite(param) || param<=0 || param>9000.0f) return;
    float before=F32(p,0x27dc);
    if (!__builtin_isfinite(before)) return;
    if (before<0) before=0;
    float cap=9000.0f+60.0f*param;
    if (before>=cap) return;
    float after=before+30.0f*param;
    F32(p,0x27dc)=after>cap?cap:after;
#ifndef IAI_HOST_TEST
    iai_log_reward_delay();
#endif
}
/* Native guard has already applied attack-type, angle and eligibility rules.
   LS virtual590 is the same reaction overridden by CRO514->7E40->9A00.
   Only this matching scoped call can latch a counter and skip recoil. */
int iai_guard_success(void *p) {
    if (!guard.active || guard.depth!=1 || guard.player!=p || !guard_window(p)) return 0;
    state.parried=1;
    if(state.slash==2)small_regen_start(p,1800);else reward_gp(p);
    /* Original CRO9A1C..38: remaining =20-current draw frame, passed to
       import318 after the successful GP. Switch native LS119D238..244
       matches CRO6B28..34 and independently identifies API2A6760.
       This API controls the bounded native protection timer2B40; it is
       NOT an animation hitstop. Mode0 preserves a longer native timer.
       Native29CF38/53AA84 decrements and clamps it; never refresh it here. */
    float remaining=20.0f-F32(p,0x500);
#ifdef IAI_HOST_TEST
    test_guard_protect(p,state.slash==2?1:0,remaining);
    test_guard_event(p,9);
#else
    ((void (*)(void *,u32,float))(iai_main_base()+0x2a6760))(p,state.slash==2?1:0,remaining);
    iai_log_gp();
    if(state.slash==2)iai_log_span("MHGU_LSO_P3_SMALL_GP",sizeof("MHGU_LSO_P3_SMALL_GP")-1);
    ((u32 (*)(void *,u32,void *,u32))(iai_main_base()+0x282624))
        (p,9,(u8 *)p+0x2550,1);
#endif
    return 1;
}
static void consume_combo(void *p) {
    U16(p,0x13f4)&=(u16)~0x880;
    U16(p,0x13f8)&=(u16)~0x880;
    U32(p,0x13fc)&=~0x880u;
    U32(p,0x1404)&=~0x880u;
}
static void consume_x(void *p) {
    U16(p,0x13f4)&=(u16)~0x20;U16(p,0x13f8)&=(u16)~0x20;
    U32(p,0x13fc)&=~0x20u;U32(p,0x1404)&=~0x20u;
}
static void consume_a(void *p) {
    U16(p,0x13f4)&=(u16)~0x100;U16(p,0x13f8)&=(u16)~0x100;
    U32(p,0x13fc)&=~0x100u;U32(p,0x1404)&=~0x100u;
}
static void consume_zr(void *p) {
    U16(p,0x13f4)&=(u16)~0x800;
    U16(p,0x13f8)&=(u16)~0x800;
    U32(p,0x13fc)&=~0x800u;
    U32(p,0x1404)&=~0x800u;
}
static void consume_helm(void *p) {
    U16(p,0x13f4)&=(u16)~0x810;U16(p,0x13f8)&=(u16)~0x810;
    U32(p,0x13fc)&=~0x810u;U32(p,0x1404)&=~0x810u;
}
static void consume_y(void *p) {
    U16(p,0x13f4)&=(u16)~0x10;U16(p,0x13f8)&=(u16)~0x10;
    U32(p,0x13fc)&=~0x10u;U32(p,0x1404)&=~0x10u;
}
static int attack_scope(void *p) {
    return player_gate(p) && U16(p,0x2506)==16 && U16(p,0x250a)<154 &&
        U8(p,5)!=0 && (U8(p,0x2749)&1) && (U16(p,0x4b4)>>8)==5 &&
        __builtin_isfinite(F32(p,0x2534));
}
/* Only the first three proven ground locomotion dispatch cases. No climb,
   airborne, item, damage or arbitrary common-action range is admitted. */
static int move_scope(void *p) {
    return player_gate(p) && U16(p,0x2506)==2 && U16(p,0x250a)<=2 &&
        U8(p,5)!=0 && (U8(p,0x2749)&1) && (U16(p,0x4b4)>>8)==5 &&
        __builtin_isfinite(F32(p,0x2534));
}
/* shortcut: only initialized roll6/phase2/clip28; widen after mapping other native dodges. */
static int roll_scope(void *p) {
    float frame=F32(p,0x500);
    return player_gate(p) && U16(p,0x2506)==1 && U16(p,0x250a)==6 &&
        U8(p,5)==2 && (U8(p,0x2749)&1) && U16(p,0x4b4)==0x1c &&
        !(U8(p,0x4b6)&4) && __builtin_isfinite(frame) && frame>=0 && frame<=4 &&
        __builtin_isfinite(F32(p,0x2534)) && !(U32(p,0x2500)&1);
}
/* The native sampler just copied this magnitude at 2A7B20/24. Zero only
   axis 0 for this owned sample: angles, camera axis and buttons survive.
   The next native input sample restores the physical stick automatically. */
static void hold_position(void *p) { U16(p,0x1410)=0; }
/* Sample before hold_position: magnitude is freshly copied by native input.
   27E608 classifies signed magnitude>=50 as moving. Use native 27F5D0 to
   include the active camera/reference basis instead of guessing angle math.
   Native command commits 2EA514/524 and 2F4A18/1C store a normalized angle
   to BOTH FEC (current) and 14A0 (target). Write once at accepted draw entry;
   there is no locomotion, continuous steering, or camera/position write. */
static void aim_draw(void *p) {
    if ((short)U16(p,0x1410)<50) return;
#ifdef IAI_HOST_TEST
    u32 yaw=test_stick_yaw(p)&0xffffu;
#else
    u32 yaw=((u32 (*)(void *))(iai_main_base()+0x27f5d0))(p)&0xffffu;
#endif
    U32(p,0xfec)=yaw;U32(p,0x14a0)=yaw;
}
/* The native hit setup supplies source and record (r6/r9 at2B7504).
   Never inspect a cached shell pointer. Its current native class and unique
   command vptr must both match; ordinary Sakura Slash remains untouched. */
void iai_mark_attack(void *p,void *source,void *record) {
    uptr base=iai_main_base();
    if (player_gate(p) && source && source!=p && record &&
        U32(source,0)==base+0x1760cb4 && U32(record,0)==base+0x173eb2c &&
        U32(record,0xc)==(uptr)source) {
        void *cmd=(void *)(uptr)U32(source,0x136c);
        if (cmd && U32(cmd,0)==(uptr)(counter_vtable+2) &&
            U32(cmd,4)==45 && U32(cmd,8)==2 && U32(cmd,0xc)==(uptr)p) {
            u32 v=U16(record,0x38);
            if (v==10) U16(record,0x38)=5;
            else if (v==40) U16(record,0x38)=20;
            else if (v==15) {
                u32 n=U32(cmd,0x24)+1;U32(cmd,0x24)=n;
                U16(record,0x38)=(n&1)?7:8;
            }
        }
    }
    if (!attack_scope(p)) return;
    if(reentry.player==p)reentry.quiet=0;
    if (follow.player!=p || follow.group!=16 || follow.action!=U16(p,0x250a) ||
        F32(p,0x2534)<follow.clock) follow.pending=0;
    follow.player=p;follow.group=16;follow.action=U16(p,0x250a);follow.marked=1;
    follow.clock=F32(p,0x2534);
}
static int witnessed(void *p) {
    return follow.player==p && follow.marked && follow.group==U16(p,0x2506) &&
        ((follow.group==16 && attack_scope(p)) || (follow.group==2 && move_scope(p))) &&
        follow.action==U16(p,0x250a) && F32(p,0x2534)>=follow.clock;
}
static int finish_for_stance(void *p) {
    const u32 *header=clip(p,0x5f9);
    if (!header || header[2]!=150 || header[3]!=90) return 0;
    /* Clear the token BEFORE this call: finish invokes the same LS override.
       Its nested request must delegate, never recursively intercept itself. */
    follow.marked=follow.pending=follow.queued=0;
    state.active=0;state.reset_idle=0;state.player=p;
    consume_combo(p);
#ifdef IAI_HOST_TEST
    test_finish(p,0,4,0,0);
#else
    ((void (*)(void *,u32,u32,u32,u32))(iai_main_base()+0x2b6e94))(p,0,4,0,0);
#endif
    if (player_gate(p) && idle(p) && (U8(p,0x2749)&1)) {
        follow.player=p;follow.queued=1;follow.budget=2;
    }
    return 1;
}

/* Called where the LS override was about to call native transition_prepare.
   Return 1 only after replacing an accepted native dodge by the full native
   finish API. Other requests still execute the exact original call. */
int iai_request(void *p,u32 group,u32 action,u32 options) {
    (void)options;
    /* Keep native A/X/walking from preempting the owned preparation; B still wins. */
    if(state.player==p && state.active && state.slash==3 && player_gate(p) && idle(p) &&
       (!state.started || U16(p,0x4b4)==0x5f0) && !(U16(p,0x13f4)&0x80) &&
       (group==1 || group==2 || group==16)){
        U32(p,0x2500)&=~1u;return 1;
    }
    /* Author486C vetoes only automatic requests made inside this floor query. */
    if(air.in_floor && air.active && air.player==p && state.player==p && state.active &&
       state.slash>=4 && state.slash<=6 && player_gate(p) &&
       (group==1 || group==2 || group==16 || (group==0x40 && action==0x21))){
        air.veto=1;U32(p,0x2500)&=~1u;return 1;
    }
    if(state.player==p && state.active && state.started && state.slash>=8 &&
       player_gate(p) && idle(p) && U16(p,0x4b4)==body_clip() &&
       U32(p,0x13c4)==foresight.owner && U8(p,0x1054)==foresight.actor &&
       !(U16(p,0x13f4)&0x80) && (group==1 || group==2 || group==16)){
        U32(p,0x2500)&=~1u;return 1;
    }
    /* Walking can already be queued when ZR binds F6, outside the floor query. */
    if(group==2 && action<=2 && air.active && air.player==p && state.player==p &&
       state.active && state.started && state.slash>=4 && state.slash<=6 &&
       player_gate(p) && idle(p) && U16(p,0x4b4)==body_clip() &&
       U32(p,0x13c4)==air.owner && U8(p,0x1054)==air.actor &&
       __builtin_isfinite(F32(p,0x2534)) && F32(p,0x2534)>=state.clock){
        hold_position(p);U32(p,0x2500)&=~1u;return 1;
    }
    /* 2831E0 sets pending2500 bit0 BEFORE this virtual; it is not a new action yet. */
    if(state.player==p && state.active && state.suppress_chord && !state.slash &&
       player_gate(p) && idle(p) && state.started &&
       U16(p,0x4b4)==(state.exiting?0x5fb:0x5f9) &&
       __builtin_isfinite(F32(p,0x2534)) && F32(p,0x2534)>=state.clock &&
       group==1 && (action==6 || action==7 || action==13 || action==14)){
        U32(p,0x2500)&=~1u;
#ifndef IAI_HOST_TEST
        if(!(state.blade_log_mask&8)){state.blade_log_mask|=8;iai_log_span("IAI_R38_CHORD_VETO",sizeof("IAI_R38_CHORD_VETO")-1);}
#endif
        return 1;
    }
    int own_derivative=combo.player==p && combo.active;
    if (combo.player==p) combo.active=combo.buffer=0;
    int accept=follow.pending && witnessed(p) && group==1 &&
        (action==6 || action==7 || action==13 || action==14);
    /* CRO9ACC..9B80 checks the held chord at an accepted native request, not hit setup. */
    int held_cancel=!accept && !own_derivative && group==1 && (action==6 || action==7) &&
        attack_scope(p) && F32(p,0x2534)>=0 && (U16(p,0x13f4)&0x880)==0x880 &&
        U16(p,0x4b4)!=0x5f9 && U16(p,0x4b4)!=0x5fa && U16(p,0x4b4)!=0x5fb;
    accept|=held_cancel;
    if (follow.player==p) { follow.marked=0;follow.pending=0;follow.queued=0; }
    if (!accept) {
        /* The native accepted-request boundary wins over this private idle.
           Deactivate our hit before native X/A/dodge/damage can run. */
        if (state.player==p && state.active) {
#ifndef IAI_HOST_TEST
            if(state.slash>=3){u32 words[]={group,action,options};log_words("MHGU_LSO_P17_REQUEST_RELEASE",words,3);}
#endif
            /* Hook runs BEFORE native transition cleanup/motion replacement.
               Ordinary X/A and common locomotion exits need the same root
               transfer as GP-only derivatives. Damage/foreign groups win. */
            if (group==1 || group==2 || group==16) exit_turn(p);
            stop(p);
        }
        return 0;
    }
    int result=finish_for_stance(p);
    if(held_cancel && result && follow.queued){
        if(reentry.player==p)reentry.quiet=reentry.reported=0;
#ifndef IAI_HOST_TEST
        iai_log_span("IAI_R38_NATIVE_CANCEL_COMMIT",sizeof("IAI_R38_NATIVE_CANCEL_COMMIT")-1);
#endif
    }
#ifndef IAI_HOST_TEST
    if (result && action==7 && follow.queued) iai_log_cancel7();
#endif
    return result;
}

/* Original CRO78D0..78EC bakes a half-turn on normal completion;
   payload BF2590..25DC bakes interpolated root yaw for cancel/rechain,
   and CRO96D4..9724 does it before GP derivatives. R21 only covered the
   last path. Never write an unknown cache or force facing every frame.
   Bake exactly once while the live callback still owns body5250; next
   native actions keep their normal steering rules. */
static void exit_turn(void *p) {
    if (state.player!=p || !state.active || !state.started || state.slash!=1 ||
        state.yaw_done || !player_gate(p) || !idle(p) || U16(p,0x4b4)!=0x5fa ||
        !__builtin_isfinite(F32(p,0x2534)) || F32(p,0x2534)<state.clock) return;
    float frame=F32(p,0x500);
    if (!__builtin_isfinite(frame) || frame<0) return;
    /* Match existing pose_F1 clamp. Native completion can overshoot176;
       use the final sample without reading a nonexistent sample177. */
    if (frame>176.0f) frame=176.0f;
    u32 index=(u32)frame;
    float first=iai_combo_yaw_tables[index];
    float next=index==176?first:iai_combo_yaw_tables[index+1];
    float delta=first+(next-first)*(frame-(float)index);
    u32 yaw=(U32(p,0xfec)+(int)delta)&0xffffu;
    state.yaw_done=1;blend.player=p;blend.active=1;blend.budget=2;
    U32(p,0xfec)=yaw;U32(p,0x14a0)=yaw;
#ifndef IAI_HOST_TEST
    iai_log_yaw_exit();
#endif
}
/* Entry53A164 is the native body-motion wrapper, r0=player,r1=motion,
   s0=blend. Never pick/replace the native clip; only the next bank5 clip
   leaving our F1 may skip an old-root crossfade. No persistent override. */
int iai_motion_blend(void *p,u32 motion) {
    if (!blend.active || blend.player!=p) return 0;
    if (!blend.budget || !player_gate(p) || U16(p,0x4b4)!=0x5fa ||
        (U16(p,0x2506)!=1 && U16(p,0x2506)!=2 && U16(p,0x2506)!=16)) {
        blend.active=0;return 0;
    }
    if (motion<5000 || motion>=6000 || motion==5250) return 0;
    blend.active=0;
#ifndef IAI_HOST_TEST
    iai_log_blend_skip();
#endif
    return 1;
}
static int combo_request(void *p,u32 action) {
    clear_hit(p);state.active=0;state.reset_idle=0;
    state.combo_ready=state.combo_buffer=state.rechain_buffer=0;
    if (follow.player==p) follow.marked=follow.pending=follow.queued=0;
    u32 options=action==119?0x1000:0x8000;
    if (action==119) consume_x(p);else consume_zr(p);
#ifndef IAI_HOST_TEST
    if (action==119) iai_log_alt_request();
#endif
#ifdef IAI_HOST_TEST
    test_combo_request(p,16,action,options);
#else
    ((void (*)(void *,u32,u32,u32))(iai_main_base()+0x2831e0))(p,16,action,options);
#endif
    int accepted=player_gate(p) && U16(p,0x2506)==16 && U16(p,0x250a)==action;
#ifndef IAI_HOST_TEST
    if (accepted) { if (action==119) iai_log_alt_accepted();else if (action==117) iai_log_combo_second();else iai_log_combo_first(); }
#endif
    return accepted;
}
static int combo_first(void *p,u32 held,u32 previous,u32 pressed) {
    if (state.slash!=1 || !state.started || !state.parried || (held&0x80) ||
        (U8(p,0x4b6)&4) || F32(p,0x2534)-state.clock>=300.0f) return 0;
    float frame=F32(p,0x500);
    if (!__builtin_isfinite(frame) || frame<54.0f || frame>=176.0f) return 0;
    if ((held&0x800) && !(previous&0x800) && (pressed&0x800)) state.combo_buffer|=1;
    /* CRO96B0/9760/9764: alternate original action119, options1000.
       Expose the branch on verified Switch X(sPad20), not raw1000.
       Consume a buffered X immediately so native X cannot preempt it. */
    if ((held&0x20) && !(previous&0x20) && (pressed&0x20)) state.combo_buffer|=2;
    if (state.combo_buffer&2) consume_x(p);
    if (!state.combo_ready) { state.combo_ready=1;return 0; }
    if (!state.combo_buffer) return 0;
    u32 action=(state.combo_buffer&1)?((short)U16(p,0x1410)>=50?135:116):119;
    exit_turn(p);
    if (combo_request(p,action) && action!=119) {
        combo.player=p;combo.action=action;combo.active=1;combo.buffer=0;
        combo.clock=F32(p,0x2534);
    }
    return 1;
}
/* CRO9900/99C8: our GP derivative buffers ZR ->117 or ZR+B ->F0.
   Both wait for recovery40 in the proven phase3/5B9 window30..69.
   A separate B or a foreign/pending transition wins over the buffer. */
static int combo_native(void *p,u32 held,u32 previous,u32 pressed) {
    if (!combo.active || combo.player!=p) return 0;
    if (!player_gate(p) || U16(p,0x2506)!=16 || U16(p,0x250a)!=combo.action ||
        !(U8(p,0x2749)&1) || (U32(p,0x2500)&1) ||
        !__builtin_isfinite(F32(p,0x2534)) || F32(p,0x2534)<combo.clock ||
        F32(p,0x2534)-combo.clock>=300.0f || ((held&0x80) && (held&0x880)!=0x880)) {
        combo.active=combo.buffer=0;return 0;
    }
    if (U8(p,5)!=3 || U16(p,0x4b4)!=0x5b9 || (U8(p,0x4b6)&4)) return 0;
    float frame=F32(p,0x500);
    if (!__builtin_isfinite(frame) || frame<0 || frame>=70.0f) { combo.active=combo.buffer=0;return 0; }
    if (frame<30.0f) return 0;
    int chord=(held&0x880)==0x880;
    if(chord && (previous&0x880)!=0x880 && (pressed&0x880) && !(combo.buffer&2)){
        const u32 *header=clip(p,0x5f9);
        if(!header || header[2]!=150 || header[3]!=90){combo.active=combo.buffer=0;return 0;}
        combo.buffer=2; /* Creator clears Strong Spirit's bit when choosing the stance. */
#ifndef IAI_HOST_TEST
        iai_log_span("IAI_R38_DERIVATIVE_SHEATH_BUFFER",sizeof("IAI_R38_DERIVATIVE_SHEATH_BUFFER")-1);
#endif
    }
    if(chord && !(combo.buffer&2)){combo.active=combo.buffer=0;return 0;}
    if(!chord && (held&0x800) && !(previous&0x800) && (pressed&0x800))combo.buffer|=1;
    if (!combo.buffer) return 0;
    if(combo.buffer&2){
        if(frame<40.0f){consume_combo(p);return 1;}
        combo.active=combo.buffer=0;
        int accepted=finish_for_stance(p);
#ifndef IAI_HOST_TEST
        if(accepted && follow.queued)iai_log_span("IAI_R38_DERIVATIVE_SHEATH_COMMIT",sizeof("IAI_R38_DERIVATIVE_SHEATH_COMMIT")-1);
#endif
        return accepted;
    }
    if (frame<40.0f) { consume_zr(p);return 1; }
    combo.active=combo.buffer=0;
    combo_request(p,117);
    return 1;
}

/* Reverse the authored hip sheathe; no native back-draw or attack state. */
static float exit_source_frame(float frame) {
    if(frame<=20)return 90-2*frame;
    if(frame>=40)return 30;
    float elapsed=frame-20;
    return 50-2*elapsed+elapsed*elapsed/20;
}
static int stance_timeout(void *p) {
    if(state.slash || state.exiting || !state.started || !(U8(p,0x2749)&1))return 0;
    const u32 *header=clip(p,0x5fb);
    if(!header || header[2]!=71 || header[3]!=0xffffffffu)return 0;
    hold_position(p);state.exiting=1;state.started=0;state.clock=F32(p,0x2534);state.audio_mask=0;
    state.combo_ready=state.combo_buffer=state.rechain_buffer=0;
    if(follow.player==p)follow.marked=follow.pending=follow.queued=0;
    if((U16(p,0x13f4)&0x880)==0x880)consume_combo(p);
#ifndef IAI_HOST_TEST
    iai_log_span("IAI_R38_STANCE_EXIT",19);
#endif
    return 1;
}
static void small_regen_tick(void *p) {
    if(small_regen.remaining<=0 || small_regen.player!=p || !player_gate(p))return;
    if(small_regen.owner!=U32(p,0x13c4) || small_regen.actor!=U8(p,0x1054)){small_regen.remaining=0;return;}
#ifndef IAI_HOST_TEST
    void *quest=(void *)(uptr)U32((void *)iai_main_base(),0x188ad90);
    if(!quest || U32(quest,0)!=iai_main_base()+0x1753b30 || U8(quest,0x50)!=2){small_regen.remaining=0;return;}
#endif
    float now=F32(p,0x2534),delta=now-small_regen.clock;
    small_regen.clock=now;
    if(!__builtin_isfinite(now) || !__builtin_isfinite(delta) || delta<=0 || delta>60 || small_regen.remaining<=0)return;
    if(delta>small_regen.remaining)delta=small_regen.remaining;
    small_regen.remaining-=delta;small_regen.carry+=delta;
    u32 amount=(u32)(small_regen.carry/6.0f);
    small_regen.carry-=6.0f*(float)amount;
    if(amount){
#ifdef IAI_HOST_TEST
        extern void test_small_gauge(void *,u32);
        test_small_gauge(p,amount);
#else
        ((void (*)(void *,u32,int))(iai_main_base()+0x2a309c))(p,amount,-1);
#endif
    }
}
static int helm_thrust_commit(void *p) {
    const u32 *header=clip(p,0x5f0);
    if(!header || header[2]!=245 || header[3]!=0xffffffffu)return 0;
    aim_draw(p);clear_hit(p);
    helm_move.active=0;helm_natural_landing=0;
    if(state.player!=p)state.hit_registered=0;
    state.player=p;state.active=1;state.started=state.reset_idle=0;
    state.slash=3;state.exiting=0;state.released=1;state.zr_released=0;
    state.hit_attempted=state.parried=state.counter_attempted=0;
    state.combo_ready=state.combo_buffer=state.yaw_done=state.rechain_buffer=0;
    state.window_cued=state.audio_mask=state.blade_log_mask=0;
    state.clock=F32(p,0x2534);follow.marked=follow.pending=0;
    hold_position(p);consume_helm(p);
#ifndef IAI_HOST_TEST
    iai_log_span("MHGU_LSO_P17_HELM_THRUST",sizeof("MHGU_LSO_P17_HELM_THRUST")-1);
#endif
    return 1;
}
static int helm_thrust_start(void *p,u32 held,u32 previous,u32 pressed) {
    if(state.active && state.player==p && state.slash==2){
        if(!current(p))return 0;
        consume_y(p);
        float frame=F32(p,0x500);
        if(!state.started || state.exiting || !__builtin_isfinite(frame) || frame<0 ||
           !(U8(p,0x2749)&1) || U8(p,0x2505)==2 || (U8(p,0x2515)&4))return 0;
        /* X may still be held; keep both Iai hit windows before the continuation. */
        if((held&0x9d0)==0x810 && (previous&0x810)!=0x810 && (pressed&0x810))state.combo_buffer|=4;
        if((state.combo_buffer&4) && frame>=36 && !(held&0x80) && !guard.depth){
            small_contact(p);consume_x(p);return helm_thrust_commit(p);
        }
        return 0;
    }
    /* shortcut: native attack cancels remain gated separately from locomotion. */
    if((held&0x9f0)!=0x810 || (previous&0x810)==0x810 || !(pressed&0x810) ||
       !(U8(p,0x2749)&1) || (U32(p,0x2500)&1) ||
       !__builtin_isfinite(F32(p,0x2534)) || !__builtin_isfinite(F32(p,0x500)) || F32(p,0x500)<0)return 0;
    int owned=current(p);
    /* A may arrive one poll after ZR selected the draw; no hit/GP can be stolen. */
    int stance=owned && !state.exiting && (!state.slash ||
        (state.slash==1 && !guard.depth && !state.parried && !state.hit_attempted &&
         (!state.started || F32(p,0x500)<=6)));
    int ready=!state.active && !combo.active;
    int early_attack=attack_scope(p) && U8(p,5)==1 && F32(p,0x500)<=6 &&
        !(U32(p,0x2510)&16) && !(U32(p,0x2514)&2);
    if(ready && (move_scope(p) || early_attack) && U8(p,0x2505)!=2 && !(U8(p,0x2515)&4)){
        const u32 *header=clip(p,0x5f0);
        if(!header || header[2]!=245 || header[3]!=0xffffffffu)return 0;
        aim_draw(p);
        if(!finish_for_stance(p))return 0;
        helm_move.player=p;helm_move.active=follow.queued;
        helm_move.owner=U32(p,0x13c4);helm_move.actor=U8(p,0x1054);consume_helm(p);return 1;
    }
    if(!stance && !(ready && idle(p) && U8(p,5)==1 && U16(p,0x4b4)==0x501))return 0;
    return helm_thrust_commit(p);
}
static int helm_dive_start(void *p,u32 held,u32 previous,u32 pressed);
static void helm_cuts_step(void *p,float frame);
#if !defined(IAI_HOST_TEST) && !defined(IAI_RELEASE)
static void log_words(const char *prefix,const u32 *words,u32 count) {
    char message[256];u32 at=0;
    while(prefix[at] && at<48){message[at]=prefix[at];++at;}
    if(count>22)count=22;
    for(u32 j=0;j<count && at+9<=sizeof(message);++j){
        message[at++]=' ';
        for(u32 k=0;k<8;++k){u32 n=(words[j]>>(28-k*4))&15;message[at++]=(char)(n<10?'0'+n:'A'+n-10);}
    }
    iai_log_span(message,at);
}
static void helm_phase_trace(const char *tag,void *p) {
    u32 words[]={U32(p,0x500),U32(p,0x44),U32(p,0x10f0),U32(p,0x1014),U32(p,0x1024),U32(p,0x50c),helm.level,U32(p,0x1c)};
    log_words(tag,words,8);
}
static void helm_input_trace(void *p) {
    static u32 loaded,seen;
    if(!loaded){loaded=1;iai_log_span("MHGU_LSO_P17_INPUT_READY",sizeof("MHGU_LSO_P17_INPUT_READY")-1);}
    if(!p || seen>=64)return;
    u32 held=U16(p,0x13f4),pressed=U16(p,0x13f8);
    if(!pressed || !((held|pressed)&0x800))return;
    if(U32(p,0)!=iai_main_base()+0x174cf64)return;
    ++seen;
    u32 index=0xffffffffu,raw=0;
    void *pad=(void *)(uptr)U32((void *)iai_main_base(),0x2126888);
    if(pad && !((uptr)pad&3)){
        index=U32(pad,0x6b60);
        if(index<9)raw=U32(pad,index*0xb50+0x1d0);
    }
    int gate=player_gate(p);
    const u32 *header=gate?clip(p,0x5f0):0;
    u32 words[]={held,U16(p,0x13f6),pressed,U32(p,0),U8(p,5),
        U16(p,0x2506),U16(p,0x250a),U16(p,0x4b4),U32(p,0x500),U32(p,0x2534),
        U32(p,0x2500),U8(p,0x2749),state.active|(state.started<<1)|(state.slash<<8),
        state.reset_idle,combo.active,follow.queued,(u32)gate,raw,index,
        header?header[2]:0,header?header[3]:0,header!=0};
    log_words("MHGU_LSO_P17_INPUT",words,sizeof(words)/sizeof(words[0]));
}
#endif
void iai_input(void *p) {
    int valid_player=player_gate(p);
#ifndef IAI_HOST_TEST
#ifndef IAI_RELEASE
    helm_input_trace(p);
#endif
    fx_tick(valid_player);
#endif
    if (blend.active && blend.player==p) {
        if (!blend.budget) blend.active=0;else --blend.budget;
    }
    if (!valid_player) {
        if(state.player==p)helm.point_valid=helm.detached=0;
        if(helm_move.player==p)helm_move.active=0;
        if(small_regen.player==p)small_regen.remaining=0;
        if (state.player==p && state.active) stop(p);
        if (follow.player==p) follow.marked=follow.pending=follow.queued=0;
        if (combo.player==p) combo.active=combo.buffer=0;
        return;
    }
    small_regen_tick(p);
    if(foresight.round && foresight.player==p &&
       (U16(p,0x2506)!=16 || U16(p,0x250a)!=30)){
        if(U32(p,0x13c4)==foresight.owner && U8(p,0x1054)==foresight.actor && U8(p,0x333d)==1)U8(p,0x333d)=(u8)foresight.round_before;
        foresight.round=0;
    }
    if(helm.detached){
        if(state.player!=p || helm.owner!=U32(p,0x13c4) || helm.actor!=U8(p,0x1054))helm.point_valid=helm.detached=0;
        else {
            float dt=F32(p,0x1c);
            if(__builtin_isfinite(dt) && dt>0 && dt<60){helm.elapsed+=dt;helm_cuts_step(p,helm.elapsed);}
        }
    }
    if (state.player && state.player!=p) { state.active=0;state.reset_idle=0;state.player=p; }
    u32 held=U16(p,0x13f4), pressed=U16(p,0x13f8), previous=U16(p,0x13f6);
    state.suppress_chord=0;
    if(helm_move.active && (helm_move.player!=p || !follow.queued ||
       helm_move.owner!=U32(p,0x13c4) || helm_move.actor!=U8(p,0x1054))){
        helm_move.active=0;follow.queued=0;
    }
    if(reentry.player==p && reentry.quiet){
        if(held&0x880)reentry.quiet=18;else --reentry.quiet;
        if(!state.active && (idle(p) || move_scope(p) || roll_scope(p)) && (held&0x880)==0x880){
            if(follow.player==p)follow.marked=follow.pending=follow.queued=0;
#ifndef IAI_HOST_TEST
            if(!reentry.reported){reentry.reported=1;iai_log_span("IAI_R38_REENTRY_LOCK",sizeof("IAI_R38_REENTRY_LOCK")-1);}
#endif
            consume_combo(p);return;
        }
    }
    if (combo_native(p,held,previous,pressed)) return;
    if (follow.player==p && follow.queued) {
        /* Let native idle initialize first. A different native action wins;
           a stalled initialization expires in two input observations. */
        if (!idle(p) || !follow.budget || !__builtin_isfinite(F32(p,0x2534))) {
            follow.queued=0;helm_move.active=0;return;
        }
        --follow.budget;
        /* Do not let the held stick immediately request walking again while
           the native idle initializes after an accepted movement cancel. */
        hold_position(p);
        if (U8(p,5)==1 && (U8(p,0x2749)&1) && U16(p,0x4b4)==0x501 && !(U32(p,0x2500)&1)) {
            if(helm_move.active && helm_move.player==p){u32 kind=helm_move.active;follow.queued=0;helm_move.active=0;if(kind==2)foresight_commit(p);else helm_thrust_commit(p);return;}
            follow.queued=0;state.player=p;state.active=1;state.started=0;
            state.reset_idle=0;state.released=!(held&0x80);state.clock=F32(p,0x2534);
            state.exiting=0;state.slash=0;state.zr_released=!(held&0x800);state.rechain_buffer=0;
            state.hit_attempted=state.hit_registered=0;
            if ((held&0x880)==0x880) consume_combo(p);
        }
        return;
    }
    if(foresight_start(p,held,previous,pressed) || helm_thrust_start(p,held,previous,pressed))return;
    if (!state.active && roll_scope(p) && (held&0x880)==0x880 &&
        (previous&0x880)!=0x880 && (pressed&0x880) && finish_for_stance(p)) {
#ifndef IAI_HOST_TEST
        if (follow.queued) iai_log_roll_entry();
#endif
        return;
    }
    if (current(p)) {
        if(state.slash==3){hold_position(p);consume_a(p);consume_x(p);consume_helm(p);}
        if(state.slash>=4 && state.slash<=6)hold_position(p);
        if(!state.slash && !state.exiting){consume_a(p);consume_y(p);}
        if(state.slash==1 && !state.parried && (!state.started || F32(p,0x500)<12))consume_a(p);
        if(state.slash>=8){
            hold_position(p);consume_a(p);
            if(state.slash==9 && !(held&0x80)){
                if(foresight.evaded && (held&0x800) && !(previous&0x800) && (pressed&0x800))foresight.buffer|=1;
                if(F32(p,0x500)>=30 && !(held&0x800)){
                    if((pressed&held&0x20) && !(previous&0x20))foresight.buffer|=2;
                    if((pressed&held&0x100) && !(previous&0x100))foresight.buffer|=4;
                }
                foresight_tick(p);
                if(!state.active)return;
            }
        }
        if(state.slash==4 || state.slash==5){
            u32 released=state.zr_released;state.zr_released=!(held&0x800);
            if(state.slash==5 && state.started && released && helm_dive_start(p,held,previous,pressed))return;
        }
        if (!(held&0x80)) state.released=1;
        if (!(held&0x800)) state.zr_released=1;
        if(state.exiting){
            if(state.released && (pressed&0x80) && (held&0x880)!=0x880){stop(p);return;}
            hold_position(p);
            if((held&0x880)==0x880){state.suppress_chord=1;consume_combo(p);}
            else if(held&0x800)consume_zr(p);
            return;
        }
        if(!state.slash && F32(p,0x2534)-state.clock>=300.0f &&
            !(state.released && (pressed&0x80) && (held&0x880)!=0x880)) {
            if(!stance_timeout(p))stop(p);return;
        }
        /* A repeated stance chord is not a separate B dodge or a new entry. */
        if(!state.slash && (held&0x880)==0x880 && !(pressed&0x60)) {
            state.suppress_chord=1;hold_position(p);consume_combo(p);return;
        }
        /* Creator BF2528..2558 stores the rechain request BEFORE testing
           frame20 at BF2564..2574. R24 interrupted the strike too early.
           User-requested adaptation: wait for the proven recovery boundary54
           used by the original GP derivatives, not the old cancel minimum20.
           Keep a private one-draw latch; holding/releasing needs no second
           press. No native B injection or forced action ID is used. */
        float chain_frame=F32(p,0x500);
        int fresh_chain=(held&0x880)==0x880 && (previous&0x880)!=0x880 && (pressed&0x880);
        int chain_scope=(state.slash==1 || state.slash==2) && state.started &&
            __builtin_isfinite(chain_frame) && chain_frame>=0.0f && chain_frame<(state.slash==2?138.0f:176.0f) &&
            !(U8(p,0x4b6)&4) && F32(p,0x2534)-state.clock<300.0f;
        if (chain_scope && fresh_chain && !state.rechain_buffer) {
            const u32 *header=clip(p,0x5f9);
            if (header && header[2]==150 && header[3]==90) {
                state.rechain_buffer=1;
#ifndef IAI_HOST_TEST
                if (chain_frame<54.0f) iai_log_chain_buffer();
#endif
            }
        }
        /* A later, separate B wins over a queued chord. Preserve the
           already validated immediate native B cancellation behavior. */
        if (state.rechain_buffer && (pressed&0x80) && (held&0x880)!=0x880)
            state.rechain_buffer=0;
        if (state.rechain_buffer && chain_scope) {
            const u32 *header=clip(p,0x5f9);
            if (!header || header[2]!=150 || header[3]!=90) {
                state.rechain_buffer=0;
            } else if (chain_frame>=54.0f) {
                exit_turn(p);clear_hit(p);state.rechain_buffer=0;
                state.exiting=0;state.slash=0;state.started=0;state.released=0;
                state.zr_released=0;state.hit_attempted=0;
                state.clock=F32(p,0x2534);
#ifndef IAI_HOST_TEST
                iai_log_chain_commit();
#endif
                hold_position(p);consume_combo(p);return;
            } else {
                hold_position(p);consume_combo(p);return;
            }
        } else if (!chain_scope) state.rechain_buffer=0;
        if ((state.released && (pressed&0x80)) || F32(p,0x2534)-state.clock>=300.0f) {
            exit_turn(p);stop(p); return; /* Leave B intact for the native dodge. */
        }
        if (combo_first(p,held,previous,pressed)) return;
        if (!state.slash && state.started && (pressed&0x20) &&
            !(previous&0x20) && __builtin_isfinite(F32(p,0x500)) && F32(p,0x500)>=0) {
            const u32 *header=clip(p,0x5f5);
            if(header && header[2]==138 && header[3]==0xffffffffu){
                aim_draw(p);state.slash=2;state.started=0;state.exiting=0;
                state.hit_attempted=0;state.parried=state.counter_attempted=0;
                state.combo_ready=state.combo_buffer=state.yaw_done=state.rechain_buffer=0;
                state.window_cued=state.audio_mask=state.blade_log_mask=0;
                state.clock=F32(p,0x2534);consume_x(p);consume_combo(p);
#ifndef IAI_HOST_TEST
                iai_log_span("MHGU_LSO_P3_SMALL_IAI",sizeof("MHGU_LSO_P3_SMALL_IAI")-1);
#endif
            }
        }
        /* ZR is now reserved for the original draw animation in this owned
           stance. Require release after entry, then a fresh edge at >=72,
           exactly the readiness threshold in original payload BF2C68..B4.
           R12 prepares the normal hit later, at frame12; no parry flag is set. */
        if (!state.slash && state.started && state.zr_released && !(held&0x190) &&
            (held&0x800) && !(previous&0x800) && (pressed&0x800) &&
            __builtin_isfinite(F32(p,0x500)) && F32(p,0x500)>=72.0f) {
            const u32 *header=clip(p,0x5fa);
            if (header && header[2]==176 && header[3]==0xffffffffu) {
                aim_draw(p);
                state.exiting=0;state.slash=1;state.started=0;state.zr_released=0;state.parried=0;state.counter_attempted=0;state.combo_ready=state.combo_buffer=state.yaw_done=state.rechain_buffer=state.window_cued=state.audio_mask=0;state.blade_log_mask=0;
                state.hit_attempted=0; /* Retain cancellation ticket until replaced. */
                state.clock=F32(p,0x2534);
            }
        }
        hold_position(p);
        if(state.slash==2)consume_x(p);
        if(state.slash>=3)consume_helm(p);
        if (held&0x800) consume_zr(p);
        if ((held&0x880)==0x880) consume_combo(p);
        return;
    }
    u32 edge=(held&0x880)==0x880 && (previous&0x880)!=0x880 && (pressed&0x880);
    /* Restore the R6 free idle entry, retaining its exact initialized motion
       and draw-state checks. Stop walking before native command evaluation. */
    if (!state.reset_idle && idle(p) && U8(p,5)==1 && (U8(p,0x2749)&1) &&
        U16(p,0x4b4)==0x501 && !(U32(p,0x2500)&1) && edge &&
        __builtin_isfinite(F32(p,0x2534))) {
        const u32 *header=clip(p,0x5f9);
        if (!header || header[2]!=150 || header[3]!=90) return;
        if (follow.player==p) follow.marked=follow.pending=follow.queued=0;
        state.player=p;state.active=1;state.started=0;state.released=0;
        state.exiting=0;state.slash=0;state.zr_released=0;state.rechain_buffer=0;
        state.hit_attempted=state.hit_registered=0;
        state.reset_idle=0;state.clock=F32(p,0x2534);
        hold_position(p);consume_combo(p);return;
    }
    /* Moving entry needs no prior attack, but still waits for the engine's
       accepted B cancel and full cleanup. Never force the internal setter. */
    if (edge && move_scope(p) && !(U32(p,0x2500)&1)) {
        follow.player=p;follow.group=2;follow.action=U16(p,0x250a);
        follow.clock=F32(p,0x2534);follow.marked=1;follow.pending=1;
    }
    if (!witnessed(p) || (U32(p,0x2500)&1)) {
        if (follow.player==p) follow.marked=follow.pending=0;
        return;
    }
    if ((held&0x880)==0x880 && (previous&0x880)!=0x880 && (pressed&0x880)) follow.pending=1;
    /* Keep native B, its buffering and cancel-window checks. Remove only ZR
       while this exact attack or movement owns the request. */
    if (follow.pending) consume_zr(p);
}

static int hit_resources(void *p) {
    void *hdp=(void *)(uptr)U32(p,0x3198),*hts=(void *)(uptr)U32(p,0x319c);
    uptr base=iai_main_base();
    if (!hdp || !hts || U32(hdp,0)!=base+0x1747ca0 ||
        U32(hts,0)!=base+0x173ebb8) return 0;
    void *raw=(void *)(uptr)U32(hdp,0x64);
    if (!raw || U32(raw,0)!=0x00504448 || U32(raw,4)!=0x20160209 ||
        (U32(raw,8)!=85 && U32(raw,8)!=94 && U32(raw,8)!=96)) return 0;
#ifdef IAI_HOST_TEST
    const u8 *entry=test_hit_entry(p,30);
#else
    const u8 *entry=((const u8 *(*)(void *,u32))(base+0x2a8a88))(p,30);
#endif
    if (!entry || U32(entry,0)!=0x000c001a || U16(entry,4)!=42 || U8(entry,0x20)!=0) return 0;
    raw=(void *)(uptr)U32(hts,0x64);
    if (!raw || U32(raw,0)!=0x00535448 || U32(raw,4)!=0x20131015 || U32(raw,8)!=3) return 0;
#ifdef IAI_HOST_TEST
    void *group=test_hit_group(hts,1);
#else
    void *group=((void *(*)(void *,u32))(base+0x16a4e4))(hts,1);
#endif
    return group && U32(group,4)==1;
}
static void fill_counter(void *cmd,void *p,const float *pos,u32 target_id,u32 part_id) {
    for (u32 k=0;k<12;++k) U32(cmd,k*4)=0;
    U32(cmd,0)=(uptr)(counter_vtable+2);U32(cmd,4)=45;U32(cmd,8)=2;
    U32(cmd,0xc)=(uptr)p;
    for (u32 k=0;k<3;++k) F32(cmd,0x10+k*4)=pos[k];
    U16(cmd,0x20)=target_id;U16(cmd,0x22)=part_id;
}
static void foresight_release(void *p) {
    if(foresight.player!=p || !foresight.protect)return;
    foresight.protect=0;
    if(!player_gate(p) || U32(p,0x13c4)!=foresight.owner || U8(p,0x1054)!=foresight.actor)return;
    if(__builtin_isfinite(F32(p,0x2b40)) && F32(p,0x2b40)>0 && F32(p,0x2b40)<=24){
#ifdef IAI_HOST_TEST
        test_guard_protect(p,1,0);
#else
        ((void (*)(void *,u32,float))(iai_main_base()+0x2a6760))(p,1,0);
#endif
    }
}
static int foresight_commit(void *p) {
    const u32 *back=clip(p,0x5f6),*returning=clip(p,0x5fe);
    if(!back || !returning || back[2]!=141 || returning[2]!=140 || back[3]!=0xffffffffu || returning[3]!=0xffffffffu || !hit_resources(p))return 0;
    void *raw=(void *)(uptr)U32((void *)(uptr)U32(p,0x3198),0x64);
    if(U32(raw,8)!=96)return 0;
    aim_draw(p);clear_hit(p);
    for(u32 k=0;k<sizeof(state)/4;++k)U32(&state,k*4)=0;
    state.player=p;state.active=state.released=1;state.slash=8;state.clock=F32(p,0x2534);
    foresight.player=p;foresight.owner=U32(p,0x13c4);foresight.actor=U8(p,0x1054);
    foresight.evaded=foresight.hit=foresight.buffer=foresight.protect=0;foresight.yaw=U32(p,0xfec);
    follow.marked=follow.pending=0;U32(p,0x2780)=0;
    hold_position(p);consume_a(p);consume_zr(p);
    if(!helm_motion_start(p))return 0;
    foresight.protect=1;
#ifdef IAI_HOST_TEST
    test_guard_protect(p,1,24);
#else
    ((void (*)(void *,u32,float))(iai_main_base()+0x2a6760))(p,1,24);
    iai_log_span("MHGU_LSO_P17_FORESIGHT",sizeof("MHGU_LSO_P17_FORESIGHT")-1);
#endif
    return 1;
}
static int foresight_start(void *p,u32 held,u32 previous,u32 pressed) {
    float frame=F32(p,0x500);
    int early=state.active && state.player==p && state.slash==3 &&
        !state.hit_attempted && !state.exiting && __builtin_isfinite(frame) && frame>=0 &&
        (!state.started || (U16(p,0x4b4)==0x5f0 && frame<=6));
    u32 chord=early?(held&~0x10u):held; /* Y may still be held from ZR+Y. */
    if((chord&0x9f0)!=0x900 || (previous&0x900)==0x900 || !(pressed&0x900) ||
       (state.active && !early) || combo.active || U8(p,0x2505)==2 || (U8(p,0x2515)&4) ||
       !(U8(p,0x2749)&1) || (U32(p,0x2500)&1) || !__builtin_isfinite(F32(p,0x2534)))return 0;
    const u32 *back=clip(p,0x5f6),*returning=clip(p,0x5fe);
    if(!back || !returning || back[2]!=141 || returning[2]!=140 || back[3]!=0xffffffffu || returning[3]!=0xffffffffu)return 0;
    if(early){
        if(!current(p))return 0;
        consume_y(p);return foresight_commit(p);
    }
    if(move_scope(p) || attack_scope(p)){
        if(!clip(p,0x5f6) || !clip(p,0x5fe) || !finish_for_stance(p))return 0;
        helm_move.player=p;helm_move.active=follow.queued?2:0;
        helm_move.owner=U32(p,0x13c4);helm_move.actor=U8(p,0x1054);consume_a(p);consume_zr(p);return 1;
    }
    if(!idle(p) || U16(p,0x4b4)!=0x501)return 0;
    return foresight_commit(p);
}
static void foresight_success(void *p) {
    if(foresight.evaded)return;
    foresight.evaded=1;
#ifdef IAI_HOST_TEST
    test_guard_event(p,9);
#else
    ((u32 (*)(void *,u32,void *,u32))(iai_main_base()+0x282624))(p,9,(u8 *)p+0x2550,1);
    iai_log_span("MHGU_LSO_P17_FORESIGHT_EVADE",sizeof("MHGU_LSO_P17_FORESIGHT_EVADE")-1);
#endif
}
static u32 foresight_probe_state[27];
u32 *foresight_probe_ctx=foresight_probe_state;
u32 foresight_probe_gate(void *p) {
    return p && state.player==p && foresight.player==p && state.active && state.started && state.slash==8 &&
        player_gate(p) && idle(p) && U16(p,0x4b4)==0x5f6 &&
        U32(p,0x13c4)==foresight.owner && U8(p,0x1054)==foresight.actor;
}
float foresight_probe_frame(void *p) { return F32(p,0x500); }
void foresight_probe_success(void *p) {
    if(foresight_probe_gate(p) && __builtin_isfinite(F32(p,0x500)) && F32(p,0x500)>=0 && F32(p,0x500)<24)foresight_success(p);
}
static void foresight_probe(void *record,float *shape,u32 kind) {
#ifndef IAI_HOST_TEST
    void *p=state.player;
    if(!foresight_probe_gate(p) || foresight.evaded)return;
    foresight_probe_state[0]=(uptr)p;foresight_probe_state[1]=foresight.actor;
    foresight_probe_state[12]=state.started;foresight_probe_state[26]=0;
    extern void foresight_probe_raw(void *,float *,u32);
    foresight_probe_raw(record,shape,kind);
#else
    (void)record;(void)shape;(void)kind;
#endif
}
u32 iai_foresight_damage(void *p) {
    if(!p || state.player!=p || foresight.player!=p || !state.active || !state.started ||
       (state.slash!=8 && state.slash!=9) || !player_gate(p) || !idle(p) ||
       U32(p,0x13c4)!=foresight.owner || U8(p,0x1054)!=foresight.actor || U16(p,0x4b4)!=body_clip())return 0;
    float frame=F32(p,0x500);
    if(!__builtin_isfinite(frame) || frame<0 || frame>=1000000 ||
       (state.slash==8?(frame>=24 && !foresight.evaded):(!foresight.evaded || frame>=12)) ||
       !U8(p,0x31e0) || (short)U16(p,0x321c)<0)return 0;
    switch(U8(p,0x3216)){
        case 0x19:case 0x1a:case 0x1b:case 0x1c:case 0x1d:case 0x3a:case 0x43:case 0x44:case 0x46:return 0;
    }
    if(state.slash==8 && frame<24)foresight_success(p);
    U8(p,0x31e0)=0;return 1;
}
static void foresight_tick(void *p) {
    if(state.slash!=9 || !state.started)return;
    float frame=F32(p,0x500);
    if(foresight.evaded && !foresight.hit && (state.hit_attempted&1) && owns_hit(p) &&
       U8(p,0x30d4)==1 && U32(p,0x30d0) && U32(p,0x30cc) && U32((void *)(uptr)U32(p,0x30d0),0x1428)){
        foresight.hit=1;
#ifdef IAI_HOST_TEST
        test_small_gauge(p,100);
#else
        ((void (*)(void *,u32,int))(iai_main_base()+0x2a309c))(p,100,-1);
        iai_log_span("MHGU_LSO_P17_FORESIGHT_CONTACT",sizeof("MHGU_LSO_P17_FORESIGHT_CONTACT")-1);
#endif
    }
    if(frame>=30 && foresight.evaded && foresight.hit && (foresight.buffer&1)){
        foresight_release(p);
        if(combo_request(p,30)){
            foresight.round_before=U8(p,0x333d);U8(p,0x333d)=1;foresight.round=1;
#ifndef IAI_HOST_TEST
            iai_log_span("MHGU_LSO_P17_ROUNDSLASH",sizeof("MHGU_LSO_P17_ROUNDSLASH")-1);
#endif
        }
        return;
    }
    if(frame>=36 && (foresight.buffer&6)){
        u32 action=(foresight.buffer&6)==6?8:(foresight.buffer&4)?4:0;
        foresight_release(p);combo_request(p,action);return;
    }
    if(!__builtin_isfinite(frame) || frame<21 || frame>=30){clear_hit(p);return;}
    if(state.hit_attempted&1)return;
    state.hit_attempted|=1;
    if(U32(p,0x3020)!=iai_main_base()+0x173eb2c || U8(p,0x3025) || !hit_resources(p))return;
    u32 index=foresight.evaded?94:95;U8(p,0x30d4)=0;
#ifdef IAI_HOST_TEST
    test_hit_prepare(p,0,index,0);
#else
    ((void (*)(void *,u32,u32,u32))(iai_main_base()+0x2b7240))(p,0,index,0);
#endif
    if(U8(p,0x3025)!=1 || U32(p,0x302c)!=(uptr)p || U32(p,0x3094)!=5254)return;
    state.hit_ticket=U16(p,0x3054);state.hit_registered=1;state.hit_motion=5254;
    F32(p,0x307c)=0;F32(p,0x3080)=9;U32(p,0x30a8)=1;
}
static int helm_dive_start(void *p,u32 held,u32 previous,u32 pressed) {
    float frame=F32(p,0x500),height=F32(p,0x44)-F32(p,0x10f0);
    float velocity=F32(p,0x1014);
    if(!(held&0x800) || (previous&0x800) || !(pressed&0x800) || (held&0x80) ||
       !__builtin_isfinite(frame) || frame<4 || !__builtin_isfinite(height) || height<200 ||
       !__builtin_isfinite(velocity) || velocity>2*.22727272f)return 0;
    const u32 *fall=clip(p,0x5f3),*land=clip(p,0x5f4);
    if(!fall || !land || fall[2]!=115 || land[2]!=139 || fall[3]!=0xffffffffu || land[3]!=0xffffffffu || !hit_resources(p))return 0;
    void *raw=(void *)(uptr)U32((void *)(uptr)U32(p,0x3198),0x64);
    if((U32(raw,8)!=94 && U32(raw,8)!=96))return 0;
#ifdef IAI_HOST_TEST
    u32 level=test_spirit_level(p);
#else
    u32 level=((u32 (*)(void *))(iai_main_base()+0x119ed80))(p);
#endif
    if(level>5)return 0;
    if(level>3)level=3; /* Author clamps Valor's extra levels before selecting the damage row. */
    clear_hit(p);helm.level=level;helm.launched=helm.frozen=0;
    sweep.valid=sweep.count=sweep_logged=0;++sweep.epoch;
    helm.speed0=U32(p,0x14b0);helm.speed1=U32(p,0x50c);helm.speed_owned=1;
    helm.point_valid=helm.captured=helm.sent=helm.started=helm.detached=0;
    helm.owner=U32(p,0x13c4);helm.actor=U8(p,0x1054);
    state.slash=6;state.started=0;state.hit_attempted=0;state.clock=F32(p,0x2534);state.audio_mask=0;
#ifndef IAI_HOST_TEST
    fx_trail_reset();
#endif
    for(u32 k=0;k<4;++k)U32(p,0x1100+k*4)=0;
    air_apply(p);consume_zr(p);
#ifndef IAI_HOST_TEST
    iai_log_span("MHGU_LSO_P17_DESCENT",sizeof("MHGU_LSO_P17_DESCENT")-1);
#endif
    return helm_motion_start(p);
}
static void *helm_target(u32 id) {
#ifdef IAI_HOST_TEST
    return test_target_actor(id);
#else
    void *manager=(void *)(uptr)U32((void *)iai_main_base(),0x1851fe0);
    return manager?((void *(*)(void *,u32))(iai_main_base()+0x5ceb4))(manager,id):0;
#endif
}
static const float *helm_matrix(void *target,u32 part) {
    int index=part==255?-1:(short)part;
#ifdef IAI_HOST_TEST
    return test_target_matrix(target,index);
#else
    void *vt=(void *)(uptr)U32(target,0);
    uptr getter=U32(vt,index<0?0x54:0xd0);
    return getter?((const float *(*)(void *,int))getter)(target,index):0;
#endif
}
static __attribute__((noinline,used)) int helm_queue(void *p,const float *pos,u32 target,u32 part,u32 level) {
#ifdef IAI_HOST_TEST
    return test_helm_queue(p,pos,target,part,level);
#else
    uptr base=iai_main_base();void *manager=(void *)(uptr)U32((void *)base,0x1892550);
    if(!manager || !U32(manager,0x1c+45*4))return 0;
    void *cmd=((void *(*)(u32,u32))(base+0x474204))(48,16);
    if(!cmd)return 0;
    fill_counter(cmd,p,pos,target,part);U32(cmd,0)=base+0x1760e2c;U32(cmd,8)=0;
    /* Native sShell owns cmd after this call, including on rejection. */
    void *shell=((void *(*)(void *,void *,u32))(base+0x48c4f0))(manager,cmd,0);
    if(!shell || U32(shell,0)!=base+0x1760cb4)return 0;
    U16(shell,6)=0x101;
    ((void (*)(void *,void *,void *,u32,u32,u32))(base+0x2b7520))
        (p,shell,(u8 *)shell+0x13a8,1,90+level,0);
    if(U32(shell,0x13a8)!=base+0x173eb2c || U32(shell,0x13b4)!=(uptr)shell)return 0;
    F32(shell,0x1408)=6;F32(shell,0x1404)=0;F32(shell,0x1378)=280;U32(shell,0x13d8)|=0x8000000;
    fx_shell_raw(shell);
    return 1;
#endif
}
static void helm_capture(void *p) {
    u32 bit=state.slash==6?1:2;
    if((helm.captured&bit) || !(state.hit_attempted&1) || !owns_hit(p) || U8(p,0x30d4)!=1)return;
    void *target=(void *)(uptr)U32(p,0x30d0),*part=(void *)(uptr)U32(p,0x30cc);
    if(!target || !part)return;
    void *identity=(void *)(uptr)U32(target,0x1428);if(!identity)return;
    u32 id=U8(identity,0x1b5),part_id=U16(part,0);
    if(helm_target(id)!=target)return;
    const float *matrix=helm_matrix(target,part_id);if(!matrix)return;
    float pos[3]={F32(p,0x3040),F32(p,0x3044),F32(p,0x3048)},local[3];
    for(u32 k=0;k<3;++k)if(!__builtin_isfinite(pos[k]))return;
    if(!helm_point_local(matrix,pos,local))return;
    for(u32 k=0;k<3;++k)if(!__builtin_isfinite(local[k]))return;
    for(u32 k=0;k<3;++k)helm.local[k]=local[k];
    helm.target=id;helm.part=part_id;helm.identity=(uptr)identity;helm.target_vt=U32(target,0);
    u32 first=!helm.captured;helm.captured|=bit;helm.point_valid=1;
    if(first){
#ifdef IAI_HOST_TEST
        test_helm_sound(p);
#else
        helm_sound_base=iai_main_base();helm_sound_raw(p);
#endif
    }
}
static void helm_cuts_step(void *p,float frame) {
    if(!helm.point_valid || helm.sent>=7 || U32(p,0x13c4)!=helm.owner || U8(p,0x1054)!=helm.actor){helm.detached=0;return;}
    if(!__builtin_isfinite(frame) || frame<0)return;
    if(!helm.started){if(frame<((helm.captured&2)?4:24))return;helm.started=1;helm.start_frame=(float)(u32)frame;}
    if(frame>helm.start_frame+180){helm.point_valid=helm.detached=0;return;}
    if(frame<helm.start_frame+6*(float)helm.sent)return;
    void *target=helm_target(helm.target);
    if(!target || U32(target,0)!=helm.target_vt || U32(target,0x1428)!=helm.identity){helm.point_valid=helm.detached=0;return;}
    const float *matrix=helm_matrix(target,helm.part);if(!matrix)return;
    float pos[3];
    for(u32 k=0;k<3;++k){pos[k]=matrix[k]*helm.local[0]+matrix[k+4]*helm.local[1]+matrix[k+8]*helm.local[2]+matrix[k+12];if(!__builtin_isfinite(pos[k]))return;}
    if(helm_queue(p,pos,helm.target,helm.part,helm.level)){
        ++helm.sent;
        if(helm.sent==7)helm.detached=0;
#ifndef IAI_HOST_TEST
        u32 words[]={helm.sent,helm.level,90+helm.level,U32(p,0x500),U32(p,0x2534)};
        log_words("MHGU_LSO_P17_IMPACT",words,5);
#endif
    }
}
static void helm_impact_tick(void *p) {
    if(helm_natural_landing){clear_hit(p);return;}
    float frame=F32(p,0x500);helm_capture(p);
#ifndef IAI_HOST_TEST
    if(state.slash==6 && frame>=14)fx_trail(p);
#endif
    if(state.slash==7)helm_cuts_step(p,frame);
    if(!__builtin_isfinite(frame) || frame<0 || (state.slash==7 && frame>=24)){clear_hit(p);return;}
    if(state.hit_attempted&1)return;state.hit_attempted|=1;
    if(U32(p,0x3020)!=iai_main_base()+0x173eb2c || U8(p,0x3025) || !hit_resources(p))return;
    void *raw=(void *)(uptr)U32((void *)(uptr)U32(p,0x3198),0x64);
    u32 index=state.slash==6?88:89;
    if((U32(raw,8)!=94 && U32(raw,8)!=96) || U16(raw,16+index*56+4)!=42 || U16(raw,16+index*56+2)!=(state.slash==6?0x7fff:24))return;
    U8(p,0x30d4)=0; /* NRO379C clears the previous contact before each new native hit setup. */
#ifdef IAI_HOST_TEST
    test_hit_prepare(p,0,index,10);
#else
    ((void (*)(void *,u32,u32,u32))(iai_main_base()+0x2b7240))(p,0,index,10);
#endif
    if(U8(p,0x3025)!=1 || U32(p,0x302c)!=(uptr)p)return;
    u32 motion=body_motion();if(U32(p,0x3094)!=motion){U8(p,0x3025)=0;return;}
    state.hit_ticket=U16(p,0x3054);state.hit_registered=1;state.hit_motion=motion;
    F32(p,0x307c)=0;F32(p,0x3080)=state.slash==6?32767:24;U32(p,0x30a8)=1;U32(p,0x3050)|=0x8000000;
}
/* Exact native analogue of CRO157A0, independently located in the
   Switch LS handlers119C864/119CBF4. Native sShell takes ownership of cmd
   on success AND rejection; never touch/free cmd after48C4F0 returns. */
static void counter_tick(void *p) {
    if (!state.parried || state.counter_attempted || !owns_hit(p) ||
        U8(p,0x30d4)!=1) return;
    void *target=(void *)(uptr)U32(p,0x30d0);
    void *contact=(void *)(uptr)U32(p,0x30cc);
    if (!target || !contact) return;
    state.counter_attempted=1;
    void *identity=(void *)(uptr)U32(target,0x1428);
    if (!identity) return;
    float pos[3]={F32(p,0x3040),F32(p,0x3044),F32(p,0x3048)};
    for (u32 k=0;k<3;++k) if (!__builtin_isfinite(pos[k])) return;
    u32 target_id=U8(identity,0x1b5), part_id=U16(contact,0);
#ifdef IAI_HOST_TEST
    test_counter_queue(p,pos,target_id,part_id);
#else
    uptr base=iai_main_base();
    void *manager=(void *)(uptr)U32((void *)base,0x1892550);
    /* Resource45 must already be loaded by the native quest loader.
       Fail closed if missing; do not substitute a foreign resource. */
    if (!manager || !U32(manager,0x1c+45*4)) return;
    void *cmd=((void *(*)(u32,u32))(base+0x474204))(48,16);
    if (!cmd) return;
    for (u32 k=0;k<8;++k) counter_vtable[k]=U32((void *)base,0x1760e24+k*4);
    fill_counter(cmd,p,pos,target_id,part_id);
    void *spawned=((void *(*)(void *,void *,u32))(base+0x48c4f0))(manager,cmd,0);
    if (spawned) iai_log_cuts();
#endif
}
static void small_contact(void *p) {
    if(!owns_hit(p) || U8(p,0x30d4)!=1 || !U32(p,0x30d0) || !U32(p,0x30cc))return;
    if(!U32((void *)(uptr)U32(p,0x30d0),0x1428))return;
    if(state.hit_attempted&4)return;
    state.hit_attempted|=4;
    if(!state.parried)small_regen_start(p,900);
#ifndef IAI_HOST_TEST
    iai_log_span("MHGU_LSO_P3_SMALL_CONTACT",sizeof("MHGU_LSO_P3_SMALL_CONTACT")-1);
#endif
}
static void small_hit_tick(void *p) {
    float frame=F32(p,0x500);
    small_contact(p);
    u32 stage=frame>=12 && frame<16?1:frame>=32 && frame<36?2:0;
    if(!__builtin_isfinite(frame) || !stage){clear_hit(p);return;}
    if(state.hit_attempted&stage)return;
    clear_hit(p);state.hit_attempted|=stage;
    if(U32(p,0x3020)!=iai_main_base()+0x173eb2c || U8(p,0x3025) || !hit_resources(p))return;
    void *hdp=(void *)(uptr)U32(p,0x3198);
    void *raw=(void *)(uptr)U32(hdp,0x64);
    if((U32(raw,8)!=94 && U32(raw,8)!=96))return;
    u32 index=stage==1?85:86;
    const u8 *entry=(const u8 *)raw+16+index*56;
    if(U16(entry,4)!=(stage==1?18:22) || U16(entry,2)!=4)return;
#ifdef IAI_HOST_TEST
    test_hit_prepare(p,0,index,0);
#else
    ((void (*)(void *,u32,u32,u32))(iai_main_base()+0x2b7240))(p,0,index,0);
#endif
    if(U8(p,0x3025)!=1 || U32(p,0x302c)!=(uptr)p)return;
    if(U32(p,0x3094)!=5245){U8(p,0x3025)=0;return;}
    state.hit_ticket=U16(p,0x3054);state.hit_registered=1;state.hit_motion=5245;
    F32(p,0x307c)=0;F32(p,0x3080)=4;U32(p,0x30a8)=1;
#ifndef IAI_HOST_TEST
    const char *message=stage==1?"MHGU_LSO_P3_SMALL_HIT1":"MHGU_LSO_P3_SMALL_HIT2";
    iai_log_span(message,sizeof("MHGU_LSO_P3_SMALL_HIT1")-1);
#endif
}
static int helm_air_begin(void *p) {
    const u32 *takeoff=clip(p,0x5f1),*flight=clip(p,0x5e2);
    if(!takeoff || !flight || takeoff[2]!=47 || flight[2]!=134 ||
       takeoff[3]!=0xffffffffu || flight[3]!=0xffffffffu)return 0;
    if(!__builtin_isfinite(F32(p,0x44)) || !__builtin_isfinite(F32(p,0x10f0)))return 0;
    clear_hit(p);
    helm_natural_landing=0;helm.point_valid=helm.detached=0;
    helm.owner=U32(p,0x13c4);helm.actor=U8(p,0x1054);
    air.player=p;air.owner=U32(p,0x13c4);air.actor=U8(p,0x1054);air.active=1;
    air.mode=U8(p,0x2505);air.flag=U8(p,0x2ed3);air.gravity=U32(p,0x1024);
    state.slash=4;state.started=0;state.clock=F32(p,0x2534);state.audio_mask=0;
    for(u32 k=0;k<4;++k)U32(p,0x1100+k*4)=0;
    air_apply(p);
#ifndef IAI_HOST_TEST
    iai_log_span("MHGU_LSO_P17_TAKEOFF",sizeof("MHGU_LSO_P17_TAKEOFF")-1);
#endif
    return helm_motion_start(p);
}
static void helm_thrust_tick(void *p) {
    if((state.hit_attempted&1) && owns_hit(p) && U8(p,0x3025)==1 && !(state.hit_attempted&2) && U8(p,0x30d4)==1 &&
       U32(p,0x30d0) && U32(p,0x30cc) && U32((void *)(uptr)U32(p,0x30d0),0x1428)){
        state.hit_attempted|=2;
#ifndef IAI_HOST_TEST
        iai_log_span("MHGU_LSO_P17_HELM_CONTACT",sizeof("MHGU_LSO_P17_HELM_CONTACT")-1);
#endif
        if(helm_air_begin(p))return;
    }
    float frame=F32(p,0x500);
    if(!__builtin_isfinite(frame) || frame<88 || frame>=94){clear_hit(p);return;}
    if(state.hit_attempted&1)return;
    state.hit_attempted|=1;
    if(U32(p,0x3020)!=iai_main_base()+0x173eb2c || U8(p,0x3025) || !hit_resources(p))return;
    void *raw=(void *)(uptr)U32((void *)(uptr)U32(p,0x3198),0x64);
    if((U32(raw,8)!=94 && U32(raw,8)!=96) || U16(raw,16+87*56+2)!=6 || U16(raw,16+87*56+4)!=18)return;
    U8(p,0x30d4)=0;
#ifdef IAI_HOST_TEST
    test_hit_prepare(p,0,87,0);
#else
    ((void (*)(void *,u32,u32,u32))(iai_main_base()+0x2b7240))(p,0,87,0);
#endif
    if(U8(p,0x3025)!=1 || U32(p,0x302c)!=(uptr)p)return;
    if(U32(p,0x3094)!=5240){U8(p,0x3025)=0;return;}
    state.hit_ticket=U16(p,0x3054);state.hit_registered=1;state.hit_motion=5240;
    F32(p,0x307c)=0;F32(p,0x3080)=6;U32(p,0x30a8)=1;U32(p,0x3050)|=0x8000000;
}
static void iai_hit_tick(void *p) {
    if (!state.slash || !state.started) return;
    if(state.slash>=8){foresight_tick(p);return;}
    if(state.slash==2){small_hit_tick(p);return;}
    if(state.slash==3){helm_thrust_tick(p);return;}
    if(state.slash==6 || state.slash==7){helm_impact_tick(p);return;}
    if(state.slash==4 || state.slash==5){clear_hit(p);return;}
    float frame=F32(p,0x500);
    if (__builtin_isfinite(frame) && frame>=12.0f && frame<=28.0f) counter_tick(p);
    if (!__builtin_isfinite(frame) || frame<0 || frame>=28.0f) {
        clear_hit(p);return;
    }
    if (frame<12.0f || state.hit_attempted) return;
    state.hit_attempted=1;
    /* Never borrow a still-active native record or replace an unknown type.
       Entry30 is byte-identical in the creator's HDP and our Switch copy.
       The Switch API, ABI and flag selector10 are independently decoded. */
    if (U32(p,0x3020)!=iai_main_base()+0x173eb2c || U8(p,0x3025) || !hit_resources(p)) return;
#ifdef IAI_HOST_TEST
    test_hit_prepare(p,0,state.parried?45:30,state.parried?0:10);
#else
    ((void (*)(void *,u32,u32,u32))(iai_main_base()+0x2b7240))(p,0,state.parried?45:30,state.parried?0:10);
#endif
    if (U8(p,0x3025)!=1 || U32(p,0x302c)!=(uptr)p) return;
    if (U32(p,0x3094)!=5250) { U8(p,0x3025)=0;return; }
    state.hit_ticket=U16(p,0x3054);state.hit_registered=1;state.hit_motion=5250;
    /* Payload BF2FE0/FE4/FE8/FF0: final record30 normal,51GP,float1.
       Only our slot0, owner, motion5250 and initialization ticket passed
       the checks above. Native attacks and queued auxiliary cuts untouched. */
    U16(p,0x3058)=state.parried?51:30;F32(p,0x30b8)=1.0f;
    /* CRO1577C/84/90: record delay0, duration16, HTS index1. Fields on
       Switch are proven by168960/16919C/1696FC, not copied from 3DS. */
    F32(p,0x307c)=0.0f;F32(p,0x3080)=16.0f;U32(p,0x30a8)=1;
}
/* Called ONLY at169824 (record1696FC r4, player r1, rawHTS r2,out r3).
   Return the original pointer for every foreign/native collision. A zero
   result is a normal empty geometry (native builder also returns zero),
   with its 36-byte output initialized, for our canceled/out-of-window hit. */
const void *iai_geometry(void *p,void *record,const void *original,void *out) {
    if (!p || state.player!=p || record!=(void *)((u8 *)p+0x3020) || !owns_hit(p)) return original;
    if(state.slash>=2 && state.slash<=7){
        float frame=F32(p,0x500);IaiPose pose;
        int active=__builtin_isfinite(frame) && frame>=0 &&
            (state.slash==6 || (state.slash==7 && frame<24) ||
            (state.slash==3 && frame>=88 && frame<94) || (state.slash==9 && frame>=21 && frame<30) ||
             (state.slash==2 && ((frame>=12 && frame<16)||(frame>=32 && frame<36))));
        if(current(p) && state.started && U8(p,0x3025) &&
           active && authored_pose(p,9,&pose)){
            collision_shape.bone0=collision_shape.bone1=12;
            /* Requested tolerance: widen only the two tight ground capsules; keep authored reach/windows. */
            collision_shape.type=collision_shape.flags=1;collision_shape.unused=0;collision_shape.radius=state.slash==6?48:state.slash==7?36:state.slash==3?32:state.slash==9?12:24;
            float *t=pose.transform,z=(state.slash==6?320:280)*t[10];
            float cx=2*t[5]*z,cy=-2*t[4]*z;
            float dx=t[7]*cx-t[6]*cy,dy=t[7]*cy+t[6]*cx,dz=z+t[4]*cy-t[5]*cx;
            for(u32 k=0;k<8;k++)collision_shape.endpoints[k]=0;
            for(u32 k=0;k<3;k++)collision_shape.endpoints[k]=t[k];
            collision_shape.endpoints[4]=t[0]+dx;collision_shape.endpoints[5]=t[1]+dy;collision_shape.endpoints[6]=t[2]+dz;
            return &collision_shape;
        }
        clear_hit(p);if(out)for(u32 k=0;k<9;k++)U32(out,k*4)=0;return 0;
    }
    if (current(p) && state.started && state.slash && U8(p,0x3025) &&
        iai_collision_shape(iai_collision_tables,544,F32(p,0x500),&collision_shape))
        return &collision_shape;
    clear_hit(p);
    if (out) for (u32 i=0;i<9;++i) U32(out,i*4)=0;
    return 0;
}

/* NRO9FFC/A4E8: sweep world capsules, reusing the native hit record/deduplication. */
static int sweep_owned(void *p,void *record) {
    return p && state.player==p && state.active && state.started &&
        !helm_natural_landing && (state.slash==6 || state.slash==7) &&
        record==(void *)((u8 *)p+0x3020) && owns_hit(p) && U8(p,0x3025) &&
        helm.owner==U32(p,0x13c4) && helm.actor==U8(p,0x1054) &&
        player_gate(p) && idle(p) && !(U32(p,0x2500)&1) && U16(p,0x4b4)==body_clip();
}
static float sweep_madd(float a,float b,float c) {
#ifdef IAI_HOST_TEST
    return (float)((double)a*b+c);
#else
    __asm__("vfma.f32 %0, %1, %2" : "+t"(c) : "t"(a),"t"(b));
    return c;
#endif
}
static void sweep_capture(void *manager,void *p,void *record,const float *shape,u32 kind) {
    if(!shape || kind!=2 || !sweep_owned(p,record))return;
    for(u32 k=0;k<9;++k)if(k!=3 && k!=7 &&
       (!__builtin_isfinite(shape[k]) || __builtin_fabsf(shape[k])>=1.e30f)){
        sweep.valid=sweep.count=0;++sweep.epoch;return;
    }
    if(shape[8]<=0 || shape[8]>2048){sweep.valid=sweep.count=0;++sweep.epoch;return;}
    u32 ticket=U16(record,0x34);sweep.count=0;
    if(sweep.valid && sweep.player==p && sweep.manager==manager &&
       sweep.actor==U8(p,0x1054) && sweep.owner==U32(p,0x13c4) && sweep.stage==6 &&
       ((state.slash==6 && sweep.ticket==ticket) || state.slash==7)){
        float distance2=0;
        for(u32 end=0;end<8;end+=4){
            float d2=0;
            for(u32 k=0;k<3;++k){float d=shape[end+k]-sweep.previous[end+k];d2=sweep_madd(d,d,d2);}
            if(d2>distance2)distance2=d2;
        }
        if(__builtin_isfinite(distance2) && distance2>.0001f && distance2<=16777216.f){
            float distance=__builtin_sqrtf(distance2);u32 count=(u32)(distance*.0625f)+1;
            if(count>12)count=12;
            float radius=shape[8]>sweep.previous[8]?shape[8]:sweep.previous[8];
            radius+=distance/(count*2)+.001f;
            for(u32 i=0;i<count;++i){
                float t=(i+.5f)/count;
                for(u32 k=0;k<8;++k)sweep.extra[i][k]=sweep_madd(t,shape[k]-sweep.previous[k],sweep.previous[k]);
                sweep.extra[i][8]=radius;
            }
            sweep.count=count;
        }
    }
    for(u32 k=0;k<9;++k)sweep.previous[k]=shape[k];
    sweep.player=p;sweep.manager=manager;sweep.record=record;
    sweep.owner=U32(p,0x13c4);sweep.actor=U8(p,0x1054);
    sweep.ticket=ticket;sweep.stage=state.slash;sweep.valid=1;++sweep.epoch;
#ifndef IAI_HOST_TEST
    if(sweep.count && !(sweep_logged&(1u<<state.slash))){
        sweep_logged|=1u<<state.slash;
        u32 words[]={sweep.count,sweep.stage,sweep.ticket};log_words("MHGU_LSO_P17_SWEEP",words,3);
    }
#endif
}
u32 iai_geometry_world(float scale,void *manager,void *p,const void *shape,float *out,u32 kind,void *record) {
#ifdef IAI_HOST_TEST
    extern u32 test_native_world(float,void *,void *,const void *,float *,u32);
    u32 result=test_native_world(scale,manager,p,shape,out,kind);
#else
    u32 result=((u32 (*)(float,void *,void *,const void *,float *,u32))(iai_main_base()+0x16dddc))
        (scale,manager,p,shape,out,kind);
#endif
    sweep_capture(manager,p,record,out,result);foresight_probe(record,out,result);return result;
}
void iai_sweep_contact(void *record,void *target,void *part) {
    if(sweep_owned(state.player,record) && U8(record,0xb4)==1 &&
       U32(record,0xb0)==(uptr)target && U32(record,0xac)==(uptr)part){
        ++sweep.serial;helm_capture(state.player);
#ifndef IAI_HOST_TEST
        if(sweep.busy){u32 words[]={state.slash,sweep.ticket,sweep.count};log_words("MHGU_LSO_P17_SWEEP_CONTACT",words,3);}
#endif
    }
}
static u32 sweep_native_query(void *manager,void *record,void *target) {
#ifdef IAI_HOST_TEST
    extern u32 test_native_collision(void *,void *,void *);
    return test_native_collision(manager,record,target);
#else
    extern u32 sweep_query_gateway(void *,void *,void *);
    return sweep_query_gateway(manager,record,target);
#endif
}
u32 iai_sweep_query(void *manager,void *record,void *target) {
    u32 serial=sweep.serial,epoch=sweep.epoch;
    u32 result=sweep_native_query(manager,record,target);
    if(sweep.busy || !sweep.count || !sweep.valid || sweep.manager!=manager ||
       sweep.record!=record || sweep.serial!=serial || sweep.epoch!=epoch ||
       !sweep_owned(sweep.player,record) || sweep.ticket!=U16(record,0x34) ||
       sweep.stage!=state.slash || U32(record,0x1c)>=64)return result;
    void *destination=(u8 *)manager+0x2030+U32(record,0x1c)*32;
    if(U8(destination,0)!=2)return result;
    u32 saved[8];for(u32 k=0;k<8;++k)saved[k]=U32(destination,k*4);
    sweep.busy=1;
    for(u32 i=0;i<sweep.count && i<12;++i){
        const float *shape=sweep.extra[i];F32(destination,4)=shape[8];
        for(u32 k=0;k<3;++k){F32(destination,8+k*4)=shape[k];F32(destination,20+k*4)=shape[4+k];}
        sweep_native_query(manager,record,target);
        if(sweep.serial!=serial || sweep.epoch!=epoch || !sweep_owned(sweep.player,record) ||
           sweep.ticket!=U16(record,0x34))break;
    }
    for(u32 k=0;k<8;++k)U32(destination,k*4)=saved[k];
    sweep.busy=0;return result;
}

/* Native MSS100 ABI and unconditional LS donor cues; resolve the live weapon bank. */
static void audio_emit(void *p,u32 request,int kind) {
#ifdef IAI_HOST_TEST
    test_audio(p,request,19,kind,0,-1);
#else
    uptr base=iai_main_base();
    void *mgr=(void *)(uptr)U32((void *)base,0x1896f5c);
    if (!mgr) { iai_log_audio_missing();return; }
    ((void (*)(void *,void *,u32,int,int,u32,int))(base+0x4f0be8))
        (mgr,p,request,19,kind,0,-1);
    if (kind==2) iai_log_audio_cut();
    else if (request==9) iai_log_audio_draw();else iai_log_audio_sheath();
#endif
}
static void audio_tick(void *p) {
    float frame=F32(p,0x500);
    if (!__builtin_isfinite(frame) || frame<0) return;
    if(state.exiting){
        if(frame>=9 && !(state.audio_mask&8)){state.audio_mask|=8;audio_emit(p,9,1);}
    } else if (!state.slash) {
        if (frame>=64 && !(state.audio_mask&1)) { state.audio_mask|=1;audio_emit(p,1,1); }
    } else if(state.slash>=3){
        if(state.slash==3 && frame>=88 && frame<94 && !(state.audio_mask&4)){state.audio_mask|=4;audio_emit(p,13,2);}
        if(state.slash==9 && frame>=21 && frame<30 && !(state.audio_mask&4)){state.audio_mask|=4;audio_emit(p,13,2);}
    } else if(state.slash==2){
        if(!(state.audio_mask&2)){state.audio_mask|=2;audio_emit(p,9,1);}
        if(frame>=12 && frame<16 && !(state.audio_mask&4)){state.audio_mask|=4;audio_emit(p,13,2);}
        if(frame>=32 && frame<36 && !(state.audio_mask&16)){state.audio_mask|=16;audio_emit(p,13,2);}
    } else {
        if (!(state.audio_mask&2)) { state.audio_mask|=2;audio_emit(p,9,1); }
        if (frame>=12 && frame<176 && !(state.audio_mask&4)) { state.audio_mask|=4;audio_emit(p,13,2); }
    }
}
/* NRO3230 uses282FD8 then281C30 (motion flags11), and binds each stage immediately. */
static int helm_motion_start(void *p) {
    /* NRO3230 sets the native action phase before every Helm motion wrapper. */
    U8(p,5)=1;
#ifdef IAI_HOST_TEST
    test_helm_init(p);test_motion(p,body_motion(),0,0);
#else
    uptr base=iai_main_base();
    ((void (*)(void *,u32,u32,u32))(base+0x282fd8))(p,0,0,0);
    ((void (*)(void *,u32,float,float))(base+0x281c30))(p,body_motion(),0,0);
#endif
    if(!state.active || state.player!=p || U16(p,0x4b4)!=body_clip()){
        if(state.active && state.player==p)stop(p);
        return 0;
    }
    state.started=1;state.clock=F32(p,0x2534);state.audio_mask=state.blade_log_mask=0;
    if(state.slash>=4)for(u32 k=0;k<4;++k)U32(p,0x1100+k*4)=0;
    if(state.slash>=4 && state.slash<=6)air_apply(p);
    if(state.slash==6 || state.slash==7)helm_impact_tick(p);
#ifndef IAI_HOST_TEST
    u32 words[]={state.slash,U16(p,0x4b4),U8(p,0x4b6),U32(p,0x500),U32(p,0x50c),U32(p,0x10f8),U32(p,0x2500)};
    log_words("MHGU_LSO_P17_CLIP_READY",words,7);
#endif
    audio_tick(p);return 1;
}
/* Return 1 only when the custom idle was handled. All other players/weapons
   reach a gateway that replays the original prologue and native idle. */
int iai_idle(void *p) {
    if (current(p)) {
        if(state.slash==8 && state.started && F32(p,0x500)>=49){
            foresight_release(p);clear_hit(p);U32(p,0xfec)=foresight.yaw;
            aim_draw(p);state.slash=9;state.started=state.hit_attempted=0;
            state.clock=F32(p,0x2534);return helm_motion_start(p);
        }
        int landed=0;
        if(state.slash==4 && state.started && __builtin_isfinite(F32(p,0x500)) &&
           F32(p,0x500)>=0 && (F32(p,0x500)>=46 || (U8(p,0x4b6)&4))){
            clear_hit(p);state.slash=5;state.started=0;state.clock=F32(p,0x2534);
            for(u32 k=0;k<4;++k)U32(p,0x1100+k*4)=0;
#ifndef IAI_HOST_TEST
            iai_log_span("MHGU_LSO_P17_AIRBORNE",sizeof("MHGU_LSO_P17_AIRBORNE")-1);
#endif
            int result=helm_motion_start(p);
            if(result){F32(p,0x1014)=14;air_apply(p);}
            return result;
        }
        if(state.slash==6 && state.started){
            float height=F32(p,0x44)-F32(p,0x10f0),velocity=F32(p,0x1014);
            if(!__builtin_isfinite(height) || !__builtin_isfinite(velocity) ||
               !__builtin_isfinite(F32(p,0x500)) || F32(p,0x500)<0){stop(p);return 0;}
            air_apply(p);
            if(helm.frozen && height<=2 && velocity==0){
                helm_capture(p);clear_hit(p);air_release(p);
#ifndef IAI_HOST_TEST
                helm_phase_trace("MHGU_LSO_P17_DROP_GROUNDED",p);
#endif
                state.slash=7;state.started=0;state.hit_attempted=0;
                state.clock=F32(p,0x2534);state.audio_mask=0;
#ifndef IAI_HOST_TEST
                iai_log_span("MHGU_LSO_P17_LANDING",sizeof("MHGU_LSO_P17_LANDING")-1);
#endif
                return helm_motion_start(p);
            }
        }
        if(state.slash==5 && state.started){
            float y=F32(p,0x44),floor=F32(p,0x10f0),velocity=F32(p,0x1014);
            if(!__builtin_isfinite(y) || !__builtin_isfinite(floor) || !__builtin_isfinite(velocity)){stop(p);return 0;}
            landed=velocity==0 && y-floor<=2;
#ifndef IAI_HOST_TEST
            if(landed)iai_log_span("MHGU_LSO_P17_LANDED",sizeof("MHGU_LSO_P17_LANDED")-1);
#endif
            if(landed){
                const u32 *land=clip(p,0x5f4);
                if(!land || land[2]!=139 || land[3]!=0xffffffffu){stop(p);return 0;}
                clear_hit(p);air_release(p);helm_natural_landing=1;
                helm.point_valid=helm.detached=0;
                state.slash=7;state.started=state.hit_attempted=0;
                return helm_motion_start(p);
            }
        }
        /* Native 950BCC(controller) is LDRB +6 / UBFX bit2. Native LS
           action31 passes player+4B0. No guessed end-frame polling. */
        if (!state.exiting && F32(p,0x2534)-state.clock>=300.0f) {
            if(stance_timeout(p))return 1;
            exit_turn(p);stop(p);
        }
        else if ((state.slash || state.exiting) && state.started &&
                 ((state.slash==5?landed:state.slash==6?0:(U8(p,0x4b6)&4)) || (state.exiting && F32(p,0x2534)-state.clock>=120))) {
            float frame=F32(p,0x500);u32 passive=state.exiting;
            exit_turn(p);stop(p);
            if (__builtin_isfinite(frame) && frame>=0) {
                /* CRO78F0..7914 finishes through the native cleanup chain;
                   resetting only the idle phase skips that handoff. */
                state.reset_idle=0;
#ifdef IAI_HOST_TEST
                test_finish(p,0,0,0,0);
#else
                ((void (*)(void *,u32,u32,u32,u32))(iai_main_base()+0x2b6e94))(p,0,0,0,0);
                iai_log_native_end();
#endif
                if(passive){
                    /* The completed exit already separates entries; keep the lock for manual cancels only. */
                    if(reentry.player==p)reentry.quiet=reentry.reported=0;
                    state.exiting=state.suppress_chord=0;
                    state.reset_idle=(player_gate(p) && idle(p) && U16(p,0x4b4)==0x5fb)?2:0;
#ifndef IAI_HOST_TEST
                    iai_log_span("IAI_R38_EXIT_RELEASED",sizeof("IAI_R38_EXIT_RELEASED")-1);
#endif
                }
                return 1; /* Do not initialize another native action in this old callback. */
            }
        } else {
#ifdef IAI_HOST_TEST
            test_idle_flags(p,0x200);
#else
            uptr vt=U32(p,0);
            ((void (*)(void *,u32))(uptr)U32((void *)vt,0x260))(p,0x200);
#endif
            if (!state.started) {
                if(state.slash>=3)return helm_motion_start(p);
#ifdef IAI_HOST_TEST
                test_motion(p,body_motion(),state.slash>=3?0.0f:4.0f,0.0f);
#else
                ((void (*)(void *,u32,float,float))(iai_main_base()+0x281998))(p,body_motion(),state.slash>=3?0.0f:4.0f,0.0f);
#endif
                if(state.slash>=4 && state.slash<=7)for(u32 k=0;k<4;++k)U32(p,0x1100+k*4)=0;
                if (U16(p,0x4b4)!=body_clip()) { stop(p); }
                else {
                    state.started=1;state.audio_mask=0;state.blade_log_mask=0;
                    if(state.slash==6){
                        air_apply(p);
                    }
                    if(state.slash==6 || state.slash==7)helm_impact_tick(p);
#ifndef IAI_HOST_TEST
                    if(!state.slash && !state.exiting)iai_log_span("IAI_R38_STANCE_ENTER",sizeof("IAI_R38_STANCE_ENTER")-1);
#endif
                    cue_guard_window(p);audio_tick(p);return 1;
                }
            } else {
                cue_guard_window(p);
                audio_tick(p);
                iai_hit_tick(p);
                return 1;
            }
        }
    }
    if (state.player==p && state.reset_idle) {
        /* Wait for native request processing; never reset a pending transition. */
        if(state.reset_idle==2 && player_gate(p) && idle(p) && U16(p,0x4b4)==0x5fb && (U32(p,0x2500)&1))return 0;
        /* Never reset a native damage/attack/area-change action. */
        if (player_gate(p) && idle(p) && (U16(p,0x4b4)==0x5f9 || U16(p,0x4b4)==0x5fa || U16(p,0x4b4)==0x5fb || U16(p,0x4b4)==0x5f5 || U16(p,0x4b4)==0x5f0 || U16(p,0x4b4)==0x5f1 || U16(p,0x4b4)==0x5e2 || U16(p,0x4b4)==0x5f3 || U16(p,0x4b4)==0x5f4) &&
            !(U32(p,0x2500)&1)) {
#ifndef IAI_HOST_TEST
            if(U16(p,0x4b4)==0x5fb)iai_log_span("IAI_R38_IDLE_REPAIR",sizeof("IAI_R38_IDLE_REPAIR")-1);
#endif
            U8(p,5)=0;
        }
        state.reset_idle=0;
    }
    return 0;
}

static int part_gate(void *m,void *p,u32 part) {
    if ((part!=9 && part!=10) || !p || U32(m,0x30)!=(uptr)p ||
        U32(m,0x13d0)!=part) return 0;
    /* Confirm the effective attachment method before using its layout. */
    void *vt=(void *)(uptr)U32(m,0);uptr base=iai_main_base();
    return U32(vt,0x48)==base+0x45884 && U32(vt,0x4c)==base+0x94e224 &&
           U32(vt,0x50)==base+0x94e250;
}
void iai_restore(void *m,void *p,u32 part) {
    if (part!=9 && part!=10) return;
    Saved *s=&saved[part-9];
    if (!s->applied || s->part!=m) return;
    int owned=s->parent==p;
    if(s->flag_mask && owned && !((U32(m,0x110)^s->flag_after)&s->flag_mask))
        U32(m,0x110)=(U32(m,0x110)&~s->flag_mask)|(s->flag_before&s->flag_mask);
    s->flag_mask=0;
    for (u32 k=0;k<WORDS && owned;++k) if (U32(m,fields[k])!=s->after[k]) owned=0;
    if (owned) for (u32 k=0;k<WORDS;++k) U32(m,fields[k])=s->before[k];
    s->applied=0;
}
/* Value snapshot only; identity pointers are compared, never dereferenced later. */
typedef struct { void *part,*player; u32 owner,resource,mesh,valid; IaiPose pose; } IdlePart;
static IdlePart idle_blade,idle_sheath;
static void log_blade_binding(char stage,u32 bone,u32 selector) {
#ifndef IAI_RELEASE
    char text[]="IAI_R38_BLADE_BIND S 00000000 00000000";
    u32 at=sizeof("IAI_R38_BLADE_BIND ")-1;text[at]=stage;
    for(u32 j=0;j<2;++j)for(u32 k=0;k<8;++k){u32 n=((j?selector:bone)>>(28-4*k))&15;text[at+2+j*9+k]=(char)(n<10?'0'+n:'A'+n-10);}
    iai_log_span(text,sizeof(text)-1);
#else
    (void)stage;(void)bone;(void)selector;
#endif
}
static int idle_part_matches(const IdlePart *cache,void *m,void *p) {
    return cache->valid && cache->part==m && cache->player==p &&
        cache->owner==U32(p,0x13c4) && cache->resource==U32(m,0xf0) && cache->mesh==U32(m,0xfc);
}
static void remember_idle_part(void *m,void *p,u32 part) {
    IdlePart *cache=part==9?&idle_blade:&idle_sheath;
    if ((state.active && state.player==p) || !player_gate(p) || !idle(p) || U8(p,5)!=1 ||
        U16(p,0x4b4)!=0x501 || !(U8(p,0x2749)&1) || (U32(p,0x2500)&1) ||
        !__builtin_isfinite(F32(p,0x2534)) || F32(p,0x2534)<0 || !U32(m,0xf0) || (saved[part-9].applied && saved[part-9].part==m)) return;
    IaiPose candidate,checked;
    candidate.bone=U32(m,0x13cc);candidate.selector=U32(m,0x13d4);
    if(candidate.bone>254 || candidate.selector>20)return;
    if(idle_part_matches(cache,m,p) && candidate.bone==cache->pose.bone && candidate.selector==cache->pose.selector){
        u32 k=0;
        while(k<12 && U32(m,0x40+k*4)==U32(&cache->pose.transform,k*4))++k;
        if(k==12)return;
    }
    for(u32 k=0;k<12;++k)candidate.transform[k]=F32(m,0x40+k*4);
    if(!iai_settle_weapon(&candidate,candidate.transform,0,0,1,&checked))return;
    int first=!idle_part_matches(cache,m,p);
    cache->part=m;cache->player=p;cache->owner=U32(p,0x13c4);
    cache->resource=U32(m,0xf0);cache->mesh=U32(m,0xfc);cache->pose=checked;cache->valid=1;
    if(first && part==9)log_blade_binding('I',checked.bone,checked.selector);
}
static int idle_blade_matches(void *m,void *p){return idle_part_matches(&idle_blade,m,p);}
static int recovery_range(float *start,float *end) {
    if(state.slash==2){*start=100;*end=130;return 1;}
    if(state.slash==3){*start=200;*end=237;return 1;}
    if(state.slash==7){*start=95;*end=131;return 1;}
    if(state.slash==9){*start=100;*end=132;return 1;}
    return 0;
}
static const float *bone_matrix(void *p,u32 bone) {
#ifdef IAI_HOST_TEST
    return test_bone_matrix(p,bone);
#else
    return ((const float *(*)(void *,u32))(iai_main_base()+0x27c40c))(p,bone);
#endif
}
static void settle_weapon(void *m,void *p,u32 part,float frame,IaiPose *pose) {
    float start=state.exiting?20:136,end=state.exiting?69:168;
    int tail=recovery_range(&start,&end);
    if(state.slash>=2 && !tail)return;
    if ((!state.slash && !state.exiting) || !__builtin_isfinite(frame) || frame<=start) return;
    IaiPose target;
    if(state.exiting || tail){
        const IdlePart *cache=part==9?&idle_blade:&idle_sheath;
        if(!idle_part_matches(cache,m,p))return;
        target=cache->pose;
    } else if(part==9){
        if(!(state.blade_log_mask&1)){state.blade_log_mask|=1;log_blade_binding('S',U32(m,0x13cc),U32(m,0x13d4));}
        if(!idle_blade_matches(m,p)){
            if(!(state.blade_log_mask&4)){state.blade_log_mask|=4;log_blade_binding('N',0,0);}return;
        }
        target=idle_blade.pose;
        if(!(state.blade_log_mask&2)){state.blade_log_mask|=2;log_blade_binding('G',target.bone,target.selector);}

    } else {
        target.bone=U32(m,0x13cc);target.selector=U32(m,0x13d4);
        if(target.bone!=2 || target.selector!=2 || pose->bone!=target.bone)return;
        for(u32 k=0;k<12;++k)target.transform[k]=F32(m,0x40+k*4);
    }
    float t=(frame-start)/(end-start);if(t>1)t=1;t=t*t*(3-2*t);
    const float *old=0,*next=0;
    if(t<1 && pose->bone!=target.bone){old=bone_matrix(p,pose->bone);next=bone_matrix(p,target.bone);if(!old||!next)return;}
    IaiPose settled;
    if (!iai_settle_weapon(pose,target.transform,old,next,t,&settled)) return;
    settled.bone=target.bone;settled.selector=target.selector;*pose=settled;
}
void iai_apply(void *m) {
    void *p=(void *)(uptr)U32(m,0x30);u32 part=U32(m,0x13d0);
    if (!part_gate(m,p,part))return;
    /* A second callback must not snapshot our own pose as the native restore target. */
    iai_restore(m,p,part);
    remember_idle_part(m,p,part);
    if(!current(p) || !state.started)return;
    float frame=F32(p,0x500); IaiPose pose;
    if(state.exiting && (!__builtin_isfinite(frame) || frame<0))return;
    if (state.slash>=2) {
        if(!authored_pose(p,part,&pose))return;
    } else if (state.slash) {
        if (!iai_pose_f1(iai_f1_tables,33984,frame,part,&pose)) return;
    } else if (!iai_pose_f0(iai_tables,12576,state.exiting?exit_source_frame(frame):frame,part,&pose)) return;
    settle_weapon(m,p,part,frame,&pose);
    Saved *s=&saved[part-9];s->part=m;s->parent=p;
    s->flag_mask=state.slash>=2?0x200001u:0;
    if(s->flag_mask){
        /* NRO7A94 owns these two attachment flags before native matrices. */
        s->flag_before=U32(m,0x110);
        s->flag_after=(s->flag_before&~s->flag_mask)|1;
        U32(m,0x110)=s->flag_after;
    }
    for (u32 k=0;k<WORDS;++k) s->before[k]=U32(m,fields[k]);
    for (u32 k=0;k<12;++k) F32(m,0x40+k*4)=pose.transform[k];
    U32(m,0x13c8)=U32(m,0x13cc);U32(m,0x13cc)=pose.bone;
    U32(m,0x13d4)=pose.selector;U32(m,0x1408)=0;
#ifdef IAI_HOST_TEST
    test_attach(m,p,pose.bone);
#else
    ((void (*)(void *,void *,u32))(iai_main_base()+0x45884))(m,p,pose.bone);
#endif
    /* Native virtual +4C rebuilds the local matrix unconditionally from
       +40/+50; copying the original 3DS flag write at +10C is unnecessary. */
    for (u32 k=0;k<WORDS;++k) s->after[k]=U32(m,fields[k]);
    s->applied=1;
}
int iai_part_sync(void *m,void *p) {
    u32 part=U32(m,0x13d0);
    if (!part_gate(m,p,part) || !current(p) || !state.started) return 0;
    float frame=F32(p,0x500);
    if (!__builtin_isfinite(frame) || frame<0) return 0;
    if(state.slash>=2 && part==10)return 1; /* Author leaves sheath layers alone. */
    int blend_exit=state.exiting && frame>20 && idle_part_matches(part==9?&idle_blade:&idle_sheath,m,p);
    float recovery_start=0,recovery_end=0;
    int tail=recovery_range(&recovery_start,&recovery_end) && frame>recovery_start && idle_part_matches(part==9?&idle_blade:&idle_sheath,m,p);
    if(((state.slash==1 && part==9 && frame>136 && idle_blade_matches(m,p)) || blend_exit || tail) && clip(m,1) && clip(m,0x101)) {
        for(u32 layer=0;layer<2;++layer){
            u32 id=(layer<<8)|1;
            if(U16(m,0x4b4+layer*0x140)!=id){
                float remaining=(tail?recovery_end:state.exiting?69:168)-frame;if(remaining<0)remaining=0;
#ifdef IAI_HOST_TEST
                test_part_blend(m,layer,id,0,remaining,0,1);
#else
                ((void (*)(void *,u32,u32,u32,float,float,float))(iai_main_base()+0x954640))
                    (m,layer,id,0,remaining,0,1);
#endif
            }
#ifdef IAI_HOST_TEST
            test_part_motion(m,p,layer,layer,1,1,0);
#else
            ((void (*)(void *,void *,u32,u32,u32,u32,float))(iai_main_base()+0x308210))
                (m,p,layer,layer,1,1,0);
#endif
        }
        return 1;
    }
    if(state.exiting)frame=exit_source_frame(frame);
    u32 slot=state.slash>=2?body_motion()-5000:(state.slash?248:(frame>=73.0f?247:248));
    /* Native bank mapping at 308B90/308C10: body bank 5 maps to
       part banks 0/1, layers 0/1. CRO 9E64..9E98 supplies 248->247. */
    for (u32 layer=0;layer<2;++layer) {
        if(state.slash>=2 && layer>=U32(m,0x4a4))break;
        u32 selected=state.slash>=2 && layer==1?248:slot;
        if (clip(m,(layer<<8)|selected)) {
#ifdef IAI_HOST_TEST
            test_part_motion(m,p,layer,layer,selected,1,frame);
#else
            ((void (*)(void *,void *,u32,u32,u32,u32,float))
                (iai_main_base()+0x308210))(m,p,layer,layer,selected,1,frame);
#endif
        } else {
#ifdef IAI_HOST_TEST
            test_part_disable(m,p,layer);
#else
            ((void (*)(void *,void *,u32))(iai_main_base()+0x3082a8))(m,p,layer);
#endif
        }
    }
    return 1;
}
/* Scene teardown owns native objects; clear only our pending state here. */
void iai_quest_clear(void) {
    foresight.player=0;foresight.protect=foresight.round=0;
    state.player=0;state.active=state.reset_idle=state.started=0;
    state.hit_registered=state.hit_attempted=state.hit_motion=state.hit_ticket=0;
    air.active=0;helm.speed_owned=helm.point_valid=helm.detached=0;
    helm_move.active=small_regen.remaining=0;
    sweep.valid=sweep.count=0;++sweep.epoch;
    follow.marked=follow.pending=follow.queued=combo.active=combo.buffer=0;
    blend.active=reentry.quiet=reentry.reported=0;reentry.player=0;
    guard.player=0;guard.depth=guard.active=guard.add_first=guard.add_second=0;
    idle_blade.valid=idle_sheath.valid=0;
    for(u32 k=0;k<2;++k){saved[k].part=0;saved[k].parent=0;saved[k].applied=saved[k].flag_mask=0;}
}
int iai_helm_active(void *p) {
    return p && state.player==p && state.active && state.started && state.slash==6 &&
        !helm_natural_landing && U16(p,0x4b4)==0x5f3 && player_gate(p) &&
        U32(p,0x13c4)==helm.owner && U8(p,0x1054)==helm.actor;
}
#ifdef IAI_HOST_TEST
u32 test_active(void) { return state.active; }
u32 test_started(void) { return state.started; }
u32 test_pending(void) { return follow.pending; }
u32 test_queued(void) { return follow.queued; }
u32 test_slash(void) { return state.slash; }
u32 test_exiting(void) { return state.exiting; }
float test_exit_source(float frame){return exit_source_frame(frame);}
u32 test_reentry(void) { return reentry.quiet; }
u32 test_rechain_buffer(void) { return state.rechain_buffer; }
void test_fill_counter(void *cmd,void *p,const float *pos,u32 id,u32 part) { fill_counter(cmd,p,pos,id,part); }
const u32 *test_counter_vtable(void) { return counter_vtable+2; }
u32 test_parried(void) { return state.parried; }
float test_small_regen(void) { return small_regen.remaining; }
void test_reset(void) {
    foresight.player=0;foresight.protect=foresight.round=0;
    air.player=0;air.active=air.in_floor=air.veto=0;
    helm.point_valid=helm.captured=helm.sent=helm.started=helm.speed_owned=helm.launched=helm.frozen=helm.detached=0;
    helm_move.active=0;
    small_regen.player=0;small_regen.remaining=small_regen.carry=small_regen.clock=0;
    state.hit_motion=0;
    idle_blade.valid=idle_sheath.valid=0;reentry.player=0;reentry.quiet=reentry.reported=0;
    state.exiting=state.suppress_chord=0;
    state.player=0;state.active=state.started=state.released=state.reset_idle=0;state.clock=0;
    state.slash=state.zr_released=0;
    state.hit_attempted=state.hit_registered=state.hit_ticket=state.parried=state.counter_attempted=0;
    combo.player=0;combo.active=combo.buffer=combo.action=0;combo.clock=0;
    blend.player=0;blend.active=blend.budget=0;
    state.combo_ready=state.combo_buffer=state.yaw_done=state.rechain_buffer=state.window_cued=state.audio_mask=0;state.blade_log_mask=0;
    guard.player=0;guard.depth=guard.active=guard.add_first=guard.add_second=0;
    follow.player=0;follow.group=follow.marked=follow.pending=follow.queued=follow.budget=follow.action=0;follow.clock=0;
    for (u32 k=0;k<2;++k) { saved[k].part=0;saved[k].parent=0;saved[k].applied=0;saved[k].flag_mask=0; }
}
#endif
