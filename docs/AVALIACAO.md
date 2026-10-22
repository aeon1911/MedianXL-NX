# Avaliação técnica objetiva

## Ambiente do utilizador

- Install: Diablo II LoD **1.13c** + **D2SE 2.2.0** em `/mnt/steam/Diablo II LoD 1.13c -D2SE/` (READ ONLY neste trabalho).
- MPQs presentes (aprox.): d2data 256 MiB, d2exp 239 MiB, d2char 255 MiB, Patch_D2 2.1 MiB, d2music 535 MiB, d2sfx 50 MiB, d2speech 156 MiB, d2video 7.7 MiB, d2gl.mpq 63 MiB.
- Cores D2SE: `1.07 1.08 1.09b 1.09d 1.10f 1.11b 1.12a 1.13c`.
- ActiveFolder D2SE: `D2SE_Eastern Sun R6E 4.1N` (core **1.10f**).

## Runtime

Confirmado PE32 i386 para cliente vanilla e wrappers (D2GL/ddraw, glide3x, PlugY, D2Pad).  
**Não há caminho honesto que execute essas DLLs nativamente no Horizon OS ARM64.**

## Engines open-source

| | Completude | Licença | Fit Switch |
|--|------------|---------|------------|
| D2MOO | lógica 1.10f parcial; não cliente completo | MIT | melhor candidato futuro C++ |
| OpenDiablo2 | Act1 parcial; legado | as-is | Go — fraco fit homebrew |
| AbyssEngine | early | GPL-3 | Go |
| OpenD2 | halted | GPL-3 | C++ mas incompleto |

## O que esta sessão entregou

Fatia **dados→DC6→framebuffer 1280×720** + input/save/launcher **compilada e executada no PC**.  
Não: campanha, combate real, mods binários, `.nro` testado.

## Risco principal

Expectativa de “full campaign + all mods + 60 FPS Lite” exige anos de RE/reimplementação. O caminho correcto é fatias mensuráveis: mapa → entity → combat → save → features de mod uma a uma.
