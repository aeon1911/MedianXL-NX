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

### E. 23/10/2026 — teclado do console, `−` no menu e "Generic bad character file"
- **R3/L3/− no menu = teclado do console** (`KEYBOARD`): o D2NXPad chama `NtUserCallHwndParam(hwnd, 0, 30)`
  (ShowSoftwareKeyboard do win32u do Autorun), o mesmo teclado do atalho `− + R3`. Só é chamado quando o
  `ntdll` exporta `wine_nx_pe_teb`/`wine_nx_user_shared_data` (Wine-NX); fora dele cai no nome fixo.
  Motivo: nenhuma injeção de tecla do D2NXPad (PostMessage/SendInput) chega à caixa de nome no Wine-NX
  (verificado no Ryujinx por screenshot); o teclado do Autorun entra como teclado de hardware.
  Evidência: `[OSK] opened by the program` no log do runtime durante o teste.
- **`Minus` no `[Menu]` não manda mais `ESC`**: `− + R3` é o atalho do teclado do Autorun e o `ESC` do `−`
  tirava da tela de nome / fechava o jogo pelo menu principal. Botões ainda apertados quando o teclado fecha
  (o `−` que o fecha) ficam travados até serem soltos (`pad_latch`), para não virarem ação no jogo.
- **Ouro inicial removido**: `apply_and_save_gameplay_menu()` gravava `data/global/excel/charstats.bin` com
  `gld ` no kit inicial — personagem novo travava/era recusado pelo Median XL. Agora o menu apaga esse arquivo
  e o `D2Autorun` o remove no boot quando contém `gld ` (testado no PC e no Ryujinx).
- **Diagnóstico de saves** no `d2nxpad.log`: cada `.d2s` de `%APPDATA%\MedianXL\save` com tamanho e validade
  do cabeçalho (assinatura, tamanho, checksum).
- **`AutoCreate=1`** (`[AutoTest]`, só teste): cria personagem novo no emulador e tira screenshots (Print Screen
  do jogo) a cada passo; complementa o `AutoJoin`, que só carrega personagens existentes.

### F. 23/10/2026 (tarde) — causa real do "Generic bad character file", editor de nome e teclas
Relato do console depois da versão E: o R3 travava o mouse e os botões digitavam letras, mas o teclado não
aparecia, e o "Generic bad character file" continuava. Tudo abaixo foi reproduzido e validado no Ryujinx
com o `AutoCreate`, sem saves no SD de teste, como num console novo.
- **Memória acima de 2 GB (causa do Halt/"bad character file")**: o Wine do Autorun liga
  `WINE_LARGE_ADDRESS_AWARE` por padrão (como o Proton) e o runtime não passa variável de ambiente, então
  o Game.exe 1.13c (não-LAA) recebe heap acima de 0x80000000. Na criação de personagem o D2Common passou ao
  Fog o ponteiro `0xC27291D4` (pilha no `Logs/D2Debug-*.txt`) e o realloc do pool parou em
  `Halt line #1696` (bloco "não alocado": o pool usa aritmética com sinal). O Fog já recusava ponteiro
  acima de 0x7FFFFFFF na validação que o patch +11690 desliga. Correção no `D2Autorun`: gancho no stub do
  `NtAllocateVirtualMemory` do ntdll de 32 bits que pede `ZeroBits = 1` quando não há endereço fixo
  (programa não-LAA, como no Windows); sem memória baixa, cai no pedido original. Reservar o espaço alto
  com `VirtualAlloc` não funciona (erro 487) e trocar só o `VirtualAlloc` da IAT não pega o heap.
  `[MedianXL] Memoria2GB=0` no `d2nx\d2nx.ini` desliga. Antes: Halt em 3 de 3 testes; depois: personagem
  criado, salvo e recarregado em todos.
- **Pasta de saves**: o Median XL não cria `%APPDATA%\MedianXL\save` (no Windows é o instalador). Sem ela o
  log do jogo mostra `[PLAYER LOAD] ClientAddPlayerToGame() Error Loading:<nome> Error:14`, que é o
  "Generic bad character file". O `D2Autorun` cria a pasta no boot.
- **Teclado do Autorun invisível com OpenGL**: o runtime desenha o teclado na troca de quadros do OpenGL
  (`nx_osk_draw`), mas no Ryujinx ele não aparece nem num programa OpenGL mínimo (tela verde +
  `NtUserCallHwndParam(...,30)`). O teclado abre (captura o controle) sem aparecer, que é o que o
  usuário viu. Fica como ação `OSK` no ini; `KEYBOARD` agora é o editor de nome do D2NXPad.
- **Editor de nome** (`[Menu]` R3/L3/−): escreve um nome sugerido na caixa do jogo e o direcional edita
  (↑↓ avançam/voltam a última letra, → acrescenta, ← apaga, Y sorteia, + ou X cria, A/B/R3 saem). Texto por
  `WM_CHAR` postado com contagem de repetição 1 e Backspace por `WM_KEYDOWN VK_BACK` postado, os dois na
  mesma fila para manter a ordem. Medido no emulador, uma forma de cada vez, conferindo a caixa de nome nas
  capturas: `WM_CHAR` com contagem 0 e o `SendInput` de letras não escrevem; `WM_CHAR 8` não apaga.
  Teste: `Teste`, ↑, →, ←, ↓, ↓ → save `Testd.d2s`.
- **Teclas por um caminho só**: o `tap_key` mandava cada tecla por `SendInput`, `keybd_event` e
  `PostMessage` juntos. No Ryujinx cada um, sozinho, abre/fecha o menu do ESC dentro do jogo, então um
  toque alternava o painel 2-3 vezes. Agora vai só por `SendInput` (+ `WM_CHAR` para letras/dígitos); o
  `+` abre o menu e o Save and Exit funciona no teste.
- **Diagnóstico de Halt**: o `D2Autorun` rotulava todo Halt como "#904 / falta de memória"; agora só o
  #904 recebe esse texto, e o log mostra o uso de memória abaixo/acima de 2 GB.
- Correções à seção E: a injeção chega, sim, à caixa de nome (só precisava de contagem 1). O `charstats.bin`
  com ouro era uma causa real no PC, mas no console a falha continuava por causa da memória alta e da
  pasta de saves.
