# HANDOFF — retomar o projeto (troca de distro 28/10/2026; atualizado em 31/10)

Ponto de partida: `origin/main` (https://github.com/aeon1911/MedianXL-NX). Leia antes `CLAUDE.md`, `AGENTS.md`,
`RULES.md` (regras permanentes) e `docs/D2PAD_CHANGES.md` (seções E a N = o que foi feito e medido).
`docs/STATUS.md`, `ARQUITETURA.md`, `CONTROLES.md`, `PROGRESSO.md` descrevem abordagens antigas: nao usar como verdade.

## COMECE AQUI (31/10/2026) — leia esta seção antes de qualquer outra
**Missão:** port do Median XL Σ 2.14.6 (Diablo II LoD 1.13c) para o Switch Lite (Atmosphère), rodando no AUTORUN / Wine-NX, com
controle pelo D2NXPad. Falar com o usuário em português. Commit e push só quando ele pedir.

**Onde está o código**
- Trabalho atual: branch `perf/stutter-console-28out`, 3 commits à frente do `origin/main` (`cd6f061` luz baixa + telemetria, `0a85427`
  andar+habilidade/barra de baixo/vendedores/tabelas no MPQ, `df5037e` ferramentas de análise dos logs). **Não foi enviada ao GitHub.**
  Vive no worktree `.claude/worktrees/perf-console` (com `build` e `dist` ligados por symlink à pasta principal). Se o worktree sumir:
  `git worktree add .claude/worktrees/perf-console perf/stutter-console-28out` e refazer os dois `ln -s`.
- A pasta principal está no `main` local antigo (história reescrita, 16 à frente / 22 atrás do `origin/main`) e com edições não commitadas
  velhas (DLLs, `d2nxpad.c`, `CLAUDE.md`). **Não trabalhe nela nem use o `sd-card/` dela**; o conteúdo útil está no branch acima.

**O pacote para testar no console** (é o mais atual; nada mudou no jogo depois dele, só ferramentas e docs)
- `dist/d2-medianxl-autorun-switch-2.14.6-final.zip`, 2,27 GB, 278 arquivos, 29/10 21:07. SHA256
  `48a41e59e5c7e54e35adcb8c3e9f18b26a6f1b188291b6c61a6eabc88b3f12dd` (também em `…zip.sha256`). D2NXPad 1.2.0 + D2Autorun 0.4.5.
- Tem de levar `switch/d2-autorun/` (o `.nro`) e o `switch/wine/` completo; o `medianxl-data.mpq` (548 MB) mudou, então o usuário recopia tudo.
  Esse pacote já está no cartão do usuário (o log de 30/10 mostra 0.4.5 / 1.2.0).
- Para gerar outro: `python3 tools/assemble_sd_tree.py` (monta `dist/sd-card/` e a árvore de teste do Ryujinx) e `python3 tools/package_release.py`
  (o nome do zip é fixo e sobrescreve; renomeie o anterior se quiser guardá-lo). Em 31/10 só existe o `-final.zip` em `dist/`
  (os zips antigos e `dist/sd-card/` foram removidos de lá; `assemble_sd_tree.py` recria a pasta).

**O que já está confirmado no console (log de 30/10, `docs/D2PAD_CHANGES.md` O)**
- Boot até o jogo, criação de personagem com o editor de nome (R3), lojas, `preferences.json` criado no 1º boot com luz baixa.
- Luz baixa: 55-60 FPS fora de carga em cidade + Blood Moor, thread principal em 20-48% de um núcleo, GL preso no vsync (~16,4 ms/quadro).

**O que NÃO foi visto no console ainda (pedir ao usuário)**
1. Combate pesado (muitos monstros, magias azuis) com luz baixa: era a queixa original e o log de 30/10 não tem essa cena.
2. Andar + soltar habilidade (Bárbaro "deslizando"): corrigido e medido só no Ryujinx.
3. Barra de baixo pelo direcional (com painel aberto, ZL = HUD): validada só no Ryujinx.
4. Full Rejuvenation nos vendedores: só o Akara foi verificado (Ryujinx).

**Como coletar os logs do console** (o usuário liga o DBI por MTP; os logs são sobrescritos a cada partida)
`python3 tools/coletar_logs_console.py "$(ls -d /run/user/1000/gvfs/mtp*)/1: Cartão SD"` copia para `build/logs-console/<data>/` e imprime o
resumo de FPS (`analisar_perf_log.py`) e o cruzamento runtime × pad (`correlacionar_runtime_console.py`: `sd_ms` do SD, GL por quadro, núcleos).

**Problemas abertos, em ordem de valor**
1. Travadinhas de 50-130 ms por ~15 s ao entrar em área nova e congelamentos de 1,1-1,5 s na troca de área: acompanham a leitura do SD
   (r = 0,78 entre `sd_ms` e quadros > 33 ms; ~120 KB por leitura a 31-55 MB/s). **Sem alavanca no projeto:** o cache de SD (128 KB × 8 por arquivo)
   é fixo no `wine-nx-runtime.nro` do AUTORUN e não há toolchain devkitPro/libnx aqui (só a imagem de DLLs de Windows). Box64 já esgotado
   (`docs/DESEMPENHO.md`). Hipótese de hardware não medida: cartão SD mais rápido.
2. Se o combate pesado ainda ficar abaixo de ~45 FPS: experimento `d2gl.frame_latency=2` (linha comentada no `sd-card/.../d2nx/d2nx.ini`;
   só mensurável no console, custa 1 quadro de atraso) ou sessão com `profile=1` no `Game.wine-nx.txt` (derruba o Ryujinx; só no console).
3. Não investigados (têm números, falta a causa): ~600 mil syscalls (nº 0x34 e 0x14ca) em 10 s na tela de personagem, ~19 s da confirmação
   do nome até a 1ª janela de FPS na cidade, e as travadas de 0,6-5,6 s com o inventário aberto em Stony Field (28/10).
4. Menores: o menu de cheats (ZL+−) não aplica em tempo real; ouro inicial indisponível; o `[RT]` do `d2nxpad.log` não lê o runtime no console
   (erro 2): o `autorun_runtime.log` vem pelo MTP.

**Regras que não se negociam** (texto completo em `AGENTS.md` e `RULES.md`)
- Nada vai ao console sem validação no Ryujinx; proibido chutar causa (evidência de log/medição). "Funcionou" exige o herói mudando de posição.
- Movimento e ações pela camada nativa (rotina de comando do D2Client); proibido mouse/cursor para andar ou atacar.
- Boot rápido; nenhum arquivo da Blizzard no git (DLLs e zips também ficam de fora; só as fontes).

**Documentos: o que é verdade**
- Verdade: este arquivo, `CLAUDE.md`, `AGENTS.md`, `RULES.md`, `docs/D2PAD_CHANGES.md` (seções E a O) e `docs/DESEMPENHO.md`.
- Abordagens antigas, não usar: `docs/STATUS.md`, `ARQUITETURA.md`, `CONTROLES.md`, `PROGRESSO.md` (este último superestima).
- Memória do Claude Code (fora do repositório): `~/.claude/projects/-mnt-STEAM-Projetos-DIABLO-2-LOD-SWITCH-PORT/memory/`. O essencial
  dela já está neste arquivo e em `CLAUDE.md`; outra IA não precisa dela.

## Estado
- Funciona no console (Switch Lite, portátil): boot, jogo, andar/atacar nativos, habilidades, seletor de habilidades
  (+ e ZR/ZL), menus, criação de personagem, teclado de nome (R3).
- Regressão no Ryujinx (29/10, build 1.2.0): aceitação 17/17, criação de personagem em console novo 2/2, barra de baixo (roteiro `barra_hud`), vendedor (Akara), andar+habilidade.
- Regressão no Ryujinx (28/10): aceitação 17/17 (na 1ª rodada 16/17: o critério "troca de alvo" depende de um monstro chegar a tempo;
  passou na 2ª), seletor 10/10, menus 16/16, criação 2/2, com o D2NXPad 1.1.1+telemetria. O 1.1.2 do pacote só muda o número da versão e o
  texto do INI padrão; nele foram repetidos o seletor (10/10) e a rodada de fluidez (60 FPS, `[THR]`/`[RT]` no log).
- Pacote: `dist/d2-medianxl-autorun-switch-2.14.6-final.zip` (29/10, D2NXPad 1.2.0 + D2Autorun 0.4.5; SHA256
  48a41e59e5c7e54e35adcb8c3e9f18b26a6f1b188291b6c61a6eabc88b3f12dd). Anteriores (históricos; em 31/10 já não estão em `dist/`):
  `-final-28out.zip` (luz baixa + telemetria, SHA f1564cbdd9f9e0cca790b0ce37de17e14c352eb2f700a5c409a711c813ed4382; gerou o log de 29/10),
  `-final-27out.zip` (gerou o log de 28/10), `-final-seletor.zip`, `-final-24out.zip`. DLLs e zips NAO vao para o git (so as fontes).

## 29/10: o que mudou depois do log do console (detalhes e números em `docs/D2PAD_CHANGES.md` N)
- 1º boot: o D2Autorun cria o `preferences.json` com luz baixa (antes a 1ª partida inteira rodava com luz alta).
- Movimento: espera a animação de ataque terminar (`[Movimento] EsperarAcao=1`) e anda direto com stamina baixa (`AndarCansado=1`).
- Barra de baixo: direcional com painel aberto + ZL = HUD (`[Painel] ZL=HUD`).
- **Tabelas dentro do MPQ**: `build/bin/mpq_overlay` (compilar com `tools/build_tools.sh`) grava as tabelas do `patch_gameplay.py` no
  `build/test_game/medianxl-data.mpq` (feito pelo `assemble_sd_tree.py`). Os multiplicadores de XP/runas/únicos/raros passam a valer (antes não valiam);
  `RejuvenescimentoNosVendedores=1` põe a Full Rejuvenation nos 18 vendedores. O menu de cheats continua sem efeito em tempo real.
- Para o usuário copiar: o `medianxl-data.mpq` muda (548 MB), então o pacote inteiro precisa ser recopiado.
- Falta no console: jogar ~10 min (campo + área nova) e mandar os logs; comparar o FPS com a luz baixa e conferir se as travadas de SD continuam.

## Stutter no console: causa provável tratada em 28/10 (confirmar no console)
Log do console recebido em 28/10 (24 min, Stony Field incluído) e analisado em `docs/D2PAD_CHANGES.md` seção M. Resumo: cidade a 60 FPS,
campo 37-46, Stony Field 14-40; o custo é contínuo por quadro (mediana 61 ms nos quadros lentos), sem degraus de vsync.
- **Mudança**: luz baixa por padrão (`[Desempenho] QualidadeLuz=0` no `d2nx.ini`; o D2Autorun grava `legacy_light_quality=0` no
  `preferences.json` do Median XL a cada boot). No Ryujinx o custo do quadro cai de 17-21 ms para 7-11 ms (cidade e campo).
  Cada fonte de luz pesa na luz alta, o que casa com as magias azuis do relato. Reverter: `QualidadeLuz=2`.
- **Falta no console**: jogar ~10 min incluindo Stony Field e rodar `tools/coletar_logs_console.py <raiz do SD>` (ou mandar
  `switch/wine/drive_c/DiabloII/d2nx/d2nxpad.log`, `D2Autorun.log` e `switch/wine/logs/autorun_runtime.log`). O `d2nxpad.log` agora
  traz `[THR]`, `[MEM]` e `[RT]` (linhas do runtime: memória de código traduzido, heap), então basta ele.
- **Ainda sem causa**: travadas de 0,6-5,6 s com o inventário aberto em Stony Field (16:48 do log). Se voltarem, ver `[MEM]`/`[RT]`
  do mesmo trecho (`code_mb=N/N` = memória de código esgotada; `heap_free_mb` baixo = pouca memória).
- **Se ainda faltar FPS em cenas pesadas**: sessão com `profile=1` no `Game.wine-nx.txt` (módulos x86 mais quentes; só no console).
- **Medição no Ryujinx**: `tools/medir_fluidez_ryujinx.sh` agora fixa o áudio SDL3 (volume 0): com `Dummy` o mixer do DirectSound
  fica a ~80% de um núcleo e distorce o `[THREADS]`. Para comparar custo de quadro sem o teto de 60 FPS: `tools/medir_campo_ryujinx.sh`.
- Achados do runtime (fonte em `build/autorun-src/wine-nx-probe`): `box64env.dynacache = 0` fixo em `source/wow64_box64_dynarec.c` e
  `BOX64_DYNACACHE` não está em `source/box64_options.h` -> o cache de tradução não liga pelo `Game.box64.txt`.
  `BOX64_DYNAREC_ALIGNED_ATOMICS=1` derruba o jogo; `BIGBLOCK=0` é 25% mais lento, `BIGBLOCK=1` derrubou o Ryujinx uma vez em duas.
- Se o `Game.box64.txt` der problema: apagar o arquivo (volta ao padrão). D2GL: `d2gl.fxaa=true`/`d2gl.sharpen=true` no `d2nx.ini`.

## Ambiente atual: Gentoo (03/11/2026) — validado no Ryujinx
- DLLs sem podman: `PATH=/mnt/STEAM/Projetos/devkit-tools/llvm-mingw/bin:$PATH sh src-d2nx/build.sh` (clang 23).
  Importações só USER32/ADVAPI32/msvcrt/KERNEL32. Aceitação 17/17 com essas DLLs e 17/17 com as do pacote de 29/10 (gcc);
  FPS igual (~60 no campo). D2NXPad 1.2.0 / D2Autorun 0.4.5 nas duas.
- Ryujinx 1.3.3: `flatpak install --user flathub io.github.ryubing.Ryujinx`; `~/.local/bin/ryujinx` ->
  `~/.local/share/flatpak/app/io.github.ryubing.Ryujinx/current/active/files/bin/Ryujinx` (roda fora do sandbox).
- **`vm.max_map_count` >= 524288** (`/etc/sysctl.d/90-ryujinx.conf`). Com o padrão 65530 o Ryujinx abre um aviso e fica
  parado: o log termina em `CheckLaunchState` e o teste sai sem `d2nxpad.log`.
- Capturas sem ImageMagick: `~/.local/bin/import` é um script que faz `import -silent -window <id> <arquivo>` com
  `ffmpeg -f x11grab -window_id` (a janela do Ryujinx é XWayland).
- Chaves e firmware 22.5.0 do console do usuário já estão em `build/ryujinx-test/system` e `build/ryujinx-test/bis` (pasta real).

## Reconstruir o ambiente numa distro nova
1. `git clone https://github.com/aeon1911/MedianXL-NX` (ou usar a pasta ja existente no disco STEAM).
2. Ferramentas: podman (ou docker), python3, flatpak, ImageMagick (`import`) e xprop (capturas), gcc/g++ para
   `tools/build_tools.sh` (StormLib). Em Bazzite/Fedora Atomic o podman ja vem.
3. Compilar as DLLs (imagem em `tools/docker/Dockerfile.pe-builder`; no CachyOS/Docker, se o grupo `docker` ainda não valeu na sessão:
   `sudo -u $USER -g docker docker ...`, e `--user $(id -u):$(id -g)` no `run` para os `.dll` não saírem de root):
   `podman build -t d2-autorun-pe-builder:latest -f tools/docker/Dockerfile.pe-builder tools/docker`
   `podman run --rm --security-opt label=disable -v "$PWD":/w -w /w/src-d2nx d2-autorun-pe-builder:latest sh build.sh`
   (sem `:Z`). Em 27/10 compilei com mingw i686 gcc 13 posix (extraido de .deb); a imagem usa o gcc do Debian bookworm.
   Depois de compilar, conferir que o `D2Autorun.dll` importa so ADVAPI32, KERNEL32, msvcrt, USER32 e rodar a aceitacao.
4. Ryujinx 1.3.3: no CachyOS é o pacote `ryujinx` (`/usr/bin/ryujinx`; os scripts usam `which ryujinx`); no Bazzite era o flatpak
   `io.github.ryubing.Ryujinx` com `~/.local/bin/ryujinx` apontando para ele. Chaves/firmware do console do usuário (NÃO versionados):
   copiar `prod.keys` e `title.keys` (backup em `build/backup-troca-distro/ryujinx-config/system/`) para `build/ryujinx-test/system/`
   (cópia, não symlink) e instalar o firmware com `tools/instalar_firmware_ryujinx.py ~/Downloads/Firmware.22.5.0.zip`
   (cria `build/ryujinx-test/bis/system/Contents/registered/`; o antigo symlink `build/ryujinx-test/bis` apontava para o flatpak).
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
