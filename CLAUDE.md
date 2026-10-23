@AGENTS.md

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

Feito nesta sessão (validado em PC/Ryujinx, **aguardando teste no console**):
- **"Generic bad character file"**: causa = o menu de gameplay in-game do D2NXPad regravava
  `data/global/excel/charstats.bin` com `gld ` no kit inicial (o Antigravity só tirou do script Python).
  Com essa tabela o personagem novo trava no D2Sigma ao entrar (reproduzido no PC). Agora: menu não gera,
  `D2Autorun` apaga no boot (log "charstats.bin com ouro inicial (gld) encontrado: removido", testado no Ryujinx).
  Ouro inicial ficou indisponível.
- **R3/L3/− no `[Menu]` = `KEYBOARD`**: abre o teclado do Autorun (`NtUserCallHwndParam(hwnd,0,30)`), validado no
  Ryujinx (`[OSK] opened by the program`). Injeção de teclas do D2NXPad NÃO chega à caixa de nome no Wine-NX.
- `−` no menu não manda mais ESC; botões apertados ao fechar o teclado ficam travados até soltar.
- Saves do Median ficam em `drive_c/users/steamuser/AppData/Roaming/MedianXL/save` (não `DiabloII/save`);
  `d2nxpad.log` lista cada save e se o cabeçalho é válido.

## Testes
- `./tools/run_fase3_ryujinx_test.sh`: regressão (AutoJoin carrega Barbaro/Reunow, AutoWalk anda 30 s).
- `tools/run_ryujinx_fresh_test.sh`: `AutoCreate=1` (cria personagem; exige que não haja saves em
  `.../AppData/Roaming/MedianXL` do SD de teste — backup em `build/ryujinx-test-saves-backup-20261023/`).
  Tira screenshots com Print Screen do jogo em `drive_c/DiabloII/Screenshots/` (único jeito de "ver" o emulador).
  Limitação: digitar o nome não funciona por injeção; a criação completa no emulador não foi automatizada.
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
