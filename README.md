# Median XL Σ 2.14.6 — Nintendo Switch (AUTORUN)

Roda o **Median XL Σ 2.14.6** (mod do Diablo II: Lord of Destruction 1.13c) no **Nintendo Switch com Atmosphère**,
direto no Horizon OS, pelo runtime **AUTORUN** (Wine + Box64 para Switch). Sem Linux/L4T e sem D2SE.
O controle é o **D2NXPad**: movimento analógico nativo (pela rotina de comando do próprio jogo, sem cursor
simulado), ataque no monstro detectado, editor de nome na criação de personagem e menu de cheats.

## ⚠️ Arquivos do jogo NÃO estão neste repositório

Este repositório tem só o código deste projeto, o runtime AUTORUN (Wine, LGPL) e configurações.
**Nenhum arquivo da Blizzard ou do Median XL é distribuído aqui**, nem no histórico do git. Você precisa de:

| O quê | De onde vem | Quem busca |
|---|---|---|
| MPQs base do Diablo II + LoD (`d2data`, `d2exp`, `d2char`, `d2sfx`, `d2speech`, `d2music`, `d2video`, `d2xmusic`, `d2xtalk`, `d2xvideo`) | **Sua** instalação do Diablo II: Lord of Destruction (CD ou instalador da Blizzard, qualquer versão) | Você aponta a pasta; o `setup.sh` copia |
| Median XL 2.14.6 (`medianxl-*.mpq`, `D2Sigma.dll`, `Fog.dll`, DLLs/`Game.exe` 1.13c, D2GL, `patch_d2.mpq`) | Canal **oficial** do launcher do Median XL (`launcher.median-xl.com` / `get.median-xl.com`), com MD5 conferido | `tools/fetch_medianxl.py` baixa sozinho |

As alterações que o port precisa nesses arquivos são **feitas na sua máquina** pelos scripts (código aberto,
cada byte conferido antes de gravar), nunca distribuídas prontas.

## 🚀 Montar o cartão SD (um comando)

Num PC Linux (ou WSL / macOS com Docker):

```bash
git clone https://github.com/aeon1911/MedianXL-NX.git
cd MedianXL-NX
./setup.sh "/caminho/do/seu/Diablo II"
```

Saída: `dist/sd-card/switch/` → copie a pasta `switch/` para a **raiz** do cartão SD (FAT32 ou exFAT,
~3 GB livres). Com `--zip` no fim ele também gera `dist/d2-medianxl-autorun-switch-2.14.6-final.zip`.

Precisa de `python3` e de ferramentas de compilação (`i686-w64-mingw32-gcc`, `g++`, `cmake`, `make`, `git`,
zlib e bzip2). **Se faltar alguma, o `setup.sh` usa o Docker** com `tools/Dockerfile` (nada é instalado no
sistema). Internet é necessária para baixar o Median XL (~600 MB) e a StormLib.

### O que o `setup.sh` faz (passo a passo, `tools/build_all.sh`)

1. `src-d2nx/build.sh` — compila `D2Autorun.dll` e `D2NXPad.dll` (código deste repositório).
2. `tools/fetch_medianxl.py` — baixa o Median XL 2.14.6 do manifesto oficial para `build/mxl214/` e confere o
   MD5 de cada arquivo (rodar de novo retoma o download).
3. `tools/build_tools.sh` — baixa a [StormLib](https://github.com/ladislav-zezula/StormLib) (versão fixa) e
   compila `build/bin/merge_mpqs` e `build/bin/mpq_extract`.
4. `tools/prepare_game.py --d2 <pasta>` — monta `build/test_game/`:
   - copia os 10 MPQs base da sua instalação (aceita `D2XMUSIC.MPQ` etc. em maiúsculas);
   - copia o pacote do Median XL e aplica: seção `.d2auto` no `Game.exe` (carrega o `D2Autorun.dll`),
     correção do bit de relocação do `Fog.dll`, teste de ponteiro nulo no `D2Sigma.dll`;
   - junta os 56 `medianxl-*.mpq` em um `medianxl-data.mpq` (o boot no cartão do Switch abre 1 arquivo em vez
     de 56) **copiando os blocos comprimidos sem recomprimir** — os sons do Median XL usam a compressão da
     Blizzard que a `Storm.dll` 1.13c entende;
   - extrai do Median XL as tabelas originais usadas pelo menu de gameplay.
5. `tools/assemble_sd_tree.py` — monta `dist/sd-card/` (runtime + jogo + configurações + tabelas de gameplay:
   XP 2x, runas 2x, únicos/raros 1,5x, ajustáveis em `[Gameplay]` do `d2nxpad.ini`).

Cada passo também pode ser rodado sozinho, na mesma ordem.

## 🎮 No console

1. Abra o hbmenu por **Title Override** (segure `R` ao abrir um jogo instalado). Pelo álbum (Applet Mode) a
   memória não basta e o jogo para com "Halt Fog line #904".
2. Inicie o **d2-autorun**; o Median XL abre direto.
3. Saves: `sdmc:/switch/wine/drive_c/users/steamuser/AppData/Roaming/MedianXL/save/`.

Controles (detalhes no [INSTALL.md](INSTALL.md), regras de projeto no [RULES.md](RULES.md)):
analógico esquerdo anda/corre, **A** ataca o monstro detectado, **X** habilidade direita, **B** interage,
**Y** inventário, **+** menu do jogo, **ZL + −** menu de cheats. Na criação de personagem, **R3** abre o editor
de nome.

## 📁 Repositório

```text
├── setup.sh                  # tudo em um comando
├── src-d2nx/                 # D2Autorun.dll (compatibilidade Wine-NX) e D2NXPad.dll (controle), em C
├── tools/
│   ├── build_all.sh          # passos do setup em ordem
│   ├── fetch_medianxl.py     # download oficial do Median XL com MD5
│   ├── build_tools.sh        # StormLib + merge_mpqs + mpq_extract
│   ├── merge_mpqs.cpp        # MPQ único por cópia de blocos (sem recompressão)
│   ├── mpq_extract.cpp
│   ├── prepare_game.py       # monta build/test_game a partir dos seus arquivos
│   ├── inject_pe_import.py   # seção .d2auto no Game.exe
│   ├── fix_fog_characteristics.py
│   ├── patch_gameplay.py     # tabelas de XP/runas/drops
│   ├── assemble_sd_tree.py   # dist/sd-card
│   ├── package_release.py    # zip final
│   ├── Dockerfile            # ambiente de build
│   └── run_*_ryujinx.sh      # testes no emulador Ryujinx
├── sd-card/switch/           # runtime AUTORUN (Wine-NX) e configurações; sem arquivos do jogo
└── docs/                     # notas técnicas (D2PAD_CHANGES.md tem o histórico das correções)
```

`build/` e `dist/` (gerados) estão no `.gitignore`, assim como todos os nomes de arquivos do jogo.

## 🧪 Testes (Ryujinx)

Os scripts `tools/run_*_ryujinx.sh` rodam o SD de teste em `build/ryujinx-test` no Ryujinx e avaliam pelos logs:
`run_acceptance_ryujinx.sh` (movimento nativo, A/X/B, menu de cheats — 17 critérios),
`run_fase3_ryujinx_test.sh` (carrega um personagem e anda), `run_ryujinx_fresh_test.sh` (cria personagem do zero,
salva e recarrega) e `run_item_test_ryujinx.sh` (usa o item de 1.500 de ouro do kit inicial e confere a leitura
dos sons do MPQ).

## ⚖️ Aviso legal

- **Diablo II** e **Diablo II: Lord of Destruction** são marcas da **Blizzard Entertainment, Inc.** Este projeto
  de fãs, sem fins lucrativos, não distribui arquivos, músicas, vídeos ou código da Blizzard. Use sua cópia.
- **Median XL** é desenvolvido pela equipe Median XL ([median-xl.com](https://www.median-xl.com/)); os arquivos
  são baixados do canal oficial deles, como faz o launcher oficial.
- **AUTORUN** ([autorunhq/autorun](https://github.com/autorunhq/autorun)) é baseado no Wine (LGPL 2.1+) e no
  Box64 (MIT); licenças e fontes das alterações em `sd-card/switch/wine/licenses/`.
- **D2GL** (vídeo) e **StormLib** pertencem aos seus autores e são obtidos dos canais oficiais.
