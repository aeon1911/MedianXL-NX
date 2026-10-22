# Saves

## Install do utilizador (READ ONLY)

Pasta `Save/` observada:

| Ficheiro | Notas |
|----------|-------|
| `*.d2s` | personagem |
| `*.key` / `*.map` / `*.ma0..ma2` | auxiliares (mapa/mercenário/stash PlugY-like) |
| `pd2_shared.stash` | PD2 |
| `backup/` | backups do utilizador |

**Este projecto não modifica essa pasta.**

## Desenho d2nx

```
<save_root>/<core_id>/<game_version>/<mod_id>/<ficheiro>
+ d2nx_save.meta   # core_id=… / game_version=… / mod_id=…
```

Cada core tem namespace separado. Misturar saves 1.09d com lógica 1.13c é recusado via meta.
Escrita segura: `*.tmp` → `fsync` → `rename` → mantém `*.bak`.

Abertura: se `d2nx_save.meta` existir e `mod_id`/`game_version` ≠ pedido → **recusa**.

## Header `.d2s` verificado (cópia `/tmp`)

Ficheiro origem (não alterado): `Save/TioChicoMarreta.d2s`

| Campo | Offset | Valor lido |
|-------|--------|------------|
| signature | 0 | `0xAA55AA55` (ok) |
| version | 4 | `96` |
| file_size | 8 | `5144` |
| checksum | 12 | `0xCF187EC0` |
| name | 20 | `TioChicoMarreta` |

Código: `src/save/save_service.cpp` — `parse_d2s_header`.
