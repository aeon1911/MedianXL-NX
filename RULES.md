# RULES — regras permanentes do projeto

Complementam o `AGENTS.md` (validação no emulador, nada de "chutar" causas, boot rápido, nenhum arquivo da
Blizzard no git). Valem para toda alteração futura; a implementação tem de obedecer a elas, não só citá-las.

## NATIVE ANALOG MOVEMENT — NON-NEGOTIABLE REQUIREMENT

- É estritamente proibido utilizar emulação de mouse para controlar a movimentação do personagem.
- É estritamente proibido converter o eixo do analógico em movimentos de cursor, eventos de mouse, cliques,
  deslocamentos de ponteiro ou comandos equivalentes.
- A movimentação deve ser implementada por meio de uma camada de controle analógico nativo integrada à lógica
  do personagem e ao sistema de movimentação da engine.
- Os botões físicos A, X e Y devem possuir ações individuais, distintas e explicitamente mapeadas.
- Nenhum botão pode provocar movimentação involuntária, ataques repetidos, alternância de ações ou
  comportamento de pressionamento intermitente (flickering).
- O menu de cheats deve existir como interface funcional, visível quando aberto e acessível durante a execução
  do jogo.
- Nenhuma entrega pode ser considerada concluída enquanto os testes de aceitação desta ordem não forem
  satisfeitos.

### Como isso se traduz no Diablo II 1.13c (verificado no binário do D2Client)

- O "sistema de movimentação da engine" é a rotina de comando do jogador do D2Client
  (`D2Client+0x2A7C0`): recebe a unidade do jogador, um comando e o destino em coordenadas do mundo,
  executa a ação localmente (caminho, colisão, animação — `D2Client+0x2A750`) e envia o comando ao servidor
  (`D2Client+0x29FA0` → `D2Client+0x143E0`). É a mesma rotina para onde um clique termina; chamá-la direto
  não passa por cursor, mouse nem clique.
- Comandos usados: 1 andar até (x,y), 3 correr até (x,y), 5/0x0C habilidade esquerda/direita em (x,y),
  6/0x0D habilidade esquerda/direita na unidade (tipo,id), 4 correr até a unidade, 0x13 interagir.
- Essas chamadas só podem ser feitas na thread do jogo (gancho por quadro), nunca da thread do D2NXPad.
- Mouse/cursor só existem em menus e painéis (inventário, baú, loja, habilidades, menu do jogo), onde o
  cursor é visível e é a interface do próprio jogo.
- "Funcionou" exige evidência no emulador: posição do herói mudando com o analógico, capturas da janela do
  Ryujinx, logs. Um teste que aprova o herói parado é um teste errado.
