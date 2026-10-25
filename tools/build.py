"""Build the incremental Iai Slash prototype; original projects are read-only."""
import hashlib
import importlib.util
import json
import re
import shutil
import struct
import subprocess
import sys
import zipfile
from pathlib import Path
from prepare import ROOT,PRIOR,main as prepare

from game_image import read_nso,u32
from inspect_nro import dynamic

BASE=PRIOR/'r38'
NDK=Path(__import__('os').environ['MHGU_NDK_BIN'])
BUILD=ROOT/'build'
PACKAGE=ROOT/'dist/MHGU Iai Spirit Slash'
sha=lambda b:hashlib.sha256(b).hexdigest()

def run(args):
    p=subprocess.run(list(map(str,args)),cwd=BUILD,capture_output=True)
    if p.returncode:raise RuntimeError(p.stderr.decode(errors='replace'))
    return p.stdout

def main():
    prepare()
    spec=importlib.util.spec_from_file_location('baseline_build',BASE/'construir.py')
    baseline=importlib.util.module_from_spec(spec);spec.loader.exec_module(baseline)
    m=json.loads((PRIOR/'outputs/port_iai_r38_audio_estilos/MANIFESTO_R38.json').read_text())
    game=Path(__import__('os').environ['MHGU_GAME'])
    main_raw=(game/'exefs/main').read_bytes()
    assert sha(main_raw)==m['main_sha256'] and main_raw[64:96].hex().upper()==m['module_id']
    assert baseline.nso_info(main_raw)['image_size']+baseline.nso_info((game/'exefs/subsdk0').read_bytes())['image_size']==0x2715000
    for name in ['f0_tables.bin','f1_tables.bin','collision_tables.bin','combo_yaw_tables.bin']:
        shutil.copyfile(PRIOR/'dados_locais/pacote_iai_r38'/name,BUILD/name)
    flags=['--target=armv7a-none-eabihf','-march=armv8-a','-marm','-mfpu=neon-vfpv4','-mfloat-abi=hard',
           '-DIAI_RELEASE','-Oz','-fPIC','-fvisibility=hidden','-ffreestanding','-fno-builtin','-fno-math-errno',
           '-ffp-contract=off','-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables',
           '-Wall','-Wextra','-Werror','-I'+str(BUILD),'-I'+str(BASE),'-I'+str(PRIOR/'work/port_iai_handler')]
    entry=(BASE/'entry.S').read_text()
    assert entry.count('JUMP native_geometry')==1
    entry=entry.replace('JUMP native_geometry','JUMP helm_geometry_entry')
    # Reuse text padding for708 immutable bytes, keeping subsequent addons at their old bases.
    needle='.section .rodata,"a",%progbits\n.align 4\n.global iai_combo_yaw_tables'
    assert entry.count(needle)==1
    entry=entry.replace(needle,needle.replace('.rodata,"a"','.text.compat_tables,"ax"'))
    needle='.balign 16\n.global iai_collision_tables'
    assert entry.count(needle)==1
    entry=entry.replace(needle,'.section .text.compat_tables,"ax",%progbits\n'+needle)
    _,game_image,_=read_nso(game/'exefs/main')
    scene_hooks=[0x3a93ac,0x3a9314,0x3a24ac]
    for k,at in enumerate(scene_hooks):
        words=struct.unpack_from('<3I',game_image,at)
        assert words==[(0xe5d01050,0xe3510002,0x112fff1e),(0xe92d4010,0xe1a04000,0xe5d40050),(0xe92d4010,0xe1a04000,0xe3a00003)][k]
        entry+=f'\n.section .text.fx_scene,"ax",%progbits\n.balign 4\n.global fx_scene_entry{k}\n.hidden fx_scene_entry{k}\nfx_scene_entry{k}:\n    SAVE\n    ldr r0,[sp,#264]\n    bl fx_scene\n    RESTORE\n'
        entry+=''.join(f'    .word 0x{w:08x}\n' for w in words)+f'    JUMP fx_scene_rest{k}\n'
    # Keep logger symbols for ABI/tests, without debug SVCs in the public build.
    logger_count=len(re.findall(r'^iai_log_\w+:',entry,re.M))
    entry,n=re.subn(r'^(iai_log_\w+):\n.*?^    pop \{r0-r12,pc\}\n',r'\1:\n    bx lr\n',entry,flags=re.M|re.S)
    assert n==logger_count and n>0 and 'svc #0x27' not in entry
    (BUILD/'entry.S').write_text(entry+'\n.include "'+(ROOT/'src/sweep.S').as_posix()+'"\n')
    linker=(BASE/'module.ld').read_text()
    linker+='\nHIDDEN(sweep_native_query_body = 0x0016C0C4 - 0x02715000);\nHIDDEN(sweep_native_contact_rest = 0x0016C940 - 0x02715000);\n'
    linker+=''.join(f'HIDDEN(fx_scene_rest{k} = 0x{at+12:08x} - 0x02715000);\n' for k,at in enumerate(scene_hooks))
    linker+='HIDDEN(fx_particle_body = 0x009B710C - 0x02715000);\n'
    linker+='HIDDEN(foresight_damage_rest = 0x002A6800 - 0x02715000);\n'
    (BUILD/'module.ld').write_text(linker)
    sources=[ROOT/'src/runtime.c',ROOT/'src/effects.c',BASE/'weapon_settle.c',PRIOR/'work/port_iai_handler/pose_f0.c',
             BASE/'pose_f1.c',BASE/'collision.c',BUILD/'entry.S',BUILD/'small-pose.S']
    objects=[]
    for p in sources:
        obj=BUILD/(p.stem+'.o');run([NDK/'clang.exe',*flags,'-c',p,'-o',obj]);objects.append(obj)
    elf=BUILD/'overhaul.elf'
    run([NDK/'ld.lld.exe','-m','armelf','-shared','-Bsymbolic','--hash-style=sysv','--no-undefined',
         '-T',BUILD/'module.ld','-o',elf,*objects])
    raw,loads=baseline.load_elf(elf)
    dyn=run([NDK/'llvm-readelf.exe','-d','-r',elf]).decode()
    assert 'TEXTREL' not in dyn and 'NEEDED' not in dyn
    relocs=[]
    for line in dyn.splitlines():
        match=re.match(r'^([0-9a-f]{8})\s+([0-9a-f]{8})\s+(R_ARM_\S+)',line)
        if match:
            off,info=int(match[1],16),int(match[2],16)
            assert info==23 and match[3]=='R_ARM_RELATIVE'
            s=loads[2];assert s['rva']<=off<=s['rva']+len(s['raw'])-4
            relocs.append(off)
    symbols={}
    for line in run([NDK/'llvm-nm.exe','-n',elf]).decode().splitlines():
        match=re.match(r'([0-9a-fA-F]+)\s+\w\s+(\S+)',line)
        if match:symbols[match[2]]=int(match[1],16)
    header=bytearray(256);header[:4]=b'NSO0';struct.pack_into('<I',header,12,0x38)
    header[64:96]=hashlib.sha256(b''.join(s['raw'] for s in loads)).digest()
    at=256;payload=bytearray()
    for i,s in enumerate(loads):
        struct.pack_into('<III',header,16+i*16,at,s['rva'],len(s['raw']))
        struct.pack_into('<I',header,96+i*4,len(s['raw']))
        header[160+i*32:192+i*32]=hashlib.sha256(s['raw']).digest()
        payload.extend(s['raw']);at+=len(s['raw'])
    assert loads[2]['rva']+loads[2]['mem']<=0x2c000, 'Runtime exceeds the module reservation'
    # Keep following addon module bases stable even when tables move into RomFS.
    struct.pack_into('<I',header,60,0x2c000-loads[2]['rva']-len(loads[2]['raw']))
    exefs=PACKAGE/'exefs';exefs.mkdir(exist_ok=True)
    (exefs/'subsdk1').write_bytes(header+payload)
    cave=0x13f1dec;module=0x2715000
    _,main_image,_=read_nso(game/'exefs/main')
    assert main_image[cave+432:cave+528]==bytes(96), 'Air/sweep veneer cave is occupied'
    assert u32(main_image,0x16c0c0)==0xe92d4ff0 and u32(main_image,0x16c93c)==0xe594600c, 'Sweep hook instructions changed'
    assert u32(main_image,0x174d2cc)==0x2e846c, 'Native terrain virtual changed'
    assert any(r['offset']==0x174d2cc and r['type']==23 for r in dynamic(main_image)['relocations']), 'Terrain virtual must be relocated by main RTLD'
    entries=['input_entry','idle_entry','restore_entry','apply_entry','sync_entry','mark_entry','request_entry']
    blob=b''.join(baseline.veneer(cave+i*32,module+symbols[n]) for i,n in enumerate(entries))
    original=next(r for r in m['records'] if int(r['rva'],16)==cave)
    old=bytes.fromhex(original['after'])
    blob+=old[224:248]
    blob+=baseline.veneer(cave+248,module+symbols['geometry_entry'])
    for off,n in [(280,'guard_begin_entry'),(312,'guard_end_entry'),(344,'guard_success_entry')]:
        blob+=baseline.veneer(cave+off,module+symbols[n])
    blob+=old[376:392]+baseline.veneer(cave+392,module+symbols['motion_entry'])+old[424:432]
    assert len(blob)==432
    blob+=baseline.veneer(cave+432,module+symbols['iai_air_physics'])
    blob+=baseline.veneer(cave+464,module+symbols['iai_sweep_query'])
    blob+=baseline.veneer(cave+496,module+symbols['sweep_contact_entry'])
    records=[]
    for r in m['records']:
        records.append((int(r['rva'],16),blob if int(r['rva'],16)==cave else bytes.fromhex(r['after'])))
    records.append((0x174d2cc,struct.pack('<I',cave+432)))
    records.append((0x16c0c0,baseline.branch(0x16c0c0,cave+464)))
    records.append((0x16c93c,baseline.branch(0x16c93c,cave+496)))
    assert u32(main_image,0x9b7108)==0xe92d4010
    records.append((0x9b7108,baseline.branch(0x9b7108,module+symbols['fx_particle_update'])))
    assert u32(main_image,0x174cf64+0x12c)==0x2ae6b0
    assert any(r['offset']==0x174cf64+0x12c and r['type']==23 for r in dynamic(main_image)['relocations'])
    records.append((0x174cf64+0x12c,struct.pack('<I',module+symbols['fx_event'])))
    for k,at in enumerate(scene_hooks):
        # ABI function entry: IP is scratch. Replay all three displaced instructions in the bridge.
        records.append((at,struct.pack('<3I',0xe59fc000,0xe08ff00c,(module+symbols[f'fx_scene_entry{k}']-at-12)&0xffffffff)))
    assert struct.unpack_from('<3I',main_image,0x2a67f4)==(0xe3021506,0xe19010b1,0xe3a00000)
    records.append((0x2a67f4,struct.pack('<3I',0xe59fc000,0xe08ff00c,(module+symbols['foresight_damage_entry']-0x2a67f4-12)&0xffffffff)))
    ips=bytearray(b'IPS32')
    for at,b in records:ips.extend(struct.pack('>IH',at+256,len(b)));ips.extend(b)
    ips.extend(b'EEOF');(exefs/(m['module_id']+'.ips')).write_bytes(ips)
    (PACKAGE/'LEIA-ME.txt').write_bytes((ROOT/'INSTALACAO.md').read_bytes())
    assert baseline.nso_info(header+payload)['image_size']==0x2c000, 'Do not shift other addon module bases'
    manifest=dict(prototype='v1.0.0-performance-release',diagnostics=False,addon_name='MHGU LS Spirit Mod',helm_breaker_stage='thrust_takeoff_airborne_descent_landing_seven_native_impacts',diagnostic_only=False,title_id=m['title_id'],module_id=m['module_id'],main_sha256=m['main_sha256'],
        baseline='R38',hooks=21,code_hooks=19,vtable_hooks=2,cave_bytes=528,forge_required=False,gameplay_verified=False,
        patch_ranges=[dict(rva=at,bytes=len(b)) for at,b in records],elf_symbols=symbols,dynamic_relocations=relocs,
        baseline_source_hash='8af54207fdfb85dc0fe4c5a3c6207c74e7749b13f07a9bfc26f0f269308d2916',runtime_source_hash=sha((ROOT/'src/runtime.c').read_bytes()),
        files={p.relative_to(PACKAGE).as_posix():sha(p.read_bytes()) for p in PACKAGE.rglob('*') if p.is_file()})
    (ROOT/'research/build-p1.json').write_text(json.dumps(manifest,indent=2))
    run([NDK/'llvm-objdump.exe','-d',elf])
    print(json.dumps(dict(nso_bytes=len(header+payload),relocations=len(relocs),hooks=21,files=len(manifest['files']))))

if __name__=='__main__':main()
