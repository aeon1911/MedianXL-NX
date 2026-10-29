#!/usr/bin/env python3
"""Instala um firmware do Switch (zip de NCAs) na raiz de dados do Ryujinx de teste, sem abrir a interface.

  uso: tools/instalar_firmware_ryujinx.py <Firmware.zip> [raiz=build/ryujinx-test]

Faz o mesmo que "Tools > Install Firmware" do Ryujinx 1.3.3: cada .nca do zip (".cnmt.nca" vira ".nca") vai para
<raiz>/bis/system/Contents/registered/<id>.nca/00, trocando a pasta "registered" inteira. As chaves (prod.keys e title.keys,
do console do usuario, NAO versionadas) ficam em <raiz>/system/ e precisam ser copiadas, nao ligadas por symlink."""
import os
import shutil
import sys
import zipfile


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    zip_path = sys.argv[1]
    root = sys.argv[2] if len(sys.argv) > 2 else "build/ryujinx-test"
    contents = os.path.join(root, "bis", "system", "Contents")
    registered, temp = os.path.join(contents, "registered"), os.path.join(contents, "temp")
    shutil.rmtree(temp, ignore_errors=True)
    n = 0
    with zipfile.ZipFile(zip_path) as z:
        for e in z.infolist():
            if not e.filename.lower().endswith(".nca"):
                continue
            nca_id = e.filename.replace(".cnmt", "").split("/")[-1]
            d = os.path.join(temp, nca_id)
            os.makedirs(d, exist_ok=True)
            with z.open(e) as src, open(os.path.join(d, "00"), "wb") as dst:
                shutil.copyfileobj(src, dst)
            n += 1
    shutil.rmtree(registered, ignore_errors=True)
    os.rename(temp, registered)
    print(f"{n} NCAs instaladas em {registered}")
    for k in ("prod.keys", "title.keys"):
        if not os.path.isfile(os.path.join(root, "system", k)):
            print(f"ATENCAO: falta {os.path.join(root, 'system', k)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
