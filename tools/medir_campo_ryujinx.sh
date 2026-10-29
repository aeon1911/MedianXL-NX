#!/bin/bash
# Fluidez NO CAMPO no Ryujinx, sem limite de FPS: usa o cenario da aceitacao (AutoWalk=2: anda na cidade, sai pelo portao para
# o Blood Moor, ataca) e resume os [PERF] por nivel (1 = cidade, 2 = Blood Moor). O limite de 60 FPS do D2GL (vsync e
# foreground_fps) e desligado so nesta rodada para o FPS mostrar o custo real do quadro: com o limite, tudo acima de 60 some.
#   uso: SHOTS_DIR=build/campo-<rotulo> tools/medir_campo_ryujinx.sh   (capturas e logs em SHOTS_DIR; o Ryujinx mostra o FPS na barra)
# Para comparar uma opcao do jogo, edite antes o preferences.json do save de teste (ex.: legacy_light_quality).
# (derivado de run_acceptance_ryujinx.sh; so o cenario, sem o teste do menu de cheats)
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$BASE_DIR"
ROOT=build/ryujinx-test
WINE="$ROOT/sdcard/switch/wine"
D2="$WINE/drive_c/DiabloII"
INI="$D2/d2nx/d2nxpad.ini"
SHOTS=${SHOTS_DIR:-build/field-shots}
if pgrep -f "^/tmp/ryujinx-d2" >/dev/null; then echo "ERRO: outro Ryujinx de teste ja esta rodando"; exit 1; fi
python3 tools/assemble_sd_tree.py >/dev/null 2>&1 || { echo "ERRO: montagem do SD falhou"; exit 1; }
python3 - "$D2/d2nx/d2nx.ini" <<'PY'
import sys
p=sys.argv[1]; t=open(p).read()
t=t.replace("[Video]\n","[Video]\nd2gl.foreground_fps=false\nd2gl.vsync=false\n",1)
open(p,"w").write(t)
PY
rm -rf "$SHOTS"; mkdir -p "$SHOTS"

run() {   # $1 = nome, $2 = linhas extras do [AutoTest], $3 = segundos no jogo
    # Debug=1: os criterios de A/X leem mensagens dbg() (o INI do pacote vai com Debug=0, sem gravar no SD a cada 2 s)
    sed -i '/^AutoCreate=/d; /^MenuTest=/d; /^CursorTest=/d; s/^Debug=.*/Debug=1/; s/^AutoJoin=.*/AutoJoin=1/; s/^AutoWalk=.*/AutoWalk=0/' "$INI"
    sed -i "s/^AutoJoin=1/AutoJoin=1\n$2/" "$INI"
    echo "sdmc:/switch/wine/drive_c/DiabloII/Game.exe" > "$WINE/run-next.txt"
    rm -f "$ROOT/ryu.log" "$D2/D2Autorun.log" "$D2/d2nx/d2nxpad.log" "$WINE"/logs/*
    ln -sf "$(which ryujinx)" /tmp/ryujinx-d2
    nice -n 10 /tmp/ryujinx-d2 --root-data-dir "$BASE_DIR/$ROOT" --handheld-mode --hide-updates \
        "$BASE_DIR/$ROOT/hbl/ncas/nca-1.nca" > "$ROOT/ryu.log" 2>&1 &
    local ryu=$! win="" n=0 t_in=0 end=$((SECONDS + 420))
    while [ $SECONDS -lt $end ] && kill -0 $ryu 2>/dev/null; do
        sleep 1
        if [ -z "$win" ] && command -v xprop >/dev/null && command -v import >/dev/null; then
            for id in $(xprop -root _NET_CLIENT_LIST 2>/dev/null | sed 's/.*# //; s/,//g'); do
                xprop -id $id WM_CLASS 2>/dev/null | grep -qi ryujinx && win=$id
            done
        fi
        if grep -q 'SUCESSO: Personagem entrou' "$D2/d2nx/d2nxpad.log" 2>/dev/null; then
            [ $t_in = 0 ] && t_in=$SECONDS
            if [ -n "$win" ]; then
                n=$((n + 1)); f="$SHOTS/$1-$(printf %03d $n).png"
                timeout 15 import -silent -window $win "$f" 2>/dev/null && echo "$(date -u +%T) $(basename "$f")" >> "$SHOTS/times.txt"
            fi
            [ $((SECONDS - t_in)) -ge $3 ] && break
        fi
    done
    kill -15 $ryu 2>/dev/null; sleep 2; kill -9 $ryu 2>/dev/null; wait $ryu 2>/dev/null
    cp "$D2/d2nx/d2nxpad.log" "$SHOTS/$1.log"
    grep -c 'Unhandled exception' "$ROOT/ryu.log" > "$SHOTS/$1.crash"
}

run cenario "AutoWalk=2" 85

python3 - "$SHOTS/cenario.log" <<'PY'
import re, sys, collections, statistics as st
rows = collections.defaultdict(list)
for l in open(sys.argv[1], errors="replace"):
    m = re.search(r"\[PERF\] fps ([\d.]+) \| pior quadro (\d+) ms \(nivel (\d+)", l)
    if m:
        rows[int(m[3])].append((float(m[1]), int(m[2])))
for lvl in sorted(rows):
    f = [x[0] for x in rows[lvl]]
    print(f"nivel {lvl}: {len(f)} janelas, FPS medio {st.mean(f):.1f} (min {min(f):.1f}), {1000 / st.mean(f):.1f} ms por quadro, pior quadro {max(x[1] for x in rows[lvl])} ms")
PY
