# Arquitetura — d2nx

## Premissa verificada

| Artefato | Resultado `file` / PE Machine | Usável no Switch ARM64? |
|----------|-------------------------------|-------------------------|
| `D2Client.dll` | PE32 i386 (`0x014c`) | **Não** |
| `D2Common.dll` | PE32 i386 | **Não** |
| `Fog.dll` | PE32 i386 | **Não** |
| `Storm.dll` | PE32 i386 | **Não** |
| `Game.exe` / `Diablo II.exe` | PE32 i386 | **Não** |
| `PlugY.dll` / `D2Pad.dll` / `ddraw.dll` (D2GL) / `glide3x.dll` | PE32 i386 | **Não** |

Conclusão: o runtime tem de ser **nativo C/C++ ARM64**. MPQs são dados; StormLib (MIT) substitui `Storm.dll`.

## Requisito: todos os cores

O install expõe **8 cores D2SE** (`1.07` … `1.13c`). Cada um é um **alvo de lançamento separado** (`LaunchIdentity{core_id, game_version, mod_id}`).  
É **proibido** usar uma única simulação 1.10f ou 1.13c para “cobrir” os restantes. Ver `docs/CORES.md`.

### Partilhado vs divergente

| Partilhado (formato/contentor) | Divergente (obrigatório por core) |
|--------------------------------|-----------------------------------|
| MPQ via StormLib | `Patch_D2` / `patch_d2` da pasta do core |
| DC6, DT1 7.6, DS1, pal.dat (layout) | Conteúdo de tabelas / skills / items |
| Input pad nativo, escala 1280×720 | Campo versão `.d2s` (87/89/92/96) |
| | Ordinals/hooks PE32 → reimplementar por core |

## Escolha de engine

### Candidatos avaliados

| Projeto | Licença | Linguagem | Patch alvo | Completude campanha | Acoplamento Windows | Decisão |
|---------|---------|-----------|------------|---------------------|---------------------|---------|
| **D2MOO** (ThePhrozenKeep) | MIT | C++ | **1.10f** | Reimplementação parcial de D2Common/D2Game (ordinal patching); **não** é cliente ARM standalone completo | Alto (foco em patch de DLLs originais) | **Base futura só do ramo 1.10f**; **não** substitui 1.07–1.09d / 1.11–1.13c |
| OpenDiablo2 (Go) | as-is / legado | Go | LoD assets | Act1 town / UI parcial (status ~2021); repo legado | Baixo | Rejeitado: Go+Ebiten no Switch homebrew é custo alto; campanha incompleta; projeto supersedido |
| AbyssEngine | GPL-3.0 | Go | genérico ARPG | Early; shim D2 incompleto | Baixo | Rejeitado: Go; GPL ok mas incompleto; runtime JS planejado |
| OpenD2 | GPL-3.0 | C++/SDL2 | D2 data | Unplayable / halted | Médio | Rejeitado: parado e incompleto |

### Decisão desta sessão

**Núcleo próprio `d2nx` (C++17 portátil)** + StormLib pinado **v9.40**:

1. Caminho de dados/render/input/saves **já compilável e executado no host**.
2. Registry + preflight + saves **por core** (8 entradas vanilla).
3. Fatia mapa/entidade **tagueada 1.13c** — evidência só desse core.
4. D2MOO, se integrado, activa **apenas** o ramo `1.10f`; os outros cores permanecem `nao_testado` até terem o seu próprio ramo.

## Renderer

| Fase | API | Justificativa |
|------|-----|---------------|
| **Agora (host)** | Framebuffer software RGBA8 + escala CPU | Prova real de DC6→PNG; microbench medido |
| **Primeiro `.nro` testável** | libnx console + (seguinte) framebuffer / **SDL2** via switch-sdl2 | Menor atrito para input+frame; Lite 1280×720 |
| **Caminho de performance** | **deko3d** (ou OpenGL ES se já empacotado) | Upload de atlas DC6/DT1; 60 FPS stock Lite |

Nesta sessão o `.nro` Makefile usa **console libnx** (frame de texto + decode DC6 se `romfs`/`sdmc` tiver sample). Blit framebuffer RGBA é o próximo passo gráfico — documentado em `STATUS.md`.

## Áudio

**Pendente.** Candidatos: switch-sdl2 mixer ou libnx audren. Stub implícito (sem áudio).

## Dados

```
SD/config data_root
  ├── d2data.mpq, d2exp.mpq, d2char.mpq, Patch_D2.mpq, …
  └── (mods: apenas camadas de dados quando existirem sem DLLs)
         ↓ StormLib
      DC6 / pal.dat / (futuro DT1/DS1/DCC)
         ↓
      framebuffer 800×600 → scale → 1280×720
```

## Subsystems — implementado vs em falta

| Subsystem | Estado |
|-----------|--------|
| MPQ open/list/extract | **implementado** (host) |
| DC6 + pal.dat | **implementado** |
| Escala integer/nearest/bilinear | **implementado** |
| Input deadzone/remap | **implementado** |
| Save temp+fsync+rename+meta | **implementado** |
| Launcher model + preflight | **implementado** |
| DT1/DS1 mapa | **não** (próximo para viewport de mapa) |
| Pathfinding / skills / missiles / AI / items / quests | **não** |
| Codec `.d2s` completo | só **header** read-only |
| Simulação D2MOO | **não integrada** |

## Motion smoothing (D2GL)

**Não** alterar tick de simulação. Interpolação visual só depois de existir simulação fixa. Estado: **pendente** (ver `D2GL.md`).
