#!/usr/bin/env python3
"""Cruza o autorun_runtime.log (telemetria do runtime, a cada 10 s) com as janelas [PERF] do d2nxpad.log.

  uso: tools/correlacionar_runtime_console.py <pasta com d2nxpad.log, D2Autorun.log e autorun_runtime.log>

O relogio do runtime comeca no boot; o do D2NXPad e de parede. A hora do boot vem da 1a linha do D2Autorun.log.
Por intervalo de 10 s mostra: quadros/s do runtime, ms de GL por quadro (preso em ~16,4 ms = esperando o vsync, nao e
custo de CPU), leitura do SD (sd_ms, rd_ms), invalidacoes do tradutor, nucleos usados e as 3 threads mais ocupadas
(4w = principal), ao lado do FPS medio, pior quadro e quadros > 33/50 ms das janelas de 5 s do pad.
Serve para ver se as travadinhas acompanham a leitura do SD (sd_ms alto) ou a CPU da thread principal."""
import os
import re
import statistics as st
import sys


def wall(t):
    h, m, s = t.split(":")
    return int(h) * 3600 + int(m) * 60 + float(s)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    d = sys.argv[1]
    t0 = None
    for l in open(os.path.join(d, "D2Autorun.log"), errors="replace"):
        m = re.match(r"\[(\d+:\d+:[\d.]+)\]", l)
        if m:
            t0 = wall(m[1])
            break
    if t0 is None:
        print("D2Autorun.log sem hora na 1a linha")
        return 1
    rt = []
    for l in open(os.path.join(d, "autorun_runtime.log"), errors="replace"):
        m = re.match(r"\[PROGRESS\] (\d+)s (.*)", l)
        if m:
            rt.append((int(m[1]), dict(re.findall(r"(\w+)=(\S+)", m[2]))))
            continue
        m = re.match(r"\[THREADS\] \d+ threads use ([\d.]+) cores: (.*)", l)
        if m and rt:
            rt[-1][1]["cores"] = m[1]
            rt[-1][1]["thr"] = " ".join(m[2].split()[:3])
    perf = []
    for l in open(os.path.join(d, "d2nxpad.log"), errors="replace"):
        m = re.match(r"\[(\d+:\d+:[\d.]+)\] \[PERF\] fps ([\d.]+) \| pior quadro (\d+) ms \(nivel (\d+).*"
                     r">33ms (\d+) >50ms (\d+)", l)
        if m:
            perf.append((wall(m[1]) - t0, float(m[2]), int(m[3]), int(m[4]), int(m[5]), int(m[6])))
    print("t_rt  fps_rt  gl_ms/fr  sd_ms  rd_ms  ninv  cores | pad: fps  pior  >33  >50  nivel | threads")
    prev = None
    for t, kv in rt:
        if prev:
            dt, p = t - prev[0], prev[1]
            dl = lambda k: float(kv[k]) - float(p[k])
            fr = dl("frames")
            w = [x for x in perf if t - dt <= x[0] < t]
            pp = ""
            if w:
                pp = (f"{st.mean(x[1] for x in w):5.1f} {max(x[2] for x in w):5d} {sum(x[4] for x in w):4d} "
                      f"{sum(x[5] for x in w):4d}  {w[0][3]}")
            print(f"{t:4d}  {fr / dt:6.1f}  {dl('gl_ms') / max(fr, 1):8.1f}  {dl('sd_ms'):5.0f}  {dl('read_ms'):5.0f}  "
                  f"{dl('invalidations'):5.0f}  {kv.get('cores', ''):>5} | {pp} | {kv.get('thr', '')}")
        prev = (t, kv)
    return 0


if __name__ == "__main__":
    sys.exit(main())
