#!/usr/bin/env python3
"""
tools/package_release.py
Valida rigorosamente a arvore SD gerada e empacota o arquivo final:
  dist/d2-medianxl-autorun-switch-2.14.6-final.zip
juntamente com seu respectivo checksum SHA256.
"""

import os
import sys
import zipfile
import hashlib

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SD_ROOT = os.path.join(BASE_DIR, "dist/sd-card")
SWITCH_DIR = os.path.join(SD_ROOT, "switch")
DIST_DIR = os.path.join(BASE_DIR, "dist")
OUTPUT_ZIP = os.path.join(DIST_DIR, "d2-medianxl-autorun-switch-2.14.6-final.zip")
OUTPUT_SHA256 = OUTPUT_ZIP + ".sha256"

def log(msg):
    print(f"[PACKAGE] {msg}", flush=True)

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def validate_tree():
    log("Iniciando validacao estrita da arvore do SD Card...")
    if not os.path.isdir(SWITCH_DIR):
        log(f"ERRO: Diretorio nao encontrado: {SWITCH_DIR}")
        sys.exit(1)

    d2_dir = os.path.join(SWITCH_DIR, "wine/drive_c/DiabloII")
    wine_dir = os.path.join(SWITCH_DIR, "wine")

    # Arquivos obrigatorios
    autorun_dir = os.path.join(SWITCH_DIR, "d2-autorun")
    mandatory = [
        os.path.join(autorun_dir, "d2-autorun.nro"),
        os.path.join(autorun_dir, "catalog.json"),
        os.path.join(autorun_dir, "root.txt"),
        os.path.join(wine_dir, "wine-nx-runtime.nro"),
        os.path.join(wine_dir, "run-next.txt"),
        os.path.join(wine_dir, "target.txt"),
        os.path.join(wine_dir, "launcher-library.txt"),
        os.path.join(wine_dir, "launcher-library-v2.ini"),
        os.path.join(wine_dir, "drive_c/autorun-setup.exe"),
        os.path.join(wine_dir, "drive_c/windows/syswow64/crtdll.dll"),
        os.path.join(d2_dir, "Game.exe"),
        os.path.join(d2_dir, "Game.args.txt"),
        os.path.join(d2_dir, "Game.wine-nx.txt"),
        os.path.join(d2_dir, "D2Autorun.dll"),
        os.path.join(d2_dir, "Fog.dll"),
        os.path.join(d2_dir, "D2Sigma.dll"),
        os.path.join(d2_dir, "medianxl-data.mpq"),
        os.path.join(d2_dir, "glide3x.dll"),
        os.path.join(d2_dir, "d2gl.mpq"),
        os.path.join(d2_dir, "d2gl.ini"),
        os.path.join(d2_dir, "lootfilterconf.json"),
        os.path.join(d2_dir, "data/local/use"),
        os.path.join(d2_dir, "d2nx/D2NXPad.dll"),
        os.path.join(d2_dir, "d2nx/d2nxpad.ini"),
    ]

    for p in mandatory:
        if not os.path.exists(p):
            log(f"FALHA: Arquivo obrigatorio ausente: {p}")
            sys.exit(1)

    # Validacao de conteudo de configuracao
    with open(os.path.join(wine_dir, "run-next.txt"), "r") as f:
        rn = f.read().strip()
        assert rn == "sdmc:/switch/wine/drive_c/DiabloII/Game.exe", f"run-next.txt invalido: {rn}"

    with open(os.path.join(d2_dir, "Game.args.txt"), "r") as f:
        args = f.read().strip()
        assert "-3dfx" in args and "-w" not in args, f"Game.args.txt invalido (D2GL exige tela cheia sem -w): {args}"

    # Verificacao de proibicoes estritas
    forbidden_global = ["D2SE.exe", "D2SE", "MODS", "PlugY", "ProjectD2"]
    forbidden_in_game = ["xinput1_4.dll", "pad.txt"]  # Nao deve haver mock no diretorio do jogo

    for root, dirs, files in os.walk(SWITCH_DIR):
        is_game_dir = d2_dir in os.path.abspath(root)
        for d in list(dirs):
            if d in forbidden_global:
                log(f"FALHA: Diretorio proibido detectado: {os.path.join(root, d)}")
                sys.exit(1)
        for f in files:
            if f in forbidden_global:
                log(f"FALHA: Arquivo proibido detectado: {os.path.join(root, f)}")
                sys.exit(1)
            if is_game_dir and f in forbidden_in_game:
                log(f"FALHA: Mock proibido detectado no jogo: {os.path.join(root, f)}")
                sys.exit(1)
            if f.endswith(".log") and not root.endswith("logs"):
                log(f"ALERTA: Removendo arquivo de log residual antes do zip: {os.path.join(root, f)}")
                os.remove(os.path.join(root, f))

        # Verificacao estrita de colisao de maiusculas/minusculas (incompativel com FAT32)
        seen_case = {}
        for f in files:
            fl = f.lower()
            if fl in seen_case:
                log(f"FALHA FATAL FAT32: Colisao de case detectada em {root}: {seen_case[fl]} vs {f}")
                sys.exit(1)
            seen_case[fl] = f

    log("Validacao estrita concluida com 100% de sucesso!")

def build_zip():
    log(f"Criando arquivo ZIP de distribuicao: {OUTPUT_ZIP}...")
    file_count = 0
    total_uncompressed = 0

    with zipfile.ZipFile(OUTPUT_ZIP, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for root, dirs, files in os.walk(SWITCH_DIR):
            for file in files:
                full_path = os.path.join(root, file)
                rel_path = os.path.relpath(full_path, SD_ROOT)
                zf.write(full_path, rel_path)
                file_count += 1
                total_uncompressed += os.path.getsize(full_path)

    zip_size = os.path.getsize(OUTPUT_ZIP)
    sha256 = sha256_file(OUTPUT_ZIP)

    with open(OUTPUT_SHA256, "w", encoding="utf-8") as f:
        f.write(f"{sha256}  {os.path.basename(OUTPUT_ZIP)}\n")

    log("=== PACOTE CRIADO COM SUCESSO ===")
    log(f"  Arquivo: {OUTPUT_ZIP}")
    log(f"  Total de Arquivos: {file_count}")
    log(f"  Tamanho Descompactado: {total_uncompressed / (1024*1024):.2f} MB")
    log(f"  Tamanho do ZIP: {zip_size / (1024*1024):.2f} MB")
    log(f"  SHA256: {sha256}")
    log(f"  Checksum gravado em: {OUTPUT_SHA256}")

if __name__ == "__main__":
    validate_tree()
    build_zip()
