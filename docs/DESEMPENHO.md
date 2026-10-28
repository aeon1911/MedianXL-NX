# Desempenho

## Alvo (ainda não medido no hardware)

- **60 FPS**, **1280×720**, Switch **Lite**, clocks **stock**, **sem OC**.

## Medido neste PC (HOST ONLY)

| Teste | Resultado | Log |
|-------|-----------|-----|
| DC6 render + scale | ~2.5 ms/frame CPU (sessão anterior) | `build/logs/host_demo.log` |
| Mapa townE1 DS1+DT1 + sim 4s @25Hz | EXIT 0; PNG 1280×642 com tiles reais | `build/logs/map_demo.log` |

Estes números **não** são FPS de Switch Lite.

## Medido no Ryujinx (27/10/2026) — ver D2PAD_CHANGES.md, seção L

| Cenário (80 s de jogo andando) | FPS em regime | quadros > 33 ms por 5 s |
|---|---|---|
| Box64 padrão, D2GL com FXAA+nitidez | 51,2 (6 rodadas, 50,6-51,5) | 1,2-1,9 |
| `SAFEFLAGS=0` | 54,9 (3 rodadas) | 0,6-1,2 |
| pacote final (SAFEFLAGS=0, sem FXAA/nitidez, D2NXPad leve) | 54,7 | 1,6 (140 s) |

O FPS do emulador não é o do console; serve para comparar versões. A telemetria `[PERF]` do `d2nxpad.log` mede o mesmo
no console.

## Não medido

- GPU Switch / deko3d / Lite 60 FPS
- Custo de simulação completa por core
