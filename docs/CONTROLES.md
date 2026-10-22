# Controlos — D2Pad → Switch nativo

Evidência complementar: inventário read-only de [Analisar D2Pad2](403d05cc-fde0-4f66-beba-8c6875f72ae1).

## Achados D2Pad2

Caminho: `/mnt/steam/Diablo II LoD 1.13c -D2SE/D2Pad2/`

| Item | Facto |
|------|-------|
| Código-fonte | **Ausente** (sem `.c`/`.cpp`/`.h`/`.cs`/`.pas`) |
| `D2Pad.dll` | PE32 i386, 100 000 B; ImageBase `0x64740000`; linker 2.41 |
| Exports | `D2Pad_Init` (ord 1), `Init` (ord 2) — entrada típica PlugY/D2SE in-process |
| Input backend | `XInputGetState` (LoadLibrary dinâmico) **ou** WINMM `joyGetPosEx` (HID) |
| Output | `SendInput` + `PostMessageA` (`PostKeys=1` → HWND do jogo) |
| Leitura de jogo | strings/uso de `D2Client.dll` / `D2Common.dll` (aim/UI); **sem** RPM/WPM cross-process |
| Outros | `VirtualProtect`, `CreateThread`, `ChainLoad` via `LoadLibraryA` |

**Não portável no Switch:** PE loader, XInput/WINMM, SendInput/PostMessage, inject PlugY/D2SE.

Scripts (`instalar-controle.sh`, `restaurar-original.sh`, `corrigir-fog.py`, `_j.sh`) são utilitários PC/Wine — **não** são o motor do pad. `d2dx/glide3x.dll` é vídeo, não input.

## Diff `D2Pad.ini` vs `D2Pad_median.ini`

| Área | Default | Median |
|------|---------|--------|
| `ChainLoad` | vazio | `P113c.dll` |
| `AlwaysRun` | 1 | omitido |
| Aim extra | `MeleeReach`, `RangedCone`, `RangedDistance`, `HideCursor` | omitidos |
| `NavRepeatDelay` | 320 | 230 |
| `[Game] Back` | `LAYER3` | `KEY:TAB` |
| `[Telas]` | presente (`TapKey=TAB`) | **ausente** |

## Valores preservados em d2nx (defaults)

| Chave | Valor | Estado d2nx |
|-------|-------|-------------|
| `DeadzoneLeft` / `DeadzoneRight` | 0.22 / 0.25 | **implementado** |
| `CursorSpeed` / `CursorCurve` | 1.10 / 2.0 | speed default; curve pendente |
| `PollHz` | 125 | alvo futuro no loop Switch |
| `WalkRadius` / `WalkRepeatMs` / `CenterOffsetY` | 110 / 90 / 19 | **pendente** (precisa simulação) |
| `AimAssist` + cones/ranges | ver INI | **pendente** (maior gap vs remap simples) |
| `InteractRange` / `RepeatMs` / `PickItemsFirst` | 16 / 380 / 1 | **pendente** |
| `PostKeys` / `ChainLoad` | Win-only | **não portados** |

## Remap default Switch (posicional = Preset SWITCH)

| Switch | Acção d2nx | Origem D2Pad `[Game]` |
|--------|------------|------------------------|
| B | attack_primary | A=ATTACK_L |
| Y | attack_secondary | X=ATTACK_R |
| A | interact_pickup | B=INTERACT |
| X | inventory | Y=KEY:I |
| Plus | escape | Start=ESC |
| Minus | map | Back/`TapKey`≈TAB |
| LStick | run_toggle | L3=KEY:R |
| RStick | character | R3=KEY:C |
| DPad | skill_1..4 / menu nav | KEY:1..4 |

Layers LT/RT → F1–F12: modeladas de forma simplificada como `skill_*`; FSM completa (Game/Panel/Menu/Telas + Back+Start toggle) = **pendente**.

## Checklist de reconstrução nativa (sem DLL)

1. Pad hardware + deadzones + preset SWITCH  
2. Parser INI (secções/acções/layers)  
3. FSM UI: jogo / painel / menu / LAYER3  
4. Movimento (WalkRadius/Repeat/AlwaysRun)  
5. Aim/combat (lê entidades do motor nativo — não D2Client)  
6. Interact + PanelNav + cliques sintéticos → API interna  
7. `KEY:…` → keybinds nativos  
8. Viewport transform (substitui calibração HWND)

Código actual: `src/input/input.cpp` + `docs/evidence/input_default.ini` (fatia 1–parcial de 1 e remap).
