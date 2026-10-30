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
    rejuv = 1

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
                        elif k in ("RejuvenescimentoNosVendedores", "VendorRejuvenation"): rejuv = int(v)
                    except Exception:
                        pass
    return xp_mult, rune_mult, unique_mult, rare_mult, start_gold, rejuv

def patch_tables(target_d2_dir, ini_path=None):
    if not ini_path:
        ini_path = os.path.join(target_d2_dir, "d2nx/d2nxpad.ini")
    
    xp_mult, rune_mult, unique_mult, rare_mult, start_gold, rejuv = load_gameplay_cfg(ini_path)
    print(f"[GAMEPLAY] Multiplicadores carregados: XP={xp_mult}x, Runas={rune_mult}x, Unicos={unique_mult}x, Raros={rare_mult}x, Ouro={start_gold}, Rejuvenescimento nos vendedores={rejuv}")

    excel_dir = os.path.join(target_d2_dir, "data/global/excel")
    orig_dir = os.path.join(BASE_DIR, "build/gameplay_orig")
    os.makedirs(orig_dir, exist_ok=True)
    def orig_table(name):
        keep = os.path.join(orig_dir, name)
        if not os.path.exists(keep):
            sys.exit(f"[GAMEPLAY] ERRO: {keep} nao existe (extraido do Median XL pelo tools/prepare_game.py)")
        return keep
    os.makedirs(excel_dir, exist_ok=True)

    # 1. Experience
    with open(orig_table("experience_sp.bin"), "rb") as f:
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
    with open(orig_table("itemratio_sp.bin"), "rb") as f:
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
    with open(orig_table("treasureclassex.bin"), "rb") as f:
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

    # 5. Vendedores: Full Rejuvenation Potion ("rvl ") no estoque de todos os NPCs de npcstores.bin.
    # O Median XL guarda o estoque de cada vendedor em npcstores.bin (registros de 52 bytes: NPC, codigo do item, depois
    # bytes de configuracao; as linhas de pocao/chave de Akara, Cain... sao "01 01 01 01" + zeros: sempre em estoque em
    # todas as dificuldades). Cada NPC ganha uma linha igual a de uma pocao, com o codigo trocado. Como as linhas de
    # alguns NPCs (200, 176) ja aparecem intercaladas, as novas vao no fim da tabela.
    out_ns = os.path.join(excel_dir, "npcstores.bin")
    if rejuv:
        with open(orig_table("npcstores.bin"), "rb") as f:
            ns = bytearray(f.read())
        rec = 52
        n = struct.unpack("<I", ns[:4])[0]
        assert len(ns) == 4 + n * rec, "npcstores.bin: tamanho inesperado"
        rows = [bytes(ns[4 + i * rec:4 + (i + 1) * rec]) for i in range(n)]
        tmpl = next((r[8:] for r in rows if r[4:8] == b"hp1 " and r[8:12] == b"\x01\x01\x01\x01"), None)
        if tmpl is None:
            sys.exit("[GAMEPLAY] ERRO: npcstores.bin sem linha de pocao 'hp1 ' para servir de modelo")
        npcs = []
        for r in rows:
            if r[:4] not in npcs: npcs.append(r[:4])
        added = 0
        for npc in npcs:
            if any(r[:4] == npc and r[4:8] == b"rvl " for r in rows): continue
            rows.append(npc + b"rvl " + tmpl)
            added += 1
        with open(out_ns, "wb") as f:
            f.write(struct.pack("<I", len(rows)) + b"".join(rows))
        print(f"[GAMEPLAY]   npcstores.bin gerado (Full Rejuvenation Potion em {added} vendedores de {len(npcs)})")
    elif os.path.exists(out_ns):
        os.remove(out_ns)
        print("[GAMEPLAY]   npcstores.bin removido (estoque original dos vendedores)")

    # 4. CharStats: NAO MODIFICAR.
    # Inserir "gld " como item inicial cria um item de ouro invalido no inventario do personagem novo,
    # e o save resultante e rejeitado pelo Median XL ("Bad Character File ... not compatible with Median XL").
    # Remove qualquer charstats.bin gerado por versoes anteriores para voltar ao original do MPQ.
    stale_cs = os.path.join(excel_dir, "charstats.bin")
    if os.path.exists(stale_cs):
        os.remove(stale_cs)
        print("[GAMEPLAY]   charstats.bin removido (tabela original do Median XL restaurada)")

if __name__ == "__main__":
    target = sys.argv[1] if len(sys.argv) > 1 else "sd-card/switch/wine/drive_c/DiabloII"
    patch_tables(target)
