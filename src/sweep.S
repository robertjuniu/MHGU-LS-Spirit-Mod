/* Uses the existing SAVE/RESTORE/JUMP macros; original project stays read-only. */
.section .text.helm_sweep,"ax",%progbits
.balign 4
.global helm_geometry_entry,sweep_query_gateway,sweep_contact_entry
.hidden helm_geometry_entry,sweep_query_gateway,sweep_contact_entry
/* Same hidden record argument bridge as NRO13010. s0 remains the native scale. */
helm_geometry_entry:
    push {r4,lr}
    sub sp,sp,#8
    ldr r12,[sp,#16]
    str r12,[sp]
    str r4,[sp,#4]
    bl iai_geometry_world
    add sp,sp,#8
    pop {r4,pc}
sweep_query_gateway:
    push {r4-r8,r9,r10,r11,lr}
    JUMP sweep_native_query_body
sweep_contact_entry:
    SAVE
    ldr r0,[sp,#280]
    ldr r1,[sp,#284]
    ldr r2,[sp,#292]
    bl iai_sweep_contact
    RESTORE
    ldr r6,[r4,#12]
    JUMP sweep_native_contact_rest
.global fx_particle_gateway
.hidden fx_particle_gateway
fx_particle_gateway:
    push {r4,lr}
    JUMP fx_particle_body
.section .text.foresight_damage,"ax",%progbits
.balign 4
.global foresight_damage_entry
.hidden foresight_damage_entry
foresight_damage_entry:
    SAVE
    ldr r0,[sp,#264]
    bl iai_foresight_damage
    cmp r0,#0
    beq .Lforesight_damage_native
    mov r0,#1
    str r0,[sp,#264]
    RESTORE
    bx lr
.Lforesight_damage_native:
    RESTORE
    movw r1,#0x2506
    ldrh r1,[r0,r1]
    mov r0,#0
    JUMP foresight_damage_rest
