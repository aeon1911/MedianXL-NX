#!/bin/bash
# Aceitacao dos menus pelo direcional no Ryujinx (roteiro tools/roteiros/menus.txt, so botoes do controle):
# cursor no 1o item ao abrir o inventario, direcional de casa em casa, menu do NPC (TALK -> TRADE), loja (1o item,
# caixa Sim/Nao: A abre com o cursor no Sim, baixo = Nao, B = Nao sem fechar a loja, A + A compra) e Organizar
# inventario (botao ao lado do inventario e opcao do menu de cheats), conferindo a grade do jogo.
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$BASE_DIR"
./tools/run_script_ryujinx.sh tools/roteiros/menus.txt 420 >/dev/null 2>&1
D=build/script-shots/menus
python3 - "$D" <<'PY'
import re, sys
d = sys.argv[1]
log = open(f"{d}/d2nxpad.log", errors="replace").read()
L = log.splitlines()
def mark_state(name):
    for i, l in enumerate(L):
        if f"MARCA {name}" in l:
            for m in L[i + 1:i + 3]:
                s = re.search(r"mouse jogo (-?\d+),(-?\d+) .* paineis (.*)", m)
                if s: return int(s[1]), int(s[2]), s[3].split()
    return None
def grids():
    out = []
    for i, l in enumerate(L):
        if "grade 2: 15x10" in l and "jogador" in "".join(L[max(0, i - 6):i]):
            out.append([re.sub(r"^.*\[Inv\]\s+", "", x) for x in L[i + 1:i + 11]])
    return out
def items(g):
    return len({c for row in g for c in row if c != "."})
def column_major_sorted(g):
    # arrumado: as colunas ocupadas vem antes das vazias e cada coluna e preenchida de cima para baixo
    cols = ["".join(row[x] for row in g) for x in range(15)]
    seen_empty = False
    for c in cols:
        if c.strip("."):
            if seen_empty: return False
            if "." in c.rstrip("."): return False
        else:
            seen_empty = True
    return True
res = []
def check(n, ok, info=""): res.append((n, bool(ok), info))
h = mark_state("inv_home"); check("inventario: cursor no 1o item ao abrir", h and "01" in h[2] and (h[0], h[1]) != (887, 723), h)
check("inventario: direcional anda de casa em casa", log.count("navegacao: +0,") >= 3)
n = mark_state("npc_home"); check("menu do NPC: cursor na 1a opcao", n and "08" in n[2] and n[0] < 1000, n)
l = mark_state("loja_home"); check("TRADE abre a loja (direcional + A)", l and "0C" in l[2], l)
check("loja: cursor no 1o item", l and l[0] < 512, l)
check("compra: A abre a caixa com o cursor no Sim", "loja: caixa Sim/Nao, cursor no Sim" in log)
c1 = mark_state("caixa1"); check("caixa: cursor no Sim (256,214, +-3 px)", c1 and abs(c1[0] - 256) <= 3 and abs(c1[1] - 214) <= 3, c1)
check("caixa: baixo + A = Nao", "loja: Nao" in log)
check("caixa: B = Nao", "loja: Nao (B)" in log)
a = mark_state("loja_ainda_aberta"); check("B na caixa nao fecha a loja", a and "0C" in a[2], a)
gs = grids()
check("A + A compra (item novo no inventario)", len(gs) >= 2 and items(gs[1]) > items(gs[0]) and "loja: Sim" in log,
      f"itens {items(gs[0]) if gs else '?'} -> {items(gs[1]) if len(gs) > 1 else '?'}")
check("organizar (botao): pronto", log.count("organizar: pronto") >= 1)
check("organizar (botao): grade arrumada e mesmos itens", len(gs) >= 4 and column_major_sorted(gs[3]) and items(gs[3]) == items(gs[2]),
      "\n".join(gs[3]) if len(gs) >= 4 else "")
check("organizar (menu de cheats): pronto", log.count("organizar: pronto") >= 2)
check("organizar (menu de cheats): grade arrumada", len(gs) >= 5 and column_major_sorted(gs[4]), "\n".join(gs[4]) if len(gs) >= 5 else "")
check("sem queda do emulador", open("build/ryujinx-test/ryu.log", errors="replace").read().count("Unhandled exception") == 0)
bad = 0
for name, ok, info in res:
    print(f"[{'OK ' if ok else 'FALHA'}] {name}" + (f"  ({info})" if info and not ok else ""))
    bad += not ok
print(f"--- {len(res) - bad}/{len(res)} criterios ok; capturas em {d}")
sys.exit(1 if bad else 0)
PY
