# D2GL — equivalentes nativos

Fonte: `d2gl.ini` + `d2gl.mpq` (63 MiB) + `ddraw.dll` PE32 (wrapper).  
D2GL é **hook Windows/OpenGL** sobre o cliente 1.13/glide — **não portável** como DLL.

## Tabela

| Feature d2gl.ini | Depende de hooks Win? | Equivalente d2nx | Estado |
|------------------|----------------------|------------------|--------|
| fullscreen / window size | Sim (Win window) | Horizon fullscreen 1280×720 | nativo Switch |
| vsync / fps caps | Sim | libnx / SDL swap | pendente no `.nro` |
| shader_preset / FXAA / bloom / sharpen / LUT | Sim (GL shaders) | deko3d pós-process | **pendente** |
| hd_cursor / hd_text | Sim | fonte pixel + escala | escala ok; fonte **pendente** |
| motion_prediction | Sim (D2DX pred) | interpolação render **sem** mudar tick | **pendente** (sem simulação) |
| skip_intro | Sim | flag launcher | pendente |
| no_pickup / show_item_quantity / monster_res | Sim (jogo) | lógica nativa | **pendente** |
| load_dlls_early/late | Sim | **unsupported** | bloqueado |
| stretched_horizontal/vertical | Sim | ScaleMode stretch | **implementado** (nearest/bilinear) |
| Integer / pixel look | parcial | `ScaleMode::Integer` | **implementado** |

## Nota de texto

Legibilidade em Lite: preferir **integer scale** + fonte bitmap do jogo quando o pipeline de font DC6 existir. HD text do D2GL não será recriado via hooks.
