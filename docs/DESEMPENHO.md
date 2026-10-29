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

## Medido no console (28/10/2026, `d2nxpad.log` de 24 min, pacote de 27/10) — ver D2PAD_CHANGES.md, seção M

| Cena | FPS médio | observação |
|---|---|---|
| Rogue Encampment parado | 60,0 | |
| Blood Moor / Den of Evil | 45 / 46 | |
| Cold Plains | 37 | |
| Stony Field (magias azuis, muitos monstros) | 37 (p10 22, mínimo 14) | pior cena |

815 quadros acima de 50 ms em 24 min (mediana 61 ms); 18 acima de 500 ms (30,9 s), 10 deles em ~60 s com o inventário aberto
(causa não confirmada). Analisar um log novo: `tools/coletar_logs_console.py <raiz do SD>` ou `tools/analisar_perf_log.py <d2nxpad.log>`.

## Medido no Ryujinx (28/10/2026, fw 22.5.0, áudio SDL3 em tempo real, sem limite de FPS) — custo do quadro

| Cena | luz alta (padrão do jogo) | luz baixa (`[Desempenho] QualidadeLuz=0`) |
|---|---|---|
| cidade | 17,4-17,7 ms (57 FPS) | **8,6 ms (116 FPS)** |
| cidade, cenário da aceitação | 20,7-21,2 ms | **10,1-10,8 ms** |
| campo (Blood Moor), cenário da aceitação | 16,9-18,7 ms | **7,2-7,7 ms** |

Com o limite de 60 FPS e a luz baixa o jogo fica em 59,7-60,1 FPS com a thread principal a 55-63% (antes 98%). Outras variantes
medidas e descartadas: `BIGBLOCK=0` (-25%), sombras translúcidas desligadas (sem ganho de CPU), luz normal (sem ganho).
O FPS do emulador não é o do console: o que vale é a razão entre as variantes. Confirmar no console com o log novo.

## Não medido

- GPU Switch / deko3d / Lite 60 FPS
- Custo de simulação completa por core
