#!/usr/bin/env python3
"""
tools/fetch_medianxl.py
Baixa o Median XL Sigma pelo MESMO canal publico do launcher oficial (launcher.median-xl.com /
get.median-xl.com) para build/mxl214/files/, conferindo o MD5 de cada arquivo contra o manifesto oficial.

Nada disso fica no repositorio: os arquivos sao do Median XL e da Blizzard (o pacote do Median XL traz
as DLLs do Diablo II 1.13c que o mod exige). Este script so automatiza o download que o launcher faz.

Uso: python3 tools/fetch_medianxl.py [--tag 2.14.6] [--dest build/mxl214]
  Arquivos ja baixados com MD5 correto sao mantidos (pode rodar de novo para retomar).
  Sem --tag, usa a versao do manifesto publico; se ela nao for a 2.14.6 testada, avisa e para
  (passe --tag explicitamente para aceitar outra versao).
"""
import argparse
import hashlib
import json
import os
import sys
import time
import urllib.parse
import urllib.request

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
MANIFEST_URL = "https://launcher.median-xl.com/mxl/release/public/manifest"
TESTED_TAG = "2.14.6"
UA = {"User-Agent": "MedianXL-NX-fetch/1.0"}


def log(msg):
    print(f"[FETCH-MXL] {msg}", flush=True)


def md5_file(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        while chunk := f.read(1 << 20):
            h.update(chunk)
    return h.hexdigest()


def get(url, timeout=60):
    return urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=timeout)


def download(url, dst, size):
    tmp = dst + ".part"
    for attempt in range(1, 6):
        try:
            with get(url, timeout=120) as r, open(tmp, "wb") as out:
                done = 0
                while chunk := r.read(1 << 20):
                    out.write(chunk)
                    done += len(chunk)
                    if size > 50 << 20 and done % (50 << 20) < (1 << 20):
                        log(f"    {done >> 20} / {size >> 20} MB")
            os.replace(tmp, dst)
            return
        except Exception as e:
            log(f"    tentativa {attempt} falhou: {e}")
            time.sleep(3 * attempt)
    raise SystemExit(f"ERRO: nao consegui baixar {url}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tag", help="versao do Median XL (padrao: a do manifesto publico, se for 2.14.6)")
    ap.add_argument("--dest", default=os.path.join(BASE_DIR, "build/mxl214"))
    a = ap.parse_args()

    os.makedirs(os.path.join(a.dest, "files"), exist_ok=True)
    man_path = os.path.join(a.dest, "manifest.json")
    log(f"Lendo o manifesto oficial: {MANIFEST_URL}")
    try:
        with get(MANIFEST_URL) as r:
            raw = r.read()
        man = json.loads(raw)
    except Exception as e:
        if not os.path.exists(man_path):
            raise SystemExit(f"ERRO: manifesto indisponivel ({e}) e nao ha copia em {man_path}")
        log(f"  manifesto indisponivel ({e}); usando a copia salva em {man_path}")
        man = json.load(open(man_path))
        raw = None

    if man.get("tag") != (a.tag or TESTED_TAG):
        if a.tag:
            raise SystemExit(f"ERRO: o manifesto publico esta na versao {man.get('tag')}, nao {a.tag}. "
                             "O launcher oficial so publica a versao atual.")
        raise SystemExit(f"ERRO: o manifesto publico esta na versao {man.get('tag')}; este port foi testado com "
                         f"{TESTED_TAG}. Para tentar mesmo assim: --tag {man.get('tag')}")
    if raw is not None:
        with open(man_path, "wb") as f:
            f.write(raw)

    files = man["files"]
    total = sum(f["size"] for f in files)
    log(f"Median XL {man['tag']}: {len(files)} arquivos, {total / (1 << 30):.2f} GB")
    for i, f in enumerate(files, 1):
        name = f["name"]
        if os.path.basename(name) != name or name.startswith("."):
            raise SystemExit(f"ERRO: nome inesperado no manifesto: {name!r}")
        dst = os.path.join(a.dest, "files", name)
        if os.path.exists(dst) and os.path.getsize(dst) == f["size"] and md5_file(dst) == f["md5"]:
            continue
        url = f.get("url") or (man["location"] + "?" + urllib.parse.urlencode(
            {"type": man["channel"], "tag": man["tag"], "file": name}))
        log(f"  [{i}/{len(files)}] {name} ({f['size'] >> 10} KB)")
        download(url, dst, f["size"])
        got = md5_file(dst)
        if got != f["md5"]:
            os.remove(dst)
            raise SystemExit(f"ERRO: MD5 de {name} nao confere ({got} != {f['md5']}); rode de novo")
    log(f"OK: todos os arquivos conferidos em {os.path.join(a.dest, 'files')}")


if __name__ == "__main__":
    main()
