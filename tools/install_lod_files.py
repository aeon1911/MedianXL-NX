#!/usr/bin/env python3
"""
tools/install_lod_files.py

Utilitário para copiar arquivos legítimos do Diablo II: Lord of Destruction (1.13c)
de uma pasta fornecida pelo usuário para a estrutura do cartão SD do Switch (sd-card/),
injetando automaticamente a seção .d2auto no Game.exe.
"""

import os
import sys
import shutil
import glob
import subprocess

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TARGET_D2 = os.path.join(BASE_DIR, "sd-card/switch/wine/drive_c/DiabloII")
INJECT_TOOL = os.path.join(BASE_DIR, "tools/inject_pe_import.py")

REQUIRED_MPQS = [
    "d2char.mpq", "d2data.mpq", "d2exp.mpq", "d2music.mpq", "d2sfx.mpq",
    "d2speech.mpq", "d2video.mpq", "d2xmusic.mpq", "D2XMUSIC.MPQ",
    "d2xtalk.mpq", "d2xvideo.mpq", "D2XVIDEO.MPQ", "patch_d2.mpq", "Patch_D2.mpq"
]

REQUIRED_DLLS = [
    "Storm.dll", "D2Client.dll", "D2Common.dll", "D2Game.dll", "D2Win.dll",
    "D2Lang.dll", "D2Net.dll", "D2sound.dll", "D2Launch.dll", "D2CMP.dll",
    "D2Gdi.dll", "D2gfx.dll", "Bnclient.dll", "D2MCPClient.dll",
    "D2Multi.dll", "binkw32.dll", "SmackW32.dll", "ijl11.dll"
]

def log(msg):
    print(f"[INSTALL-LOD] {msg}", flush=True)

def main():
    if len(sys.argv) < 2:
        print("Uso: python3 tools/install_lod_files.py /caminho/para/Diablo_II_LoD_1.13c")
        sys.exit(1)

    src_dir = os.path.abspath(sys.argv[1])
    if not os.path.isdir(src_dir):
        log(f"ERRO: Diretório de origem não encontrado: {src_dir}")
        sys.exit(1)

    if not os.path.isdir(TARGET_D2):
        log(f"ERRO: Pasta de destino não encontrada: {TARGET_D2}")
        log("Execute primeiro: python3 tools/assemble_github_tree.py")
        sys.exit(1)

    log(f"Copiando arquivos LoD de {src_dir} para {TARGET_D2}...")

    # 1. Copiar MPQs base
    for mpq_pattern in REQUIRED_MPQS:
        matches = glob.glob(os.path.join(src_dir, mpq_pattern))
        for m in matches:
            base = os.path.basename(m)
            dst = os.path.join(TARGET_D2, base)
            if not os.path.exists(dst):
                log(f"  + Copiando MPQ: {base}")
                shutil.copy2(m, dst)

    # 2. Copiar DLLs base
    for dll in REQUIRED_DLLS:
        # Busca case-insensitive
        found = False
        for f in os.listdir(src_dir):
            if f.lower() == dll.lower():
                src_path = os.path.join(src_dir, f)
                dst_path = os.path.join(TARGET_D2, f)
                if not os.path.exists(dst_path):
                    log(f"  + Copiando DLL base: {f}")
                    shutil.copy2(src_path, dst_path)
                found = True
                break
        if not found:
            log(f"  ! Aviso: DLL {dll} não encontrada no diretório de origem")

    # 3. Copiar e Injetar Game.exe
    game_src = os.path.join(src_dir, "Game.exe")
    if not os.path.exists(game_src):
        for f in os.listdir(src_dir):
            if f.lower() == "game.exe":
                game_src = os.path.join(src_dir, f)
                break

    if os.path.exists(game_src):
        game_dst = os.path.join(TARGET_D2, "Game.exe")
        shutil.copy2(game_src, game_dst)
        log(f"  + Injetando .d2auto em {game_dst}...")
        subprocess.run([sys.executable, INJECT_TOOL, game_dst], check=True)
        log("  + Game.exe preparado com sucesso!")
    else:
        log("ERRO: Game.exe não encontrado no diretório de origem!")
        sys.exit(1)

    log("=== Instalação de arquivos LoD concluída com sucesso! ===")
    log("A pasta sd-card/ agora está completa e pronta para o Nintendo Switch!")

if __name__ == "__main__":
    main()
