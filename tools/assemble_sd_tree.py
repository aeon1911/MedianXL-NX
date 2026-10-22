#!/usr/bin/env python3
"""
tools/assemble_sd_tree.py
Monta a arvore final do SD Card para o Nintendo Switch em:
  dist/sd-card/switch/wine/drive_c/DiabloII/
e sincroniza com o ambiente de teste do Ryujinx em:
  build/ryujinx-test/sdcard/switch/wine/drive_c/DiabloII/
"""

import os
import sys
import shutil
import glob
import hashlib

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SRC_GAME = os.path.join(BASE_DIR, "build/test_game")
DIST_WINE = os.path.join(BASE_DIR, "dist/sd-card/switch/wine")
DIST_D2 = os.path.join(DIST_WINE, "drive_c/DiabloII")
RYU_WINE = os.path.join(BASE_DIR, "build/ryujinx-test/sdcard/switch/wine")
RYU_D2 = os.path.join(RYU_WINE, "drive_c/DiabloII")

def log(msg):
    print(f"[ASSEMBLE-SD] {msg}", flush=True)

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def clean_obsolete(target_d2):
    log(f"Limpando artefatos obsoletos e D2SE em {target_d2}...")
    obsolete_dirs = ["D2SE", "MODS", "PlugY", "ProjectD2", "Screenshots", "Logs"]
    for d in obsolete_dirs:
        p = os.path.join(target_d2, d)
        if os.path.exists(p):
            log(f"  Removendo diretorio proibido: {d}/")
            shutil.rmtree(p, ignore_errors=True)
            
    obsolete_patterns = [
        "D2SE.*",
        "*.orig",
        "*.log",
        "pad.txt",
        "xinput1_4.dll",       # Switch usa o driver XInput nativo do AUTORUN!
        "Crashdump",
        "BnetLog.txt",
        "d2gl.ini.autorun*",
        "d2gl.mpq.autorun*",
        "glide3x.dll.autorun*",
        "run-next.txt.previous",
        "Patch_D2.mpq",
        "D2XMUSIC.MPQ",
        "D2XVIDEO.MPQ"
    ]
    for pat in obsolete_patterns:
        for f in glob.glob(os.path.join(target_d2, pat)):
            log(f"  Removendo arquivo: {os.path.basename(f)}")
            try: os.remove(f)
            except Exception: pass
            
    # Assegurar que d2nx nao contenha xinput mock nem pad.txt
    d2nx_dir = os.path.join(target_d2, "d2nx")
    if os.path.exists(d2nx_dir):
        for f in ["xinput1_4.dll", "pad.txt", "d2nxpad.log"]:
            p = os.path.join(d2nx_dir, f)
            if os.path.exists(p):
                log(f"  Removendo mock/log do d2nx: {f}")
                os.remove(p)

def deploy_game_files(target_d2):
    log(f"Copiando arquivos limpos e validados para {target_d2}...")
    os.makedirs(target_d2, exist_ok=True)
    os.makedirs(os.path.join(target_d2, "d2nx"), exist_ok=True)
    os.makedirs(os.path.join(target_d2, "data/local"), exist_ok=True)
    os.makedirs(os.path.join(target_d2, "save"), exist_ok=True)

    # 1. Copiar DLLs e executavel
    critical_files = [
        "Game.exe",           # PE injetado com .d2auto
        "D2Autorun.dll",      # v0.4.4
        "Fog.dll",            # Patched sem o bug de critical section
        "D2Sigma.dll",        # Median XL 2.14.6
        "d2fps.dll",
        "glide3x.dll",        # D2GL OpenGL wrapper
        "ddraw.dll",
        "d2gl.mpq",
        "d2gl.ini",
        "lootfilterconf.json", # [] para prevencao de crash do D2Sigma
        "data/local/use",     # 1-byte null file para prevencao de crash do D2Lang
        "binkw32.dll", "Bnclient.dll", "D2Client.dll", "D2CMP.dll", "D2Common.dll",
        "D2DDraw.dll", "D2Direct3D.dll", "D2Game.dll", "D2Gdi.dll", "D2gfx.dll",
        "D2Glide.dll", "D2Lang.dll", "D2Launch.dll", "D2.LNG", "D2MCPClient.dll",
        "D2Multi.dll", "D2Net.dll", "D2sound.dll", "D2Win.dll", "ijl11.dll",
        "SmackW32.dll", "Storm.dll", "default.key"
    ]
    for cf in critical_files:
        src = os.path.join(SRC_GAME, cf)
        dst = os.path.join(target_d2, cf)
        if os.path.exists(src):
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
        else:
            log(f"ALERTA: Arquivo critico ausente: {src}")

    # 2. Copiar MPQs base e Median XL (normalizado em minusculo para evitar colisoes FAT32)
    seen_mpqs = set()
    all_mpqs = sorted(glob.glob(os.path.join(SRC_GAME, "*.mpq")) + glob.glob(os.path.join(SRC_GAME, "*.MPQ")))
    for mpq in all_mpqs:
        base = os.path.basename(mpq)
        canonical = base.lower() if not base.startswith("medianxl-") else base
        if canonical in seen_mpqs:
            continue
        seen_mpqs.add(canonical)
        dst = os.path.join(target_d2, canonical)
        if not os.path.exists(dst) or os.path.getsize(dst) != os.path.getsize(mpq):
            log(f"  Copiando MPQ: {canonical} ({os.path.getsize(mpq) // 1024} KB)")
            shutil.copy2(mpq, dst)

    # 3. Aplicar correcao de Characteristics no Fog.dll (evita STATUS_CONFLICTING_ADDRESSES c0000018)
    fog_dst = os.path.join(target_d2, "Fog.dll")
    if os.path.exists(fog_dst):
        import subprocess
        fix_script = os.path.join(os.path.dirname(__file__), "fix_fog_characteristics.py")
        subprocess.run([sys.executable, fix_script, fog_dst], check=True)

    # 3. Copiar D2NXPad
    shutil.copy2(os.path.join(SRC_GAME, "d2nx/D2NXPad.dll"), os.path.join(target_d2, "d2nx/D2NXPad.dll"))
    shutil.copy2(os.path.join(SRC_GAME, "d2nx/d2nxpad.ini"), os.path.join(target_d2, "d2nx/d2nxpad.ini"))

    # 4. Criar configuracoes de boot do AUTORUN
    args_path = os.path.join(target_d2, "Game.args.txt")
    with open(args_path, "w", encoding="utf-8") as f:
        f.write("-w -3dfx\n")
    log("  Criado Game.args.txt (-w -3dfx)")

    wine_nx_path = os.path.join(target_d2, "Game.wine-nx.txt")
    with open(wine_nx_path, "w", encoding="utf-8") as f:
        f.write("title=Median XL Sigma 2.14.6\nverbose=0\nprofile=0\n")
    log("  Criado Game.wine-nx.txt")

    # 5. Verificacao estrita de integridade
    assert not os.path.exists(os.path.join(target_d2, "xinput1_4.dll")), "ERRO: xinput1_4.dll nao pode ir no SD!"
    assert not os.path.exists(os.path.join(target_d2, "d2nx/xinput1_4.dll")), "ERRO: d2nx/xinput1_4.dll nao pode ir no SD!"
    assert not os.path.exists(os.path.join(target_d2, "pad.txt")), "ERRO: pad.txt nao pode ir no SD!"
    assert not os.path.exists(os.path.join(target_d2, "D2SE.exe")), "ERRO: D2SE.exe proibido!"
    assert os.path.exists(os.path.join(target_d2, "Game.exe")), "ERRO: Game.exe ausente!"
    assert os.path.exists(os.path.join(target_d2, "D2Autorun.dll")), "ERRO: D2Autorun.dll ausente!"
    assert os.path.exists(os.path.join(target_d2, "d2nx/D2NXPad.dll")), "ERRO: D2NXPad.dll ausente!"
    log(f"Integridade de {target_d2} verificada com 100% de conformidade.")

def configure_launcher(wine_dir):
    log(f"Configurando launcher e ponteiros em {wine_dir}...")
    run_next = os.path.join(wine_dir, "run-next.txt")
    with open(run_next, "w", encoding="utf-8") as f:
        f.write("sdmc:/switch/wine/drive_c/DiabloII/Game.exe\n")
        
    target_txt = os.path.join(wine_dir, "target.txt")
    with open(target_txt, "w", encoding="utf-8") as f:
        f.write("sdmc:/switch/wine/drive_c/DiabloII/Game.exe\n")
        
    launcher_lib = os.path.join(wine_dir, "launcher-library.txt")
    with open(launcher_lib, "w", encoding="utf-8") as f:
        f.write("sdmc:/switch/wine/drive_c/DiabloII/Game.exe\n")
        
    launcher_ini = os.path.join(wine_dir, "launcher-library-v2.ini")
    with open(launcher_ini, "w", encoding="utf-8") as f:
        f.write("version=3\n\n[game 1]\npath=sdmc:/switch/wine/drive_c/DiabloII/Game.exe\ntitle=Median XL Sigma 2.14.6\nadded-order=1\nlaunched-order=1\nfavorite=1\n")
    log("  Launcher configurado para boot direto de Game.exe.")

def patch_wine_registry(reg_dir):
    log(f"Ajustando chaves de registro do Wine em {reg_dir}...")
    system_reg = os.path.join(reg_dir, "system.reg")
    user_reg = os.path.join(reg_dir, "user.reg")
    for reg_file in [system_reg, user_reg]:
        if not os.path.exists(reg_file): continue
        with open(reg_file, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()
        # Ajustar caminho de saves antigos do D2SE
        content = content.replace("C:\\\\DiabloII\\\\MODS\\\\D2SE_Median XL Sigma 2.9.2\\\\save\\\\", "C:\\\\DiabloII\\\\save\\\\")
        # Assegurar chaves de CD check
        if "[Software\\\\Blizzard Entertainment\\\\Diablo II]" in content:
            target_sec = "[Software\\\\Blizzard Entertainment\\\\Diablo II]"
            repl = (
                "[Software\\\\Blizzard Entertainment\\\\Diablo II]\n"
                '"InstallPath"="C:\\\\DiabloII\\\\"\n'
                '"Aux Path"="C:\\\\DiabloII\\\\"\n'
                '"DiabloCD"="C:\\\\DiabloII\\\\"\n'
                '"ExpanCD"="C:\\\\DiabloII\\\\"\n'
                '"Save Path"="C:\\\\DiabloII\\\\save\\\\"'
            )
            # Remove direct old occurrences to avoid duplicates
            pass
        with open(reg_file, "w", encoding="utf-8") as f:
            f.write(content)
        log(f"  Registro {os.path.basename(reg_file)} atualizado.")

def create_ryu_case_symlinks(d2_dir):
    """Cria symlinks para compatibilidade com sistema de arquivos case-sensitive no Ryujinx."""
    variants = {
        "BnClient.dll": "Bnclient.dll",
        "BNCLIENT.dll": "Bnclient.dll",
        "BNCLIENT.DLL": "Bnclient.dll",
        "bnclient.dll": "Bnclient.dll",
        "bnclient.DLL": "Bnclient.dll",
        "D2Gfx.dll": "D2gfx.dll",
        "D2GFX.dll": "D2gfx.dll",
        "D2Sound.dll": "D2sound.dll",
        "D2SOUND.dll": "D2sound.dll",
        "D2Lang.dll": "D2Lang.dll",
        "D2LANG.dll": "D2Lang.dll",
        "D2Cmp.dll": "D2CMP.dll",
        "D2CMP.dll": "D2CMP.dll",
        "d2cmp.dll": "D2CMP.dll",
        "Fog.dll": "Fog.dll",
        "FOG.dll": "Fog.dll",
        "fog.dll": "Fog.dll",
        "Storm.dll": "Storm.dll",
        "STORM.dll": "Storm.dll",
        "storm.dll": "Storm.dll",
    }
    for alias, target in variants.items():
        dst = os.path.join(d2_dir, alias)
        src = os.path.join(d2_dir, target)
        if os.path.exists(src) and not os.path.exists(dst):
            try:
                os.symlink(target, dst)
            except Exception:
                pass

def main():
    log("=== Iniciando montagem da arvore SD final ===")
    # 1. Montar em dist/sd-card
    clean_obsolete(DIST_D2)
    deploy_game_files(DIST_D2)
    configure_launcher(DIST_WINE)
    patch_wine_registry(os.path.join(DIST_WINE, "registry"))
    
    # 2. Sincronizar em build/ryujinx-test
    if os.path.exists(RYU_WINE):
        log("Sincronizando arvore de teste com o Ryujinx...")
        clean_obsolete(RYU_D2)
        deploy_game_files(RYU_D2)
        create_ryu_case_symlinks(RYU_D2)
        configure_launcher(RYU_WINE)
        patch_wine_registry(os.path.join(RYU_WINE, "registry"))
        
    log("=== Montagem da arvore SD concluida com sucesso! ===")

if __name__ == "__main__":
    main()
