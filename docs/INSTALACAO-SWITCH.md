# Instalação Switch (Atmosphère)

**Estado:** instruções escritas. **`.nro` não foi gerado** neste PC (`DEVKITPRO` ausente). **Não testado em hardware.**

## Layout SD proposto

```
sdmc:/
  atmosphere/          # já existente no unlocked Switch
  switch/
    d2nx/
      d2nx.nro
  d2nx/
    d2nx.log           # criado em runtime
    input.ini          # remap opcional
    config.ini         # data_root=sdmc:/d2nx/mpq
    mpq/               # COPIAR MPQs legais do utilizador (fora do git)
      d2data.mpq
      d2exp.mpq
      d2char.mpq
      Patch_D2.mpq
      …
    saves/             # saves d2nx (não misturar com Save/ Windows sem querer)
```

Não distribuir MPQs/DLLs Blizzard no release homebrew.

## Toolchain (host de build)

Se `/opt/devkitpro` não existir:

1. Instalar [devkitPro pacman](https://devkitpro.org/wiki/devkitPro_pacman) para a distro.
2. Pacotes típicos:

```bash
sudo dkp-pacman -S switch-dev libnx switch-tools switch-sdl2
export DEVKITPRO=/opt/devkitpro
export PATH="$DEVKITPRO/devkitA64/bin:$PATH"
```

3. Build:

```bash
cd switch
make -f Makefile.nro
# output: switch/d2nx.nro
```

4. Copiar `.nro` para `sdmc:/switch/d2nx/` e abrir via **hbmenu**.

## O que o `.nro` fará na primeira fatia

- Consola texto 1280×720-ish
- Launcher pad-navegável (maioria dos mods `bloqueado`)
- Stick move offset; Minus troca scale mode; log em `sdmc:/d2nx/d2nx.log`
- Decode DC6 se `romfs:/sample.dc6` ou `sdmc:/d2nx/sample.dc6` existir (**não** commitamos assets Blizzard)

Framebuffer RGBA completo = próximo passo após toolchain.
