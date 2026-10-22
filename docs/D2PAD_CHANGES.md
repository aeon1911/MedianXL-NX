# Modificações no D2PAD / D2Autorun para Median XL Σ 2.14.6 no Switch

## 1. Autoria e Preservação
- **Autor Original do D2PAD e D2Autorun:** Desenvolvido originalmente pelo autor do projeto (`src-d2nx/`). Todos os cabeçalhos de copyright, nomes de funções e arquitetura conceitual foram integralmente preservados.
- **Objetivo das Modificações:** Viabilizar a operação in-process no **Median XL Σ 2.14.6** sem depender de D2SE, contornando proteções do anti-tamper do `D2Sigma.dll` e garantindo compatibilidade com o Wine-NX (AUTORUN) no Nintendo Switch.

---

## 2. Resumo das Alterações Realizadas

### A. Injeção PE de Importação no `Game.exe`
- **Ferramenta:** [`tools/inject_pe_import.py`](file:///mnt/steam/Projetos/DIABLO%202%20LOD%20SWITCH%20PORT/tools/inject_pe_import.py)
- **Implementação:** Cria uma nova seção PE denominada `.d2auto` no executável `Game.exe`, anexando um descritor de importação para `D2Autorun.dll` que importa o símbolo `D2Autorun_Init`.
- **Efeito:** Carregamento 100% nativo do `D2Autorun.dll` pelo loader PE do Windows/Wine, sem necessidade de hooks em `kernel32.dll` ou executáveis intermediários (como D2SE).

### B. `D2Autorun.dll` (`src-d2nx/d2autorun.c`)
1. **Detecção Automática do Median XL Σ em Boot Direto:**
   - Detecta a presença de `D2Sigma.dll` no diretório do executável.
   - Define automaticamente `g_folder = "MedianXL"`, `g_core = "1.13c"` e ativa `g_safe = 1` (`ModoSeguro`).
2. **Modo Seguro (`g_safe = 1`):**
   - No Median XL, a DLL proprietária `D2Sigma.dll` possui rotinas de integridade e checagem de código em memória.
   - Os ganchos agressivos de sistema (interceptações em `kernel32` e APIs do sistema) são desativados para não acionar o anti-tamper do Sigma.
3. **Calibração do Tratador de Exceções Vetorado (VEH):**
   - O tratador VEH (`veh()`) interceptava qualquer exceção `0xC0000005`, inclusive leituras legítimas de páginas de guarda da heap do Wine (`kind == 0`).
   - A guarda foi ajustada para intervir estritamente em tentativas de execução/escrita de código (`kind == 1 || kind == 8`) em seções de imagem executável (`MEM_IMAGE` com flags de código executável), preservando a estabilidade da alocação de memória do Wine.
4. **Fixação em Memória (PIN) via `LdrRegisterDllNotification`:**
   - Registra notificações de carregamento de DLL e aplica `PIN` (`LoadLibraryExA(..., NULL, LOAD_WITH_ALTERED_SEARCH_PATH)`) imediatamente sobre `D2Sigma.dll`, `glide3x.dll` (D2GL) e `D2NXPad.dll`, evitando descarregamentos acidentais.

### C. `D2NXPad.dll` (`src-d2nx/d2nxpad.c`)
1. **Compatibilidade de Foco com Wine Virtual Desktop:**
   - No modo Virtual Desktop do Wine (`wine explorer /desktop=d2,1280x720`), `GetForegroundWindow()` retorna a janela do desktop gerenciada pelo processo `explorer.exe`.
   - Adicionada verificação de ancestralidade (`GetParent` loop): se a janela em foco for a janela do jogo (`g_hwnd`) ou um ancestral direto dela, o D2PAD reconhece o foco como ativo e mantém o processamento de inputs analógicos e botões.
2. **Resolução de XInput em Ambiente de Teste e Produção:**
   - Atualizado `load_xinput()` para testar caminhos relativos locais (`.\\xinput1_4.dll`, `d2nx\\xinput1_4.dll`) antes de recorrer à busca padrão do sistema.
   - No Switch (AUTORUN / Wine-NX), a DLL local de mock não está presente e o D2PAD carrega diretamente o driver nativo do Switch em `xinput1_4.dll`.
3. **Leitura de Estruturas do Jogador e Mundo (Median XL Σ):**
   - Validados os offsets do `D2Client.dll` 1.13c:
     - `D2Client` base `0x6FAB0000` detectado com sucesso.
     - Detecção de jogador in-game (`player_unit()`): ponteiro `0x16A33E00`.
     - Posição no mundo lida em tempo real: `(6143.5, 5203.5)` no nível 1 (Rogue Encampment).
     - Escaneamento de 47 unidades ativas na sala (`units 47`).
     - Resolução D2GL/Median XL identificada e calibrada: `1024x768`.

### D. Mock de XInput para Testes Headless (`src-d2nx/tests/xinput_file.c`)
- **Compilação Estática:** Corrigido [`src-d2nx/build.sh`](file:///mnt/steam/Projetos/DIABLO%202%20LOD%20SWITCH%20PORT/src-d2nx/build.sh) para incluir `-static-libgcc`, eliminando dependência dinâmica de `libgcc_s_dw2-1.dll`.
- **APIs Exportadas:** Implementadas `XInputGetState`, `XInputSetState`, `XInputEnable`, `XInputGetCapabilities`, `XInputGetBatteryInformation`, `XInputGetKeystroke` e `XInputGetAudioDeviceIds`, garantindo que tanto o `glide3x.dll` quanto o `D2NXPad.dll` inicializem sem conflitos durante os testes automatizados em contêineres Docker.
- **Isolamento:** Este mock é restrito ao pipeline de teste PC (`build/test_game/`) e **NUNCA** é transferido para o SD Card do Nintendo Switch.
