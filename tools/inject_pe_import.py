#!/usr/bin/env python3
"""
tools/inject_pe_import.py

Injects a PE import section (.d2auto) into a 32-bit Windows PE executable (Game.exe)
to load a DLL (e.g. D2Autorun.dll) and bind an entry export (e.g. D2Autorun_Init).
Creates a .orig backup before modifying the target binary.
"""

import sys
import os
import shutil
import struct

def align_up(x, a):
    return (x + a - 1) & ~(a - 1)

def inject_import(exe_path, dll_name="D2Autorun.dll", proc_name="D2Autorun_Init"):
    if not os.path.exists(exe_path):
        raise FileNotFoundError(f"File not found: {exe_path}")

    backup_path = exe_path + ".orig"
    if not os.path.exists(backup_path):
        shutil.copy2(exe_path, backup_path)
        print(f"Created backup: {backup_path}")

    with open(backup_path, "rb") as f:
        data = bytearray(f.read())

    # DOS Header
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise ValueError("Invalid DOS header")
    e_lfanew = struct.unpack_from("<I", data, 0x3c)[0]

    # NT Signature
    if data[e_lfanew:e_lfanew+4] != b"PE\x00\x00":
        raise ValueError("Invalid PE signature")

    # File Header
    num_sec = struct.unpack_from("<H", data, e_lfanew + 6)[0]
    opt_hdr_sz = struct.unpack_from("<H", data, e_lfanew + 20)[0]
    opt_hdr_off = e_lfanew + 24
    sec_tbl_off = opt_hdr_off + opt_hdr_sz

    # Optional Header (PE32)
    magic = struct.unpack_from("<H", data, opt_hdr_off)[0]
    if magic != 0x10b:
        raise ValueError(f"Expected PE32 magic 0x10b, got 0x{magic:x}")

    sec_align = struct.unpack_from("<I", data, opt_hdr_off + 32)[0]
    file_align = struct.unpack_from("<I", data, opt_hdr_off + 36)[0]
    orig_import_rva, orig_import_sz = struct.unpack_from("<II", data, opt_hdr_off + 96 + 8)

    sections = []
    max_vaddr_end = 0
    max_roff_end = 0
    for i in range(num_sec):
        s_off = sec_tbl_off + i * 40
        name = data[s_off:s_off+8].rstrip(b"\x00").decode(errors="ignore")
        vsize, vaddr, rsize, roff = struct.unpack_from("<IIII", data, s_off + 8)
        if name == ".d2auto":
            print(f"Executable already contains .d2auto section! Skipping injection.")
            return
        sections.append({"name": name, "vsize": vsize, "vaddr": vaddr, "rsize": rsize, "roff": roff})
        max_vaddr_end = max(max_vaddr_end, vaddr + align_up(vsize, sec_align))
        max_roff_end = max(max_roff_end, roff + align_up(rsize, file_align))

    new_vaddr = max_vaddr_end
    new_roff = max_roff_end

    # Find original import table file offset
    orig_import_off = None
    for s in sections:
        if s["vaddr"] <= orig_import_rva < s["vaddr"] + s["vsize"]:
            orig_import_off = s["roff"] + (orig_import_rva - s["vaddr"])
            break
    if orig_import_off is None:
        raise ValueError("Original import table RVA not found within sections")

    # Read original descriptors up to NULL terminator
    orig_desc = bytearray()
    curr_off = orig_import_off
    while True:
        desc = data[curr_off:curr_off+20]
        oft, ts, fc, name_rva, ft = struct.unpack_from("<IIIII", desc, 0)
        if not (oft or ts or fc or name_rva or ft):
            break
        orig_desc.extend(desc)
        curr_off += 20

    n_orig = len(orig_desc) // 20
    print(f"Read {n_orig} original import descriptors from RVA 0x{orig_import_rva:x}")

    # Section layout
    desc_table_sz = (n_orig + 2) * 20
    dll_name_off = desc_table_sz
    dll_bytes = dll_name.encode("ascii") + b"\x00"
    if len(dll_bytes) % 2 != 0:
        dll_bytes += b"\x00"

    hint_name_off = dll_name_off + len(dll_bytes)
    hint_proc_bytes = struct.pack("<H", 0) + proc_name.encode("ascii") + b"\x00"
    if len(hint_proc_bytes) % 4 != 0:
        hint_proc_bytes += b"\x00" * (4 - (len(hint_proc_bytes) % 4))

    oft_off = hint_name_off + len(hint_proc_bytes)
    oft_bytes = struct.pack("<II", new_vaddr + hint_name_off, 0)

    ft_off = oft_off + len(oft_bytes)
    ft_bytes = struct.pack("<II", new_vaddr + hint_name_off, 0)

    used_sz = ft_off + len(ft_bytes)
    new_rsize = align_up(used_sz, file_align)
    payload = bytearray(new_rsize)

    # Copy original descriptors
    payload[:len(orig_desc)] = orig_desc

    # Add new descriptor
    new_desc = struct.pack("<IIIII", new_vaddr + oft_off, 0, 0, new_vaddr + dll_name_off, new_vaddr + ft_off)
    payload[len(orig_desc):len(orig_desc)+20] = new_desc

    # Copy strings and thunks
    payload[dll_name_off:dll_name_off+len(dll_bytes)] = dll_bytes
    payload[hint_name_off:hint_name_off+len(hint_proc_bytes)] = hint_proc_bytes
    payload[oft_off:oft_off+len(oft_bytes)] = oft_bytes
    payload[ft_off:ft_off+len(ft_bytes)] = ft_bytes

    # Check header space
    new_sec_hdr_off = sec_tbl_off + num_sec * 40
    if any(data[new_sec_hdr_off:new_sec_hdr_off+40]):
        raise ValueError("No space in PE header for new section header")

    # Add section header
    sec_name = b".d2auto\x00"
    sec_hdr = struct.pack("<8sIIIIIIHHI", sec_name, used_sz, new_vaddr, new_rsize, new_roff, 0, 0, 0, 0, 0xC0000040)
    data[new_sec_hdr_off:new_sec_hdr_off+40] = sec_hdr

    # Update PE headers
    struct.pack_into("<H", data, e_lfanew + 6, num_sec + 1)
    new_img_sz = align_up(new_vaddr + used_sz, sec_align)
    struct.pack_into("<I", data, opt_hdr_off + 56, new_img_sz)
    struct.pack_into("<II", data, opt_hdr_off + 96 + 8, new_vaddr, desc_table_sz)
    struct.pack_into("<I", data, opt_hdr_off + 64, 0) # Clear checksum

    # Append payload at new_roff
    if len(data) < new_roff:
        data.extend(b"\x00" * (new_roff - len(data)))
    data.extend(payload)

    with open(exe_path, "wb") as f:
        f.write(data)
    print(f"Successfully injected {dll_name}!{proc_name} into {exe_path}")
    print(f"New section .d2auto at RVA 0x{new_vaddr:x}, raw offset 0x{new_roff:x}, size 0x{new_rsize:x}")

if __name__ == "__main__":
    target = sys.argv[1] if len(sys.argv) > 1 else "build/test_game/Game.exe"
    inject_import(target)
