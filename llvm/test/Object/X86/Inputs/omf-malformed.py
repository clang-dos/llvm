"""Write malformed OMF objects for omf-malformed.test, as <prefix>.<name>.obj."""
import sys


def rec(typ, body, checksum=True):
    data = bytes([typ]) + (len(body) + 1).to_bytes(2, "little") + body
    return data + bytes([(-sum(data)) & 0xFF if checksum else 0])


def name(s):
    return bytes([len(s)]) + s.encode()


THEADR = rec(0x80, name("bad.c"))
LNAMES = rec(0x96, name("") + name("_TEXT") + name("CODE"))
# SEGDEF: byte aligned, public, 4 bytes long, name 2, class 3, overlay 1.
SEGDEF = rec(0x98, bytes([0x28]) + (4).to_bytes(2, "little") + bytes([2, 3, 1]))
MODEND = rec(0x8A, bytes([0]))

files = {
    "ok": THEADR + LNAMES + SEGDEF + MODEND,
    "nochecksum": rec(0x80, name("bad.c"), checksum=False) + MODEND,
    "noend": THEADR + LNAMES + SEGDEF,
    "checksum": THEADR[:-1] + bytes([THEADR[-1] ^ 1]) + MODEND,
    "pastend": THEADR + bytes([0x96, 0x40, 0x00, 0x00]),
    # LEDATA at offset 2 with 4 bytes: past the 4-byte segment.
    "data": THEADR + LNAMES + SEGDEF
    + rec(0xA0, bytes([1, 2, 0, 1, 2, 3, 4])) + MODEND,
    # FIXUPP to external index 1, with no EXTDEF.
    "fixup": THEADR + LNAMES + SEGDEF + rec(0xA0, bytes([1, 0, 0, 0, 0]))
    + rec(0x9C, bytes([0xC4, 0x00, 0x56, 0x01])) + MODEND,
    "segment": THEADR + rec(0xA0, bytes([1, 0, 0, 0])) + MODEND,
}
for k, v in files.items():
    with open(f"{sys.argv[1]}.{k}.obj", "wb") as f:
        f.write(v)
