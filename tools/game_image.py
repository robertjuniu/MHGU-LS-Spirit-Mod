"""Read-only NSO decoder. Never opens game files for writing."""
import hashlib
import struct
from pathlib import Path

def u32(b, at):
    return struct.unpack_from('<I', b, at)[0]

def lz4(b, size):
    out=bytearray(); i=0
    def length(n):
        nonlocal i
        if n==15:
            while True:
                v=b[i]; i+=1; n+=v
                if v!=255: break
        return n
    while i<len(b):
        token=b[i]; i+=1
        n=length(token>>4)
        if i+n>len(b): raise ValueError('Truncated LZ4 literals')
        out.extend(b[i:i+n]); i+=n
        if i==len(b): break
        off=int.from_bytes(b[i:i+2],'little'); i+=2
        if not 0<off<=len(out): raise ValueError('Invalid LZ4 offset')
        n=length(token&15)+4
        if len(out)+n>size: raise ValueError('LZ4 output too large')
        chunk=out[-off:]
        out.extend((chunk*((n+off-1)//off))[:n])
    if len(out)!=size: raise ValueError('Wrong LZ4 size')
    return bytes(out)

def read_nso(path):
    b=Path(path).read_bytes()
    if b[:4]!=b'NSO0': raise ValueError('Expected NSO0')
    result=[]
    for i in range(3):
        off,mem,size=struct.unpack_from('<III',b,16+16*i)
        stored=u32(b,96+4*i)
        raw=lz4(b[off:off+stored],size) if u32(b,12)&(1<<i) else b[off:off+size]
        if len(raw)!=size or hashlib.sha256(raw).digest()!=b[160+32*i:192+32*i]:
            raise ValueError('NSO segment hash mismatch')
        result.append(dict(rva=mem,raw=raw))
    size=(result[2]['rva']+len(result[2]['raw'])+u32(b,60)+4095)&~4095
    image=bytearray(size)
    for s in result: image[s['rva']:s['rva']+len(s['raw'])]=s['raw']
    return b,image,result

if __name__=='__main__':
    import sys,json
    b,image,segs=read_nso(sys.argv[1])
    print(json.dumps(dict(module_id=b[64:96].hex().upper(),sha256=hashlib.sha256(b).hexdigest(),image_size=len(image),segments=[dict(rva=s['rva'],size=len(s['raw'])) for s in segs])))
