# Matriz de mods

Evidência: inventário local 2026-09-26 + `file` PE i386.  
Estados: `funcionando` | `parcialmente` | `nao_testado` | `bloqueado`.

**Cores:** nenhum core D2SE foi removido do requisito — ver `docs/CORES.md` (8 vanilla `nao_testado`).

## Contagens (mods + PlugY; exclui as 8 entradas vanilla do launcher)

| Status | Qtd |
|--------|-----|
| funcionando | **0** |
| parcialmente | **0** |
| nao_testado | **0** (vanilla cores contados em CORES.md) |
| bloqueado | **9** |

## Contagens launcher completo (cores + mods)

| Status | Qtd |
|--------|-----|
| funcionando | **0** |
| parcialmente | **0** |
| nao_testado | **8** (um por core) |
| bloqueado | **9** |

## Matriz de mods

| ID | Nome | Versão | D2Core **obrigatório** | Tipo | Status | Motivo / evidência |
|----|------|--------|------------------------|------|--------|--------------------|
| eastern_sun_r6e_4_1n | Eastern Sun R6E | 4.1N | **1.10f** | dados + DLLs x86 | bloqueado | PE32; não usar core 1.13c |
| median_xl_sigma_2_9_2 | Median XL Sigma | 2.9.2 | **1.13c** | D2Sigma/P113c | bloqueado | PE32; não usar 1.10f |
| nezeramontias_1_09 | Nezeramontias | 1.09 | **1.10f** | DLLs + D2GL | bloqueado | PE32 |
| zyel_4_8_4_1 | Zy-El (Kenet) | 4.8.4.1 | **1.10f** | DLLs + D2GL | bloqueado | PE32 |
| zyel_v44c | Zy-El TbF | 4.4c | **1.09d** | HackIt/maphack | bloqueado | PE32; core **1.09d** |
| disotheb | Disotheb | v1 | **1.10f** | D2Dreamland | bloqueado | PE32 |
| project_d2 | Project Diablo 2 | PD2 | **1.13c** | ProjectDiablo/BH | bloqueado | PE32 |
| d2hackmap_v1_14 | d2hackmap | 1.14 | **1.13c** | overlay | bloqueado | PE32; sem D2SE_SETUP |
| plugy_9_00 | PlugY 9.00 | 9.00 | multi | hooks | bloqueado | PE32; não é core |

## Pior blockers

1. **Um ramo de simulação por core** ainda inexistente (requisito “todos os cores”).
2. PlugY / D2Sigma / BH — hooks x86.
3. Trocar core do mod (ex. Zy-El 1.09d em lógica 1.10f) = drop silencioso — **proibido**; preflight amarra `required_core`.
