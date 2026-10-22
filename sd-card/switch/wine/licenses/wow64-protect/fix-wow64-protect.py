#!/usr/bin/env python3
"""Bounded ARM64 compatibility patch for AUTORUN test-build-4 wow64.dll.
Supply a local output for a NULL old-protection pointer; preserve real calls.
"""
import hashlib, struct
from pathlib import Path
ORIGINAL='0e9c7d11812d44ba45334568832abe6929671ebfd9ecfd632c35f31d5ba6f4db'
SITE=0x20994

def patch(data):
    if hashlib.sha256(data).hexdigest()!=ORIGINAL: raise ValueError('Unknown wow64.dll; refusing patch')
    b=bytearray(data)
    get=lambda o:struct.unpack_from('<I',b,o)[0]
    pe=get(0x3c);opt=pe+24
    assert struct.unpack_from('<H',b,pe+4)[0]==0xaa64
    assert struct.unpack_from('<H',b,opt)[0]==0x20b
    count=struct.unpack_from('<H',b,pe+6)[0];table=opt+struct.unpack_from('<H',b,pe+20)[0]
    sections=[table+40*i for i in range(count)]
    first=min(get(o+20) for o in sections if get(o+20))
    header=table+40*count
    if header+40>first or any(b[header:header+40]):raise ValueError('No free section header')
    def off(rva):
        for o in sections:
            if get(o+12)<=rva<get(o+12)+get(o+16):return get(o+20)+rva-get(o+12)
        raise ValueError('Unmapped RVA')
    pos=off(SITE)
    assert get(pos)==0xb9400328 # ldr w8,[x25], the observed NULL read
    align=lambda x,a:(x+a-1)//a*a
    va=align(max(get(o+12)+max(get(o+8),get(o+16)) for o in sections),get(opt+32))
    raw=align(len(b),get(opt+36))
    branch=lambda src,dst:0x14000000|(((dst-src)//4)&0x3ffffff)
    # CBNZ x25,+16; MOV w8,wzr; STR w8,[sp,#4]; ADD x25,sp,#4;
    # LDR w8,[x25]; B back. x25 is restored by the original epilogue.
    words=[0xb5000099,0x2a1f03e8,0xb90007e8,0x910013f9,0xb9400328,branch(va+20,SITE+4)]
    payload=struct.pack('<6I',*words);rawsize=align(len(payload),get(opt+36))
    b.extend(bytes(raw-len(b)));b.extend(payload+bytes(rawsize-len(payload)))
    struct.pack_into('<8sIIIIIIHHI',b,header,b'.d2prot',len(payload),va,rawsize,raw,0,0,0,0,0x60000020)
    struct.pack_into('<H',b,pe+6,count+1)
    struct.pack_into('<I',b,opt+4,get(opt+4)+rawsize)
    struct.pack_into('<I',b,opt+56,align(va+len(payload),get(opt+32)))
    struct.pack_into('<I',b,opt+64,0)
    struct.pack_into('<I',b,pos,branch(SITE,va))
    return bytes(b),va,payload

if __name__=='__main__':
    import argparse
    a=argparse.ArgumentParser();a.add_argument('source',type=Path);a.add_argument('output',type=Path);s=a.parse_args()
    data,va,_=patch(s.source.read_bytes());s.output.write_bytes(data)
    print('wow64 NULL protection compatibility:',hex(va),hashlib.sha256(data).hexdigest())
