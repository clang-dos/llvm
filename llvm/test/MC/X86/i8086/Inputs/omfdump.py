#!/usr/bin/env python3
"""Dump the records of an OMF (Intel/Microsoft 16-bit) object file.

Used by the i8086 OMF tests. Prints one line per record (fixups are indented
under their LEDATA) and exits with the number of bad checksums.
"""
import sys

NAMES = {0x80: 'THEADR', 0x88: 'COMENT', 0x8A: 'MODEND', 0x8C: 'EXTDEF',
         0x90: 'PUBDEF', 0x94: 'LINNUM', 0x96: 'LNAMES', 0x98: 'SEGDEF',
         0x9A: 'GRPDEF', 0x9C: 'FIXUPP', 0xA0: 'LEDATA', 0xA2: 'LIDATA',
         0xB0: 'COMDEF', 0xB4: 'LEXTDEF', 0xB6: 'LPUBDEF', 0xC2: 'COMDAT',
         0xC6: 'ALIAS'}
LOC = {0: 'lobyte', 1: 'offset16', 2: 'base16', 3: 'ptr16:16', 4: 'hibyte',
       5: 'offset16(ldr)', 9: 'offset32', 11: 'ptr16:32', 13: 'offset32(ldr)'}


class R:
    def __init__(self, b):
        self.b, self.i = b, 0

    def u8(self):
        v = self.b[self.i]; self.i += 1; return v

    def u16(self):
        v = self.b[self.i] | self.b[self.i + 1] << 8; self.i += 2; return v

    def name(self):
        n = self.u8(); s = self.b[self.i:self.i + n]; self.i += n
        return s.decode('latin1')

    def idx(self):
        v = self.u8()
        if v & 0x80:
            v = (v & 0x7F) << 8 | self.u8()
        return v

    def more(self):
        return self.i < len(self.b)


def dump(path, out=sys.stdout):
    data = open(path, 'rb').read()
    pos = 0
    lnames = ['']
    errors = 0
    while pos < len(data):
        typ = data[pos]
        ln = data[pos + 1] | data[pos + 2] << 8
        rec = data[pos + 3:pos + 3 + ln - 1]
        if sum(data[pos:pos + 3 + ln]) & 0xFF and data[pos + 2 + ln] != 0:
            print('  ** bad checksum', file=out); errors += 1
        pos += 3 + ln
        r = R(rec)
        nm = NAMES.get(typ & 0xFE, '%02X' % typ)
        if typ & 1:
            nm += '32'
        if typ == 0x80:
            print('THEADR', repr(r.name()), file=out)
        elif typ == 0x88:
            r.u8(); cls = r.u8()
            print('COMENT class=%02X %r' % (cls, bytes(rec[2:])), file=out)
        elif typ == 0x96:
            base = len(lnames)
            while r.more():
                lnames.append(r.name())
            print('LNAMES', ' '.join('%d=%r' % (base + k, n) for k, n in
                                     enumerate(lnames[base:])), file=out)
        elif typ == 0x98:
            a = r.u8(); ln16 = r.u16(); si, ci, oi = r.idx(), r.idx(), r.idx()
            align = {0: 'abs', 1: 'byte', 2: 'word', 3: 'para', 4: 'page', 5: 'dword'}[a >> 5]
            comb = {0: 'private', 2: 'public', 4: 'public4', 5: 'stack', 6: 'common', 7: 'public7'}.get((a >> 2) & 7, '?')
            print('SEGDEF %s align=%s combine=%s big=%d use32=%d len=%04X class=%s' % (
                lnames[si], align, comb, a >> 1 & 1, a & 1, ln16, lnames[ci]), file=out)
        elif typ == 0x9A:
            g = r.idx(); segs = []
            while r.more():
                r.u8(); segs.append(r.idx())
            print('GRPDEF %s segs=%s' % (lnames[g], segs), file=out)
        elif typ in (0x8C, 0xB4):
            while r.more():
                n = r.name(); t = r.idx()
                print('%s %r type=%d' % (nm, n, t), file=out)
        elif typ == 0xC6:
            while r.more():
                n = r.name(); t = r.name()
                print('ALIAS %r -> %r' % (n, t), file=out)
        elif typ in (0x90, 0xB6):
            g = r.idx(); s = r.idx()
            if s == 0:
                r.u16()
            while r.more():
                n = r.name(); off = r.u16(); t = r.idx()
                print('%s %r group=%d seg=%d off=%04X' % (nm, n, g, s, off), file=out)
        elif typ == 0xA0:
            s = r.idx(); off = r.u16()
            print('LEDATA seg=%d off=%04X len=%d %s' % (s, off, len(rec) - r.i, bytes(rec[r.i:]).hex()), file=out)
        elif typ == 0x9C:
            while r.more():
                b = r.u8()
                if not b & 0x80:
                    print('  THREAD %02X' % b, file=out); r.idx(); continue
                lo = r.u8(); locat = (b << 8) | lo
                m = (b >> 6) & 1; loc = (b >> 2) & 0xF; off = locat & 0x3FF
                fd = r.u8()
                f = (fd >> 4) & 7; t = fd & 3; p = (fd >> 2) & 1
                fdat = r.idx() if (not fd & 0x80) and f < 3 else None
                tdat = r.idx() if not fd & 8 else None
                disp = None if p else r.u16()
                print('  FIXUP @%03X %s %s frame=F%d%s target=T%d:%s disp=%s' % (
                    off, 'seg' if m else 'self', LOC.get(loc, loc), f,
                    '' if fdat is None else ':%d' % fdat, t + 4 * p, tdat, disp), file=out)
        elif typ == 0xC2:
            fl = r.u8(); attr = r.u8(); al = r.u8(); off = r.u16(); r.idx()
            g = s = 0
            if attr & 0xF == 0:
                g = r.idx(); s = r.idx()
            n = r.idx()
            print('COMDAT %r flags=%02X attr=%02X align=%d group=%d seg=%d off=%04X len=%d %s' % (
                lnames[n], fl, attr, al, g, s, off, len(rec) - r.i,
                bytes(rec[r.i:]).hex()), file=out)
        elif typ == 0xB0:
            while r.more():
                n = r.name(); r.idx(); kind = r.u8()

                def length():
                    b = r.u8()
                    if b < 0x80:
                        return b
                    if b == 0x81:
                        return r.u16()
                    if b == 0x84:
                        return r.u16() | r.u8() << 16
                    return r.u16() | r.u16() << 16
                if kind == 0x61:
                    n_el = length(); size = length()
                    print('COMDEF %r far count=%d size=%d' % (n, n_el, size), file=out)
                else:
                    print('COMDEF %r near size=%d' % (n, length()), file=out)
        elif typ in (0x8A, 0x8B):
            mt = r.u8()
            if mt & 0x40:
                fd = r.u8(); f = (fd >> 4) & 7; t = fd & 3; p = (fd >> 2) & 1
                fdat = r.idx() if f < 3 else None
                tdat = r.idx()
                disp = None if p else r.u16()
                print('MODEND main=%d start frame=F%d%s target=T%d:%d disp=%s' % (
                    mt >> 7, f, '' if fdat is None else ':%d' % fdat, t + 4 * p,
                    tdat, '%04X' % disp if disp is not None else None), file=out)
            else:
                print('MODEND main=%d' % (mt >> 7), file=out)
        else:
            print('%s %s' % (nm, rec.hex()), file=out)
    return errors


if __name__ == '__main__':
    sys.exit(dump(sys.argv[1]))
