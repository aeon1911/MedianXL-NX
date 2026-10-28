#!/bin/bash
# Testes de aceitacao do RULES.md (NATIVE ANALOG MOVEMENT) no Ryujinx, com criterios avaliados pelo log.
#  1) AutoWalk=2: o D2NXPad injeta analogico/botoes como o controle (zona morta, inclinacao leve, soltar,
#     diagonal, ir ate o NPC e apertar B, sair da cidade, segurar A no campo por 25 s (os monstros nascem em
#     lugares diferentes a cada partida e precisam de tempo para chegar), segurar X).
#  2) MenuTest=1: abre o menu de cheats com ZL+-, navega, aplica com A e fecha com B.
# Usa o personagem de teste do SD (AutoJoin). Capturas da janela em build/acceptance-shots/ (xprop+import).
# Limite: o controle fisico do Switch nao e testado aqui (a entrada e injetada no mesmo ponto do XInput).
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$BASE_DIR"
ROOT=build/ryujinx-test
WINE="$ROOT/sdcard/switch/wine"
D2="$WINE/drive_c/DiabloII"
INI="$D2/d2nx/d2nxpad.ini"
SHOTS=build/acceptance-shots
if pgrep -f "^/tmp/ryujinx-d2" >/dev/null; then echo "ERRO: outro Ryujinx de teste ja esta rodando"; exit 1; fi
python3 tools/assemble_sd_tree.py >/dev/null 2>&1 || { echo "ERRO: montagem do SD falhou"; exit 1; }
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
run menu "MenuTest=1" 26
sed -i '/^MenuTest=/d; s/^AutoJoin=.*/AutoJoin=0/; s/^AutoWalk=.*/AutoWalk=0/' "$INI"
python3 tools/assemble_sd_tree.py >/dev/null 2>&1   # devolve ini e tabelas do SD de teste (o menu aplicou)

python3 - "$SHOTS" <<'EOF'
import re, sys
d = sys.argv[1]
log = open(f"{d}/cenario.log", errors="replace").read()
menu = open(f"{d}/menu.log", errors="replace").read()
rows = [dict(t=float(m[0]), ph=int(m[1]), A=int(m[2]), B=int(m[3]), X=int(m[4]), x=float(m[5]), y=float(m[6]),
             mode=int(m[7]), lvl=int(m[8]), mx=int(m[9]), my=int(m[10]))
        for m in re.findall(r"\[Aceite\] t=([\d.]+)s fase (\d+) analogico \S+ A=(\d) B=(\d) X=(\d) pos ([\d.]+),([\d.]+) "
                            r"modo (\d+) nivel (-?\d+) painel \d mouse (-?\d+),(-?\d+)", log)]
def ph(p): return [r for r in rows if r["ph"] == p]
def span(rs):
    return max((abs(a["x"] - b["x"]) + abs(a["y"] - b["y"])) for a in rs for b in rs) if rs else -1
res = []
def check(name, ok, info=""): res.append((name, ok, info))
# o heroi ainda termina a caminhada de chegada na cidade nos primeiros segundos (mais lenta com o PC carregado):
# so conta depois de 1,5 s da fase, como o "soltar o analogico para" abaixo
p1 = [r for r in ph(1) if r["t"] - ph(1)[0]["t"] >= 1.5] if ph(1) else []
check("zona morta: analogico 0,12 nao move", 0 <= span(p1) < 1.0, f"deslocamento {span(p1):.1f}")
check("inclinacao leve anda", span(ph(2)) > 8, f"deslocamento {span(ph(2)):.1f}")
p3 = [r for r in ph(3) if r["t"] - ph(3)[0]["t"] >= 1.0] if ph(3) else []
check("soltar o analogico para", 0 <= span(p3) < 1.0, f"deslocamento depois de 1 s solto {span(p3):.1f}")
check("inclinacao total corre (modo 3)", any(r["mode"] == 3 for r in ph(4) + ph(5) + ph(6)))
check("colisao/caminho: sai da cidade pela saida (nivel 2)", any(r["lvl"] == 2 for r in rows))
mv = [r for r in rows if 1 <= r["ph"] <= 7 and r["ph"] not in (41, 42)]
check("sem mouse: mouse do jogo parado enquanto anda", len({(r["mx"], r["my"]) for r in mv}) == 1,
      f"posicoes do mouse {sorted({(r['mx'], r['my']) for r in mv})[:3]}")
check("B interage com NPC (abre o menu do NPC)", "paineis abertos: 0x08" in log)
check("B no painel fecha (volta ao jogo)", re.search(r"modo 2 -> 3 \(paineis abertos: nenhum\)", log) is not None)
check("A ataca (modos 7/8) sem andar", any(r["mode"] in (7, 8) for r in ph(8)) and span(ph(8)) < 1.0,
      f"deslocamento {span(ph(8)):.1f}")
check("A troca de alvo (deteccao)", "ataque: novo alvo" in log)
check("X usa a habilidade direita sem andar", "habilidade direita" in log and span(ph(81)) < 1.0)
check("menu de cheats abre pelo ZL+-", "Menu aberto via controle" in menu)
check("menu de cheats aplica (A grava tabelas)", "Salvo e aplicado" in menu)
check("menu de cheats fecha (B)", "Menu fechado (B)" in menu)
check("desenho nativo resolvido", re.search(r"DrawText [0-9a-f]{8}, SetFont [0-9a-f]{8}", menu) is not None and "00000000" not in re.search(r"desenho nativo:.*", menu).group(0))
for f in ("cenario", "menu"):
    check(f"sem queda do emulador ({f})", open(f"{d}/{f}.crash").read().strip() == "0")
bad = 0
for name, ok, info in res:
    print(f"[{'OK ' if ok else 'FALHA'}] {name}" + (f"  ({info})" if info else ""))
    bad += not ok
print(f"--- {len(res) - bad}/{len(res)} criterios ok; capturas em {d} (menu-*.png mostram o menu na tela)")
sys.exit(1 if bad else 0)
EOF
