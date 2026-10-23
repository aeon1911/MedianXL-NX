# MANUAL DE INSTALAÇÃO E USO — MEDIAN XL Σ 2.14.6 NO NINTENDO SWITCH (AUTORUN)

Este projeto monta o port de **Median XL Σ 2.14.6** para o Nintendo Switch rodando sobre a camada de compatibilidade **AUTORUN (Wine-NX)** no Horizon OS, com integração total de controle analógico via **D2PAD** in-process e inicialização direta sem qualquer dependência do D2SE.

---

## 1. Requisitos do Console
- **Console:** Nintendo Switch (V1, V2, Lite ou OLED) desbloqueado.
- **Ambiente CFW:** Atmosphère atualizado.
- **Sistema Operacional:** Horizon OS oficial.
- **Modo de Acesso:** Homebrew Menu (hbmenu). Recomendado iniciar o hbmenu através do modo **Title Override** (segurando o botão `R` ao abrir qualquer jogo comercial instalado) para garantir acesso total aos 4 GB de memória RAM do Switch.
- **Formatação do Cartão SD:** FAT32 (recomendado para máxima estabilidade) ou exFAT.
- **Espaço Livre Necessário:** ~3.0 GB livres no cartão MicroSD.

---

## 2. Gerar os arquivos do cartão SD (no PC)

Este repositório **não contém arquivos da Blizzard nem do Median XL** (direitos autorais). Eles são montados na
sua máquina pelo `setup.sh`, a partir da sua instalação do Diablo II: Lord of Destruction e do download oficial
do Median XL:

```bash
./setup.sh "/caminho/do/seu/Diablo II"          # gera dist/sd-card/
./setup.sh "/caminho/do/seu/Diablo II" --zip    # gera também dist/d2-medianxl-autorun-switch-2.14.6-final.zip
```

- **Da sua instalação** saem só os 10 MPQs base: `d2char`, `d2data`, `d2exp`, `d2music`, `d2sfx`, `d2speech`,
  `d2video`, `d2xmusic`, `d2xtalk`, `d2xvideo` (`.mpq`, maiúsculas ou minúsculas). Instalação do CD: copie também
  dos discos os MPQs de música/vídeo que o instalador deixou no CD.
- **Do Median XL** (`tools/fetch_medianxl.py`, manifesto oficial `launcher.median-xl.com`, MD5 conferido):
  `medianxl-*.mpq`, `D2Sigma.dll`, `Fog.dll`, `Game.exe` e DLLs 1.13c, `Storm.dll`, D2GL (`glide3x.dll`,
  `d2gl.mpq`) e `patch_d2.mpq`.
- **Ferramentas:** `python3`, `i686-w64-mingw32-gcc`, `g++`, `cmake`, `make`, `git`, zlib e bzip2. Se faltar
  alguma, o `setup.sh` usa o Docker (`tools/Dockerfile`).
- **O que é alterado (na sua máquina, conferido byte a byte):** `Game.exe` ganha a seção `.d2auto` que carrega o
  `D2Autorun.dll`; `Fog.dll` tem o bit `IMAGE_FILE_RELOCS_STRIPPED` corrigido; `D2Sigma.dll` ganha um teste de
  ponteiro nulo em `0x1005FC77`; os 56 `medianxl-*.mpq` viram um `medianxl-data.mpq` (cópia dos blocos
  comprimidos, sem recompressão — boot mais rápido no cartão).

Os passos, um por um, estão em `tools/build_all.sh` e no `README.md`.

## 3. Instalação no Cartão SD

1. Conecte o cartão MicroSD ao computador (leitor de cartão ou FTP/DBI MTP Responder).
2. Copie a pasta `dist/sd-card/switch` para a **raiz** do cartão (`sdmc:/switch`), mesclando com a `switch/`
   existente. Se gerou o zip, extraia-o na raiz do cartão.
3. Uma versão anterior com o `medianxl-data.mpq` antigo deve ser **substituída**: aquele MPQ tinha os sons do
   Median XL recomprimidos em zlib e fechava o jogo ("The file data is corrupt") ao tocar alguns sons, por
   exemplo ao usar o item de 1.500 de ouro do kit inicial.
4. Verifique se a estrutura no cartão SD ficou exatamente desta forma:
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
                   ├── d2gl.mpq
                   ├── medianxl-data.mpq
                   ├── lootfilterconf.json
                   ├── data/local/use
                   ├── d2nx/
                   │   ├── D2NXPad.dll
                   │   └── d2nxpad.ini
                   └── *.mpq
   ```
5. Ejete com segurança o cartão SD do computador e insira-o novamente no Nintendo Switch.

---

## 4. Como Iniciar o Jogo

1. No Nintendo Switch, abra o **Homebrew Menu** (segure `R` em qualquer jogo da tela inicial).
2. Na lista de aplicativos, selecione o ícone **D2 AUTORUN (Median XL Sigma)** ou **Wine-NX**.
3. O launcher detectará automaticamente a configuração `sdmc:/switch/wine/drive_c/DiabloII/Game.exe` e iniciará o executável com as flags `-3dfx -direct -log`.
4. Aguarde a inicialização da tela inicial (o `D2Autorun` calibra os ganchos do `Fog.dll` e carrega o `D2Sigma.dll` de forma síncrona e segura).
5. O menu principal do Median XL Sigma 2.14.6 aparecerá em resolução 800x600 nativa a 60 quadros por segundo.

---

## 5. Mapeamento de Controles do D2NXPad

O movimento e as ações do herói usam a **rotina de comando do próprio jogo** (camada nativa, ver `RULES.md`):
nada de mouse, cursor ou clique simulado. Dentro do jogo o cursor fica oculto enquanto nenhum painel está aberto.

| Botão / Alavanca | Ação no Diablo II / Median XL |
|---|---|
| **Analógico esquerdo** | Movimento direto na direção apontada. Inclinação leve anda, inclinação forte corre; soltar para o herói. Zona morta em `[Movimento] ZonaMortaEsquerda`, ponto de correr em `IntensidadeCorrer`. |
| **Analógico direito** | Direção de mira para atacar/lançar sem alvo. |
| **A** | Ataque (habilidade esquerda) no monstro mais próximo detectado na tela; segurando, repete a cada golpe e troca de alvo quando ele morre. Sem monstro (fora da cidade), golpeia à frente. |
| **X** | Habilidade direita no monstro detectado (ou à frente). |
| **B** | Interagir: vai até o NPC, baú, portal ou item mais próximo e interage. |
| **Y** | Inventário (`I`). |
| **L** (segurar) | Mostrar itens no chão (`Alt`). |
| **R / ZR** | Habilidades 4 / 5 (tecla F4/F5 + habilidade direita no alvo). |
| **ZL** (segurar) | Camada: Y/A/X = habilidades 6/7/8, B = ataque, D-pad = Shift+1..4. |
| **Plus (+)** | Toque: menu do jogo (`ESC`). Segurando: atalhos (Y inventário, X personagem, A habilidades, B missões...). |
| **Minus (−)** | Automapa (`Tab`). |
| **ZL + − (ou − + +)** | **Menu de cheats** (XP, runas, únicos, raros). Direcional escolhe/altera, **A** aplica, **B** fecha. Vale a partir do próximo boot do jogo. |
| **R3** | Trocar armas (`W`). |
| **D-Pad** | Cinto: poções 1–4. |

### Fora da partida (menu principal, seleção e criação de personagem)

| Botão | Ação |
|---|---|
| **A** | Clique (confirmar / escolher classe) |
| **B** | Voltar (`ESC`) |
| **X / Plus (+)** | `Enter` (confirmar o nome e criar o personagem) |
| **R3, L3 ou Minus (−)** | Abre o **editor de nome**: escreve um nome pronto na caixa "Character Name" |

Com o editor de nome aberto (o texto aparece na própria caixa do jogo):

| Botão | Ação |
|---|---|
| **↑ / ↓** | Avança / volta a última letra (segure para correr o alfabeto) |
| **→** | Acrescenta uma letra |
| **←** | Apaga a última letra |
| **Y** | Sorteia outro nome |
| **+ ou X** | Cria o personagem (`Enter`) |
| **A, B ou R3** | Fecha o editor e mantém o nome |

O teclado flutuante do Autorun (atalho `− + R3`) abre mas **não aparece** na tela com o vídeo D2GL; evite esse atalho no Median XL.

---

## 6. Salvamento, Desempenho e Dicas

- **Gravação de Savegames:** o Median XL Σ grava os personagens na pasta do usuário do Wine, não em `DiabloII/save`:
  ```text
  sdmc:/switch/wine/drive_c/users/steamuser/AppData/Roaming/MedianXL/save/
  ```
  Você pode fazer backup ou transferir personagens entre o PC e o Nintendo Switch copiando os arquivos `.d2s` (com `.key`, `.ma0`, `.map`). O `d2nx/d2nxpad.log` lista, a cada boot, cada save com tamanho e se o cabeçalho está íntegro.
- **"Generic bad character file":** tinha três causas, todas corrigidas no `D2Autorun`/`D2NXPad`. (1) o Wine do Autorun entrega memória acima de 2 GB a programas 32-bit e o Diablo II 1.13c não foi feito para isso: na criação do personagem o jogo parava. O `D2Autorun` agora mantém o jogo abaixo de 2 GB, como no Windows. (2) O Median XL não cria a pasta de saves acima; o `D2Autorun` a cria no boot. (3) Versões antigas do menu de gameplay gravavam `data/global/excel/charstats.bin` com ouro inicial; o arquivo é apagado no boot e a opção "Ouro inicial" ficou indisponível. Personagens criados antes desta versão podem continuar recusados: apague-os na pasta acima e crie de novo.
- **Cursor:** dentro do jogo o cursor fica oculto enquanto nenhum painel estiver aberto (o herói anda pelo analógico e ataca o alvo mais próximo). Ele aparece no inventário, habilidades, personagem, baús, lojas e no menu do `+`.
- **Caixa de chat:** se ela abrir sozinha logo depois de entrar no jogo (um Enter atrasado do menu), o D2NXPad a fecha na hora — com o chat aberto o herói não anda, e era isso o "anda dois passos e congela".
- **Prevenção de Falhas do Loot Filter:** O arquivo `lootfilterconf.json` incluído na raiz vem configurado com uma lista válida `[]`. Caso decida customizar suas regras de filtro de itens, assegure-se de manter o formato JSON sintaticamente válido.
- **Configuração de Vídeo (D2GL):** O mod está configurado com o wrapper Glide `glide3x.dll` acelerado por hardware via OpenGL, proporcionando taxas estáveis de 60 FPS com renderização suave e sem cortes de áudio.
- **Para Sair do Jogo:** Pressione `Plus (+)` para abrir o menu do jogo, selecione **Save and Exit**, e feche o aplicativo normalmente pelo botão Home do Switch.
