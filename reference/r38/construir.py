import struct,hashlib
sha=lambda b:hashlib.sha256(b).hexdigest()
U=lambda b,p:struct.unpack_from("<I",b,p)[0]
align=lambda n:(n+4095)&~4095
def require(ok,why):
    if not ok:raise ValueError(why)

def load_elf(path):
    b=path.read_bytes();require(b[:7]==b'\x7fELF\x01\x01\x01','Expected ELF32LE')
    require(struct.unpack_from('<H',b,18)[0]==40,'Expected ARM')
    po=U(b,28);pn=struct.unpack_from('<H',b,44)[0];pe=struct.unpack_from('<H',b,42)[0]
    loads=[]
    for i in range(pn):
        typ,off,va,pa,n,mem,flags,al=struct.unpack_from('<8I',b,po+i*pe)
        if typ==1:loads.append(dict(rva=va,raw=b[off:off+n],mem=mem,flags=flags))
    require(len(loads)==3 and [s['flags'] for s in loads]==[5,4,6],'Unexpected load permissions')
    require(loads[0]['rva']==0 and all(s['rva']%4096==0 for s in loads),'Unaligned segment')
    return b,loads

def nso_info(b):
    require(b[:4]==b'NSO0','Expected NSO')
    data_rva,data_size=U(b,0x34),U(b,0x38)
    return dict(module_id=b[64:96].hex().upper(),sha256=sha(b),image_size=align(data_rva+data_size+U(b,0x3c)))

def branch(src,dst,link=False):
    d=dst-src-8;require(d%4==0 and -(1<<25)<=d<(1<<25),'Branch outside A32 range')
    return struct.pack('<I',(0xEB000000 if link else 0xEA000000)|((d>>2)&0xffffff))

def veneer(at,target):
    # push IP/LR; anchor ADR; LDR LR,+12; ADD IP; restore LR;
    # overwrite saved PC slot; pop IP/PC; signed PC-relative delta.
    anchor=at+4
    return struct.pack('<8I',0xE92D5000,0xE24FC008,0xE59FE00C,0xE08CC00E,
                       0xE59DE004,0xE58DC004,0xE8BD9000,(target-anchor)&0xffffffff)
