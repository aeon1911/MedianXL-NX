#!/bin/bash
# Fluidez no Ryujinx: entra no jogo (AutoJoin) e anda (AutoWalk) por JANELA segundos; depois resume, pelo
# [PROGRESS]/[THREADS] do runtime do Autorun, os quadros por segundo do D2GL (gl_frames), o tempo dentro do GL, a
# parcela de CPU de cada thread e (com PROFILE=1) os modulos x86 mais quentes ([PROF]).
# O FPS do emulador NAO e o do console (CPU/GPU do PC), mas o numero de instrucoes e chamadas GL por quadro e a
# carga por thread valem para comparar uma variante com a outra no mesmo cenario.
#   uso: tools/medir_fluidez_ryujinx.sh <rotulo> [janela_s=75]
#   variaveis: PROFILE=1 liga o amostrador; BOX64_TXT=<arquivo> instala como Game.box64.txt;
#              WINE_NX_TXT=<arquivo> acrescenta linhas ao Game.wine-nx.txt; D2NX_INI_EXTRA=<arquivo> acrescenta ao [Video] do d2nx.ini;
#              NO_BOX64=1 mede sem o Game.box64.txt do projeto;
#              KEEP_SD=1 nao remonta a arvore do SD (usa o que ja esta em build/ryujinx-test)
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$BASE_DIR"
LABEL="${1:-teste}"; WIN="${2:-75}"
ROOT=build/ryujinx-test
WINE="$ROOT/sdcard/switch/wine"
D2="$WINE/drive_c/DiabloII"
OUT=build/fluidez/$LABEL
if pgrep -f "^/tmp/ryujinx-d2" >/dev/null; then echo "ERRO: outro Ryujinx de teste ja esta rodando"; exit 1; fi
mkdir -p "$OUT"
[ -n "$KEEP_SD" ] || python3 tools/assemble_sd_tree.py >/dev/null 2>&1 || { echo "ERRO: montagem do SD falhou"; exit 1; }
INI="$D2/d2nx/d2nxpad.ini"
sed -i '/^AutoCreate=/d; /^MenuTest=/d; /^SkillTest=/d; s/^AutoJoin=.*/AutoJoin=1/; s/^AutoWalk=.*/AutoWalk='$((WIN + 20))'/' "$INI"
[ -n "$NO_BOX64" ] && rm -f "$D2/Game.box64.txt"   # NO_BOX64=1: medir o padrao do Box64, sem o perfil do projeto
[ -n "$BOX64_TXT" ] && cp "$BOX64_TXT" "$D2/Game.box64.txt"
[ -n "$WINE_NX_TXT" ] && cat "$WINE_NX_TXT" >> "$D2/Game.wine-nx.txt"
if [ -n "$D2NX_INI_EXTRA" ]; then
    sed -i '/^d2gl\./d' "$D2/d2nx/d2nx.ini" 2>/dev/null
    python3 - "$D2/d2nx/d2nx.ini" "$D2NX_INI_EXTRA" <<'PY'
import sys
p, extra = sys.argv[1], open(sys.argv[2]).read()
t = open(p).read()
t = t.replace("[Video]\n", "[Video]\n" + extra.rstrip("\n") + "\n", 1)
open(p, "w").write(t)
PY
fi
# o amostrador liga pela configuracao do programa (profile=1 no Game.wine-nx.txt); profile.txt e o ajuste global
if [ -n "$PROFILE" ]; then echo 1 > "$WINE/profile.txt"; sed -i 's/^profile=0/profile=1/' "$D2/Game.wine-nx.txt"; else rm -f "$WINE/profile.txt"; fi
echo "sdmc:/switch/wine/drive_c/DiabloII/Game.exe" > "$WINE/run-next.txt"
rm -f "$ROOT/ryu.log" "$D2/D2Autorun.log" "$D2/d2nx/d2nxpad.log" "$WINE"/logs/*
ln -sf "$(which ryujinx)" /tmp/ryujinx-d2
nice -n 10 /tmp/ryujinx-d2 --root-data-dir "$BASE_DIR/$ROOT" --handheld-mode --hide-updates \
    "$BASE_DIR/$ROOT/hbl/ncas/nca-1.nca" > "$ROOT/ryu.log" 2>&1 &
RYU=$!
t_in=0; end=$((SECONDS + 420))
while [ $SECONDS -lt $end ] && kill -0 $RYU 2>/dev/null; do
    sleep 1
    if grep -q 'SUCESSO: Personagem entrou' "$D2/d2nx/d2nxpad.log" 2>/dev/null; then
        [ $t_in = 0 ] && t_in=$SECONDS
        [ $((SECONDS - t_in)) -ge $WIN ] && break
    fi
done
kill -15 $RYU 2>/dev/null; sleep 2; kill -9 $RYU 2>/dev/null; wait $RYU 2>/dev/null
cp "$WINE/logs/autorun_runtime.log" "$OUT/runtime.log" 2>/dev/null
cp "$D2/d2nx/d2nxpad.log" "$OUT/d2nxpad.log" 2>/dev/null
cp "$D2/D2Autorun.log" "$OUT/d2autorun.log" 2>/dev/null
sed -i 's/^AutoJoin=.*/AutoJoin=0/; s/^AutoWalk=.*/AutoWalk=0/' "$INI"
rm -f "$WINE/profile.txt"
python3 tools/resumir_fluidez.py "$OUT" "$LABEL" "$t_in"
