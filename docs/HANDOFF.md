# HANDOFF — retomar o projeto (troca de distro, 28/10/2026)

Ponto de partida: `origin/main` (https://github.com/aeon1911/MedianXL-NX). Leia antes `CLAUDE.md`, `AGENTS.md`,
`RULES.md` (regras permanentes) e `docs/D2PAD_CHANGES.md` (seções E a L = o que foi feito e medido).
`docs/STATUS.md`, `ARQUITETURA.md`, `CONTROLES.md`, `PROGRESSO.md` descrevem abordagens antigas: nao usar como verdade.

## Estado
- Funciona no console (Switch Lite, portatil): boot, jogo, andar/atacar nativos, habilidades, seletor de habilidades
  (+ e ZR/ZL), menus, criacao de personagem, teclado de nome (R3).
- Regressao no Ryujinx (27/10, binarios finais): aceitacao 17/17, seletor 10/10, menus 16/16, criacao 2/2.
- Pacote: `dist/d2-medianxl-autorun-switch-2.14.6-final.zip` (SHA256 865a82c0ce3e2a6b8f30080ceb6d32aeca0eab2f7348a341dbd0d2c368bf18e6).
  Anteriores: `-final-seletor.zip`, `-final-24out.zip`. DLLs e zips NAO vao para o git (so as fontes).

## PROBLEMA ABERTO: stutter no console
Relato do usuario apos ~1 h no Lite: continua travado. Quedas fortes de FPS (a) com as magias azuis dos monstros ao
redor das pedras do portal do Cain em Stony Field e (b) com muitos monstros na tela. A otimizacao de 27/10
(SAFEFLAGS=0, D2GL sem FXAA/sharpen, D2NXPad leve, Debug=0) deu so ~7% no Ryujinx e nao foi perceptivel.
- Causa NAO confirmada (regra: nada de chute). Hipoteses: GPU do Lite (fill-rate de efeitos transparentes) ou CPU
  (chamadas GL / upload de sprites novos do D2GL; traducao Box64 de codigo novo).
- **Falta o log do console.** Pedir ao usuario, logo apos jogar ~10 min incluindo Stony Field:
  `switch/wine/drive_c/DiabloII/d2nx/d2nxpad.log` (linhas `[PERF]`: fps, pior quadro com nivel e posicao, picos) e
  `switch/wine/logs/autorun_runtime.log` (`[PROGRESS]`, `[THREADS]`). Sao sobrescritos a cada partida.
- Candidatos, em ordem: (1) reduzir a resolucao interna do D2GL (ex.: 720p -> 540p esticado) / efeitos do D2GL, se for GPU;
  (2) mudar o envio de efeitos do D2GL, se for CPU; (3) recompilar o runtime com o cache de traducao ligado;
  (4) pre-aquecer na carga. Medir antes/depois no Ryujinx (`tools/medir_fluidez_ryujinx.sh`) e validar no console.
- Achados do runtime (fonte em `build/autorun-src/wine-nx-probe`): `box64env.dynacache = 0` fixo em
  `source/wow64_box64_dynarec.c` e `BOX64_DYNACACHE` nao esta na lista de `source/box64_options.h` -> o cache de
  traducao nao liga pelo `Game.box64.txt`. `BOX64_DYNAREC_ALIGNED_ATOMICS=1` derruba o jogo. Memoria de codigo
  traduzido ficou em 16/32 MB sem purge (PURGE nao ajuda). BIGBLOCK/FORWARD/WEAKBARRIER/frame_latency/vsync: sem efeito medido.
- Se o `Game.box64.txt` der problema: apagar o arquivo (volta ao padrao). D2GL: `d2gl.fxaa=true`/`d2gl.sharpen=true` no `d2nx.ini`.

## Reconstruir o ambiente numa distro nova
1. `git clone https://github.com/aeon1911/MedianXL-NX` (ou usar a pasta ja existente no disco STEAM).
2. Ferramentas: podman (ou docker), python3, flatpak, ImageMagick (`import`) e xprop (capturas), gcc/g++ para
   `tools/build_tools.sh` (StormLib). Em Bazzite/Fedora Atomic o podman ja vem.
3. Compilar as DLLs (imagem em `tools/docker/Dockerfile.pe-builder`):
   `podman build -t d2-autorun-pe-builder:latest -f tools/docker/Dockerfile.pe-builder tools/docker`
   `podman run --rm --security-opt label=disable -v "$PWD":/w -w /w/src-d2nx d2-autorun-pe-builder:latest sh build.sh`
   (sem `:Z`). Em 27/10 compilei com mingw i686 gcc 13 posix (extraido de .deb); a imagem usa o gcc do Debian bookworm.
   Depois de compilar, conferir que o `D2Autorun.dll` importa so ADVAPI32, KERNEL32, msvcrt, USER32 e rodar a aceitacao.
4. Ryujinx 1.3.3: `flatpak install --user io.github.ryubing.Ryujinx`; `~/.local/bin/ryujinx` -> binario dentro do
   flatpak (os scripts usam `which ryujinx`). Chaves/firmware do console do usuario (NAO versionados): `prod.keys`,
   `title.keys` em `build/ryujinx-test/system/` e `build/ryujinx-test/bis` = symlink para
   `~/.var/app/io.github.ryubing.Ryujinx/config/Ryujinx/bis` (recriar o symlink; o conteudo esta no backup).
5. Arquivos do Diablo II / Median XL: nao estao no git. `./setup.sh "<pasta do Diablo II>"` refaz `build/test_game`;
   `build/gameplay_orig/` guarda as tabelas originais para `tools/patch_gameplay.py`.
6. Montar e testar: `python3 tools/assemble_sd_tree.py`, `tools/run_acceptance_ryujinx.sh`,
   `tools/run_skill_ryujinx.sh`, `tools/run_menus_ryujinx.sh`, `tools/run_ryujinx_fresh_test.sh`,
   `tools/run_fase3_ryujinx_test.sh`. Empacotar: `python3 tools/package_release.py`.

## Armadilhas (custaram rodadas de teste)
- `pkill -f ryujinx-d2` mata o proprio shell: use `pgrep` + `kill <pid>`.
- `profile=1` (amostrador do runtime) derruba o Ryujinx: so serve no console.
- O PTC do Ryujinx muda o FPS (frio ~37-40, quente ~51): comparar so com o mesmo estado; usar `[PERF]` em regime.
- Aceitacao oscila com o PC carregado (load average ~8): repetir; comparar com o binario antigo antes de culpar a mudanca.
- Os testes leem `dbg()`: o INI do pacote vai com `Debug=0`; os scripts ligam `Debug=1` na arvore de teste.
- `d2nx.ini`, `d2nxpad.ini` e `Game.box64.txt` vem de `sd-card/` (copiados por `assemble_sd_tree.py`).
- No console o hbmenu abre por Title Override (segurar R ao abrir um jogo); em Applet Mode da Halt Fog #904.

## O que nao esta no git e onde esta o backup
Pasta `build/backup-troca-distro/` (disco STEAM, nao versionada):
- `repo-todas-as-branches.bundle`: todas as branches locais. O `main` local antigo tem historia SEM ancestral comum
  com `origin/main` (reescrita); o conteudo util ja esta no origin. Restaurar: `git clone <bundle>`.
- `main-nao-commitado.patch`, `main-status.txt`, `d2nxpad.c.main-local`, `CLAUDE.md.main-local`: edicoes nao
  commitadas do checkout antigo (superadas pelo origin; guardadas por seguranca).
- `ryujinx-config/`: `prod.keys`, `title.keys`, `Profiles.json`, `Config.json` (do console do usuario: nao publicar).
- `claude-memoria/`: memoria do Claude Code e a transcricao desta sessao. Restaurar em
  `~/.claude/projects/<caminho-da-pasta-com-hifens>/memory/` (o nome muda se o ponto de montagem mudar).
`build/` (~32 GB) e `dist/` (~11 GB) tambem ficam no disco STEAM: nao formate essa particao.
