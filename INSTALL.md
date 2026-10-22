# MANUAL DE INSTALAÇÃO E USO — MEDIAN XL Σ 2.14.6 NO NINTENDO SWITCH (AUTORUN)

Este pacote contém o port completo e pronto para jogar de **Median XL Σ 2.14.6** para o Nintendo Switch rodando sobre a camada de compatibilidade **AUTORUN (Wine-NX)** no Horizon OS, com integração total de controle analógico via **D2PAD** in-process e inicialização direta sem qualquer dependência do D2SE.

---

## 1. Requisitos do Console
- **Console:** Nintendo Switch (V1, V2, Lite ou OLED) desbloqueado.
- **Ambiente CFW:** Atmosphère atualizado.
- **Sistema Operacional:** Horizon OS oficial.
- **Modo de Acesso:** Homebrew Menu (hbmenu). Recomendado iniciar o hbmenu através do modo **Title Override** (segurando o botão `R` ao abrir qualquer jogo comercial instalado) para garantir acesso total aos 4 GB de memória RAM do Switch.
- **Formatação do Cartão SD:** FAT32 (recomendado para máxima estabilidade) ou exFAT.
- **Espaço Livre Necessário:** ~3.0 GB livres no cartão MicroSD.

---

## 2. Instruções de Instalação no Cartão SD

1. Conecte o cartão MicroSD do Nintendo Switch ao computador (via leitor de cartão ou FTP/DBI MTP Responder).
2. Localize o arquivo de entrega final gerado pelo projeto:
   ```text
   dist/d2-medianxl-autorun-switch-2.14.6-final.zip
   ```
3. Extraia todo o conteúdo do arquivo ZIP diretamente na **raiz** do seu cartão SD (`sdmc:/`).
4. Se solicitado para mesclar pastas existentes (`switch/`), confirme a substituição/mesclagem.
5. Verifique se a estrutura no cartão SD ficou exatamente desta forma:
   ```text
   sdmc:/
   └── switch/
       ├── d2-autorun/
       │   └── d2-autorun.nro
       └── wine/
           ├── wine-nx-runtime.nro
           ├── run-next.txt
           ├── target.txt
           ├── launcher-library.txt
           ├── launcher-library-v2.ini
           ├── registry/
           └── drive_c/
               └── DiabloII/
                   ├── Game.exe
                   ├── Game.args.txt
                   ├── Game.wine-nx.txt
                   ├── D2Autorun.dll
                   ├── Fog.dll
                   ├── D2Sigma.dll
                   ├── glide3x.dll
                   ├── d2gl.ini
                   ├── d2fps.dll
                   ├── lootfilterconf.json
                   ├── data/local/use
                   ├── d2nx/
                   │   ├── D2NXPad.dll
                   │   └── d2nxpad.ini
                   └── *.mpq
   ```
6. Ejete com segurança o cartão SD do computador e insira-o novamente no Nintendo Switch.

---

## 3. Como Iniciar o Jogo

1. No Nintendo Switch, abra o **Homebrew Menu** (segure `R` em qualquer jogo da tela inicial).
2. Na lista de aplicativos, selecione o ícone **D2 AUTORUN (Median XL Sigma)** ou **Wine-NX**.
3. O launcher detectará automaticamente a configuração `sdmc:/switch/wine/drive_c/DiabloII/Game.exe` e iniciará o executável com as flags `-w -3dfx`.
4. Aguarde a inicialização da tela inicial (o `D2Autorun` calibra os ganchos do `Fog.dll` e carrega o `D2Sigma.dll` de forma síncrona e segura).
5. O menu principal do Median XL Sigma 2.14.6 aparecerá em resolução 800x600 nativa a 60 quadros por segundo.

---

## 4. Mapeamento de Controles do D2PAD

O controle do jogo foi desenvolvido pelo usuário através do **D2PAD** (`src-d2nx/d2nxpad.c`), operando de forma 100% in-process via chamadas XInput nativas mapeadas no driver HID do Switch:

| Botão / Alavanca | Ação no Diablo II / Median XL | Função / Comportamento |
|---|---|---|
| **Alavanca Esquerda (L-Stick)** | Movimentação Analógica Direta | O herói corre e se desloca na direção exata do analógico. |
| **Alavanca Direita (R-Stick)** | Cursor do Mouse Virtual | Movimenta o ponteiro livremente na tela para menus e mira. |
| **Botão A** | Clique Esquerdo do Mouse | Ataque principal, interagir com NPCs, coletar itens do chão. |
| **Botão B** | Clique Direito do Mouse | Ataque secundário / Habilidade ativa selecionada com botão direito. |
| **Botão X** | Mirar Parado (Shift) / Skill 1 | Mantém o herói fixo disparando magias ou ataques sem andar. |
| **Botão Y** | Exibir Itens (Alt / Show Items) | Liga/desliga a exibição de nomes dos itens caídos no chão. |
| **Gatilho ZL** | Poção de Vida (Cinto Slot 1) | Consome instantaneamente a poção de cura do slot 1 do cinto. |
| **Gatilho ZR** | Poção de Mana (Cinto Slot 2) | Consome instantaneamente a poção de mana do slot 2 do cinto. |
| **Botão L** | Abrir Inventário (`I`) | Alterna abertura e fechamento da mochila/inventário. |
| **Botão R** | Árvore de Habilidades (`T`) | Alterna abertura da tela de Skills para alocar pontos. |
| **D-Pad Cima** | Slot 3 do Cinto | Poção de Rejuvenescimento ou slot 3. |
| **D-Pad Baixo** | Slot 4 do Cinto | Poção de Portal da Cidade (TP) ou slot 4. |
| **D-Pad Esquerda / Direita** | Troca Rápida de Armas (`W`) | Alterna entre o conjunto primário e secundário de armas/escudos. |
| **Botão Plus (+)** | Menu de Pausa / Opções (`ESC`) | Abre o menu de configurações, salvar e sair do jogo. |
| **Botão Minus (-)** | Automap (`Tab`) | Alterna a exibição do mapa de navegação translúcido na tela. |
| **Clique R-Stick (R3)** | Centralizar Cursor | Reposiciona instantaneamente o ponteiro do mouse sobre o personagem. |

---

## 5. Salvamento, Desempenho e Dicas

- **Gravação de Savegames:** Seus personagens de Single Player são salvos automaticamente na pasta:
  ```text
  sdmc:/switch/wine/drive_c/DiabloII/save/
  ```
  Você pode fazer backup ou transferir personagens entre o PC e o Nintendo Switch copiando os arquivos `.d2s`.
- **Prevenção de Falhas do Loot Filter:** O arquivo `lootfilterconf.json` incluído na raiz vem configurado com uma lista válida `[]`. Caso decida customizar suas regras de filtro de itens, assegure-se de manter o formato JSON sintaticamente válido.
- **Configuração de Vídeo (D2GL):** O mod está configurado com o wrapper Glide `glide3x.dll` acelerado por hardware via OpenGL, proporcionando taxas estáveis de 60 FPS com renderização suave e sem cortes de áudio.
- **Para Sair do Jogo:** Pressione `Plus (+)` para abrir o menu do jogo, selecione **Save and Exit**, e feche o aplicativo normalmente pelo botão Home do Switch.
