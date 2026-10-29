#!/usr/bin/env python3
"""Resume as linhas [PERF] (e [THR]/[MEM]/[RT]) de um d2nxpad.log do console ou do Ryujinx.

  uso: tools/analisar_perf_log.py <d2nxpad.log> [segundos_minimos_de_partida=0]

Mostra: FPS por nivel e modo (modo 2 = painel aberto, 3 = partida), FPS por minuto fora da cidade, quadros > 500 ms com o
contexto (painel aberto, nivel), a distribuicao dos tempos de quadro das janelas sem travada (vsync travado em 16,7/33,3 ms
apareceria como degraus) e as linhas de diagnostico [THR] (threads com modulo), [MEM] e [RT] (memoria de codigo do tradutor,
heap, GL, audio, copiadas do log do runtime)."""
import collections
import re
import statistics as st
import sys

PERF = re.compile(r"\[(\d+:\d+:\d+\.\d+)\] \[PERF\] fps ([\d.]+) \| pior quadro (\d+) ms \(nivel (\d+), (\d+),(\d+)\) \| "
                  r">33ms (\d+) >50ms (\d+) >100ms (\d+) \| gancho ([\d.]+) ms/s(?: \| picos>50ms \(ms:dur\) (.*))?")
MODE = re.compile(r"\[(\d+:\d+:\d+\.\d+)\] modo (\d) -> (\d) \(paineis abertos: (.*)\)")


def secs(t):
    h, m, r = t.split(":")
    return int(h) * 3600 + int(m) * 60 + float(r)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    lines = open(sys.argv[1], errors="replace").read().splitlines()
    rows, mode, panels = [], 3, "nenhum"
    diag = []
    for l in lines:
        m = MODE.match(l)
        if m:
            mode, panels = int(m[3]), m[4].strip()
            continue
        if any(k in l for k in ("[THR]", "[MEM]", "[RT]")):
            diag.append(l)
            continue
        m = PERF.match(l)
        if not m:
            continue
        spikes = []
        if m[11]:
            spikes = [(int(a.lstrip("+")), int(b)) for a, b in (x.split(":") for x in m[11].split())]
        rows.append(dict(t=secs(m[1]), fps=float(m[2]), worst=int(m[3]), lvl=int(m[4]), n33=int(m[7]), n50=int(m[8]),
                         n100=int(m[9]), hook=float(m[10]), mode=mode, panels=panels, spikes=spikes))
    if not rows:
        print("nenhuma linha [PERF] (Perf=0 no d2nxpad.ini?)")
        return 1
    t0 = rows[0]["t"]
    skip = float(sys.argv[2]) if len(sys.argv) > 2 else 0
    rows = [r for r in rows if r["t"] - t0 >= skip]
    print(f"{len(rows)} janelas de 5 s, {(rows[-1]['t'] - rows[0]['t']) / 60:.1f} min de partida\n")

    agg = collections.defaultdict(list)
    for r in rows:
        agg[(r["lvl"], r["mode"])].append(r)
    print("nivel modo janelas  fps_med  fps_min  fps_p10  >33ms/5s  >50ms/5s  gancho_ms/s")
    for k in sorted(agg):
        v, f = agg[k], sorted(x["fps"] for x in agg[k])
        print(f"{k[0]:>5} {k[1]:>4} {len(v):>7} {st.mean(f):8.1f} {f[0]:8.1f} {f[len(f) // 10]:8.1f} "
              f"{st.mean(x['n33'] for x in v):9.1f} {st.mean(x['n50'] for x in v):9.1f} {st.mean(x['hook'] for x in v):11.2f}")

    print("\nFPS por minuto (partida, fora da cidade):")
    per_min = collections.defaultdict(list)
    for r in rows:
        if r["mode"] == 3 and r["lvl"] != 1:
            per_min[int((r["t"] - t0) // 60)].append(r["fps"])
    for k in sorted(per_min):
        print(f"  min {k:>3}: fps medio {st.mean(per_min[k]):5.1f}  minimo {min(per_min[k]):5.1f}")

    print("\nquadros > 500 ms:")
    total = 0
    for r in rows:
        for off, d in r["spikes"]:
            if d >= 500:
                total += d
                print(f"  +{r['t'] - t0:7.1f}s nivel {r['lvl']} modo {r['mode']} paineis {r['panels']:<8} {d} ms")
    print(f"  soma: {total / 1000:.1f} s de {rows[-1]['t'] - rows[0]['t']:.0f} s")

    clean = sorted(1000 / r["fps"] for r in rows if r["n33"] == 0 and r["worst"] <= 35)
    if clean:
        print(f"\nTempo medio de quadro nas {len(clean)} janelas sem quadro > 33 ms (vsync travado daria degraus em 16,7 e 33,3 ms):")
        print("  " + " ".join(f"{x:.1f}" for x in clean))

    if diag:
        print("\ndiagnostico:")
        for l in diag[:40]:
            print("  " + l[:420])
    return 0


if __name__ == "__main__":
    sys.exit(main())
