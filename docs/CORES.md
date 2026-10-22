# Cores D2SE — matriz obrigatória

**Requisito do utilizador:** todos os cores e versões do install têm de funcionar.  
Cores **não são intercambiáveis**. Uma simulação 1.10f ou 1.13c **não** cobre os outros.

Inventário: `D2SE/CORES/` + `D2SE.ini` (`ActiveFolder=D2SE_Eastern Sun R6E 4.1N`) — READ ONLY, 2026-09-26.

## Contagens de status (vanilla cores)

| Status | Qtd |
|--------|-----|
| funcionando | **0** |
| parcialmente | **0** |
| nao_testado | **8** (dados/preflight OK; simulação por core ausente) |
| bloqueado | **0** (vanilla); mods em `MODS.md` à parte |

Nenhum core foi removido da lista de requisitos.

## Matriz

| Core | D2SE folder | D2Client size | head4k MD5 (D2Client) | D2Common size | Patch no core | .d2s típico | Mods ligados | Status | Componente em falta para `funcionando` |
|------|-------------|---------------|----------------------|---------------|---------------|-------------|--------------|--------|----------------------------------------|
| **1.07** | `1.07` | 1175609 | `5665787b…0c3b` | 700473 | *(nenhum)* | 87 | — | nao_testado | simulação nativa 1.07; `D2Game.dll` ausente nesta pasta |
| **1.08** | `1.08` | 1126457 | `84652e71…593e` | 688185 | `Patch_D2.mpq` ~1.3M | 87 | — | nao_testado | simulação nativa 1.08 |
| **1.09b** | `1.09b` | 1138745 | `c87cc282…2210` | 692281 | `Patch_D2.mpq` | 87 | — | nao_testado | simulação nativa 1.09b |
| **1.09d** | `1.09d` | 1134644 | `a8161642…7ab8` | 692276 | `Patch_D2.mpq` | 87 | Zy-El v44c | nao_testado | simulação nativa 1.09d |
| **1.10f** | `1.10f` | 1085505 | `a99d67e7…88d0` | 725057 | `Patch_D2.mpq` ~3.0M | 89 | Eastern Sun, Nezeramontias, Zy-El 4.8, Disotheb | nao_testado | simulação nativa 1.10f (D2MOO candidato, **não** integrado) |
| **1.11b** | `1.11b` | 1093632 | `2671fcfe…ed3a` | 679936 | `patch_d2.mpq` ~3.1M | 92 | — | nao_testado | simulação nativa 1.11b |
| **1.12a** | `1.12a` | 1093632 | `f499a3ff…e67c` | 679936 | `patch_d2.mpq` | 92 | — | nao_testado | simulação nativa 1.12a |
| **1.13c** | `1.13c` | 1093632 | `0554262a…e00a` | 679936 | `patch_d2.mpq` 2.1M | 96 | Median XL, PD2, d2hackmap* | nao_testado | simulação nativa 1.13c; **demo mapa host tagueada 1.13c** ≠ funcionando |

\* d2hackmap sem `D2SE_SETUP`; assumido cliente 1.13c do install.

Fingerprints distintos em **todos** os cores (head4k MD5) — confirma que não se pode misturar DLLs/patch entre pastas.

## Partilhado vs divergente

| Camada | Partilhado entre cores? | Notas |
|--------|-------------------------|-------|
| Contentor MPQ / StormLib | Sim | |
| Formatos DC6, DT1 7.6, DS1, pal.dat | Sim (layout) | Conteúdo de `Patch_D2` diverge |
| MPQs base `d2data/d2exp/d2char` | Sim no install | |
| `Patch_D2` / `patch_d2` **por core** | **Não** | Preflight exige ficheiro do core |
| Tabelas bin/txt em patch | **Não** | Skills/items/stats por versão |
| Campo versão `.d2s` | **Não** | 87 / 89 / 92 / 96 típicos |
| Ordinals / hooks PE32 | **Não** + inutilizável ARM64 | Cada core = ramo de simulação nativo |
| PlugY / D2Pad / D2GL | Código x86 | Bloqueado em todos os cores |

## Código version-aware

- `src/core/cores.hpp` / `cores.cpp` — registry dos 8 cores
- `LaunchIdentity{core_id, game_version, mod_id}` + `launch_key()`
- Saves: `build/test-saves/<core_id>/<game_version>/<mod_id>/`
- Launcher: 8 entradas vanilla + mods com `required_core` exacto
- Preflight: recusa patch/core folder errado; **não** promove outro core

## Evidência mapa (só 1.13c)

Host: `docs/evidence/map_townE1_1.13c.png` — DS1 `townE1` + DT1 floor/fence/stones/objects dos MPQs base, **tagueado core=1.13c**.  
Isto **não** prova 1.07–1.12a nem 1.10f.
