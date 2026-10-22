# STATUS — D2SE Switch (launcher + camada de compatibilidade)

Objetivo: iniciar o Diablo II **existente** do usuário (D2SE.exe/Game.exe, cores de
`D2SE/CORES`, mods, D2GL e D2Pad dele) via Box64 + Wine. Nada do jogo é reimplementado.

## Artefatos

| Item | Caminho | Estado |
|---|---|---|
| Menu Switch (.nro) | `dist/d2se-switch.nro` | escrito, compilado, **não executado no Switch** |
| Menu PC | `build/d2se_menu` | escrito, compilado, executado (lista a instalação real) |
| Runner | `compat/d2se-run.sh` | escrito, dry-run executado; recusa de tela verificada |
| Box64 aarch64 | `build/compat/box64-build/box64` | compilado, **não executado** (sem qemu/ARM aqui) |
| Versões fixadas | `compat/versions.env` | Box64 v0.4.4 (Tegra X1, BOX32), Wine 11.17 WoW64, devkitA64 20260219 |

### d2se-switch.nro
- Tamanho: **825541 bytes**
- sha256: **`1aa159d8ea56ddc2d19d59342411069353f10beba036839c99fbf5d4ccd5b3aa`**
- Compilado com `devkitpro/devkita64:20260219` (docker, sem rede). Log: `docs/evidence/nro-build.log`.
- Faz: lê `sdmc:/d2se/Diablo II LoD 1.13c -D2SE` (ou `root=` em
  `sdmc:/switch/d2se-launcher/config.ini`), lista cores e mods, flag D2GL (X), preflight (Y),
  grava `sdmc:/switch/d2se-launcher/launch.ini` (A) com `root=` traduzido para o ponto de
  montagem do L4T (`l4t_mount=`, padrão `/mnt/sd`).
- Não faz: reiniciar no Linux sozinho (a libnx desta imagem não tem a API de reboot-to-payload do
  Atmosphère); o usuário reinicia no L4T pelo hekate.

### Box64
- sha256 `e7d9e9e4d81fa8c88f93799e3be9d68544258495dfa6b0aeaef6708902fc17c5`, ELF aarch64,
  tag v0.4.4 (commit 2f130fa), `-DTEGRAX1=1 -DBOX32=ON`.

## Catálogo lido da instalação real (PC)
- 8 cores: 1.07, 1.08, 1.09b, 1.09d, 1.10f, 1.11b, 1.12a, 1.13c (D2GL indisponível em 1.07/1.08/1.09b).
- Mods D2SE: Eastern Sun (1.10f), Median XL Sigma 2.9.2 (1.13c, D2Pad via ModDll1),
  Nezeramontias (1.10f), Zy-El 4.8 (1.10f), Zy-El Trial by Fire (1.09d), Disotheb (1.10f).
- `d2hackmap_v1.14`: não é mod D2SE (sem `D2SE_SETUP.ini`).
- ProjectD2: standalone (Game.exe próprio, base 1.13c).

## Guardas do runner
- **D2VidTst**: grava `InstallPath` + `VideoConfig\Render` antes de iniciar; usa o stub do kit
  `D2VidTst_fix` do usuário (já instalado na raiz, sha256 `f7e9916d…`) com `Render` do descritor;
  derruba o prefixo se `D2VidTst.exe` aparecer mais de uma vez.
- **Tela**: `compat/d2se-run.sh` sai com código 6 sem abrir Wine se não houver
  `D2SE_ALLOW_DISPLAY=1` ou `D2SE_HEADLESS_DISPLAY`. No L4T o autostart define
  `D2SE_ALLOW_DISPLAY=1`.

## Executado neste PC
- `d2se_menu --list` sobre a instalação real: ok.
- Runner em dry-run: overlay, save de teste, `ActiveFolder`, D2GL e comando corretos.
- Execuções reais anteriores (host, Wine) abriram o D2SE.exe em `:0` e dispararam o teste de
  vídeo repetidamente; foram encerradas. Por isso a guarda de tela existe. Não há prova de janela
  do jogo neste PC nesta sessão (falta Xvfb para rodar isolado).
- A instalação do usuário não foi alterada (572 arquivos conferidos por sha256).

## Não testado
- Nada foi executado no Switch Lite. Sem medição de FPS; não há promessa de 60 FPS.
- Box64 + Wine x86_64 no aarch64 não foram executados.
- D2Pad sob Wine/Box64 não verificado.

## Bloqueio principal para ver a janela real do D2
Um ambiente Linux aarch64 (L4T no Switch) com Box64 + Wine instalados e a instalação do jogo
copiada para o SD; no PC, um display isolado (Xvfb) para a prova de host sem usar a tela do usuário.
