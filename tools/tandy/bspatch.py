import sys, bz2, struct, zlib
def offtin(b):
    y = int.from_bytes(b, 'little'); 
    if y & (1 << 63): y = -(y & ~(1 << 63))
    return y
def patch(old, pat):
    assert pat[:8] == b'BSDIFF40', pat[:8]
    cl, dl, nl = struct.unpack('<qqq', pat[8:32])
    ctrl = bz2.decompress(pat[32:32+cl]); diff = bz2.decompress(pat[32+cl:32+cl+dl]); extra = bz2.decompress(pat[32+cl+dl:])
    out = bytearray(nl); op = np = cp = dp = ep = 0
    while np < nl:
        x, y, z = (offtin(ctrl[cp+8*i:cp+8*i+8]) for i in range(3)); cp += 24
        for i in range(x):
            b = diff[dp+i]
            if 0 <= op+i < len(old): b = (b + old[op+i]) & 255
            out[np+i] = b
        np += x; dp += x; op += x
        out[np:np+y] = extra[ep:ep+y]; np += y; ep += y; op += z
    return bytes(out)
if __name__ == '__main__':
    old = open(sys.argv[1], 'rb').read(); new = patch(old, open(sys.argv[3], 'rb').read()); open(sys.argv[2], 'wb').write(new)
    print(sys.argv[2], len(new), '%08X' % zlib.crc32(new))
