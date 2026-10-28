#!/usr/bin/env python3
"""Resume build/fluidez/<rotulo>/runtime.log: FPS do D2GL por intervalo de [PROGRESS], tempo no GL por quadro,
carga das threads e (se houver) os modulos x86 mais quentes. Considera so os intervalos com o heroi andando
(gl_frames subindo) - os ultimos intervalos antes de a medicao terminar."""
import re, sys, statistics

out, label, t_in = sys.argv[1], sys.argv[2], sys.argv[3]
log = open(f"{out}/runtime.log", errors="replace").read().splitlines()
prog = []
for l in log:
    m = re.match(r"\[PROGRESS\] (\d+)s ", l)
    if not m:
        continue
    kv = dict(re.findall(r"(\w+)=(-?\d+)", l))
    prog.append((int(m[1]), {k: int(v) for k, v in kv.items()}, l))
rows = []
for (t0, a, _), (t1, b, _) in zip(prog, prog[1:]):
    dt = t1 - t0
    f = b.get("gl_frames", 0) - a.get("gl_frames", 0)
    if dt <= 0 or f < 0:
        continue
    gl = (b.get("gl_ms", 0) - a.get("gl_ms", 0))
    sw = (b.get("swap_ms", 0) - a.get("swap_ms", 0))
    rows.append(dict(t=t1, fps=f / dt, gl_ms_f=gl / f if f else 0, swap_ms_f=sw / f if f else 0,
                     calls_f=(b.get("gl_calls", 0) - a.get("gl_calls", 0)) / f if f else 0,
                     inval=b.get("invalidations", 0) - a.get("invalidations", 0),
                     sd_ms=b.get("sd_ms", 0) - a.get("sd_ms", 0),
                     code=f"{b.get('code_mb', 0)}"))
walk = [r for r in rows if r["fps"] > 5]
steady = walk[-6:] if len(walk) >= 6 else walk
print(f"=== {label} (entrou no jogo apos {t_in}s do script) ===")
for r in rows[-9:]:
    print(f"  t={r['t']:4d}s fps {r['fps']:5.1f} | GL {r['gl_ms_f']:5.2f} ms/quadro (swap {r['swap_ms_f']:4.2f}, {r['calls_f']:4.0f} chamadas) | "
          f"invalidacoes {r['inval']:5d} | SD {r['sd_ms']:4d} ms")
if steady:
    fps = [r["fps"] for r in steady]
    print(f"  >> FPS mediana {statistics.median(fps):.1f}  min {min(fps):.1f}  max {max(fps):.1f} (ultimos {len(steady)} intervalos)")
    print(f"  >> GL {statistics.mean(r['gl_ms_f'] for r in steady):.2f} ms/quadro, "
          f"{statistics.mean(r['calls_f'] for r in steady):.0f} chamadas/quadro")
th = [l for l in log if l.startswith("[THREADS]")]
for l in th[-2:]:
    print("  " + l[:210])
prof = [l for l in log if l.startswith("[PROF]")]
if prof:
    print(f"  [PROF] {len(prof)} linhas; modulos x86 por thread (ultimas):")
    for l in [p for p in prof if "x86 by module" in p][-6:]:
        print("   " + l[:330])
    for l in [p for p in prof if "samples=" in p][-4:]:
        print("   " + l[:330])
