# PROGRESSO DO PROJETO — MEDIAN XL Σ 2.14 NO NINTENDO SWITCH VIA AUTORUN

## Status Geral
- **Data de Início:** 21/10/2026
- **Fase Atual:** FASE 0 & FASE 1 Concluídas → Iniciando FASE 2 (D2PAD In-Process)

---

## Tabela de Fases

| Fase | Descrição | Status | Evidência / Documento |
|---|---|---|---|
| **FASE 0** | Reconhecimento, hashes, emulador e manifesto oficial | **CONCLUÍDA** | [`docs/FASE0_RECON.md`](file:///mnt/steam/Projetos/DIABLO%202%20LOD%20SWITCH%20PORT/docs/FASE0_RECON.md) |
| **FASE 1** | Montagem limpa no PC (Core 1.13c + Median XL 2.14.6 + D2GL) | **CONCLUÍDA** | [`docs/FASE1_PC.md`](file:///mnt/steam/Projetos/DIABLO%202%20LOD%20SWITCH%20PORT/docs/FASE1_PC.md) |
| **FASE 2** | D2PAD in-process no PC (controle funcional sem D2SE) | Em andamento | A ser gerado (`docs/FASE2_D2PAD.md`) |
| **FASE 3** | Autorun (Árvore SD em `build/sd/` + teste emulador) | Pendente | A ser gerado (`docs/FASE3_AUTORUN.md`) |
| **FASE 4** | Pacote de entrega + `INSTALL.md` para console | Pendente | A ser gerado (`INSTALL.md`) |
| **FASE 5** | Iteração com testes no console real | Pendente | - |
| **FASE 6** | Preparação de repositório Git local limpo | Pendente | - |

---

## Resumo dos Fatos Técnicos Verificados na Fase 0
1. **Ambiente Read-Only:** `/mnt/steam/Diablo II LoD 1.13c -D2SE/` validado e intocado. Todos os hashes do Core 1.13c e MPQs extraídos.
2. **Emulador Ryujinx:** Versão 1.3.3 instalada, com isolamento de dados via `--root-data-dir build/ryujinx-test` verificado.
3. **Compatibilidade do Autorun:** Comprovada em telemetria do Ryujinx (`autorun_runtime.log` com mais de 2.290 frames e módulos x86 executando).
4. **Obtenção do Median XL:** Versão mais recente identificada como **2.14.6** através do manifesto oficial `https://launcher.median-xl.com/mxl/release/public/manifest`. Download direto via API oficial testado com sucesso.
5. **D2PAD:** DLL de 102.400 bytes identificada em `/mnt/steam/Diablo II LoD 1.13c -D2SE/D2Pad2/D2Pad.dll`, nativa para 1.13c, com leitura via XInput e configuração via `.ini`.
