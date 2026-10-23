#!/bin/bash
# Teste de CRIACAO de personagem no Ryujinx (o AutoJoin so carrega personagem existente).
# Liga AutoCreate=1 no d2nxpad.ini do SD de teste: escolhe a Amazon, escreve o nome com as funcoes do
# editor de nome do R3 (esperado "Testd"), entra no jogo, Save and Exit pelo ESC e recarrega o save.
# Uso: tools/run_ryujinx_fresh_test.sh [segundos]
#   ROOT=build/ryujinx-test (padrao) ou outra raiz do emulador com hbl/ncas/nca-1.nca e sdcard/.
#   Os saves de teste (MedianXL) saem do SD durante o teste e voltam no fim; o que o teste criou fica
#   em build/fresh-test-saves/. EMPTYDIR=1 cria a pasta save vazia antes (sem ele: console recem-instalado).
#   Com xprop e ImageMagick (import), captura a janela do emulador em build/fresh-test-shots/ (o
#   Print Screen do jogo nao mostra o menu do ESC).
# Obs.: o host do Ryujinx diferencia maiusculas; o SD de teste tem variantes de nome das DLLs/MPQs
# que no FAT32 do console nao sao necessarias.
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$BASE_DIR"
ROOT=${ROOT:-build/ryujinx-test}
WINE="$ROOT/sdcard/switch/wine"
D2="$WINE/drive_c/DiabloII"
INI="$D2/d2nx/d2nxpad.ini"
MXL="$WINE/drive_c/users/steamuser/AppData/Roaming/MedianXL"
PARK=build/fresh-test-parked
SHOTS=build/fresh-test-shots
LIMIT=${1:-420}
if pgrep -f "^/tmp/ryujinx-d2" >/dev/null; then echo "ERRO: outro Ryujinx de teste ja esta rodando"; exit 1; fi
if [ -d "$PARK" ]; then echo "ERRO: $PARK existe (saves de um teste interrompido): devolva para $MXL antes"; exit 1; fi
[ -d "$MXL" ] && mv "$MXL" "$PARK"
[ -n "$EMPTYDIR" ] && mkdir -p "$MXL/save"
cp src-d2nx/D2NXPad.dll "$D2/d2nx/D2NXPad.dll"
cp src-d2nx/D2Autorun.dll "$D2/D2Autorun.dll"
sed -i 's/^AutoJoin=.*/AutoJoin=0/; s/^AutoWalk=.*/AutoWalk=0/; /^AutoCreate=/d' "$INI"
sed -i 's/^AutoJoin=0/AutoJoin=0\nAutoCreate=1/' "$INI"
echo "sdmc:/switch/wine/drive_c/DiabloII/Game.exe" > "$WINE/run-next.txt"
rm -f "$ROOT/ryu.log" "$D2/D2Autorun.log" "$D2/d2nx/d2nxpad.log" "$WINE"/logs/*
rm -rf "$SHOTS"; mkdir -p "$SHOTS"
ln -sf "$(which ryujinx)" /tmp/ryujinx-d2
nice -n 10 /tmp/ryujinx-d2 --root-data-dir "$BASE_DIR/$ROOT" --handheld-mode --hide-updates \
    "$BASE_DIR/$ROOT/hbl/ncas/nca-1.nca" > "$ROOT/ryu.log" 2>&1 &
RYU=$!
echo "Ryujinx PID $RYU (limite ${LIMIT}s)"
win=""
n=0
end=$((SECONDS + LIMIT))
while [ $SECONDS -lt $end ] && kill -0 $RYU 2>/dev/null; do
    sleep 2
    if [ -z "$win" ] && command -v xprop >/dev/null && command -v import >/dev/null; then
        for id in $(xprop -root _NET_CLIENT_LIST 2>/dev/null | sed 's/.*# //; s/,//g'); do
            xprop -id $id WM_CLASS 2>/dev/null | grep -qi ryujinx && win=$id
        done
    fi
    if [ -n "$win" ] && grep -q 'AutoCreate\] classe' "$D2/d2nx/d2nxpad.log" 2>/dev/null; then
        n=$((n + 1)); f="$SHOTS/$(printf %03d $n).png"
        timeout 15 import -window $win "$f" 2>/dev/null && echo "$(date -u +%T) $(basename "$f")" >> "$SHOTS/times.txt"
    fi
    if grep -q -E '\[AutoCreate\] (SUCESSO 2/2|FALHA)' "$D2/d2nx/d2nxpad.log" 2>/dev/null; then sleep 3; break; fi
done
kill -15 $RYU 2>/dev/null; sleep 2; kill -9 $RYU 2>/dev/null; wait $RYU 2>/dev/null
sed -i '/^AutoCreate=/d' "$INI"
echo "--- AutoCreate"; grep -E 'AutoCreate|AutoTest|editor de nome' "$D2/d2nx/d2nxpad.log" 2>/dev/null || echo "(sem log do D2NXPad)"
echo "--- D2Autorun"; grep -E 'memoria abaixo|pasta de saves|Halt|DIAGNOSTICO' "$D2/D2Autorun.log" 2>/dev/null
echo "--- saves criados"; find "$MXL" -type f -printf '%s %p\n' 2>/dev/null
rm -rf build/fresh-test-saves
[ -d "$MXL" ] && mv "$MXL" build/fresh-test-saves
[ -d "$PARK" ] && mv "$PARK" "$MXL"
echo "--- capturas: $SHOTS ($n)"
