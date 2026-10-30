#!/usr/bin/env python3
"""Copia do cartao SD do Switch os logs de uma sessao e resume a fluidez.

  uso: tools/coletar_logs_console.py <raiz do cartao SD> [pasta de destino]

Copia (sao sobrescritos a cada partida, entao rode logo depois de jogar):
  switch/wine/drive_c/DiabloII/d2nx/d2nxpad.log   [PERF] (FPS, quadros lentos), [THR], [MEM], [RT] (copia do log do runtime)
  switch/wine/drive_c/DiabloII/D2Autorun.log      boot do D2Autorun
  switch/wine/logs/autorun_runtime.log            [PROGRESS], [THREADS], [DYNAREC] (memoria de codigo do tradutor), [PROF]
Depois imprime o resumo do d2nxpad.log (tools/analisar_perf_log.py) e o cruzamento com a telemetria do runtime
(tools/correlacionar_runtime_console.py)."""
import os
import shutil
import subprocess
import sys
import time

FILES = [
    "switch/wine/drive_c/DiabloII/d2nx/d2nxpad.log",
    "switch/wine/drive_c/DiabloII/D2Autorun.log",
    "switch/wine/logs/autorun_runtime.log",
]


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    sd = sys.argv[1]
    dest = sys.argv[2] if len(sys.argv) > 2 else time.strftime("build/logs-console/%Y%m%d-%H%M%S")
    os.makedirs(dest, exist_ok=True)
    got = {}
    for rel in FILES:
        src = os.path.join(sd, rel)
        if not os.path.isfile(src):
            print(f"  (nao achei) {rel}")
            continue
        out = os.path.join(dest, os.path.basename(rel))
        shutil.copy2(src, out)
        got[os.path.basename(rel)] = out
        print(f"  copiado {rel} ({os.path.getsize(out)} bytes) -> {out}")
    here = os.path.dirname(os.path.abspath(__file__))
    log = got.get("d2nxpad.log")
    if log:
        print()
        subprocess.call([sys.executable, os.path.join(here, "analisar_perf_log.py"), log])
    if len(got) == len(FILES):
        print("\nruntime x pad (tools/correlacionar_runtime_console.py):")
        subprocess.call([sys.executable, os.path.join(here, "correlacionar_runtime_console.py"), dest])
    return 0


if __name__ == "__main__":
    sys.exit(main())
