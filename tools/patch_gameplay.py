#!/usr/bin/env python3
"""
tools/patch_gameplay.py
Gera as tabelas de gameplay customizadas (XP, Runas, Drops, Ouro) diretamente em data/global/excel/
usando o modo nativo -direct do Diablo II, sem tocar no medianxl-data.mpq.
"""

import os
import sys
import struct
import configparser

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

def load_gameplay_cfg(ini_path):
    xp_mult = 2.0
    rune_mult = 2.0
    unique_mult = 1.5
    rare_mult = 1.5
    start_gold = 10000

    if os.path.exists(ini_path):
        with open(ini_path, "r", encoding="utf-8", errors="replace") as f:
            in_gameplay = false = False
            for line in f:
                line = line.strip()
                if line.startswith("[") and line.endswith("]"):
                    in_gameplay = (line.lower() == "[gameplay]")
                    continue
                if in_gameplay and "=" in line and not line.startswith(";") and not line.startswith("#"):
                    k, v = [x.strip() for x in line.split("=", 1)]
                    v = v.split(";")[0].split("#")[0].strip()
                    try:
                        if k in ("MultiplicadorXP", "XpMultiplier"): xp_mult = float(v)
                        elif k in ("MultiplicadorRunas", "RuneMultiplier"): rune_mult = float(v)
                        elif k in ("MultiplicadorUnicos", "UniqueMultiplier"): unique_mult = float(v)
                        elif k in ("MultiplicadorRaros", "RareMultiplier"): rare_mult = float(v)
                        elif k in ("OuroInicial", "StartingGold"): start_gold = int(v)
                    except Exception:
                        pass
    return xp_mult, rune_mult, unique_mult, rare_mult, start_gold

def patch_tables(target_d2_dir, ini_path=None):
    if not ini_path:
        ini_path = os.path.join(target_d2_dir, "d2nx/d2nxpad.ini")
    
    xp_mult, rune_mult, unique_mult, rare_mult, start_gold = load_gameplay_cfg(ini_path)
    print(f"[GAMEPLAY] Multiplicadores carregados: XP={xp_mult}x, Runas={rune_mult}x, Unicos={unique_mult}x, Raros={rare_mult}x, Ouro={start_gold}")

    excel_dir = os.path.join(target_d2_dir, "data/global/excel")
    os.makedirs(excel_dir, exist_ok=True)

    # 1. Experience
    with open("/tmp/experience_sp.bin", "rb") as f:
        exp_data = bytearray(f.read())
    cnt = (len(exp_data) - 0x20) // 32
    for lvl in range(cnt):
        offset = 0x20 + lvl * 32
        for c in range(7):
            pos = offset + 4 + c * 4
            orig = struct.unpack("<I", exp_data[pos:pos+4])[0]
            new_val = max(1, int(orig / xp_mult)) if orig > 0 else 0
            exp_data[pos:pos+4] = struct.pack("<I", new_val)
    with open(os.path.join(excel_dir, "experience_sp.bin"), "wb") as f:
        f.write(exp_data)
    print(f"[GAMEPLAY]   experience_sp.bin gerado ({cnt} niveis, {xp_mult}x XP)")

    # 2. ItemRatio
    with open("/tmp/itemratio_sp.bin", "rb") as f:
        ratio_data = bytearray(f.read())
    rows = struct.unpack("<I", ratio_data[:4])[0]
    for r in range(rows):
        offset = 4 + r * 68
        u = struct.unpack("<I", ratio_data[offset+8:offset+12])[0]
        r_val = struct.unpack("<I", ratio_data[offset+20:offset+24])[0]
        s = struct.unpack("<I", ratio_data[offset+32:offset+36])[0]
        new_u = max(5, int(u / unique_mult))
        new_r = max(5, int(r_val / rare_mult))
        new_s = max(6, int(s / unique_mult))
        ratio_data[offset+8:offset+12] = struct.pack("<I", new_u)
        ratio_data[offset+20:offset+24] = struct.pack("<I", new_r)
        ratio_data[offset+32:offset+36] = struct.pack("<I", new_s)
    with open(os.path.join(excel_dir, "itemratio_sp.bin"), "wb") as f:
        f.write(ratio_data)
    print(f"[GAMEPLAY]   itemratio_sp.bin gerado (Unicos {unique_mult}x, Raros {rare_mult}x)")

    # 3. TreasureClassEx
    with open("/tmp/treasureclassex.bin", "rb") as f:
        tc_data = bytearray(f.read())
    tc_rows = struct.unpack("<I", tc_data[:4])[0]
    row_size = 736
    rune_mods = 0
    for r in range(tc_rows):
        offset = 4 + r * row_size
        tc_name = tc_data[offset:offset+0x38].split(b"\x00")[0].decode("ascii", errors="replace")
        for item_idx in range(10):
            item_off = offset + 0x38 + item_idx * 64
            item_name = tc_data[item_off:item_off+64].split(b"\x00")[0].decode("ascii", errors="replace")
            prob_pos = offset + 0x2b8 + item_idx * 4
            prob = struct.unpack("<I", tc_data[prob_pos:prob_pos+4])[0]
            if "rune" in item_name.lower() and not tc_name.startswith("Runes "):
                new_prob = min(65535, int(prob * rune_mult))
                tc_data[prob_pos:prob_pos+4] = struct.pack("<I", new_prob)
                rune_mods += 1
    with open(os.path.join(excel_dir, "treasureclassex.bin"), "wb") as f:
        f.write(tc_data)
    print(f"[GAMEPLAY]   treasureclassex.bin gerado ({rune_mods} entradas de runas com {rune_mult}x)")

    # 4. CharStats
    with open("/tmp/charstats.bin", "rb") as f:
        cs_data = bytearray(f.read())
    cs_classes = struct.unpack("<I", cs_data[:4])[0]
    row_len = 196
    for c in range(cs_classes):
        offset = 4 + c * row_len
        box_pos = cs_data.find(b"box ", offset, offset + row_len)
        if box_pos != -1:
            item_start = box_pos - 8
            slot9_pos = item_start + 8 * 8
            cs_data[slot9_pos:slot9_pos+4] = b"gld "
            cs_data[slot9_pos+4:slot9_pos+8] = struct.pack("<HH", 0, min(10000, start_gold))
    with open(os.path.join(excel_dir, "charstats.bin"), "wb") as f:
        f.write(cs_data)
    print(f"[GAMEPLAY]   charstats.bin gerado (Ouro inicial={min(10000, start_gold)} para 7 classes)")

if __name__ == "__main__":
    target = sys.argv[1] if len(sys.argv) > 1 else "sd-card/switch/wine/drive_c/DiabloII"
    patch_tables(target)
