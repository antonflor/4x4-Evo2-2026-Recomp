# Minimal InstallShield 5 cabinet extractor (single volume), written for the 4x4 Evo 2 disc.
# usage: python is5_extract.py data1.hdr data1.cab <outdir> [--list]
import struct, sys, os, zlib
hdr=open(sys.argv[1],'rb').read(); cabp=sys.argv[2]; out=sys.argv[3]; lst='--list' in sys.argv
u32=lambda b,o: struct.unpack_from('<I',b,o)[0]; u16=lambda b,o: struct.unpack_from('<H',b,o)[0]
assert u32(hdr,0)==0x28635349, 'bad sig'
ver=u32(hdr,4); cdo=u32(hdr,12)
p=cdo+0xc
fto=u32(hdr,p); dirc=u32(hdr,p+0x10); filec=u32(hdr,p+0x1c)
base=cdo+fto
ft=[u32(hdr,base+4*i) for i in range(dirc+filec)]
cstr=lambda o: hdr[o:hdr.index(b'\0',o)].decode('cp1252')
dirs=[cstr(base+ft[i]) for i in range(dirc)]
print('version %08x dirs %d files %d'%(ver,dirc,filec), file=sys.stderr)
cab=open(cabp,'rb')
tot=0; bad=0
for i in range(filec):
    q=base+ft[dirc+i]
    no,di,fl,es,cs=struct.unpack_from('<IIHII',hdr,q); do=u32(hdr,q+18+0x14)
    if fl&8 or no==0: continue
    name=cstr(base+no); d=dirs[di] if di<dirc else '?'
    rel=os.path.join(d.replace('\\','/'),name)
    tot+=es
    if lst: print('%10d %10d %x %s'%(es,cs,fl,rel)); continue
    cab.seek(do); raw=cab.read(cs)
    if fl&4:
        data=bytearray(); o=0
        while o<len(raw) and len(data)<es:
            n=u16(raw,o); o+=2
            data+=zlib.decompressobj(-15).decompress(raw[o:o+n]); o+=n
    else: data=raw
    if len(data)!=es: bad+=1; print('SIZE MISMATCH',rel,len(data),es,file=sys.stderr)
    fp=os.path.join(out,rel); os.makedirs(os.path.dirname(fp),exist_ok=True); open(fp,'wb').write(bytes(data[:es]))
print('total expanded',tot,'bad',bad,file=sys.stderr)
