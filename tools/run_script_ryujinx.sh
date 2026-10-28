#!/bin/bash
# Roda um roteiro de teste do D2NXPad no Ryujinx (AutoTest Script=): o roteiro aperta os botoes do controle
# pelo mesmo caminho do XInput (direcional, A, B, analogicos...). Comandos: veja script_tick em d2nxpad.c.
# Uso: tools/run_script_ryujinx.sh <roteiro.txt> [limite em segundos]
#   Carrega o personagem de teste (AutoJoin). Capturas da janela a cada ~1 s em build/script-shots/<nome>/
#   (times.txt tem a hora UTC de cada captura, a mesma do log, para casar com as MARCAs do d2nxpad.log).
#   SAVES=<pasta>: usa os saves dessa pasta (copia) em vez dos do SD de teste.
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$BASE_DIR"
SCRIPT="$1"; LIMIT=${2:-300}
[ -f "$SCRIPT" ] || { echo "uso: $0 <roteiro.txt> [segundos]"; exit 1; }
ROOT=build/ryujinx-test
WINE="$ROOT/sdcard/switch/wine"
D2="$WINE/drive_c/DiabloII"
INI="$D2/d2nx/d2nxpad.ini"
MXL="$WINE/drive_c/users/steamuser/AppData/Roaming/MedianXL"
PARK=build/script-parked
NAME=$(basename "$SCRIPT" .txt)
SHOTS=build/script-shots/$NAME
if pgrep -f "^/tmp/ryujinx-d2" >/dev/null; then echo "ERRO: outro Ryujinx de teste ja esta rodando"; exit 1; fi
[ -d "$PARK" ] && { echo "ERRO: $PARK existe (saves de um teste interrompido): devolva para $MXL antes"; exit 1; }
python3 tools/assemble_sd_tree.py >/dev/null 2>&1 || { echo "ERRO: montagem do SD falhou"; exit 1; }
rm -rf "$SHOTS"; mkdir -p "$SHOTS"
if [ -n "$SAVES" ]; then
    [ -d "$MXL" ] && mv "$MXL" "$PARK"
    mkdir -p "$MXL"; cp -a "$SAVES/." "$MXL/"
fi
cp "$SCRIPT" "$D2/d2nx/roteiro.txt"
# Debug=1: os criterios leem mensagens dbg() (o INI do pacote vai com Debug=0)
sed -i '/^Script=/d; /^AutoCreate=/d; /^MenuTest=/d; /^WalkTest=/d; s/^Debug=.*/Debug=1/; s/^AutoJoin=.*/AutoJoin=1/; s/^AutoWalk=.*/AutoWalk=0/' "$INI"
sed -i "s/^AutoJoin=1/AutoJoin=1\nScript=roteiro.txt/" "$INI"
echo "sdmc:/switch/wine/drive_c/DiabloII/Game.exe" > "$WINE/run-next.txt"
rm -f "$ROOT/ryu.log" "$D2/D2Autorun.log" "$D2/d2nx/d2nxpad.log" "$WINE"/logs/*
ln -sf "$(which ryujinx)" /tmp/ryujinx-d2
nice -n 10 /tmp/ryujinx-d2 --root-data-dir "$BASE_DIR/$ROOT" --handheld-mode --hide-updates \
    "$BASE_DIR/$ROOT/hbl/ncas/nca-1.nca" > "$ROOT/ryu.log" 2>&1 &
RYU=$!
win=""; n=0; done_t=0; end=$((SECONDS + LIMIT))
while [ $SECONDS -lt $end ] && kill -0 $RYU 2>/dev/null; do
    sleep 0.5
    if [ -z "$win" ] && command -v xprop >/dev/null; then
        for id in $(xprop -root _NET_CLIENT_LIST 2>/dev/null | sed 's/.*# //; s/,//g'); do
            xprop -id $id WM_CLASS 2>/dev/null | grep -qi ryujinx && win=$id
        done
    fi
    if [ -n "$win" ] && grep -q 'SUCESSO: Personagem entrou' "$D2/d2nx/d2nxpad.log" 2>/dev/null; then
        n=$((n + 1)); f="$SHOTS/$(printf %04d $n).png"
        timeout 15 import -silent -window $win "$f" 2>/dev/null && echo "$(date -u +%T.%N | cut -c1-12) $(basename "$f")" >> "$SHOTS/times.txt"
    fi
    if grep -q '\[Script\] fim' "$D2/d2nx/d2nxpad.log" 2>/dev/null; then
        [ $done_t = 0 ] && done_t=$SECONDS
        [ $((SECONDS - done_t)) -ge 3 ] && break
    fi
done
kill -15 $RYU 2>/dev/null; sleep 2; kill -9 $RYU 2>/dev/null; wait $RYU 2>/dev/null
sed -i '/^Script=/d; s/^AutoJoin=.*/AutoJoin=0/' "$INI"
if [ -n "$SAVES" ]; then rm -rf "$MXL"; [ -d "$PARK" ] && mv "$PARK" "$MXL"; fi
cp "$D2/d2nx/d2nxpad.log" "$D2/D2Autorun.log" "$SHOTS/" 2>/dev/null
echo "quedas do emulador: $(grep -c 'Unhandled exception' "$ROOT/ryu.log")"
grep -E '\[Script\]' "$SHOTS/d2nxpad.log" | tail -40
echo "capturas: $SHOTS ($n)"
