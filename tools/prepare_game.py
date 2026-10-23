#!/usr/bin/env python3
"""
tools/prepare_game.py
Monta build/test_game (a pasta do jogo que o tools/assemble_sd_tree.py copia para o SD) a partir de:
  1. a SUA instalacao do Diablo II: Lord of Destruction (so os 10 MPQs base: d2data, d2exp, ...);
  2. o Median XL 2.14.6 baixado pelo tools/fetch_medianxl.py (canal oficial do launcher; o pacote do
     Median XL ja traz Game.exe/DLLs 1.13c, Storm, Fog, D2Sigma, D2GL e patch_d2.mpq);
  3. os arquivos deste repositorio (configuracoes, D2Autorun.dll e D2NXPad.dll compilados).

Alteracoes feitas nos binarios (todas conferidas byte a byte antes de gravar):
  - Game.exe: secao .d2auto que importa D2Autorun.dll (tools/inject_pe_import.py);
  - Fog.dll: bit IMAGE_FILE_RELOCS_STRIPPED errado (tools/fix_fog_characteristics.py);
  - D2Sigma.dll: teste de ponteiro nulo em 0x1005FC77 (mov [ecx],esi / mov [edx+4],esi com ecx = 0);
  - medianxl-data.mpq: os medianxl-*.mpq juntos num MPQ so (boot mais rapido no cartao), copiando os
    blocos comprimidos sem recompressao (build/bin/merge_mpqs).
E extrai do Median XL as tabelas originais que o menu de gameplay modifica (build/gameplay_orig e
d2nx/templates).

Uso: python3 tools/prepare_game.py --d2 "/caminho/do/Diablo II" [--mxl build/mxl214] [--out build/test_game]
     [--link]   (hardlink dos MPQs base em vez de copia, se estiverem no mesmo disco)
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
REPO_D2 = os.path.join(BASE_DIR, "sd-card/switch/wine/drive_c/DiabloII")
BIN = os.path.join(BASE_DIR, "build/bin")

BASE_MPQS = ["d2char.mpq", "d2data.mpq", "d2exp.mpq", "d2music.mpq", "d2sfx.mpq", "d2speech.mpq",
             "d2video.mpq", "d2xmusic.mpq", "d2xtalk.mpq", "d2xvideo.mpq"]
# do pacote do Median XL: tudo menos os medianxl-*.mpq (vao juntos), o launcher e wrappers nao usados
MXL_SKIP = {"ddraw.dll", "d2fps.dll", "diablo ii.exe"}
TABLES = ["experience_sp.bin", "itemratio_sp.bin", "treasureclassex.bin"]
REPO_FILES = ["d2gl.ini", "lootfilterconf.json", "data/local/use", "d2nx/d2nxpad.ini", "d2nx/d2nx.ini"]

# D2Sigma.dll 2.14.6 (md5 db7bdf53537caead9d3a09318eee38af): ImageBase 0x10000000, .text em RVA 0x1000 /
# arquivo 0x400. Em 0x1005FC77 "mov [ecx],esi; mov [edx+4],esi" escreve com ecx = 0 em alguns caminhos;
# desvia para o preenchimento int3 em 0x1005FCD5: test ecx,ecx; jz +2; mov [ecx],esi; mov [edx+4],esi; jmp volta.
SIGMA_MD5 = "db7bdf53537caead9d3a09318eee38af"
SIGMA_PATCH = [
    (0x1005FC77, "8931897204", "eb5c909090"),
    (0x1005FCD5, "cccccccccccccccccccccc", "85c974028931897204eb9c"),
]


def log(msg):
    print(f"[PREPARE] {msg}", flush=True)


def die(msg):
    print(f"[PREPARE] ERRO: {msg}", flush=True)
    sys.exit(1)


def md5_file(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        while chunk := f.read(1 << 20):
            h.update(chunk)
    return h.hexdigest()


def find_ci(folder, name):
    """Procura name em folder sem diferenciar maiusculas (instalacoes trazem D2XMUSIC.MPQ etc.)."""
    for n in os.listdir(folder):
        if n.lower() == name.lower() and os.path.isfile(os.path.join(folder, n)):
            return os.path.join(folder, n)
    return None


def put(src, dst, link=False):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if os.path.lexists(dst):
        if (not os.path.islink(dst) and os.path.getsize(dst) == os.path.getsize(src)
                and os.path.getmtime(dst) >= os.path.getmtime(src)):
            return
        os.remove(dst)
    if link:
        try:
            os.link(src, dst)
            return
        except OSError:
            pass
    shutil.copy2(src, dst)


def patch_sigma(path):
    data = bytearray(open(path, "rb").read())
    if md5_file(path) != SIGMA_MD5:
        die(f"{path} nao e o D2Sigma.dll do Median XL 2.14.6 (md5 diferente)")
    for va, old, new in SIGMA_PATCH:
        off = va - 0x10001000 + 0x400
        old_b, new_b = bytes.fromhex(old), bytes.fromhex(new)
        if data[off:off + len(old_b)] != old_b:
            die(f"D2Sigma.dll: bytes inesperados em 0x{va:08X}")
        data[off:off + len(new_b)] = new_b
    open(path, "wb").write(data)
    log("  D2Sigma.dll: teste de ponteiro nulo em 0x1005FC77 aplicado")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--d2", required=True, help="pasta da sua instalacao do Diablo II LoD (com d2data.mpq, d2exp.mpq...)")
    ap.add_argument("--mxl", default=os.path.join(BASE_DIR, "build/mxl214"), help="saida do tools/fetch_medianxl.py")
    ap.add_argument("--out", default=os.path.join(BASE_DIR, "build/test_game"))
    ap.add_argument("--link", action="store_true", help="hardlink dos MPQs base em vez de copiar")
    a = ap.parse_args()

    mxl_files = os.path.join(a.mxl, "files")
    man_path = os.path.join(a.mxl, "manifest.json")
    if not os.path.isfile(man_path):
        die(f"{man_path} nao existe: rode antes  python3 tools/fetch_medianxl.py")
    man = json.load(open(man_path))
    for t in ("merge_mpqs", "mpq_extract"):
        if not os.access(os.path.join(BIN, t), os.X_OK):
            die(f"build/bin/{t} nao existe: rode antes  sh tools/build_tools.sh")
    for dll in ("D2Autorun.dll", "D2NXPad.dll"):
        if not os.path.isfile(os.path.join(BASE_DIR, "src-d2nx", dll)):
            die(f"src-d2nx/{dll} nao existe: compile com  sh src-d2nx/build.sh  (ou pelo setup.sh)")

    os.makedirs(a.out, exist_ok=True)

    # 1. MPQs base da Blizzard, da instalacao do usuario
    if not os.path.isdir(a.d2):
        die(f"pasta do Diablo II nao encontrada: {a.d2}")
    missing = [m for m in BASE_MPQS if not find_ci(a.d2, m)]
    if missing:
        die("faltam na sua pasta do Diablo II: " + ", ".join(missing) +
            "\n  Use a instalacao completa do Diablo II + Lord of Destruction (CD ou instalador da Blizzard;"
            "\n  na instalacao do CD copie tambem d2music.mpq/d2video.mpq/d2xmusic.mpq/d2xvideo.mpq dos discos).")
    for m in BASE_MPQS:
        src = find_ci(a.d2, m)
        log(f"  {m} <- {src} ({os.path.getsize(src) >> 20} MB)")
        put(src, os.path.join(a.out, m), a.link)

    # 2. Pacote do Median XL (conferido pelo manifesto oficial)
    for f in man["files"]:
        src = os.path.join(mxl_files, f["name"])
        if not os.path.isfile(src) or os.path.getsize(src) != f["size"]:
            die(f"{f['name']} ausente ou incompleto em {mxl_files}: rode  python3 tools/fetch_medianxl.py")
        if f["name"].lower().startswith("medianxl-") or f["name"].lower() in MXL_SKIP:
            continue
        dst = os.path.join(a.out, f["name"])
        if f["name"] in ("Game.exe", "D2Sigma.dll", "Fog.dll"):
            for p in (dst, dst + ".orig"):
                if os.path.lexists(p):
                    os.remove(p)
            shutil.copy2(src, dst)
        else:
            put(src, dst)
    log(f"  {len(man['files'])} arquivos do Median XL {man['tag']} conferidos")

    # 3. Binarios corrigidos
    subprocess.run([sys.executable, os.path.join(BASE_DIR, "tools/inject_pe_import.py"),
                    os.path.join(a.out, "Game.exe")], check=True, stdout=subprocess.DEVNULL)
    os.remove(os.path.join(a.out, "Game.exe.orig"))
    log("  Game.exe: secao .d2auto (importa D2Autorun.dll)")
    subprocess.run([sys.executable, os.path.join(BASE_DIR, "tools/fix_fog_characteristics.py"),
                    os.path.join(a.out, "Fog.dll")], check=True, stdout=subprocess.DEVNULL)
    log("  Fog.dll: Characteristics corrigida")
    patch_sigma(os.path.join(a.out, "D2Sigma.dll"))

    # 4. MPQ unico do Median XL (copia dos blocos, sem recompressao)
    log("  juntando os medianxl-*.mpq em medianxl-data.mpq ...")
    subprocess.run([os.path.join(BIN, "merge_mpqs"), mxl_files, os.path.join(a.out, "medianxl-data.mpq")], check=True)

    # 5. Tabelas originais do Median XL para o menu de gameplay e o patch_gameplay.py
    orig = os.path.join(BASE_DIR, "build/gameplay_orig")
    tmpl = os.path.join(a.out, "d2nx/templates")
    os.makedirs(orig, exist_ok=True)
    os.makedirs(tmpl, exist_ok=True)
    args = [os.path.join(BIN, "mpq_extract"), os.path.join(a.out, "medianxl-data.mpq")]
    for t in TABLES:
        args += ["data\\global\\excel\\" + t, os.path.join(orig, t)]
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL)
    for t in TABLES:
        shutil.copy2(os.path.join(orig, t), os.path.join(tmpl, t))
    log("  tabelas originais extraidas (build/gameplay_orig, d2nx/templates)")

    # 6. Arquivos deste repositorio
    for rel in REPO_FILES:
        dst = os.path.join(a.out, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(os.path.join(REPO_D2, rel), dst)
    shutil.copy2(os.path.join(BASE_DIR, "src-d2nx/D2Autorun.dll"), os.path.join(a.out, "D2Autorun.dll"))
    shutil.copy2(os.path.join(BASE_DIR, "src-d2nx/D2NXPad.dll"), os.path.join(a.out, "d2nx/D2NXPad.dll"))
    log(f"OK: {a.out} pronto. Proximo passo:  python3 tools/assemble_sd_tree.py")


if __name__ == "__main__":
    main()
