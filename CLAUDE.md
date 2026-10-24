@AGENTS.md
@RULES.md

# Contexto do projeto (handoff do Antigravity → Claude Code, 23/10/2026)

Port do **Median XL Σ 2.14.6** (sobre Diablo II LoD 1.13c) para **Nintendo Switch (Atmosphère)** rodando no
**AUTORUN / Wine-NX** (Horizon OS, sem L4T), com controle via **D2NXPad** (reescrita in-process do D2PAD do usuário).
Repositório remoto: https://github.com/aeon1911/MedianXL-NX (branch `main`). Fale com o usuário em português.

## Arquitetura real (a que funciona)
- `Game.exe` (1.13c) recebe uma seção PE `.d2auto` (`tools/inject_pe_import.py`) que importa `D2Autorun.dll`.
- `src-d2nx/d2autorun.c` → `D2Autorun.dll`: detecta `D2Sigma.dll` → entrada "MedianXL", "modo seguro" (sem hooks de
  kernel32), VEH para c0000005 (W^X do Tegra), PIN de DLLs via `LdrRegisterDllNotification`, patch em memória no
  `Fog.dll +11690` (validação de seção crítica), ajusta `d2gl.ini` (perfil Switch 720p/60fps), carrega `d2nx\D2NXPad.dll`.
- `src-d2nx/d2nxpad.c` → `D2NXPad.dll`: lê XInput (Autorun expõe o Joy-Con como pad Xbox), lê memória do `D2Client.dll`
  1.13c (player, posição, painéis, resolução), contextos `[Game]/[Layer]/[Plus]/[Panel]/[Menu]` no `d2nxpad.ini`,
  envia input via `PostMessageA` (WM_MOUSEMOVE/LBUTTON…) para o HWND do jogo. Menu OSD de gameplay (ZL+−, −++, F10).
- Loader do Σ: `Game.exe` chama Fog.dll ordinal 10082 → carrega `D2Sigma.dll` na thread principal (não carregar pelo worker).
- `Fog.dll` do Σ tem bit `IMAGE_FILE_RELOCS_STRIPPED` errado → `tools/fix_fog_characteristics.py` (0x210f→0x210e).
- Vídeo: D2GL (`glide3x.dll`) com `-3dfx`; `Game.args.txt` = `-3dfx -direct -log`.
- Mods de gameplay (XP 2x, runas 2x, únicos/raros 1.5x) = tabelas `.bin` soltas em `data/global/excel/`
  geradas por `tools/patch_gameplay.py` a partir de originais em `build/gameplay_orig/` (usa modo `-direct`).

## Fluxo de build / teste / pacote (rodar na raiz do checkout principal)
- Do zero (usuario final): `./setup.sh "<pasta do Diablo II>" [--zip]` → `tools/build_all.sh` (build.sh,
  `fetch_medianxl.py` = manifesto oficial do launcher com MD5, `build_tools.sh` = StormLib fixa + `build/bin/
  merge_mpqs`/`mpq_extract`, `prepare_game.py --d2` = monta `build/test_game`, assemble). Sem ferramentas: Docker
  com `tools/Dockerfile`. `build/test_game.antigo` = pasta antiga (MPQ zlib quebrado), pode ser apagada.
- **Nada da Blizzard/Median XL no git, nem no historico** (pedido do usuario em 23/10): `.gitignore` cobre
  DLLs do jogo, MPQs, tabelas `.bin`, saves. `sd-card/` so tem runtime AUTORUN (LGPL) e configuracoes.
- `medianxl-data.mpq` = os 56 `medianxl-*.mpq` juntos por COPIA DE BLOCOS (`tools/merge_mpqs.cpp`). Nunca
  recomprimir: a Storm 1.13c nao le zlib (os sons viravam zlib → "The file data is corrupt" no item de ouro).
- Compilar DLLs: `docker run --rm -v "$PWD":/w -w /w/src-d2nx d2-autorun-pe-builder:latest sh build.sh`
- Montar SD: `python3 tools/assemble_sd_tree.py` (gera `dist/sd-card/` e `build/ryujinx-test/sdcard/`)
- Teste Ryujinx: `./tools/run_fase3_ryujinx_test.sh` (liga AutoJoin/AutoWalk no ini de teste; janela de 180 s;
  logs em `build/ryujinx-test/sdcard/switch/wine/{logs/autorun_runtime.log, drive_c/DiabloII/D2Autorun.log, drive_c/DiabloII/d2nx/d2nxpad.log}`)
- Pacote: `python3 tools/package_release.py` → `dist/d2-medianxl-autorun-switch-2.14.6-final.zip` (~2,2 GB).
  O pacote PRECISA conter `switch/d2-autorun/` (frontend .nro) + `switch/wine/` completo.
- `build/` (9 GB) e `dist/` (7 GB) não são versionados. Arquivos da Blizzard nunca entram no git (ver `.gitignore`).

## Limitações conhecidas do teste automatizado
- AutoJoin só **carrega personagem existente** (Barbaro/Reunow em `build/ryujinx-test/.../save`). Ele NÃO cria
  personagem novo → bugs na criação de personagem passam despercebidos. Testar criação explicitamente.
- Ryujinx no PC é case-sensitive e tem mais RAM; no console o SD é FAT32/exFAT. No console o hbmenu tem de ser aberto
  por **Title Override (segurar R ao abrir um jogo)**; em Applet Mode (~400 MB) o jogo dá Halt Fog line #904.

## Estado em 23/10/2026 (Claude Code)
Confirmado pelo usuário no console (antes desta sessão): boot até o jogo, memória (Title Override), 16:9,
cursor sem salto ao apertar A/X/Y (só `PostMessageA`), cursor oculto em gameplay.

Feito em 23/10 (validado no Ryujinx, **aguardando teste no console**) — detalhes em `docs/D2PAD_CHANGES.md` E/F:
- **"Generic bad character file"** tinha 3 causas: (1) o Wine do Autorun força LAA (`WINE_LARGE_ADDRESS_AWARE`
  ligado por padrão) e o heap do Fog ia para 0xC2xxxxxx → Halt #1696 na criação. Corrigido com gancho no stub
  do `NtAllocateVirtualMemory` 32-bit (`ZeroBits=1`, `low_memory_hook` no d2autorun.c; `[MedianXL] Memoria2GB=0`
  desliga). (2) O Median XL não cria `%APPDATA%\MedianXL\save` → `Error Loading ... Error:14`; o D2Autorun cria.
  (3) `charstats.bin` com `gld ` gravado pelo menu de gameplay (removido no boot; ouro inicial indisponível).
- O teclado do Autorun (`NtUserCallHwndParam(hwnd,0,30)`) **abre mas não aparece** com OpenGL (nem num app GL
  mínimo). R3/L3/− no `[Menu]` = **editor de nome** do D2NXPad (texto na própria caixa do jogo). Ação `OSK`
  mantém o teclado do Autorun.
- Injeção de teclas no Wine-NX (medido): texto = `WM_CHAR` postado com lParam 1 (contagem 0 é ignorada);
  apagar = `WM_KEYDOWN VK_BACK` (WM_CHAR 8 não apaga); SendInput/keybd_event/PostMessage de WM_KEYDOWN
  funcionam para teclas do jogo — usar UM caminho só (antes os 3 juntos alternavam painéis 2-3 vezes).
  Mensagens postadas são entregues antes da entrada do SendInput (não misturar na mesma sequência).
- **Camada nativa (RULES.md)**: movimento/ações pela rotina de comando do jogador do D2Client (+0x2A7C0),
  executada na thread do jogo pelo gancho por quadro em +0x16870 (rotina do cursor). Proibido voltar a
  andar/atacar por mouse. Menu de cheats desenhado nesse gancho (D2Win 10150/10184, D2Gfx 10014).
  Mapeamento no jogo: A=ATTACK, X=RIGHT, B=INTERACT, Y=I. Teste: `tools/run_acceptance_ryujinx.sh` (17 critérios).
- **"Anda dois passos e congela"** = caixa de chat aberta por um Enter atrasado do menu (herói não anda com
  chat aberto). D2NXPad fecha o chat se abrir sem Enter pedido. `WH_GETMESSAGE` NÃO funciona no Wine-NX.
- **Cursor oculto na partida**: rotinas de desenho do D2Client 1.13c +0x16870 e +0x15FC0 viram `ret` sem
  painel aberto (`set_cursor_hidden`). O mouse do jogo segue sendo usado (invisível) para andar/atacar.
- `−` no menu não manda ESC; saves ficam em `drive_c/users/steamuser/AppData/Roaming/MedianXL/save`
  (`d2nxpad.log` lista cada save).

## 24/10/2026 — movimento sem tremida, menus pelo direcional, Sim/Não da loja, Organizar (detalhes: D2PAD_CHANGES J)
- Direção do jogo: 64 passos, `floor((atan2(dy,dx)-45°)/5,625°)`; ordem de andar encaixada no centro (ModoOrdem=1).
- `move_cursor_exact` (mapa aprendido lendo o mouse do jogo) só para navegação/loja/organizar; o resto usa o mapa antigo.
- Memória achada: grade da unidade `Inventory+0x14` (2 inventário, 5 cubo, 6 baú; loja = NPC grade 2+aba), cursor
  com item `Inventory+0x20`, aba da loja `[D2Sigma+0x33DAE8]+0x10`, menu do NPC `D2Client+0x11963C` (l,t,r,b).
- Caixa Sim/Não (só compra): Sim 256,214 / Não 256,228 (1024x768), não aceita teclado.
- Storm só na thread do jogo (inventory.bin carregado no game_frame). "interagir" só com o herói parado ao lado.
- Roteiros de botões: `tools/run_script_ryujinx.sh <roteiro>` (comandos em script_tick); aceitação dos menus:
  `tools/run_menus_ryujinx.sh` (16 critérios). Nos ~8 s após entrar no jogo as teclas são ignoradas (Enter atrasado).

## Testes
- `tools/run_item_test_ryujinx.sh`: usa o personagem de `build/fresh-test-saves` (rodar antes o fresh test),
  `ReadTest=1` (CRC de `holy_ding.wav` pela Storm = 20ec3821) + `ItemTest=2` (clica no item de ouro em 856,496).
- `./tools/run_fase3_ryujinx_test.sh`: regressão (AutoJoin carrega Barbaro/Reunow, `AutoWalk=40` anda 40 s
  girando a direção; exige ≥ 10 posições distintas). NUNCA aceitar "ficou no mundo" sem o herói andar.
- Capturas da janela do Ryujinx (`xprop` + `import -window`) são a forma de ver o jogo; o Print Screen do
  jogo não mostra menus/chat. Comparar posição do mouse do jogo (log) com a captura para achar o cursor.
- `tools/run_ryujinx_fresh_test.sh`: `AutoCreate=1` — cria personagem com o editor de nome (esperado `Testd`),
  entra, Save and Exit, recarrega (`SUCESSO 2/2`). Tira os saves de teste do SD e devolve no fim
  (`EMPTYDIR=1` cria a pasta save vazia; sem ele simula console novo). Captura a janela do Ryujinx com
  `xprop` + `import -window` em `build/fresh-test-shots/` (o Print Screen do jogo não mostra o menu do ESC).
- O log do próprio jogo (`drive_c/DiabloII/Logs/D2Debug-*.txt`) tem Halts com despejo de pilha e erros de
  carga de personagem — foi o que mostrou o ponteiro acima de 2 GB e o `Error:14`.
- Código-fonte do runtime Autorun/Wine-NX: `build/autorun-src` (ntdll/win32u/wine-nx-probe).
- Harness PC (Wine + Xvfb em Docker `d2-mxl-tester`, sem tocar no desktop): `build/pc-repro/run.sh` + `x.sh`
  (xdotool + screenshot). Cópia reflink da árvore do SD em `build/pc-repro/DiabloII`.
- Nos menus o D2GL converte o mouse como 4:3 (960x720 centralizado) mas desenha esticado 16:9:
  ponto de janela = 160 + 0.75*x_tela. O 1º clique após uma tela nova às vezes é ignorado.
- O host do Ryujinx diferencia maiúsculas: o SD de teste precisa de variantes de nome de DLL/MPQ (no FAT32 não).
- `build/ryujinx-test/bis` é symlink para `~/.config/Ryujinx/bis` (config real do usuário) — herdado do Antigravity.

## Lições do histórico (não repetir)
- O agente anterior declarou "100% validado" várias vezes sem cobrir o cenário do usuário; `docs/PROGRESSO.md`
  superestima. `docs/STATUS.md`, `docs/ARQUITETURA.md`, `docs/CONTROLES.md` descrevem abordagens antigas
  abandonadas (D2SE/L4T/engine nativa) — não usar como verdade.
- Hook especulativo em `Fog.dll +1B195` causou Halt #1696 → removido. Único patch seguro no Fog é o +11690.
- Histórico completo das conversas do Antigravity: `~/.gemini/antigravity/brain/39c33ae5-c454-4148-b20c-a74b47ca24b1/.system_generated/logs/transcript.jsonl`
  (conversa "Porting Median XL To Switch") e `.../cbd64ada-128d-4258-b05d-8b03d72e6c17/...` (tentativa anterior com D2SE).
