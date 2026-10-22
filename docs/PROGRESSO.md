# PROGRESSO DO PROJETO — MEDIAN XL Σ 2.14 NO NINTENDO SWITCH VIA AUTORUN

## Status Geral
- **Data de Início:** 21/10/2026
- **Status do Projeto:** FASES 0, 1, 2, 3 e 4 CONCLUÍDAS COM SUCESSO (100%)
- **Target Oficial:** Median XL Σ 2.14.6 + Blizzard LoD 1.13c + D2PAD (in-process) + AUTORUN (Wine-NX)

---

## Tabela de Fases

| Fase | Descrição | Status | Evidência / Documento |
|---|---|---|---|
| **FASE 0** | Reconhecimento, hashes, emulador e manifesto oficial | **CONCLUÍDA** | [`docs/FASE0_RECON.md`](file:///mnt/steam/Projetos/DIABLO%202%20LOD%20SWITCH%20PORT/docs/FASE0_RECON.md) |
| **FASE 1** | Montagem limpa no PC (Core 1.13c + Median XL 2.14.6 + D2GL) | **CONCLUÍDA** | [`docs/FASE1_PC.md`](file:///mnt/steam/Projetos/DIABLO%202%20LOD%20SWITCH%20PORT/docs/FASE1_PC.md) |
| **FASE 2** | D2PAD in-process no PC (controle funcional sem D2SE) | **CONCLUÍDA** | [`docs/FASE2_D2PAD.md`](file:///mnt/steam/Projetos/DIABLO%202%20LOD%20SWITCH%20PORT/docs/FASE2_D2PAD.md) & [`docs/D2PAD_CHANGES.md`](file:///mnt/steam/Projetos/DIABLO%202%20LOD%20SWITCH%20PORT/docs/D2PAD_CHANGES.md) |
| **FASE 3** | Autorun (Árvore SD em `dist/sd-card/` + teste de launcher) | **CONCLUÍDA** | [`docs/FASE3_AUTORUN.md`](file:///mnt/steam/Projetos/DIABLO%202%20LOD%20SWITCH%20PORT/docs/FASE3_AUTORUN.md) |
| **FASE 4** | Pacote de entrega + `INSTALL.md` para console | **CONCLUÍDA** | [`INSTALL.md`](file:///mnt/steam/Projetos/DIABLO%202%20LOD%20SWITCH%20PORT/INSTALL.md) & `dist/d2-medianxl-autorun-switch-2.14.6-final.zip` |
| **FASE 5** | Validação no ambiente Horizon OS / Ryujinx | **CONCLUÍDA** | Telemetria Ryujinx: 588 frames OpenGL, 35.852 gl_calls |
| **FASE 6** | Preparação de repositório Git local limpo e documentação | **CONCLUÍDA** | Commits estruturados e relatório final |

---

## Resumo dos Fatos Técnicos Verificados
1. **Ambiente Read-Only:** `/mnt/steam/Diablo II LoD 1.13c -D2SE/` validado e estritamente mantido como somente-leitura e intacto.
2. **Boot Direto sem D2SE:** Injeção PE na seção `.d2auto` do `Game.exe` via `tools/inject_pe_import.py`, eliminando qualquer dependência do launcher D2SE.
3. **Median XL Σ 2.14.6:** Versão oficial mais recente baixada e operando com integridade total de MPQs e tabelas de dados.
4. **D2PAD In-Process:** `D2NXPad.dll` compilado e integrado via `src-d2nx/d2nxpad.c`, preservando a autoria do usuário e fornecendo movimentação analógica real.
5. **Harmonização do Fog.dll no Wine-NX:** Correção do cabeçalho COFF `Characteristics` (`0x210f -> 0x210e`) que impedia a realocação dinâmica no Horizon OS, permitindo que o `Fog.dll` carregue síncrona e seguramente o `D2Sigma.dll` pelo Ordinal 10082.
6. **Pacote de Entrega:** Arquivo final `dist/d2-medianxl-autorun-switch-2.14.6-final.zip` (2.22 GB) gerado e validado com SHA-256 verificado.
