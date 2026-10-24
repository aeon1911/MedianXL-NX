#!/bin/bash
# Teste de suavidade do movimento nativo no Ryujinx (WalkTest=1 no d2nxpad.ini de teste):
# o herói anda em 16 direcoes (a cada 22,5 graus), 2 s indo e 2 s voltando, e o D2NXPad registra cada
# quadro (posicao, modo, quadro da animacao, direcao do caminho). A analise mede, por direcao:
#   reinicios da animacao de andar (o "passinho"), trocas de direcao do caminho (tremida),
#   quadros parados no meio da caminhada e velocidade.
# Uso: tools/run_walk_test_ryujinx.sh [ModoOrdem] [RuidoAnalogico]   (padrao: o do ini, 0.0)
#   ModoOrdem=0: ordem antiga (alvo a 7 subtiles recalculado a cada 130 ms); 1: alvo estavel (atual)
#   RuidoAnalogico: ruido aleatorio somado ao analogico (o controle de verdade nunca fica parado)
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$BASE_DIR"
ROOT=build/ryujinx-test
WINE="$ROOT/sdcard/switch/wine"
D2="$WINE/drive_c/DiabloII"
INI="$D2/d2nx/d2nxpad.ini"
OUT=build/walk-test
MODE=${1:-}
NOISE=${2:-0.0}
if pgrep -f "^/tmp/ryujinx-d2" >/dev/null; then echo "ERRO: outro Ryujinx de teste ja esta rodando"; exit 1; fi
python3 tools/assemble_sd_tree.py >/dev/null 2>&1 || { echo "ERRO: montagem do SD falhou"; exit 1; }
mkdir -p "$OUT"
sed -i '/^WalkTest=/d; /^RuidoAnalogico=/d; /^AutoCreate=/d; /^MenuTest=/d; s/^AutoJoin=.*/AutoJoin=1/; s/^AutoWalk=.*/AutoWalk=0/' "$INI"
sed -i "s/^AutoJoin=1/AutoJoin=1\nWalkTest=${WT:-1}\nRuidoAnalogico=$NOISE/" "$INI"
if [ -n "$MODE" ]; then
    sed -i '/^ModoOrdem=/d' "$INI"
    sed -i "s/^\[Movimento\]/[Movimento]\nModoOrdem=$MODE/" "$INI"
fi
echo "sdmc:/switch/wine/drive_c/DiabloII/Game.exe" > "$WINE/run-next.txt"
rm -f "$ROOT/ryu.log" "$D2/D2Autorun.log" "$D2/d2nx/d2nxpad.log" "$WINE"/logs/*
ln -sf "$(which ryujinx)" /tmp/ryujinx-d2
nice -n 10 /tmp/ryujinx-d2 --root-data-dir "$BASE_DIR/$ROOT" --handheld-mode --hide-updates \
    "$BASE_DIR/$ROOT/hbl/ncas/nca-1.nca" > "$ROOT/ryu.log" 2>&1 &
RYU=$!
end=$((SECONDS + 420))
while [ $SECONDS -lt $end ] && kill -0 $RYU 2>/dev/null; do
    sleep 2
    grep -q '\[WalkTest\] fim' "$D2/d2nx/d2nxpad.log" 2>/dev/null && break
done
kill -15 $RYU 2>/dev/null; sleep 2; kill -9 $RYU 2>/dev/null; wait $RYU 2>/dev/null
sed -i '/^WalkTest=/d; /^RuidoAnalogico=/d; /^ModoOrdem=/d; s/^AutoJoin=.*/AutoJoin=0/' "$INI"
TAG="modo${MODE:-ini}-ruido$NOISE"
cp "$D2/d2nx/d2nxpad.log" "$OUT/$TAG.log"
python3 - "$OUT/$TAG.log" <<'EOF'
import re, sys, math
segs = {}; cur = None
for line in open(sys.argv[1], errors="replace"):
    m = re.search(r"\[WalkTest\] segmento (\d+) angulo (\d+) (\w+)", line)
    if m:
        cur = (int(m[1]), int(m[2]), m[3]) if m[3] != "parado" else None
        if cur: segs.setdefault(cur, [])
        continue
    m = re.search(r"\[WF\] (\d+) pos (\d+)\.(\d+),(\d+)\.(\d+) modo (\d+) gfx (\d+) dir (\d+) alvo (\d+),(\d+) mv (-?\d+) ordem (-?\d+),(-?\d+)", line)
    if m and cur:
        segs[cur].append(dict(t=int(m[1]), x=int(m[2]) + int(m[3]) / 65536, y=int(m[4]) + int(m[5]) / 65536,
                              mode=int(m[6]), gfx=int(m[7]), dir=int(m[8]), mv=int(m[11]), reta=(m[9], m[10]) == (m[12], m[13])))
if not segs:
    print("sem dados de WalkTest no log"); sys.exit(1)
tot = dict(restarts=0, dirflips=0, stalls=0, n=0)
print("seg ang  ida/volta  quadros  em_linha_reta  vel(sub/s)  trocas_dir(reta)  quadros_parado  modos")
for (k, ang, way), fr in sorted(segs.items()):
    fr = [f for f in fr if f["mv"]]
    if len(fr) < 10: continue
    fr = fr[15:]                     # ignora a arrancada e a virada
    dist = math.hypot(fr[-1]["x"] - fr[0]["x"], fr[-1]["y"] - fr[0]["y"])
    dt = (fr[-1]["t"] - fr[0]["t"]) / 1000 or 1
    restarts = sum(1 for a, b in zip(fr, fr[1:]) if b["mode"] in (2, 3) and a["mode"] in (2, 3) and b["gfx"] < a["gfx"] and a["gfx"] - b["gfx"] < 0x40000 and b["gfx"] < 0x1000)
    dirflips = sum(1 for a, b in zip(fr, fr[1:]) if a["dir"] != b["dir"] and a["reta"] and b["reta"])
    retos = sum(1 for f in fr if f["reta"])
    stalls = sum(1 for a, b in zip(fr, fr[1:]) if a["x"] == b["x"] and a["y"] == b["y"])
    modes = sorted({f["mode"] for f in fr})
    tot["restarts"] += restarts; tot["dirflips"] += dirflips; tot["stalls"] += stalls; tot["n"] += len(fr)
    tot["retos"] = tot.get("retos", 0) + retos
    print(f"{k:3d} {ang:3d}  {way:6s}  {len(fr):7d}  {retos:13d}  {dist/dt:9.2f}  {dirflips:16d}  {stalls:14d}  {modes}")
print(f"TOTAL quadros {tot['n']} em linha reta {tot.get('retos', 0)} trocas de direcao em linha reta {tot['dirflips']} quadros parado {tot['stalls']}")
EOF
