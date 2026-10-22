# D2SE Switch — launcher + camada de compatibilidade para o Diablo II existente

Este projeto **não reimplementa, não reescreve e não redesenha o Diablo II**. Ele executa o jogo
que já está instalado: `D2SE.exe` / `Game.exe` originais, os cores de `D2SE/CORES`, os mods de
`MODS`, o D2GL do próprio usuário (`glide3x.dll`/`ddraw.dll` + `d2gl.ini` + `d2gl.mpq`) e o
D2Pad que os mods já trazem.

Como o Horizon OS (Switch) não é Linux, Box64 + Wine não rodam dentro de um `.nro` comum. O
desenho é:

1. **`dist/d2se-switch.nro`** (Atmosphère/hbmenu): menu estilo D2SE que lê a instalação no SD
   (`sdmc:/d2se/Diablo II LoD 1.13c -D2SE`, ajustável em `sdmc:/switch/d2se-launcher/config.ini`),
   lista os 8 cores e os mods, cada mod preso ao core exigido no `D2SE_SETUP.ini`, mostra o
   preflight e grava `sdmc:/switch/d2se-launcher/launch.ini`.
2. **Linux no Switch (L4T Ubuntu via hekate)**: `compat/d2se-run.sh launch.ini` executa
   `box64 wine D2SE.exe` (ou `Game.exe` para standalone como o ProjectD2), com versões fixadas em
   `compat/versions.env` (Box64 v0.4.4 perfil Tegra X1 + Wine 11.17 x86_64 WoW64).
3. **PC (x86_64)**: o mesmo menu (`build/d2se_menu`) e o mesmo runner, usando Wine direto.

## Menu: como escolhe core e mod
- Cores: cada pasta em `D2SE/CORES/<versão>` (1.07, 1.08, 1.09b, 1.09d, 1.10f, 1.11b, 1.12a, 1.13c).
- Mods: cada pasta em `MODS/` com `D2SE_SETUP.ini`; o core vem de `D2Core=`. Pastas sem esse
  arquivo (ex.: `d2hackmap_v1.14`) aparecem como "não-D2SE" e não iniciam.
- D2GL: flag liga/desliga. Só é oferecido quando o wrapper do usuário existe e o core é suportado
  pelo D2GL (1.09d, 1.10f, 1.11b, 1.12a, 1.13c…); 1.07/1.08/1.09b iniciam sem D2GL.
- Preflight: MPQs base, DLLs do core, `Patch_D2.mpq`, `ModMPQ1-3`, `ModDll1-3`, PlugY, D2Pad.
- Saves: a mesma pasta que o D2SE usa (`MODS/<mod>/save`, `D2SE/CORES/<core>/save`); nenhum
  formato novo.

## Runner (`compat/d2se-run.sh`)
- Não altera a instalação: monta um overlay de symlinks em `build/runtime/`, copia só
  ini/log/txt/json/cfg, aponta o save da entrada escolhida para o save real (ou para
  `build/test-saves/` com `--scratch-saves`), grava `ActiveFolder` no `D2SE.ini` do overlay e
  `Renderer/WindowMode/NoSound` no `D2SE_SETUP.ini` do overlay.
- **Guarda do D2VidTst**: grava `InstallPath` e `VideoConfig\Render` no prefixo antes de iniciar,
  usa o stub `D2VidTst.exe` do kit `D2VidTst_fix` do usuário (overlay com `Render` do descritor) e
  derruba o prefixo se o `D2VidTst.exe` reaparecer.
- **Guarda de tela**: recusa abrir Wine/jogo (código 6) na sessão gráfica do usuário, a menos que
  `D2SE_ALLOW_DISPLAY=1` (uso no L4T, onde a tela é do jogo) ou `D2SE_HEADLESS_DISPLAY` (Xvfb).

## Uso no PC
```bash
cmake -S . -B build && cmake --build build --target d2se_menu
./build/d2se_menu --list
./build/d2se_menu --select 1.13c --write build/compat/launch.ini
compat/d2se-run.sh build/compat/launch.ini --dry-run          # mostra o comando, não abre nada
D2SE_ALLOW_DISPLAY=1 compat/d2se-run.sh build/compat/launch.ini  # abre o jogo de verdade
```

## Compilar o .nro
```bash
docker run --rm --network none -u "$(id -u):$(id -g)" -v "$PWD":/p -w /p/switch/d2se-launcher \
  devkitpro/devkita64:20260219 make
cp switch/d2se-launcher/d2se-switch.nro dist/
```

## Estado
Ver `docs/STATUS.md` (escrito / compilado / executado neste PC / **não testado no Switch**).
Não há promessa de 60 FPS no Switch Lite.

O código antigo `d2nx` (renderizador próprio, compositor DT1/DS1, simulação) está **descontinuado**
e não faz parte do produto.
