#!/bin/bash
# Teste do seletor de habilidades (painel 0x03 do jogo) no Ryujinx. SkillTest=1 aperta os botoes como o jogador:
# + e ZR abre o seletor da direita, o direcional move, A escolhe; depois + e ZL para o da esquerda; por fim + e ZR,
# analogico esquerdo (move o cursor, nao o heroi) e B fecha sem escolher. Criterios pelo log (habilidade de cada
# mao lida da unidade do jogador, cursor visivel so com o seletor aberto, heroi parado). Capturas so da janela do
# Ryujinx em build/skill-shots/ (import; sem ImageMagick usa xwd e grava .xwd).
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$BASE_DIR"
ROOT=build/ryujinx-test
WINE="$ROOT/sdcard/switch/wine"
D2="$WINE/drive_c/DiabloII"
INI="$D2/d2nx/d2nxpad.ini"
SHOTS=build/skill-shots
if pgrep -f "^/tmp/ryujinx-d2" >/dev/null; then echo "ERRO: outro Ryujinx de teste ja esta rodando"; exit 1; fi
python3 tools/assemble_sd_tree.py >/dev/null 2>&1 || { echo "ERRO: montagem do SD falhou"; exit 1; }
rm -rf "$SHOTS"; mkdir -p "$SHOTS"

shot() {   # $1 = janela, $2 = arquivo sem extensao
    if command -v import >/dev/null; then timeout 15 import -silent -window $1 "$2.png" 2>/dev/null && echo "$2.png"
    elif command -v xwd >/dev/null; then timeout 15 xwd -silent -id $1 -out "$2.xwd" 2>/dev/null && echo "$2.xwd"
    fi
}

run() {   # $1 = nome, $2 = linhas extras do [AutoTest], $3 = segundos no jogo
    sed -i '/^AutoCreate=/d; /^MenuTest=/d; /^CursorTest=/d; /^SkillTest=/d; s/^AutoJoin=.*/AutoJoin=1/; s/^AutoWalk=.*/AutoWalk=0/' "$INI"
    sed -i "s/^AutoJoin=1/AutoJoin=1\n$2/" "$INI"
    echo "sdmc:/switch/wine/drive_c/DiabloII/Game.exe" > "$WINE/run-next.txt"
    rm -f "$ROOT/ryu.log" "$D2/D2Autorun.log" "$D2/d2nx/d2nxpad.log" "$WINE"/logs/*
    ln -sf "$(which ryujinx)" /tmp/ryujinx-d2
    nice -n 10 /tmp/ryujinx-d2 --root-data-dir "$BASE_DIR/$ROOT" --handheld-mode --hide-updates \
        "$BASE_DIR/$ROOT/hbl/ncas/nca-1.nca" > "$ROOT/ryu.log" 2>&1 &
    local ryu=$! win="" n=0 t_in=0 end=$((SECONDS + 420)) f
    while [ $SECONDS -lt $end ] && kill -0 $ryu 2>/dev/null; do
        sleep 1
        if [ -z "$win" ] && command -v xprop >/dev/null; then
            for id in $(xprop -root _NET_CLIENT_LIST 2>/dev/null | sed 's/.*# //; s/,//g'); do
                xprop -id $id WM_CLASS 2>/dev/null | grep -qi ryujinx && win=$id
            done
        fi
        if grep -q 'SUCESSO: Personagem entrou' "$D2/d2nx/d2nxpad.log" 2>/dev/null; then
            [ $t_in = 0 ] && t_in=$SECONDS
            if [ -n "$win" ]; then
                n=$((n + 1)); f=$(shot $win "$SHOTS/$1-$(printf %03d $n)")
                [ -n "$f" ] && echo "$(date -u +%T) $(basename "$f")" >> "$SHOTS/times.txt"
            fi
            [ $((SECONDS - t_in)) -ge $3 ] && break
        fi
    done
    kill -15 $ryu 2>/dev/null; sleep 2; kill -9 $ryu 2>/dev/null; wait $ryu 2>/dev/null
    cp "$D2/d2nx/d2nxpad.log" "$SHOTS/$1.log"
    grep -c 'Unhandled exception' "$ROOT/ryu.log" > "$SHOTS/$1.crash"
}

run skill "SkillTest=1" 45
sed -i '/^SkillTest=/d; s/^AutoJoin=.*/AutoJoin=0/; s/^AutoWalk=.*/AutoWalk=0/' "$INI"
python3 tools/assemble_sd_tree.py >/dev/null 2>&1
python3 - "$SHOTS" <<'PY'
import re, sys
d = sys.argv[1]
log = open(f"{d}/skill.log", errors="replace").read()
rows = [dict(t=float(m[0]), sel=int(m[1]), mode=int(m[2]), mx=int(m[3]), my=int(m[4]), cur=m[5], px=float(m[6]), py=float(m[7]),
             L=int(m[8]), R=int(m[9]))
        for m in re.findall(r"\[SkillTest\] t=([\d.]+)s seletor (\d) modo (\d) mouse (-?\d+),(-?\d+) cursor (\w+) "
                            r"pos ([\d.]+),([\d.]+) res \S+ esquerda (-?\d+) direita (-?\d+)", log)]
def at(t0, t1): return [r for r in rows if t0 <= r["t"] < t1]
res = []
def check(name, ok, info=""): res.append((name, bool(ok), info))
pre = at(5, 10)
check("antes: partida sem painel, cursor oculto", pre and all(r["sel"] == 0 and r["mode"] == 3 and r["cur"] == "oculto" for r in pre))
r1 = at(11, 14)
check("+ZR abre o seletor (painel 0x03) e libera o cursor (modo painel, cursor visivel)",
      r1 and all(r["sel"] == 1 and r["mode"] == 2 and r["cur"] == "visivel" for r in r1))
check("cursor vai ao primeiro icone da direita (950,590)", r1 and abs(r1[-1]["mx"] - 950) <= 2 and abs(r1[-1]["my"] - 590) <= 2,
      f"mouse {r1[-1]['mx']},{r1[-1]['my']}" if r1 else "")
r2 = at(14.5, 16)
check("direcional salta para o icone vizinho", r2 and abs(r2[-1]["mx"] - 900) <= 2 and abs(r2[-1]["my"] - 590) <= 2,
      f"mouse {r2[-1]['mx']},{r2[-1]['my']}" if r2 else "")
b, a = at(9, 10), at(17, 20)
check("A troca a habilidade da mao direita, fecha o seletor e esconde o cursor",
      b and a and a[-1]["R"] != b[-1]["R"] and a[-1]["sel"] == 0 and a[-1]["mode"] == 3 and a[-1]["cur"] == "oculto",
      f"direita {b[-1]['R'] if b else '?'} -> {a[-1]['R'] if a else '?'}")
r3 = at(21, 24)
check("+ZL abre o seletor com o cursor no primeiro icone da esquerda (75,590)",
      r3 and r3[-1]["sel"] == 1 and r3[-1]["cur"] == "visivel" and abs(r3[-1]["mx"] - 75) <= 2 and abs(r3[-1]["my"] - 590) <= 2,
      f"mouse {r3[-1]['mx']},{r3[-1]['my']}" if r3 else "")
b, a = at(19, 20), at(27, 30)
check("A troca a habilidade da mao esquerda", b and a and a[-1]["L"] != b[-1]["L"] and a[-1]["sel"] == 0,
      f"esquerda {b[-1]['L'] if b else '?'} -> {a[-1]['L'] if a else '?'}")
r4, r5 = at(31.5, 32), at(33.2, 34)
check("analogico com o seletor aberto move o cursor e nao o heroi",
      r4 and r5 and r5[-1]["sel"] == 1 and r5[-1]["my"] < r4[-1]["my"] - 20 and abs(r5[-1]["px"] - r4[-1]["px"]) < 0.5
      and abs(r5[-1]["py"] - r4[-1]["py"]) < 0.5, f"mouse y {r4[-1]['my'] if r4 else '?'} -> {r5[-1]['my'] if r5 else '?'}")
r6 = at(35.5, 40)
check("B fecha o seletor sem escolher e sem abrir o menu do jogo", r5 and r6 and all(r["sel"] == 0 and r["mode"] == 3 for r in r6)
      and r6[-1]["R"] == r5[-1]["R"] and r6[-1]["L"] == r5[-1]["L"] and "0x09" not in log)
check("sem queda do emulador", open(f"{d}/skill.crash").read().strip() == "0")
bad = 0
for name, ok, info in res:
    print(f"[{'OK ' if ok else 'FALHA'}] {name}" + (f"  ({info})" if info else ""))
    bad += not ok
print(f"--- {len(res) - bad}/{len(res)} criterios ok; capturas (so da janela do Ryujinx) em {d}")
sys.exit(1 if bad else 0)
PY
