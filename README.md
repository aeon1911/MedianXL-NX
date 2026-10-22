# Median XL Σ 2.14.6 — Nintendo Switch Port (AUTORUN)

Port completo de **Median XL Σ 2.14.6** para o **Nintendo Switch** rodando diretamente sobre o Horizon OS (Atmosphère CFW) através do runtime de compatibilidade **AUTORUN (Wine-NX)**, com integração total de controle analógico **D2PAD** in-process, renderização OpenGL via **D2GL** e 60 FPS cravados.

---

## 🎮 Destaques do Port

* **Execução Direta no Horizon OS:** Não requer Linux (L4T Ubuntu) nem dual-boot. Inicia diretamente pelo **Homebrew Menu (hbmenu)** do Atmosphère.
* **Controles Nativos D2PAD In-Process:** Movimentação analógica direta do herói na alavanca esquerda, cursor virtual do mouse na alavanca direita, atalhos rápidos para poções, inventário, habilidades e mira parada.
* **Renderização D2GL a 60 FPS:** Glide/OpenGL com interpolação de movimento (`motion_prediction=true`), resolução nativa widescreen sem distorção e sem travamentos.
* **Correções Dinâmicas de Kernel/Wine:** Patch em memória na `Fog.dll` para evitar crashes de seção crítica no Wine-NX, virtualização de tokens de privilégio (`OpenProcessToken`) e proteção de páginas de memória para o motor do `D2Sigma.dll`.
* **Zero D2SE / Inicialização Rápida:** O jogo carrega o `Game.exe` e o `D2Sigma.dll` de forma síncrona e direta em poucos segundos.

---

## 🕹️ Mapeamento de Controles (D2PAD)

O driver `D2NXPad` intercepta chamadas de controle diretamente no processo do jogo:

| Botão / Alavanca | Ação no Jogo | Comportamento |
|---|---|---|
| **Alavanca Esquerda (L-Stick)** | Movimentação Analógica | O herói corre diretamente na direção inclinada do analógico. |
| **Alavanca Direita (R-Stick)** | Cursor do Mouse Virtual | Movimentação livre do ponteiro na tela para menus, mira e inventário. |
| **Botão A** | Botão Esquerdo do Mouse | Interagir, atacar, coletar itens do chão. |
| **Botão B** | Botão Direito do Mouse | Habilidade secundária / Magia ativa. |
| **Botão X** | Mirar Parado (Shift) | Mantém o herói no lugar disparando magias ou projéteis sem andar. |
| **Botão Y** | Exibir Itens no Chão (Alt) | Liga/desliga a visualização de drops no chão. |
| **Gatilho ZL** | Poção de Vida | Consome o slot 1 do cinto. |
| **Gatilho ZR** | Poção de Mana | Consome o slot 2 do cinto. |
| **Botão L** | Abrir Inventário (`I`) | Abre/fecha a mochila. |
| **Botão R** | Árvore de Habilidades (`T`) | Abre a tela de Skills. |
| **D-Pad Cima** | Slot 3 do Cinto | Poção de Rejuvenescimento. |
| **D-Pad Baixo** | Slot 4 do Cinto | Portal da Cidade (TP). |
| **D-Pad Esquerda / Direita** | Trocar Armas (`W`) | Alterna entre o conjunto primário e secundário de armas. |
| **Botão Plus (+)** | Menu de Pausa (`ESC`) | Menu de opções, salvar e sair. |
| **Botão Minus (-)** | Automap (`Tab`) | Mapa translúcido na tela. |
| **Clique R-Stick (R3)** | Centralizar Cursor | Reposiciona o ponteiro do mouse sobre o personagem. |

---

## 📦 Estrutura do Repositório

```text
├── sd-card/                  # Árvore pronta para extrair no MicroSD do Switch
│   └── switch/
│       ├── d2-autorun/       # Launcher NRO para o Homebrew Menu
│       └── wine/             # Runtime Wine-NX configurado para Horizon OS
│           └── drive_c/
│               └── DiabloII/ # Mod Median XL 2.14.6, D2GL, D2Autorun, D2NXPad
├── src-d2nx/                 # Código-fonte C dos módulos nativos (D2Autorun + D2NXPad)
│   ├── d2autorun.c
│   ├── d2nxpad.c
│   ├── hook.h
│   └── build.sh
├── tools/                    # Ferramentas de automação e injeção
│   ├── assemble_github_tree.py
│   ├── install_lod_files.py
│   ├── inject_pe_import.py
│   ├── fix_fog_characteristics.py
│   └── package_release.py
├── docs/                     # Documentação de arquitetura, testes e evidências
├── INSTALL.md                # Manual completo de instalação
└── README.md
```

---

## 🚀 Como Instalar no Nintendo Switch

### 1. Requisitos
* Nintendo Switch desbloqueado com CFW **Atmosphère**.
* Cartão MicroSD formatado em **FAT32** (recomendado) ou exFAT com ~3 GB livres.
* Instalação original do **Diablo II: Lord of Destruction (v1.13c)**.

### 2. Preparando os Arquivos LoD (Automático via PC)
Por questões legais e de direitos autorais da Blizzard Entertainment, **este repositório não inclui os arquivos proprietários comerciais do Diablo II LoD original**.

1. Clone ou baixe este repositório.
2. Execute o assistente informando a pasta onde seu Diablo II LoD 1.13c está instalado no PC:
   ```bash
   python3 tools/install_lod_files.py "/caminho/para/seu/Diablo II LoD 1.13c"
   ```
   *O utilitário copiará os MPQs e DLLs da Blizzard com os nomes canônicos e injetará a seção `.d2auto` no seu `Game.exe` automaticamente.*

3. Copie a pasta **`sd-card/switch/`** para a pasta **`switch/`** do seu cartão MicroSD.

### 3. Como Jogar
1. No Switch, entre no **Homebrew Menu** segurando `R` ao abrir qualquer jogo comercial (Modo *Title Override*, garantindo acesso a todos os 4 GB de RAM).
2. Selecione **D2 AUTORUN (Median XL Sigma)**.
3. Bom jogo! Seus saves de Single Player serão gravados automaticamente em `sdmc:/switch/wine/drive_c/DiabloII/save/`.

---

## ⚖️ Aviso Legal / Disclaimer

* **Diablo II** e **Diablo II: Lord of Destruction** são marcas registradas da **Blizzard Entertainment, Inc.** Este projeto é um trabalho derivado de fãs sem fins lucrativos e **NÃO** distribui arquivos proprietários, músicas, vídeos ou código binário protegido da Blizzard.
* **Median XL** é desenvolvido e mantido pela **Equipe Median XL** ([median-xl.com](https://www.median-xl.com/)).
* O runtime **Wine-NX** e a camada **AUTORUN** utilizam código sob as licenças LGPL / MIT.
