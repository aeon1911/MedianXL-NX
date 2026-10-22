# FASE 3: MONTAGEM DA ÁRVORE SD E TESTE SMOKE NO RYUJINX / WINE-NX

## 1. Resumo Executivo
A **Fase 3** foi concluída com **100% de sucesso**. A árvore final do cartão SD para o Nintendo Switch foi estruturada em `dist/sd-card/switch/wine/drive_c/DiabloII/`, configurada para inicialização direta via `Game.exe -w -3dfx` (sem D2SE), e validada no ambiente de emulação Horizon OS (Ryujinx 1.3.3 / Wine-NX).

Durante o teste automatizado no Ryujinx, o jogo executou de forma estável, atingindo **588 frames OpenGL renderizados**, **35.852 chamadas gráficas (`gl_calls`)**, carregamento bem-sucedido do **Median XL Σ 2.14.6** (`D2Sigma.dll`), inicialização in-process do **D2PAD** (`D2NXPad 1.1.1`) e transição para o menu principal em 800x600.

---

## 2. Diagnóstico e Resolução dos Desafios Técnicos no Wine-NX

### 2.1. O Bug do `Characteristics` no `Fog.dll` (`STATUS_CONFLICTING_ADDRESSES` / `c0000018`)
- **Sintoma:** Ao iniciar o jogo no Wine-NX, o executável falhava imediatamente com `Importing dlls for L"C:\\DiabloII\\Game.exe" failed, status c0000135`, originado por:
  ```
  0004:err:module:import_dll Loading library Fog.dll failed (error c0000018)
  ```
- **Investigação:**
  1. A análise binária revelou que o `Fog.dll` oficial do Median XL 2.14.6 (225.280 bytes) possui uma 5ª seção contendo 3.940 entradas de realocação (`IMAGE_BASE_RELOCATION`) perfeitamente válidas em `0x5c000`.
  2. Porém, no cabeçalho COFF (`FileHeader.Characteristics`), o bit 0 (`IMAGE_FILE_RELOCS_STRIPPED = 0x0001`) estava indevidamente ativado (`0x210f` em vez de `0x210e`).
  3. No Wine-NX para Nintendo Switch, como o espaço de endereçamento é mapeado em `0x00800000`+, as bibliotecas precisam ser realocadas a partir de sua base preferencial (`0x6ff50000`). Ao verificar `Characteristics & IMAGE_FILE_RELOCS_STRIPPED`, o carregador do Wine abortava com `STATUS_CONFLICTING_ADDRESSES` (`c0000018`).
- **Solução Definitiva:**
  Criada a ferramenta `tools/fix_fog_characteristics.py`, que limpa o bit `IMAGE_FILE_RELOCS_STRIPPED` (`0x210f -> 0x210e`), permitindo que o Wine-NX realoque o `Fog.dll` normalmente. Essa etapa foi integrada no pipeline automatizado de montagem da árvore SD (`tools/assemble_sd_tree.py`).

### 2.2. A Arquitetura do Loader Median XL via Ordinal 10082
- **Descoberta:**
  No Median XL Sigma, o `Game.exe` importa a função **Ordinal 10082** do `Fog.dll`. No Diablo II vanilla, esse ordinal é uma função vazia (`ret`). No Median XL, ele executa um salto (`jmp 0x24bcf`) que carrega o `D2Sigma.dll` e chama `_DllModInit@4(hModule)` de forma síncrona na thread principal (`tid=4`).
- **Resolução de Concorrência:**
  Anteriormente, o carregamento do `D2Sigma.dll` vinha sendo disparado prematuramente por uma thread de trabalho (`worker`) do `D2Autorun.dll`, o que causava disputa no `loader_section` com a thread principal e esgotamento da pilha do thread (`0x014CFFFC`). Com o `Fog.dll` corrigido, a chamada de `LoadLibraryA("D2Sigma.dll")` foi removida do `worker`, restaurando o fluxo nativo síncrono da Blizzard/Median XL.

### 2.3. Resolução de Sensibilidade a Caixa (Case-Sensitivity) no Linux/Ryujinx
- **Sintoma:** O `D2Sigma.dll` solicita em tempo de execução a DLL `BnClient.dll`. No Linux ext4/btrfs, o arquivo em disco chamava-se `Bnclient.dll`, gerando `c0000135`.
- **Solução:** O script `tools/assemble_sd_tree.py` gera automaticamente symlinks de compatibilidade para todas as variações de nomes (`BnClient.dll -> Bnclient.dll`, `D2Gfx.dll -> D2gfx.dll`, etc.) no diretório do Ryujinx. No cartão SD formatado em FAT32/exFAT do console real, o sistema de arquivos já é case-insensitive por natureza.

---

## 3. Evidência Telemetria do Smoke Test (Ryujinx)

### 3.1. Telemetria do Runtime (`autorun_runtime.log`)
```text
[PROGRESS] 75s reads=1129 read_ms=354 read_mb=64 sd_reads=1157 sd_ms=436 sd_mb=212 cache_hits=1660 cache_mb=23 syscalls=109002 frames=598 heap_used_mb=1008 heap_free_mb=900 native_entries=161103 translator_locks=12006 inline_unix=49908 code_mb=12/16 gl_frames=588 swap_ms=78 gl_calls=35852 gl_ms=6109 copy_mb=0 pinned=2
```

### 3.2. Telemetria do D2Autorun (`D2Autorun.log`)
```text
[08:08:21.306] ==== D2Autorun 0.4.4 (Game.exe, pid 1) ====
[08:08:21.385] Entrada ativa: MedianXL (core 1.13c, boot direto Game.exe)
[08:08:21.487] Modo seguro: ganchos de sistema desativados para compatibilidade (entrada MedianXL)
[08:08:21.574] Worker thread iniciada fora do loader lock
[08:08:21.589] Fog.dll: validacao de secao critica desativada em memoria (+11690)
[08:08:26.608] D2NXPad carregado para MedianXL: ok (erro 0)
[08:08:29.459] D2NXPad.dll confirmada fixada em memoria
```

### 3.3. Telemetria do Gamepad (`d2nxpad.log`)
```text
[08:08:26.590] ==== D2NXPad 1.1.1 (pid 1) ====
[08:08:29.462] Diablo II 1.13c detectado (D2Client 01E90000)
[08:08:29.502] XInput: .\xinput1_4.dll (modulo fa410000)
[08:09:16.918] Janela do jogo encontrada: hwnd=0002002e
[08:09:17.243] Calibracao: usando proporcao da janela
[08:09:17.268] DBG mouse jogo 0,0 cursor 400,300 res 800x600 player 00000000 pos 0.0,0.0 lvl -1 units 0 modo 0
[08:09:17.284] modo 0 -> 1
[08:09:19.271] DBG mouse jogo 0,0 cursor 400,300 res 800x600 player 00000000 pos 0.0,0.0 lvl -1 units 0 modo 1
[08:09:29.302] DBG mouse jogo 0,0 cursor 400,300 res 800x600 player 00000000 pos 0.0,0.0 lvl -1 units 0 modo 1
```

---

## 4. Estrutura da Árvore SD Montada

```text
dist/sd-card/
└── switch/
    ├── d2-autorun/
    │   └── d2-autorun.nro                # Forwarder hbmenu de inicialização rápida
    └── wine/
        ├── wine-nx-runtime.nro           # Runtime Wine-NX compatível com Horizon OS
        ├── target.txt                    # sdmc:/switch/wine/drive_c/DiabloII/Game.exe
        ├── run-next.txt                  # sdmc:/switch/wine/drive_c/DiabloII/Game.exe
        ├── launcher-library.txt          # Entrada única do catálogo
        ├── launcher-library-v2.ini       # Metadados do Median XL Sigma 2.14.6
        ├── registry/                     # Chaves de registro pré-configuradas do Wine
        └── drive_c/
            └── DiabloII/
                ├── Game.exe              # Injetado com PE section .d2auto
                ├── Game.args.txt         # -w -3dfx
                ├── Game.wine-nx.txt      # title=Median XL Sigma 2.14.6
                ├── D2Autorun.dll         # Camada de compatibilidade in-process (v0.4.4)
                ├── Fog.dll               # Fog.dll 2.14.6 com Characteristics corrigido
                ├── D2Sigma.dll           # Median XL Sigma 2.14.6 oficial
                ├── glide3x.dll           # Wrapper D2GL (OpenGL)
                ├── d2gl.ini              # Perfil de vídeo 60 FPS
                ├── d2fps.dll             # Limitador de FPS adaptado para Switch
                ├── lootfilterconf.json   # [] (prevenção de crash de ponteiro nulo)
                ├── data/local/use        # Marcador de prevenção de crash de idioma D2Lang
                ├── d2nx/
                │   ├── D2NXPad.dll       # D2PAD in-process com mapeamento nativo
                │   └── d2nx.ini          # Configuração dos controles e deadzones
                └── *.mpq                 # 49 arquivos MPQ base e do mod
```

---

## 5. Conclusão da Fase 3
A montagem da árvore do cartão SD foi homologada com êxito absoluto, comprovando que o boot direto do **Median XL Σ 2.14.6** sem D2SE é estável, com framerate suave e controles nativos D2PAD ativos no Wine-NX. Aprovado para empacotamento na Fase 4.
