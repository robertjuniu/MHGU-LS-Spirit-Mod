"""Inspect ARM32 NRO dynamic symbols and produce a local annotated disassembly."""
import json
import struct
import sys
from pathlib import Path

def u32(b, off):
    return struct.unpack_from('<I', b, off)[0]

def inspect(path):
    b = Path(path).read_bytes()
    assert b[0x10:0x14] == b'NRO0'
    return b,dynamic(b)

def dynamic(b):
    mod = u32(b, 4)
    assert b[mod:mod+4] == b'MOD0'
    dyn = mod + struct.unpack_from('<i', b, mod+4)[0]
    tags = {}
    for i in range(dyn, len(b)-7, 8):
        tag, val = struct.unpack_from('<II', b, i)
        if not tag:
            break
        tags[tag] = val
    strings, symbols = tags[5], tags[6]
    count = u32(b, tags[4]+4)
    syms = []
    for i in range(count):
        name, addr, size, info, other, shndx = struct.unpack_from('<IIIBBH', b, symbols+16*i)
        end = b.index(0, strings+name)
        name = b[strings+name:end].decode()
        syms.append(dict(name=name,addr=addr,size=size,info=info,shndx=shndx))
    rels=[]
    for start, size in [(tags.get(17,0),tags.get(18,0)),(tags.get(23,0),tags.get(2,0))]:
        for i in range(start,start+size,8):
            off, info = struct.unpack_from('<II',b,i)
            rels.append(dict(offset=off,type=info&255,symbol=syms[info>>8]['name']))
    result = dict(header=b[:128].hex(),tags=tags,symbols=syms,relocations=rels)
    return result

def disassemble(path, dest):
    import capstone
    b,r = inspect(path)
    md=capstone.Cs(capstone.CS_ARCH_ARM,capstone.CS_MODE_ARM)
    md.detail=True
    md.skipdata=True
    imports={x['offset']:x['symbol'] for x in r['relocations'] if x['symbol']}
    names={x['addr']:x['name'] for x in r['symbols'] if x['shndx']}
    # NRO text ends at the rodata start, including the PLT.
    lines=[]
    for ins in md.disasm(b[0x80:u32(b,0x28)],0x80):
        annotation=''
        if ins.address in names:
            lines.append('\n'+names[ins.address]+':')
        if ins.id and ins.mnemonic.startswith('ldr') and len(ins.operands)>1:
            op=ins.operands[1]
            if op.type==capstone.arm.ARM_OP_MEM and op.mem.base==capstone.arm.ARM_REG_PC:
                at=ins.address+8+op.mem.disp
                if 0<=at<len(b)-4:
                    val=u32(b,at)
                    annotation=f' ; [{at:x}]={val:08x}'
        lines.append(f'{ins.address:08x} {ins.bytes.hex():8} {ins.mnemonic:8} {ins.op_str}{annotation}')
    Path(dest).write_text('\n'.join(lines))
    Path(dest).with_suffix('.json').write_text(json.dumps(r,indent=2))
    print('symbols',len(r['symbols']),'relocations',len(r['relocations']))

if __name__=='__main__':
    disassemble(sys.argv[1],sys.argv[2])
