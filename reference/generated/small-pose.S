.syntax unified
.arch armv8-a
.arm
.section .text.small_pose,"ax",%progbits
.balign 16
.global small_pose_raw
.hidden small_pose_raw
small_pose_raw:
.incbin "pose-code.bin",0,2276
.word small_descriptors - (small_pose_raw+312)
.incbin "pose-code.bin",2280,0
.word small_descriptors - (small_pose_raw+156)
.incbin "pose-code.bin",2284,0
.word small_descriptors - (small_pose_raw+440)
.incbin "pose-code.bin",2288,0
.word small_descriptors - (small_pose_raw+424)
.incbin "pose-code.bin",2292,0
.word small_descriptors - (small_pose_raw+344)
.incbin "pose-code.bin",2296,0
.word small_descriptors - (small_pose_raw+472)
.incbin "pose-code.bin",2300,0
.word small_descriptors - (small_pose_raw+284)
.incbin "pose-code.bin",2304,0
.word small_descriptors - (small_pose_raw+392)
.incbin "pose-code.bin",2308,0
.word small_descriptors - (small_pose_raw+376)
.incbin "pose-code.bin",2312,0
.word small_descriptors - (small_pose_raw+488)
.incbin "pose-code.bin",2316,0
.word small_descriptors - (small_pose_raw+360)
.incbin "pose-code.bin",2320,0
.word small_descriptors - (small_pose_raw+456)
.incbin "pose-code.bin",2324,0
.word small_descriptors - (small_pose_raw+328)
.incbin "pose-code.bin",2328,0
.word small_descriptors - (small_pose_raw+504)
.incbin "pose-code.bin",2332,0
.word small_descriptors - (small_pose_raw+408)
.incbin "pose-code.bin",2336,0
.word small_tables_cell - (small_pose_raw+668)
.incbin "pose-code.bin",2340,0
.word small_api_cell - (small_pose_raw+1468)
.section .rodata.small_pose,"a",%progbits
.balign 16
small_descriptors:
.incbin "pose-descriptors.bin"
small_tables:
.incbin "small-pose-tables.bin"
.section .data.small_pose,"aw",%progbits
.balign 4
.global small_tables_cell
.hidden small_tables_cell
small_tables_cell:
.word small_tables
small_api_cell:
.word small_api
small_api:
.zero 88
.word small_pose_bone
.section .text.helm_point_local,"ax",%progbits
.balign 4
.global helm_point_local
.hidden helm_point_local
helm_point_local:
.incbin "point-local.bin"
.section .text.helm_sound_raw,"ax",%progbits
.balign 4
.global helm_sound_raw
.hidden helm_sound_raw
helm_sound_raw:
.incbin "helm-sound.bin",0,552
.word helm_sound_base - (helm_sound_raw+52)
.incbin "helm-sound.bin",556,0
.word helm_sound_base - (helm_sound_raw+176)
.incbin "helm-sound.bin",560,0
.word helm_sound_base - (helm_sound_raw+328)
.incbin "helm-sound.bin",564,0
.word helm_sound_base - (helm_sound_raw+428)
.incbin "helm-sound.bin",568,0
.word helm_sound_base - (helm_sound_raw+228)
.incbin "helm-sound.bin",572,0
.word helm_sound_base - (helm_sound_raw+260)
.incbin "helm-sound.bin",576,0
.word helm_sound_base - (helm_sound_raw+516)
.section .data.helm_sound,"aw",%progbits
.balign 4
.global helm_sound_base
.hidden helm_sound_base
helm_sound_base:
.word 0
.section .text.fx_shell,"ax",%progbits
.balign 4
.global fx_shell_raw
.hidden fx_shell_raw
fx_shell_raw:
.incbin "shell-fx.bin",0,480
.word fx_native+0 - (fx_shell_raw+44)
.incbin "shell-fx.bin",484,0
.word fx_native+8 - (fx_shell_raw+116)
.incbin "shell-fx.bin",488,0
.word fx_native+4 - (fx_shell_raw+136)
.incbin "shell-fx.bin",492,0
.word fx_native+12 - (fx_shell_raw+208)
.incbin "shell-fx.bin",496,0
.word fx_native+4 - (fx_shell_raw+220)
.incbin "shell-fx.bin",500,0
.word fx_native+4 - (fx_shell_raw+280)
.incbin "shell-fx.bin",504,0
.word fx_native+4 - (fx_shell_raw+336)
.incbin "shell-fx.bin",508,0
.word fx_native+4 - (fx_shell_raw+376)
.section .text.foresight_probe,"ax",%progbits
.balign 4
.global foresight_probe_raw
.hidden foresight_probe_raw
foresight_probe_raw:
.incbin "foresight-probe.bin",0,48
bl foresight_probe_gate
.incbin "foresight-probe.bin",52,80
bl foresight_probe_frame
.incbin "foresight-probe.bin",136,496
b foresight_probe_success
.incbin "foresight-probe.bin",636,396
.word foresight_probe_ctx - (foresight_probe_raw+24)
