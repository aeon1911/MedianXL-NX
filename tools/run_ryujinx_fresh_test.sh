#!/bin/bash
# Teste de CRIACAO de personagem no Ryujinx (o AutoJoin so carrega personagem existente).
# Liga AutoCreate no d2nxpad.ini do SD de teste: cria personagem novo, entra, salva/sai e recarrega.
# Uso: tools/run_ryujinx_fresh_test.sh [segundos]
#   ROOT=build/ryujinx-test (padrao) ou outra raiz do emulador com hbl/ncas/nca-1.nca e sdcard/.
#   Para simular "sem personagens", mova antes users/steamuser/AppData/Roaming/MedianXL para fora do SD.
# Obs.: o host do Ryujinx diferencia maiusculas; o SD de teste tem variantes de nome das DLLs/MPQs
# que no FAT32 do console nao sao necessarias.
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$BASE_DIR"
ROOT=${ROOT:-build/ryujinx-test}
WINE=$ROOT/sdcard/switch/wine
D2=$WINE/drive_c/DiabloII
LIMIT=${1:-300}
if pgrep -f "^/tmp/ryujinx-d2" >/dev/null; then echo "ERRO: outro Ryujinx de teste ja esta rodando"; exit 1; fi
INI=$D2/d2nx/d2nxpad.ini
sed -i 's/^AutoJoin=.*/AutoJoin=0/; s/^AutoWalk=.*/AutoWalk=0/; /^AutoCreate=/d' $INI
sed -i 's/^AutoJoin=0/AutoJoin=0\nAutoCreate=1/' $INI
echo "sdmc:/switch/wine/drive_c/DiabloII/Game.exe" > $WINE/run-next.txt
rm -f $ROOT/ryu.log $D2/D2Autorun.log $D2/d2nx/d2nxpad.log $WINE/logs/*
ln -sf "$(which ryujinx)" /tmp/ryujinx-d2
nice -n 10 /tmp/ryujinx-d2 --root-data-dir "$BASE_DIR/$ROOT" --handheld-mode --hide-updates \
    "$BASE_DIR/$ROOT/hbl/ncas/nca-1.nca" > $ROOT/ryu.log 2>&1 &
RYU=$!
echo "Ryujinx PID $RYU (limite ${LIMIT}s)"
end=$((SECONDS + LIMIT))
while [ $SECONDS -lt $end ] && kill -0 $RYU 2>/dev/null; do
    sleep 3
    if grep -q -E '\[AutoCreate\] (SUCESSO 2/2|FALHA)' $D2/d2nx/d2nxpad.log 2>/dev/null; then sleep 3; break; fi
done
kill -15 $RYU 2>/dev/null; sleep 2; kill -9 $RYU 2>/dev/null; wait $RYU 2>/dev/null
sed -i '/^AutoCreate=/d' $INI
echo "--- AutoCreate"; grep -E 'AutoCreate|AutoTest|teclado|Wine-NX' $D2/d2nx/d2nxpad.log 2>/dev/null || echo "(sem log do D2NXPad)"
echo "--- D2Autorun (fim)"; tail -n 8 $D2/D2Autorun.log 2>/dev/null
echo "--- OSK"; grep -h '\[OSK\]' $WINE/logs/*.log 2>/dev/null
echo "--- saves"; find $WINE/drive_c/users -iname '*.d2s' -printf '%s %p\n'
