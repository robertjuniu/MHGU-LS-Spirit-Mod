.syntax unified
.arch armv8-a
.fpu neon
.arm
.section .text.crt0,"ax",%progbits
.global __module_start,iai_main_base
.hidden __module_start,iai_main_base
__module_start:
    b module_entry
    .word mod0 - __module_start
module_entry:
    bx lr
iai_main_base:
    adr r0,__module_start
    ldr r1,.Lmain_distance
    sub r0,r0,r1
    bx lr
.Lmain_distance: .word 0x02715000

/* Every hook saves all core/VFP registers and both status registers.
   56 core +8 status +256 VFP =320 bytes; SP remains 8-byte aligned. */
.macro SAVE
    push {r0-r12,lr}
    mrs r11,APSR
    vmrs r12,FPSCR
    push {r11,r12}
    vpush {d0-d15}
    vpush {d16-d31}
.endm
.macro RESTORE
    vpop {d16-d31}
    vpop {d0-d15}
    pop {r11,r12}
    vmsr FPSCR,r12
    msr APSR_nzcvqg,r11
    pop {r0-r12,lr}
.endm
/* Long jump with IP, LR, SP and flags preserved. Destination delta is
   resolved by the static linker, never an absolute VA from a session. */
.macro JUMP target
    push {r12,lr}
.Lj_anchor\@:
    adr r12,.Lj_anchor\@
    ldr lr,.Lj_delta\@
    add r12,r12,lr
    ldr lr,[sp,#4]
    str r12,[sp,#4]
    pop {r12,pc}
.Lj_delta\@: .word \target - .Lj_anchor\@
.endm
.macro WRAP_VOID name,func,target,args
.global \name
.hidden \name
\name:
    SAVE
    ldr r0,[sp,#264]
.if \args > 1
    ldr r1,[sp,#268]
    ldr r2,[sp,#272]
.endif
    bl \func
    RESTORE
    JUMP \target
.endm
.section .text.hooks,"ax",%progbits
WRAP_VOID input_entry,iai_input,native_input,1
WRAP_VOID restore_entry,iai_restore,native_attachment,3
WRAP_VOID apply_entry,iai_apply,native_matrices,1
.macro WRAP_BOOL name,func,target,args
.global \name
.hidden \name
\name:
    SAVE
    ldr r0,[sp,#264]
.if \args > 1
    ldr r1,[sp,#268]
.endif
    bl \func
    cmp r0,#0
    beq .Ldelegate\@
    RESTORE
    bx lr
.Ldelegate\@:
    RESTORE
    JUMP \target
.endm
WRAP_BOOL idle_entry,iai_idle,idle_gateway,1
WRAP_BOOL sync_entry,iai_part_sync,sync_gateway,2
WRAP_VOID guard_begin_entry,iai_guard_begin,native_guard,1
.global guard_end_entry,guard_success_entry
.hidden guard_end_entry,guard_success_entry
guard_end_entry:
    SAVE
    ldr r0,[sp,#300] /* original r9 = player in 2EE5B0 caller */
    bl iai_guard_end
    RESTORE
    JUMP guard_end_gateway
guard_success_entry:
    SAVE
    ldr r0,[sp,#264]
    bl iai_guard_success
    cmp r0,#0
    beq .Lguard_delegate
    str r0,[sp,#264] /* handled callback returns1; every other register restored */
    RESTORE
    bx lr
.Lguard_delegate:
    RESTORE
    JUMP guard_success_gateway

.global mark_entry,request_entry
.hidden mark_entry,request_entry
mark_entry:
    SAVE
    ldr r0,[sp,#280] /* Original r4 = player at successful hit-data setup. */
    ldr r1,[sp,#288] /* Original r6 = hit source (player or shell). */
    ldr r2,[sp,#300] /* Original r9 = initialized hit record. */
    bl iai_mark_attack
    RESTORE
    JUMP mark_gateway
request_entry:
    SAVE
    ldr r0,[sp,#264]
    ldr r1,[sp,#268]
    ldr r2,[sp,#272]
    ldr r3,[sp,#276]
    bl iai_request
    cmp r0,#0
    beq .Lrequest_delegate
    RESTORE
    bx lr
.Lrequest_delegate:
    RESTORE
    JUMP native_transition

/* 169824: called by the native record iterator, r4 is that record.
   Preserve the original hard-float s0 and stack argument for native HTS
   conversion. Only r2 may change to our temporary shape. An empty geometry
   returns native result r0=0 with every other register/status restored. */
.global geometry_entry
.hidden geometry_entry
geometry_entry:
    SAVE
    ldr r0,[sp,#268]
    ldr r1,[sp,#280]
    ldr r2,[sp,#272]
    ldr r3,[sp,#276]
    bl iai_geometry
    cmp r0,#0
    beq .Lgeometry_empty
    str r0,[sp,#272]
    RESTORE
    JUMP native_geometry
.Lgeometry_empty:
    str r0,[sp,#264]
    RESTORE
    bx lr

.balign 4
.global iai_log_gp
.hidden iai_log_gp
iai_log_gp:
    push {r0-r12,lr}
    adr r0,.Lgp_message
    mov r1,#18 /* exact byte length, no trailing NUL */
    svc #0x27
    pop {r0-r12,pc}
.Lgp_message:
    .ascii "IAI_R38_GP_SUCCESS"
.Lgp_message_end:
.balign 4

.section .rodata.mod0,"a",%progbits
.balign 4
.global mod0
.hidden mod0
mod0:
    .ascii "MOD0"
    .word _DYNAMIC-mod0
    .word __bss_start-mod0
    .word __bss_end-mod0
    .word 0,0
    .word __module_object-mod0
.section .bss.module,"aw",%nobits
.balign 16
.global __module_object
.hidden __module_object
__module_object: .space 256

.section .rodata.tables,"a",%progbits
.balign 16
.global iai_tables
.hidden iai_tables
iai_tables:
    .incbin "f0_tables.bin"
.balign 16
.global iai_f1_tables
.hidden iai_f1_tables
iai_f1_tables:
    .incbin "f1_tables.bin"
.balign 16
.global iai_collision_tables
.hidden iai_collision_tables
iai_collision_tables:
    .incbin "collision_tables.bin"

.section .text,"ax",%progbits
.align 2
.global iai_log_cuts
.hidden iai_log_cuts
iai_log_cuts:
    push {r0-r12,lr}
    adr r0,.Lcuts_message
    mov r1,#18
    svc #0x27
    pop {r0-r12,pc}
.Lcuts_message:
    .asciz "IAI_R38_CUTS_SPAWN"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_combo_first
.hidden iai_log_combo_first
iai_log_combo_first:
    push {r0-r12,lr}
    adr r0,.Lcombo_first_message
    mov r1,#19
    svc #0x27
    pop {r0-r12,pc}
.Lcombo_first_message:
    .asciz "IAI_R38_COMBO_FIRST"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_combo_second
.hidden iai_log_combo_second
iai_log_combo_second:
    push {r0-r12,lr}
    adr r0,.Lcombo_second_message
    mov r1,#20
    svc #0x27
    pop {r0-r12,pc}
.Lcombo_second_message:
    .asciz "IAI_R38_COMBO_SECOND"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_reward_level
.hidden iai_log_reward_level
iai_log_reward_level:
    push {r0-r12,lr}
    adr r0,.Lreward_level
    mov r1,#21
    svc #0x27
    pop {r0-r12,pc}
.Lreward_level:
    .asciz "IAI_R38_REWARD_LEVEL"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_reward_valor
.hidden iai_log_reward_valor
iai_log_reward_valor:
    push {r0-r12,lr}
    adr r0,.Lreward_valor
    mov r1,#21
    svc #0x27
    pop {r0-r12,pc}
.Lreward_valor:
    .asciz "IAI_R38_REWARD_VALOR"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_reward_delay
.hidden iai_log_reward_delay
iai_log_reward_delay:
    push {r0-r12,lr}
    adr r0,.Lreward_delay
    mov r1,#21
    svc #0x27
    pop {r0-r12,pc}
.Lreward_delay:
    .asciz "IAI_R38_REWARD_DELAY"
.align 2

.section .rodata,"a",%progbits
.align 4
.global iai_combo_yaw_tables
.hidden iai_combo_yaw_tables
iai_combo_yaw_tables:
    .incbin "combo_yaw_tables.bin"

.section .text,"ax",%progbits
.align 2
.global iai_log_yaw_exit
.hidden iai_log_yaw_exit
iai_log_yaw_exit:
    push {r0-r12,lr}
    adr r0,.Lyaw_exit_message
    mov r1,#17
    svc #0x27
    pop {r0-r12,pc}
.Lyaw_exit_message:
    .asciz "IAI_R38_YAW_EXIT"
.align 2

.section .text.hooks,"ax",%progbits
.global motion_entry
.hidden motion_entry
motion_entry:
    SAVE
    ldr r0,[sp,#264]
    ldr r1,[sp,#268]
    bl iai_motion_blend
    cmp r0,#0
    beq .Lmotion_delegate
    mov r0,#0
    str r0,[sp,#128] /* saved s0 only; saved s1 and all other VFP survive */
.Lmotion_delegate:
    RESTORE
    JUMP motion_gateway

.section .text,"ax",%progbits
.align 2
.global iai_log_alt_request
.hidden iai_log_alt_request
iai_log_alt_request:
    push {r0-r12,lr}
    adr r0,.Lalt_request_message
    mov r1,#20
    svc #0x27
    pop {r0-r12,pc}
.Lalt_request_message:
    .asciz "IAI_R38_ALT_REQUEST"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_alt_accepted
.hidden iai_log_alt_accepted
iai_log_alt_accepted:
    push {r0-r12,lr}
    adr r0,.Lalt_accepted_message
    mov r1,#21
    svc #0x27
    pop {r0-r12,pc}
.Lalt_accepted_message:
    .asciz "IAI_R38_ALT_ACCEPTED"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_blend_skip
.hidden iai_log_blend_skip
iai_log_blend_skip:
    push {r0-r12,lr}
    adr r0,.Lblend_skip_message
    mov r1,#19
    svc #0x27
    pop {r0-r12,pc}
.Lblend_skip_message:
    .asciz "IAI_R38_BLEND_SKIP"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_chain_buffer
.hidden iai_log_chain_buffer
iai_log_chain_buffer:
    push {r0-r12,lr}
    adr r0,.Lchain_buffer_message
    mov r1,#21
    svc #0x27
    pop {r0-r12,pc}
.Lchain_buffer_message:
    .asciz "IAI_R38_CHAIN_BUFFER"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_chain_commit
.hidden iai_log_chain_commit
iai_log_chain_commit:
    push {r0-r12,lr}
    adr r0,.Lchain_commit_message
    mov r1,#21
    svc #0x27
    pop {r0-r12,pc}
.Lchain_commit_message:
    .asciz "IAI_R38_CHAIN_COMMIT"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_window_cue
.hidden iai_log_window_cue
iai_log_window_cue:
    push {r0-r12,lr}
    adr r0,.Lwindow_cue_message
    mov r1,#19
    svc #0x27
    pop {r0-r12,pc}
.Lwindow_cue_message:
    .asciz "IAI_R38_WINDOW_CUE"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_roll_entry
.hidden iai_log_roll_entry
iai_log_roll_entry:
    push {r0-r12,lr}
    adr r0,.Lroll_entry_message
    mov r1,#19
    svc #0x27
    pop {r0-r12,pc}
.Lroll_entry_message:
    .asciz "IAI_R38_ROLL_ENTRY"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_audio_sheath
.hidden iai_log_audio_sheath
iai_log_audio_sheath:
    push {r0-r12,lr}
    adr r0,.Laudio_sheath_message
    mov r1,#21
    svc #0x27
    pop {r0-r12,pc}
.Laudio_sheath_message:
    .asciz "IAI_R38_AUDIO_SHEATH"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_audio_draw
.hidden iai_log_audio_draw
iai_log_audio_draw:
    push {r0-r12,lr}
    adr r0,.Laudio_draw_message
    mov r1,#19
    svc #0x27
    pop {r0-r12,pc}
.Laudio_draw_message:
    .asciz "IAI_R38_AUDIO_DRAW"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_audio_cut
.hidden iai_log_audio_cut
iai_log_audio_cut:
    push {r0-r12,lr}
    adr r0,.Laudio_cut_message
    mov r1,#18
    svc #0x27
    pop {r0-r12,pc}
.Laudio_cut_message:
    .asciz "IAI_R38_AUDIO_CUT"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_audio_missing
.hidden iai_log_audio_missing
iai_log_audio_missing:
    push {r0-r12,lr}
    adr r0,.Laudio_missing_message
    mov r1,#25
    svc #0x27
    pop {r0-r12,pc}
.Laudio_missing_message:
    .asciz "IAI_R38_AUDIO_NO_MANAGER"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_native_end
.hidden iai_log_native_end
iai_log_native_end:
    push {r0-r12,lr}
    adr r0,.Lnative_end_message
    mov r1,#19
    svc #0x27
    pop {r0-r12,pc}
.Lnative_end_message:
    .asciz "IAI_R38_NATIVE_END"
.align 2

.section .text,"ax",%progbits
.align 2
.global iai_log_cancel7
.hidden iai_log_cancel7
iai_log_cancel7:
    push {r0-r12,lr}
    adr r0,.Lcancel7_message
    mov r1,#16
    svc #0x27
    pop {r0-r12,pc}
.Lcancel7_message:
    .asciz "IAI_R38_CANCEL7"
.align 2

/* Bounded authored text, passed in r0/r1; same existing OutputDebugString SVC. */
.section .text,"ax",%progbits
.align 2
.global iai_log_span
.hidden iai_log_span
iai_log_span:
    push {r0-r12,lr}
    svc #0x27
    pop {r0-r12,pc}
