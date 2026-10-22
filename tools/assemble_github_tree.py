#!/usr/bin/env python3
"""
tools/assemble_github_tree.py

Monta a pasta 'sd-card/' pronta para o GitHub, contendo EXCLUSIVAMENTE:
1. O runtime Wine-NX e launcher AUTORUN para Switch
2. O mod oficial Median XL Sigma 2.14.6 (MPQs, D2Sigma, Fog corrigida, D2GL, d2fps)
3. Os nossos arquivos (D2Autorun.dll, D2PAD/D2NXPad, configs, tools)

ESTRITAMENTE SEM ARQUIVOS PROPRIETÁRIOS DO BLIZZARD DIABLO II LOD:
- Sem d2data, d2char, d2exp, d2music, d2sfx, d2speech, d2video, d2xmusic, d2xtalk, patch_d2
- Sem DLLs/EXEs vanilla da Blizzard (Storm.dll, D2Client.dll, Game.exe vanilla, etc.)
"""

import os
import sys
import shutil
import glob

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SRC_DIST_SD = os.path.join(BASE_DIR, "dist/sd-card")
SRC_GAME = os.path.join(BASE_DIR, "build/test_game")
TARGET_SD = os.path.join(BASE_DIR, "sd-card")
TARGET_D2 = os.path.join(TARGET_SD, "switch/wine/drive_c/DiabloII")

BLIZZARD_FORBIDDEN = {
    # MPQs da Blizzard
    "d2char.mpq", "d2data.mpq", "d2exp.mpq", "d2music.mpq", "d2sfx.mpq",
    "d2speech.mpq", "d2video.mpq", "d2xmusic.mpq", "D2XMUSIC.MPQ",
    "d2xtalk.mpq", "d2xvideo.mpq", "D2XVIDEO.MPQ", "patch_d2.mpq", "Patch_D2.mpq",
    # Binários vanilla da Blizzard
    "Diablo II.exe", "Game.exe", "Game.exe.orig",
    "Storm.dll", "D2Client.dll", "D2Common.dll", "D2Game.dll", "D2Win.dll",
    "D2Lang.dll", "D2Net.dll", "D2sound.dll", "D2Launch.dll", "D2CMP.dll",
    "D2Gdi.dll", "D2gfx.dll", "Bnclient.dll", "BnClient.dll", "D2MCPClient.dll",
    "D2Multi.dll", "D2Direct3D.dll", "D2DDraw.dll", "D2Glide.dll",
    "binkw32.dll", "SmackW32.dll", "ijl11.dll", "D2.LNG", "default.key",
    "EmptyPage.dc6", "GoldDigger.dll", "D2VidTst.exe", "D2VidTst.ini"
}

def log(msg):
    print(f"[GH-TREE] {msg}", flush=True)

def main():
    log(f"Montando estrutura em {TARGET_SD}...")
    if os.path.exists(TARGET_SD):
        shutil.rmtree(TARGET_SD)

    # 1. Copiar base do runtime (switch/d2-autorun e switch/wine exceto DiabloII)
    os.makedirs(os.path.join(TARGET_SD, "switch"), exist_ok=True)
    shutil.copytree(os.path.join(SRC_DIST_SD, "switch/d2-autorun"), os.path.join(TARGET_SD, "switch/d2-autorun"))
    
    # Copiar wine exceto drive_c/DiabloII
    os.makedirs(os.path.join(TARGET_SD, "switch/wine"), exist_ok=True)
    for item in os.listdir(os.path.join(SRC_DIST_SD, "switch/wine")):
        s = os.path.join(SRC_DIST_SD, "switch/wine", item)
        d = os.path.join(TARGET_SD, "switch/wine", item)
        if item == "drive_c":
            os.makedirs(os.path.join(d, "windows"), exist_ok=True)
            # Copiar autorun-setup.exe se existir
            setup_exe = os.path.join(s, "autorun-setup.exe")
            if os.path.exists(setup_exe):
                shutil.copy2(setup_exe, os.path.join(d, "autorun-setup.exe"))
            # Copiar windows
            shutil.copytree(os.path.join(s, "windows"), os.path.join(d, "windows"), dirs_exist_ok=True)
        elif item != "logs" and not item.endswith(".log") and not item.endswith(".shm"):
            if os.path.isdir(s):
                shutil.copytree(s, d)
            else:
                shutil.copy2(s, d)

    # 2. Criar pasta DiabloII limpa
    os.makedirs(TARGET_D2, exist_ok=True)
    os.makedirs(os.path.join(TARGET_D2, "d2nx"), exist_ok=True)
    os.makedirs(os.path.join(TARGET_D2, "data/local"), exist_ok=True)
    os.makedirs(os.path.join(TARGET_D2, "save"), exist_ok=True)

    # 3. Copiar/Criar nossos arquivos para DiabloII
    with open(os.path.join(TARGET_D2, "Game.args.txt"), "w") as f:
        f.write("-w -3dfx\n")
    log("  + Nosso arquivo: Game.args.txt (-w -3dfx)")

    with open(os.path.join(TARGET_D2, "Game.wine-nx.txt"), "w") as f:
        f.write("title=Median XL Sigma 2.14.6\n")
    log("  + Nosso arquivo: Game.wine-nx.txt")

    our_files = [
        "D2Autorun.dll",
        "d2nx/D2NXPad.dll",
        "d2nx/d2nx.ini",
        "d2nx/d2nxpad.ini",
    ]
    for of in our_files:
        src = os.path.join(SRC_GAME, of)
        dst = os.path.join(TARGET_D2, of)
        if os.path.exists(src):
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
            log(f"  + Nosso arquivo: {of}")
        else:
            log(f"  ! ALERTA: arquivo ausente: {src}")

    # 4. Copiar arquivos oficiais do Median XL 2.14.6
    mxl_files = [
        "D2Sigma.dll",
        "Fog.dll",            # Fog com Characteristics corrigido para Wine-NX
        "d2gl.mpq",
        "d2gl.ini",
        "glide3x.dll",
        "d2fps.dll",
        "ddraw.dll",
        "lootfilterconf.json",
        "data/local/use"
    ]
    for mf in mxl_files:
        src = os.path.join(SRC_GAME, mf)
        dst = os.path.join(TARGET_D2, mf)
        if os.path.exists(src):
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
            log(f"  + Mod Median XL: {mf}")
        else:
            log(f"  ! ALERTA mod file ausente: {src}")

    # Aplicar correcao de characteristics no Fog.dll
    fog_dst = os.path.join(TARGET_D2, "Fog.dll")
    if os.path.exists(fog_dst):
        import subprocess
        fix_script = os.path.join(os.path.dirname(__file__), "fix_fog_characteristics.py")
        subprocess.run([sys.executable, fix_script, fog_dst], check=True)

    # 5. Copiar todos os MPQs do Median XL (medianxl-*.mpq)
    mxl_mpqs = glob.glob(os.path.join(SRC_GAME, "medianxl-*.mpq"))
    for mpq in mxl_mpqs:
        base = os.path.basename(mpq)
        shutil.copy2(mpq, os.path.join(TARGET_D2, base))
    log(f"  + Copiados {len(mxl_mpqs)} arquivos MPQ do Median XL.")

    # 6. Criar arquivo de instrucoes para o usuario final
    inst_txt = os.path.join(TARGET_D2, "COPIE_SEUS_ARQUIVOS_D2_LOD_AQUI.txt")
    with open(inst_txt, "w", encoding="utf-8") as f:
        f.write("""================================================================================
INSTRUÇÕES: ADICIONAR ARQUIVOS DO DIABLO II: LORD OF DESTRUCTION (1.13c)
================================================================================

Este pacote contém o runtime AUTORUN (Wine-NX) para Nintendo Switch, os controles
nativos D2PAD (in-process) e todos os arquivos do mod Median XL Σ 2.14.6.

Por questões de direitos autorais da Blizzard Entertainment, os arquivos comerciais
do Diablo II LoD original NÃO estão inclusos e devem ser copiados da sua instalação:

ARQUIVOS NECESSÁRIOS:
1. Arquivos MPQ de dados do Diablo II e Expansão:
   - d2char.mpq
   - d2data.mpq
   - d2exp.mpq
   - d2music.mpq
   - d2sfx.mpq
   - d2speech.mpq
   - d2video.mpq
   - d2xmusic.mpq (ou D2XMUSIC.MPQ)
   - d2xtalk.mpq
   - d2xvideo.mpq (ou D2XVIDEO.MPQ)
   - patch_d2.mpq (ou Patch_D2.mpq)

2. DLLs base da versão 1.13c:
   - Storm.dll
   - D2Client.dll
   - D2Common.dll
   - D2Game.dll
   - D2Win.dll
   - D2Lang.dll
   - D2Net.dll
   - D2sound.dll
   - D2Launch.dll
   - D2CMP.dll
   - D2Gdi.dll
   - D2gfx.dll
   - Bnclient.dll (ou BnClient.dll)
   - D2MCPClient.dll
   - D2Multi.dll
   - binkw32.dll
   - SmackW32.dll
   - ijl11.dll

3. Executável do Jogo:
   - Game.exe (versão 1.13c)
   Execute: python3 tools/inject_pe_import.py Game.exe
   ou use o script utilitário: python3 tools/install_lod_files.py /caminho/do/seu/D2LoD

NÃO SUBSTITUA:
- Fog.dll (este é o Fog.dll modificado do Median XL)
- D2Sigma.dll (biblioteca principal do Median XL)
- D2Autorun.dll (camada de compatibilidade do Switch)
- d2nx/ (driver do controle do Switch)
================================================================================
""")

    # 7. Validacao estrita de exclusao da Blizzard
    forbidden_found = []
    for root, dirs, files in os.walk(TARGET_D2):
        for f in files:
            if f in BLIZZARD_FORBIDDEN:
                forbidden_found.append(f)
    if forbidden_found:
        log(f"ERRO FATAL: Arquivos da Blizzard detectados em {TARGET_D2}: {forbidden_found}")
        sys.exit(1)

    log("=== Estrutura 'sd-card/' montada com sucesso (0 arquivos Blizzard LoD)! ===")

if __name__ == "__main__":
    main()
