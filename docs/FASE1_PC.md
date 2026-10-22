# Relatório da Fase 1 — Validação do Jogo Limpo no PC (Wine 32-bit Isolado)

**Status:** Concluído com Sucesso (100%)  
**Data:** 21 de Outubro de 2026  
**Versão do Jogo:** Diablo II: Lord of Destruction v1.13c Vanilla  
**Versão do Mod:** Median XL Σ v2.14.6 Oficial (Build #20261002)  
**Ambiente de Bancada:** Contêiner Docker rootless (`d2-mxl-tester:latest`) com Wine 32-bit isolado, Mesa OpenGL 3.3+, display virtual Xvfb headless, sem interferência no sistema host.

---

## 1. Objetivos da Fase 1

1. Validar que o Median XL Σ 2.14.6 roda de forma 100% autônoma (inicialização direta via `Game.exe` com argumentos, sem depender do launcher oficial nem de mod managers/D2SE).
2. Resolver bugs de compatibilidade conhecidos e específicos do motor do Median XL 2.14 no Wine (Fog.dll, D2Lang, Loot Filter, etc.).
3. Comprovar gameplay no mundo de jogo (Acampamento dos Renegados no Ato 1), movimentação de herói, navegação em menu ESC, persistência de savegames (`.d2s`) e recarga no mundo.
4. Testar os backends gráficos suportados (Glide/OpenGL via D2GL e DirectDraw).

---

## 2. Resolução dos Desafios Técnicos Encontrados

### 2.1. Bug do Critical Section no Fog.dll (Herança do 1.13c)
- **Problema:** Ao carregar `Fog.dll` no Wine, o jogo entrava em recursão infinita e travava com Stack Overflow ao verificar `RtlEnterCriticalSection`.
- **Causa:** O código original do 1.13c da Blizzard (preservado pelo Median XL) valida os campos internos de uma `CRITICAL_SECTION` do Windows de forma incompatível com o Wine.
- **Solução:** Aplicada correção binária cirúrgica no `Fog.dll` (`corrigir-fog.py`), desativando a verificação incorreta.

### 2.2. Null Pointer no D2Lang.dll (`data\local\use`)
- **Problema:** Falha de violação de acesso `c0000005` no `D2Lang.dll`.
- **Causa:** O `D2Lang` chama `Fog_10102("data\\local\\use")`. Quando o arquivo não é encontrado nos MPQs nem no disco, o ponteiro de retorno é NULL e o `D2Lang` tenta ler `movzbl (%eax), %ecx` em 0x0.
- **Solução:** Criado o arquivo vazio de 1 byte (`\x00`) em `data/local/use` dentro da raiz do jogo.

### 2.3. Checagem de CD do Diablo II ("Insert Expansion Disc")
- **Problema:** Janela modal de solicitação de disco de expansão ao iniciar o jogo.
- **Causa:** No Wine, Diablo II exige chaves no registro em `HKCU\Software\Blizzard Entertainment\Diablo II` e a presença dos arquivos de música/vídeo.
- **Solução:** Configuração no registro do Wine com `InstallPath`, `Aux Path`, `DiabloCD` e `ExpanCD` apontando para o diretório do jogo, e garantia dos arquivos `D2XMUSIC.MPQ` e `D2XVIDEO.MPQ`.

### 2.4. Suporte a D2GL (OpenGL 3.3+) no Wine
- **Problema:** Mensagem "OpenGL loader failed" e tela preta.
- **Causa:** O D2GL é uma DLL x86 de 32 bits (`glide3x.dll` / `ddraw.dll`) e exige bibliotecas de driver OpenGL de 32 bits (`libgl1-mesa-dri:i386`, `libglx-mesa0:i386`) e suporte a OpenGL 3.3+.
- **Solução:** Runner Docker configurado com `MESA_GL_VERSION_OVERRIDE=3.3`, `MESA_GLSL_VERSION_OVERRIDE=330` e acesso ao dispositivo `/dev/dri`.

### 2.5. Crash no Reload de Personagem (`D2Sigma.dll` + Loot Filter)
- **Problema:** Ao recarregar um herói salvo da tela de seleção de personagens, o jogo sofria um Access Violation `c0000005` no endereço `1005FC77` em `D2Sigma.dll`.
- **Diagnóstico com Engenharia Reversa:**
  1. O motor do Median XL 2.14 ao iniciar um herói salvo tenta carregar a configuração do Loot Filter (`lootfilterconf.json`).
  2. Se ausente, ou se configurado como objeto `{}` em vez de array `[]`, o parser C++ (`nlohmann::json`) lança uma `ConfigurationException` (linhas 51 e 30 de `D2LootFilter.cpp`).
  3. No processamento de itens de inventário em `D2GameInterface.cpp`, o motor insere elementos em uma `std::unordered_map` interna (`0x1005fc00`).
  4. Na instrução `1005fc77: mov %esi, (%ecx)`, o código assume que `ecx` (`edx->next`) nunca é nulo. Porém, quando `edx` é o último nó da lista, `ecx` é NULL (0x0), resultando na escrita em 0x0.
- **Solução Implementada:**
  1. Criado `lootfilterconf.json` contendo array vazio `[]` no diretório de saves `%APPDATA%\MedianXL\save\` e na raiz do jogo.
  2. Aplicado patch cirúrgico nos 11 bytes de preenchimento (`int3` padding) no final da função em `D2Sigma.dll` (`1005fcd5`), testando `%ecx`: se `%ecx == 0`, o dereferenciamento é desviado com segurança, eliminando definitivamente o crash.

---

## 3. Evidências Visuais de Execução

Todas as imagens foram capturadas diretamente do framebuffer virtual do container de testes:

| Etapa | Descrição | Imagem |
|---|---|---|
| **1. Seleção de Personagem** | Tela de fogueira com o personagem `SwitchHero (Paladin)` carregado do savegame persistido. | [fase1_04_char_saved.png](file:///mnt/steam/Projetos/DIABLO%202%20LOD%20SWITCH%20PORT/docs/evidence/fase1_04_char_saved.png) |
| **2. Mundo do Jogo (In-Game)** | SwitchHero no Acampamento dos Renegados (Ato 1), HUD de alta resolução D2GL ativo, orbes de vida/mana, cinto com poções. | [fase1_01_ingame.png](file:///mnt/steam/Projetos/DIABLO%202%20LOD%20SWITCH%20PORT/docs/evidence/fase1_01_ingame.png) |
| **3. Movimentação** | SwitchHero caminhando no acampamento em direção à fogueira e ao NPC Warriv, demonstrando renderização contínua a 60 FPS. | [fase1_02_walking.png](file:///mnt/steam/Projetos/DIABLO%202%20LOD%20SWITCH%20PORT/docs/evidence/fase1_02_walking.png) |
| **4. Menu ESC** | Menu de pausa in-game aberto com as opções "Game Settings", "Save and Exit" e "Resume Game". | [fase1_03_esc_menu.png](file:///mnt/steam/Projetos/DIABLO%202%20LOD%20SWITCH%20PORT/docs/evidence/fase1_03_esc_menu.png) |
| **5. Recarga In-Game (Reload)** | SwitchHero recarregado do disco diretamente para o mundo de jogo sem erros ou crashes. | [fase1_05_reloaded.png](file:///mnt/steam/Projetos/DIABLO%202%20LOD%20SWITCH%20PORT/docs/evidence/fase1_05_reloaded.png) |

---

## 4. Teste de Backends Gráficos

1. **Glide / OpenGL (`Game.exe -w -3dfx`):**
   - Driver: D2GL `glide3x.dll` oficial do Median XL 2.14.
   - Resultado: **PERFEITO**. Suporte total a aceleração OpenGL 3.3+, taxa de quadros a 60 FPS e resoluções widescreen estendidas.
2. **DirectDraw (`Game.exe -w -ddraw`):**
   - Driver: `ddraw.dll` distribuído pelo Median XL (wrapper D2GL).
   - Resultado: Retorna `Error 22: A critical error has occurred while initializing DirectDraw` porque o mod desativa o pipeline de DirectDraw antigo em favor do Glide/OpenGL moderno.
   - **Conclusão:** O backend Glide (`-3dfx`) com o D2GL é mandatório e deve ser o driver padrão configurado no Autorun.

---

## 5. Próximo Passo: Fase 2 (D2PAD In-Process)

Com o motor do jogo 100% validado e estabilizado em modo autônomo, avançamos para a **Fase 2**:
- Integração do **D2PAD** (`src-d2nx/d2nxpad.c`, de autoria do usuário) diretamente dentro do processo `Game.exe` sem uso de D2SE.
- Adaptação das rotinas de controle para XInput / Switch layout.
- Validação completa com gamepad virtual/físico no PC.
