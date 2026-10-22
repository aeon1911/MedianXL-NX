#!/usr/bin/env python3
"""
tools/fix_fog_characteristics.py

Clears IMAGE_FILE_RELOCS_STRIPPED (0x0001) in Fog.dll's Characteristics header.
Median XL 2.14.6's Fog.dll has a complete 5th section with 3,940 valid relocations,
but had Characteristics set to 0x210f instead of 0x210e, causing Wine-NX to refuse
relocation with STATUS_CONFLICTING_ADDRESSES (c0000018).
"""
import sys
import struct

def fix_fog(path):
    with open(path, "rb") as f:
        data = bytearray(f.read())

    e_lfanew = struct.unpack_from("<I", data, 0x3c)[0]
    char_off = e_lfanew + 4 + 18
    charact = struct.unpack_from("<H", data, char_off)[0]
    print(f"[FIX-FOG] {path}: Characteristics original = 0x{charact:04x}")
    
    if charact & 0x0001:
        new_charact = charact & ~0x0001
        struct.pack_into("<H", data, char_off, new_charact)
        print(f"[FIX-FOG] {path}: Characteristics corrigida = 0x{new_charact:04x}")
        with open(path, "wb") as f:
            f.write(data)
        print(f"[FIX-FOG] Gravado com sucesso.")
    else:
        print(f"[FIX-FOG] Bit IMAGE_FILE_RELOCS_STRIPPED já estava limpo.")

if __name__ == "__main__":
    target = sys.argv[1] if len(sys.argv) > 1 else "build/test_game/Fog.dll"
    fix_fog(target)
