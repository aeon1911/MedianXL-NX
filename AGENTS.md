# Regras de Desenvolvimento do Projeto Diablo II LoD Switch Port

1. **Validação Obrigatória em Emulador Antes de Qualquer Envio para Console Físico:**
   - Nenhuma modificação, patch, DLL, configuração ou pacote de release deve ser entregue ao usuário sem validação prévia comprovada no emulador (Ryujinx).
   - Testar não apenas a tela inicial, mas a inicialização completa e a jogabilidade (renderização de frames, carregamento do ato/cenário, controle de personagem via D2NXPad/AutoJoin/AutoWalk).
   - Proibido "chutar" causas ou tentar adivinhações cegas. Toda alteração precisa ser embasada em evidências de logs, depuração e comportamento de I/O / runtime.

2. **Tempo de Inicialização Rápido e Direto (Zero Espera Inaceitável):**
   - É inaceitável o usuário ter que esperar de 2 a 5 minutos a cada boot.
   - O jogo deve passar pelo carregamento do autorun e entrar diretamente no jogo sem telas pretas eternas ou esperas prolongadas.
   - O carregamento de dados (MPQs/DLLs) deve ser otimizado para a velocidade e latência de I/O do cartão MicroSD (FAT32) do Switch.

3. **Integridade e Conformidade Legal:**
   - Nenhum arquivo comercial proprietário da Blizzard deve ser versionado ou commitado no repositório Git.
