# 给程序的 arm64 那一半加一条「启动时顺带加载翻译层」的指令（LC_LOAD_WEAK_DYLIB：找不到也照常启动）。
# 用法：python3 add_dylib.py 程序文件 @executable_path/../Frameworks/cczh.dylib
import struct, sys
path, dylib = sys.argv[1], sys.argv[2].encode()
d = bytearray(open(path, 'rb').read())
def slices():
    if d[:4] == b'\xca\xfe\xba\xbe':
        n = struct.unpack('>I', d[4:8])[0]
        for i in range(n):
            cpu, sub, off, size, al = struct.unpack('>IIIII', d[8 + i*20: 28 + i*20]); yield cpu, off
    else: yield struct.unpack('<I', d[4:8])[0], 0
done = False
for cpu, off in slices():
    if cpu != 0x0100000c: continue                      # 只动 arm64
    magic, _, _, _, ncmds, sizeofcmds, flags, _ = struct.unpack('<IIIIIIII', d[off:off+32])
    assert magic == 0xfeedfacf
    end = off + 32 + sizeofcmds
    first = min(struct.unpack('<I', d[p+48+ 0:p+52])[0] for p in [off + 32]) if False else None
    # 找第一个节的文件偏移，算头部还剩多少空
    p = off + 32; minoff = 1 << 62
    for _ in range(ncmds):
        cmd, size = struct.unpack('<II', d[p:p+8])
        if cmd == 0x19:
            nsects = struct.unpack('<I', d[p+64:p+68])[0]
            for s in range(nsects):
                so = struct.unpack('<I', d[p+72+s*80+48:p+72+s*80+52])[0]
                if so: minoff = min(minoff, so)
        if cmd in (0xc, 0x80000018) and dylib in d[p:p+size]: print('已经加过'); sys.exit(0)
        p += size
    name = dylib + b'\0'; size = (24 + len(name) + 7) & ~7
    assert end + size <= off + minoff, '头部空间不够'
    assert not any(d[end:end+size]), '头部后面不是空的'
    d[end:end+size] = struct.pack('<IIIIII', 0x80000018, size, 24, 2, 0x10000, 0x10000) + name.ljust(size - 24, b'\0')
    d[off+16:off+24] = struct.pack('<II', ncmds + 1, sizeofcmds + size); done = True
assert done, '没找到 arm64'
open(path, 'wb').write(d); print('已加载指令：', dylib.decode())
