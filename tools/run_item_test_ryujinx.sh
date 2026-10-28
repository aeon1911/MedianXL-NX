#!/bin/bash
# Regressao do crash "The file data is corrupt (medianxl-data.mpq)" ao usar o item de 1.500 de ouro do
# kit inicial (o som do item estava recomprimido em zlib no MPQ unico; a Storm 1.13c nao le zlib).
# Carrega o personagem criado pelo tools/run_ryujinx_fresh_test.sh (build/fresh-test-saves, copia: o save
# nao e alterado), e no jogo:
#   ReadTest=1  le pela Storm do jogo DATA\GLOBAL\SFX\taunt\holy_ding.wav e confere o CRC (20ec3821);
#   ItemTest=2  abre o inventario e clica com o botao direito no item de ouro (ITEMX,ITEMY na janela).
# Aprovado: CRC certo, nenhuma MessageBox de erro, nenhuma queda, jogo vivo depois do clique.
# Capturas da janela do Ryujinx em build/item-test-shots/ (o ouro do inventario vai de 0 para 1.500).
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$BASE_DIR"
ROOT=build/ryujinx-test
WINE="$ROOT/sdcard/switch/wine"
D2="$WINE/drive_c/DiabloII"
INI="$D2/d2nx/d2nxpad.ini"
MXL="$WINE/drive_c/users/steamuser/AppData/Roaming/MedianXL"
PARK=build/item-test-parked
SHOTS=build/item-test-shots
SAVES=build/fresh-test-saves
if pgrep -f "^/tmp/ryujinx-d2" >/dev/null; then echo "ERRO: outro Ryujinx de teste ja esta rodando"; exit 1; fi
[ -d "$SAVES/save" ] || { echo "ERRO: $SAVES nao existe: rode antes tools/run_ryujinx_fresh_test.sh"; exit 1; }
[ -d "$PARK" ] && { echo "ERRO: $PARK existe (saves de um teste interrompido): devolva para $MXL antes"; exit 1; }
python3 tools/assemble_sd_tree.py >/dev/null 2>&1 || { echo "ERRO: montagem do SD falhou"; exit 1; }
rm -rf "$SHOTS"; mkdir -p "$SHOTS"
[ -d "$MXL" ] && mv "$MXL" "$PARK"
mkdir -p "$MXL"; cp -a "$SAVES/." "$MXL/"
# Debug=1: os criterios leem mensagens dbg() (o INI do pacote vai com Debug=0)
sed -i '/^ItemTest=/d; /^ItemX=/d; /^ItemY=/d; /^ItemDelay=/d; /^ReadTest=/d; s/^Debug=.*/Debug=1/; s/^AutoJoin=.*/AutoJoin=1/; s/^AutoWalk=.*/AutoWalk=0/' "$INI"
sed -i "s/^AutoJoin=1/AutoJoin=1\nReadTest=1\nItemTest=2\nItemX=${ITEMX:-856}\nItemY=${ITEMY:-496}\nItemDelay=${DELAY:-30000}/" "$INI"
echo "sdmc:/switch/wine/drive_c/DiabloII/Game.exe" > "$WINE/run-next.txt"
rm -f "$ROOT/ryu.log" "$D2/D2Autorun.log" "$D2/d2nx/d2nxpad.log" "$WINE"/logs/*
ln -sf "$(which ryujinx)" /tmp/ryujinx-d2
nice -n 10 /tmp/ryujinx-d2 --root-data-dir "$BASE_DIR/$ROOT" --handheld-mode --hide-updates \
    "$BASE_DIR/$ROOT/hbl/ncas/nca-1.nca" > "$ROOT/ryu.log" 2>&1 &
RYU=$!
win=""; n=0; t_in=0; alive=0; end=$((SECONDS + 420))
while [ $SECONDS -lt $end ] && kill -0 $RYU 2>/dev/null; do
    sleep 1
    if [ -z "$win" ] && command -v xprop >/dev/null && command -v import >/dev/null; then
        for id in $(xprop -root _NET_CLIENT_LIST 2>/dev/null | sed 's/.*# //; s/,//g'); do
            xprop -id $id WM_CLASS 2>/dev/null | grep -qi ryujinx && win=$id
        done
    fi
    if grep -q 'SUCESSO: Personagem entrou' "$D2/d2nx/d2nxpad.log" 2>/dev/null; then
        [ $t_in = 0 ] && t_in=$SECONDS
        if [ -n "$win" ]; then
            n=$((n + 1)); f="$SHOTS/$(printf %03d $n).png"
            timeout 15 import -silent -window $win "$f" 2>/dev/null && echo "$(date -u +%T) $(basename "$f")" >> "$SHOTS/times.txt"
        fi
        if [ $((SECONDS - t_in)) -ge ${HOLD:-50} ]; then alive=1; break; fi
    fi
done
kill -15 $RYU 2>/dev/null; sleep 2; kill -9 $RYU 2>/dev/null; wait $RYU 2>/dev/null
sed -i '/^ItemTest=/d; /^ItemX=/d; /^ItemY=/d; /^ItemDelay=/d; /^ReadTest=/d; s/^AutoJoin=.*/AutoJoin=0/' "$INI"
rm -rf "$MXL"; [ -d "$PARK" ] && mv "$PARK" "$MXL"
cp "$D2/d2nx/d2nxpad.log" "$D2/D2Autorun.log" "$SHOTS/" 2>/dev/null

bad=0
ok() { echo "[OK ] $1"; }
fail() { echo "[FALHA] $1"; bad=$((bad + 1)); }
grep -q 'ReadTest.*crc 20ec3821' "$SHOTS/d2nxpad.log" && ok "Storm do jogo le holy_ding.wav certo (crc 20ec3821)" \
    || fail "leitura de holy_ding.wav: $(grep -m1 ReadTest "$SHOTS/d2nxpad.log")"
grep -q 'ItemTest\] botao direito' "$SHOTS/d2nxpad.log" && ok "item usado (botao direito no inventario)" || fail "o clique no item nao aconteceu"
grep -q 'MessageBox \[' "$SHOTS/D2Autorun.log" && fail "MessageBox: $(grep -m1 'MessageBox \[' "$SHOTS/D2Autorun.log")" || ok "nenhuma MessageBox de erro"
[ "$(grep -c 'Unhandled exception' "$ROOT/ryu.log")" = 0 ] && ok "sem queda do emulador" || fail "o emulador caiu"
[ $alive = 1 ] && ok "jogo vivo ${HOLD:-50}s depois de entrar" || fail "o jogo fechou antes do fim"
echo "--- $((5 - bad))/5 criterios ok; capturas em $SHOTS (o ouro do inventario deve ir de 0 para 1.500)"
exit $bad
