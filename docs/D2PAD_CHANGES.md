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

### G. 23/10/2026 (noite) — "anda dois passos e congela" e cursor visível ao andar
- **Causa do congelamento**: um Enter que fica na fila do jogo durante o carregamento (o `+`/`X` que entra
  no jogo) é processado segundos depois de entrar e **abre a caixa de chat**. Com o chat aberto o herói
  não anda, e o jogo volta quando um ESC (o `+`) fecha o chat. Reproduzido no Ryujinx com capturas (chat
  aberto e herói parado por 75 s) e com o `ui_open(UI_CHAT)` no log; sem a tecla R o chat abre do mesmo
  jeito. Correção: o D2NXPad fecha o chat com ESC quando ele abre dentro do jogo sem um Enter pedido
  no último 1,5 s (log "chat do jogo aberto sem pedido").
- O `WH_GETMESSAGE` do D2NXPad nunca foi instalado no Wine-NX (`SetWindowsHookExW` retorna NULL, erro 1);
  o log agora mostra o erro. Nada pode depender dele.
- **Cursor oculto na partida**: o D2Client 1.13c desenha o cursor em +0x16870 (principal, imagem pelo tipo
  de cursor) e +0x15FC0. Sem painel aberto as duas viram `ret` (bytes conferidos antes); com painel/menu
  aberto voltam. O mouse do jogo continua sendo usado por baixo para andar e atacar, invisível. Desligando
  só a +0x15FC0 o cursor piscava. Verificado por capturas: 40 s andando sem cursor; com o inventário
  aberto o cursor aparece.
- Vigia do herói parado repete o clique a cada 0,8 s enquanto o analógico estiver segurado (antes parava
  depois de 3 tentativas).
- Testes: `AutoWalk=N` (N > 1) anda N segundos girando a direção e registra a posição a cada segundo;
  `run_fase3_ryujinx_test.sh` agora exige que o herói mude de posição (≥ 10 posições em 40 s) — o
  critério antigo ("30 s no mundo") aprovava o herói parado. `CursorTest=1` abre/fecha o inventário.

### H. 23/10/2026 (noite) — camada nativa de movimento/ações e menu de cheats visível (RULES.md)
- **Causa raiz** do movimento por mouse: o D2NXPad (reescrita do D2PAD) andava apontando o mouse do jogo
  para um ponto à frente do herói e segurando o clique (`walk_point` + `send_button`), atacava movendo o
  mouse até o monstro e clicando, e alternava andar/correr apertando `R`. Daí o cursor visível, o herói que
  parava quando o jogo perdia o "clique segurado" e os ataques intermitentes. A/X/Y eram `SKILL:2/3/1`
  (tecla F + clique direito no alvo ou à frente com Shift), então os três atacavam.
- **Camada nativa** (`d2nxpad.c`): a rotina de desenho do cursor do D2Client (+0x16870, chamada a cada
  quadro na thread do jogo) vira um salto para `frame_hook`, que executa os comandos pela rotina de comando
  do jogador do D2Client (+0x2A7C0 — a mesma para onde um clique termina: ação local com caminho/colisão/
  animação + comando ao servidor). O analógico vira direção no mundo (inversa da projeção isométrica),
  limitada a 1 (diagonal), com zona morta; ordens "andar/correr até (x,y)" 7 subtiles à frente a cada
  130 ms; soltar = "andar até onde está". Nenhuma chamada de mouse no jogo (o mouse do jogo fica parado
  em 1023,767 durante todo o teste).
- **Botões**: A = `ATTACK` (comando 6, habilidade esquerda na unidade tipo/id), X = `RIGHT` (0x0D), B =
  `INTERACT` (4 correr até a unidade, depois 0x13 interagir), Y = `KEY:I`. Sem alvo: comando no chão à frente
  (5/0x0C); na cidade A sem alvo não faz nada. Segurando, o comando só se repete quando o herói terminou o
  golpe (modos 1–6) e procura outro alvo quando o atual morre.
- **Menu de cheats** (ZL+−, −++, ZL++ ou F10): desenhado a cada quadro pelo gancho com as funções do
  próprio jogo (D2Win 10150 DrawText, 10184 SetFont, D2Gfx 10014 DrawRectangle — assinaturas conferidas nas
  chamadas do D2Client). Antes as linhas iam para uma fila esvaziada por um `WH_GETMESSAGE` que nunca
  instalou no Wine-NX, por isso o menu era invisível. Ações: XP, runas, únicos e raros (A aplica: grava o ini
  e regera `experience_sp/itemratio_sp/treasureclassex.bin`); ouro inicial segue indisponível.
- **Teste**: `tools/run_acceptance_ryujinx.sh` (AutoWalk=2 + MenuTest=1) avalia 17 critérios pelo log:
  17/17 no Ryujinx. Um bug no próprio log de teste (argumento faltando no `lg`) derrubou o emulador 4 vezes
  com acesso a 0x16 até ser achado — não era a camada nativa.

### I. 23/10/2026 (noite) — crash ao usar o item de 1.500 de ouro e repositório sem arquivos do jogo
- **Sintoma (console)**: Amazon nova, botão direito no item de ouro do kit inicial → o ouro entra e o jogo
  volta para o HOME; `D2Autorun.log`: "The file data is corrupt. File: medianxl-data.mpq".
- **Causa raiz**: o antigo `tools/merge_mpqs.cpp` relia cada arquivo dos 56 `medianxl-*.mpq` e o gravava de
  novo com **zlib**. A `Storm.dll` do 1.13c não descomprime zlib. Os 389 sons do Median XL (WAV com
  ADPCM+Huffman da Blizzard, máscaras 0x41/0x08) passaram a zlib; ao tocar um deles (o item toca
  `sfx\taunt\holy_ding.wav`) a Storm falha e o jogo fecha. Os outros arquivos usavam PKWARE, que a Storm lê,
  por isso o resto funcionava.
- **Como foi provado**: diagnóstico novo no D2Autorun (gancho em `SFileOpenFileEx`/`SFileReadFile` da Storm:
  guarda os últimos nomes abertos e registra a leitura que falhou; `[MedianXL] DiagnosticoStorm=0` desliga;
  `DiagnosticoLeitura=1` registra CRC das leituras do MPQ, só para depurar) mostrou `holy_ding.wav` antes
  do erro. As leituras do Wine-NX são exatas (139 blocos conferidos), o arquivo no SD está íntegro, e o
  mesmo erro acontece no Wine do PC: não é emulação. `ReadTest=1` (D2NXPad) lê o som pela Storm do jogo:
  com o MPQ antigo falha, com os originais e com o MPQ novo dá CRC `20ec3821`.
- **Correção**: `merge_mpqs` reescrito — copia os **blocos comprimidos como estão** (sem descomprimir nem
  recomprimir) para um MPQ formato 1 escrito à mão (tabelas de hash/blocos criptografadas com as chaves
  padrão, `(listfile)` sem compressão). Continua um MPQ só (boot rápido no cartão). Teste:
  `tools/run_item_test_ryujinx.sh` (ReadTest + ItemTest: usa o item no inventário) 5/5, ouro 0 → 1.500,
  jogo vivo.
- **Repositório**: saíram do git (e do histórico) `D2Sigma.dll`, `Fog.dll`, `glide3x.dll`, `d2gl.mpq`,
  `d2fps.dll`, `ddraw.dll`, os `medianxl-*.mpq`, as tabelas `.bin` e as imagens antigas de `docs/evidence`.
  Agora tudo é obtido e montado na máquina do usuário: `setup.sh` → `tools/build_all.sh` (compila as DLLs,
  `fetch_medianxl.py` baixa do manifesto oficial com MD5, `build_tools.sh` compila StormLib/merge/extract,
  `prepare_game.py` monta `build/test_game` com os MPQs base da instalação do usuário e aplica as correções
  conferidas byte a byte, `assemble_sd_tree.py` monta `dist/sd-card`). A saída do `prepare_game.py` é
  idêntica, byte a byte, à pasta validada no Ryujinx. O `assemble_sd_tree.py` não grava mais arquivos do
  jogo em `sd-card/`.

### J. 24/10/2026 — herói tremendo nas diagonais, menus pelo direcional, Sim/Não da loja, Organizar
- **Tremida/"passinhos"**: o jogo guarda a direção em 64 passos de 5,625° e arredonda para baixo a partir de 45°
  no mundo (medido com `WalkTest`, cada quadro: posição, modo, direção do caminho). As 8 direções principais do
  analógico caem exatamente na fronteira entre duas direções; com o alvo a ~5 subtiles recalculado a cada 130 ms,
  a direção alternava (31/32...) a cada passo e o sprite trocava de lado. Agora: direção do analógico suavizada,
  encaixada no **centro** de uma das 64 direções com histerese, alvo a 18 subtiles exatamente nessa direção e
  ordem renovada só quando precisa (mudou a direção, alvo perto, parou, andar↔correr com histerese). Idas-e-voltas
  de direção em linha reta, com ruído de analógico: 67 → 8 (as que restam são desvios de obstáculo).
  `[Movimento] ModoOrdem=0` volta à ordem antiga. Teste: `tools/run_walk_test_ryujinx.sh`.
- **Interação**: um toque no B termina a interação (antes era preciso segurar). O "interagir" só é mandado com o
  herói parado ao lado do alvo, e uma ação na fila cancela o "pare aqui" do analógico solto: mandados no meio do
  caminho, desfaziam a interação no servidor e o TRADE fechava o menu do NPC (5/5 depois da correção).
- **Cursor exato** (`move_cursor_exact`): o D2GL converte a janela para o jogo numa área 4:3 (pedir x=577 levava o
  mouse a 598). O D2NXPad aprende o mapa de cada eixo lendo o mouse do jogo e corrige; só aprende quando o jogo
  processou o movimento e descarta medidas incoerentes. O cursor livre dos analógicos e o AutoJoin não mudaram.
- **Direcional nos painéis**: geometria do `inventory.bin` que o jogo carrega (lido pela Storm na thread do jogo) e
  ocupação de cada casa pela grade da unidade (`Inventory+0x14`; grade 2 = inventário, 5 = cubo, 6 = baú; na loja,
  grade 2 + aba do NPC). Um alvo por item (um item 2x3 é um passo), por casa vazia, equipamentos, abas e botões; o
  passo escolhe o alvo mais perto na direção, preferindo o alinhado. Ao abrir um painel o cursor vai ao primeiro
  item. Aba da loja do Median XL: `[D2Sigma+0x33DAE8]+0x10`. Menu do NPC: retângulo em `D2Client+0x11963C`
  (esq, topo, dir, base), opções a cada 17,6 px. Achados no Ryujinx com os comandos `snap/diff/scan` do roteiro.
- **Sim/Não da compra**: a caixa (só na compra, a venda não pergunta) fica centralizada no painel da loja (Sim
  256,214; Não 256,228) e não aceita teclado (ESC fecha a loja, Enter abre o chat). O D2NXPad sabe que ela abriu
  porque o A foi num item da aba atual: cursor no Sim, direcional alterna, B = Não sem fechar a loja, e depois o
  cursor volta ao item.
- **Organizar inventário**: botão desenhado à esquerda do inventário e opção no menu de cheats, na hora. Cada passo
  é um clique do jogo (pegar/soltar) conferido na grade; maiores primeiro, por coluna, iguais juntos; se faltar
  espaço para abrir caminho, para (inventário válido); B cancela.
- **Testes**: `tools/run_menus_ryujinx.sh` (16 critérios, só botões do controle) e `tools/run_script_ryujinx.sh`
  com roteiros em `tools/roteiros/` (pad, stick, npc, goto, walkto, moveg, npcopt, dumpinv, snap/diff, scan).
- Observado e não alterado: nos primeiros ~8 s depois de entrar no jogo as teclas são ignoradas até o Enter
  atrasado do menu ser consumido (o chat abre e o D2NXPad fecha); depois disso Y/I funcionam.
- **Aceitação (`run_acceptance_ryujinx.sh`)**: a fase de segurar A no campo passou de 15 para 25 s e o log registra
  quantos monstros estão a 25 subtiles. O critério "A troca de alvo" depende de algum monstro chegar até a entrada do
  campo; numa rodada que falhou, o mais perto ficou a 47 subtiles os 25 s inteiros (o A golpeou à frente, como deve).
  Nas rodadas com monstros por perto o A achou e trocou de alvo.

### K. 27/10/2026 — seletor de habilidades (mão esquerda/direita) com cursor
- **Problema**: com o cursor oculto fora de painéis, o seletor de habilidades do jogo (painel 0x03, aberto pelos botões
  de habilidade do painel de controle) ficava sem cursor: o D2NXPad não o contava como painel, então o analógico
  seguia andando com o herói, o A atacava e era impossível escolher uma habilidade. Além disso o `+ e ZR`/`+ e ZL`
  clicava em ±117 px do centro, fora dos botões do Median XL, e o `+ e ZL` nunca chegava ao `[Jogo.Mais]` (o ZL era
  sempre tratado como camada).
- **Correção** (só `d2nxpad.c`; movimento, ações, cheats e demais painéis intactos): o painel 0x03 entra em
  `any_panel_open` → modo painel só enquanto o seletor está aberto (cursor visível, analógico move o cursor, A clica,
  B = ESC fecha); ao fechar volta ao modo partida (cursor oculto, movimento nativo). Botões medidos no Ryujinx em
  1024x768: centros em (res/2 − 261, res_y − 30) e (res/2 + 307, res_y − 30), clique com apertar/soltar em quadros
  diferentes. Ao abrir, o cursor vai ao 1º ícone do lado aberto (x 75 / res_x − 74, y res_y − 178) e o direcional
  pula na grade de ícones (passo 50,5 px). Com `+` segurado o ZL deixa de ser camada e vira o seletor da esquerda.
- **Teste**: `tools/run_skill_ryujinx.sh` (`SkillTest=1`, só botões do controle): 10/10 no Ryujinx 1.3.3 com firmware
  22.5.0 — abre dos dois lados, cursor no 1º ícone, direcional, A troca a habilidade de cada mão (lida da unidade do
  jogador), analógico move o cursor sem mover o herói, B fecha sem abrir o menu do jogo. Regressão:
  `tools/run_acceptance_ryujinx.sh`.

### L. 27/10/2026 — fluidez (travamentos) no Switch
- **Medição** (nova): `tools/medir_fluidez_ryujinx.sh <rótulo> [segundos]` entra no jogo (AutoJoin), anda (AutoWalk) e resume
  o `[PROGRESS]`/`[THREADS]` do runtime do Autorun e a telemetria `[PERF]` do D2NXPad (`tools/resumir_fluidez.py`).
  `[Geral] Perf=1` (padrão) grava no `d2nxpad.log` a cada 5 s: FPS, pior intervalo entre quadros (com nível e posição),
  quantos quadros passaram de 33/50/100 ms, o custo do gancho e, para cada quadro > 50 ms, o instante e a duração
  (`+1315:87` = 87 ms, 1,3 s depois do início da janela — mostra se os travamentos seguem um ritmo fixo). Custo: 2
  `QueryPerformanceCounter` por quadro; a gravação fica na thread do D2NXPad, nunca na do jogo.
- **O que o runtime mostrou** (Ryujinx 1.3.3, fw 22.5.0; o FPS do emulador NÃO é o do console):
  a thread do jogo fica em 95-97% de um núcleo (limite de CPU, não de GPU); `glBufferSubData` consome ~22% do tempo
  (4,5 chamadas por quadro, ~1,1 ms cada); o GL inteiro 7,5-10 ms por quadro. Nos primeiros ~20 s depois de entrar no
  jogo há ~85-100 quadros > 33 ms (pior ~1,3 s, o carregamento) e depois só 0-2 por 5 s: o travamento "frequente" é
  sobretudo o tradutor x86->ARM (Box64) traduzindo código na primeira execução e o D2GL subindo sprites novos
  (`glTexSubImage3D`); cada habilidade, monstro ou área nova repete isso em escala menor. No console o custo é maior.
- **Mudanças** (todas medidas, mesmo cenário, PTC do Ryujinx aquecido):
  1. `Game.box64.txt` com `BOX64_DYNAREC_SAFEFLAGS=0` (o Autorun apagava qualquer `Game.box64.txt`; agora mantém o que
     começa com `# D2NX-DESEMPENHO`): **54,9 FPS em regime contra 51,2** no padrão (3 rodadas contra 6) e 0,6-1,2 contra
     1,2-1,9 quadros > 33 ms por 5 s. Testados sem efeito: `BIGBLOCK=3`+`FORWARD=512` (50,5), `WEAKBARRIER=2` (50,9). 
     `ALIGNED_ATOMICS=1` derruba o jogo (aborto do Ryujinx) — não usar. Para voltar ao padrão: apagar o arquivo.
  2. D2GL sem FXAA e sem nitidez (`d2nx.ini` → `d2gl.fxaa=false`, `d2gl.sharpen=false`; o `d2nx.ini` agora vai no pacote,
     antes só existia na árvore de teste): chamadas GL por quadro 92 → 82, tempo no GL 5,3 → 4,8 ms/quadro,
     `glBufferSubData` 4,5 → 2,5 chamadas/quadro. No console isso ainda tira dois passes de tela cheia da GPU (não medível
     no Ryujinx). Voltar ao visual antigo: `d2gl.fxaa=true` / `d2gl.sharpen=true`.
  3. D2NXPad: `IsWindow` 1x/s, `GetForegroundWindow` 4x/s, mapeamento da janela 1x/s (ou ao mudar a resolução), unidades
     em volta 25x/s e laço a 60 Hz na partida (125 Hz com cursor/menus/cheats): a thread do D2NXPad caiu de ~4,4% para
     ~2% de um núcleo (`[THREADS]`, thread `20w`).
  4. `Debug=0` no `d2nxpad.ini` do pacote: com `Debug=1` ia uma linha de log a cada 2 s com `fflush` no cartão SD. Os
     testes que leem mensagens `dbg()` (aceitação, menus, caminhada, itens) agora ligam `Debug=1` na árvore de teste.
- **Sem efeito medido** (não adotado): `frame_latency` 3 e 5 do D2GL (o custo de `glBufferSubData` não mudou), `vsync` ligado/
  desligado no D2GL e no runtime, `BIGBLOCK`/`FORWARD`/`WEAKBARRIER`, `motion_prediction`/`show_fps`/preset de shader.
  O amostrador de CPU do runtime (`profile=1`) derruba o Ryujinx: só serve no console.
- **Regressão** (binários finais, Ryujinx): aceitação 17/17, seletor de habilidades 10/10, menus 16/16, criação de
  personagem 2/2, AutoWalk 102 posições distintas em 140 s. Atenção: a aceitação depende do PC estar sem carga (o herói
  ainda termina a caminhada de chegada nos primeiros segundos; o critério da zona morta agora só conta 1,5 s depois).
- **Para o próximo passo no console**: mandar `d2nxpad.log` (linhas `[PERF]`) e `logs/autorun_runtime.log`
  (`[PROGRESS]`, `[THREADS]`); com `profile=1` no `Game.wine-nx.txt` o runtime também lista os módulos x86 mais quentes.
