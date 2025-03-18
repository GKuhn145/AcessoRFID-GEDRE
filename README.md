# AcessoRFID-GEDRE

Implementação de controle de acesso com RFID para o laboratório do GEDRE

## Descrição do Projeto

### Recursos

- Acesso com cartão RFID à porta do GEDRE
- Planilha de controle de membros com acesso no Drive do GEDRE
- Cartões-mestres de cadastro e descadastro de outros cartões
- Tabela local no leitor, atualizada diariamente ou a cada entrada com a planilha do Drive
  
### Limitações

- Sem controle de horários de entradas e saídas
- Sem interface na instalação para interagir
- Sem vínculo automático de código RFID e portador do cartão, cadastro manual na planilha

### Componentes

- Esp32, com acesso à rede do GEDRE
- Leitor RFID RC522
- Leds indicadores (verde e vermelho), para confirmação e negação de entrada, ou representação de outros estados

## Fluxograma de Controle de Acesso

![fluxograma acesso](/Assets/Fluxograma%20de%20Controle%20de%20Acesso.svg)

## Fluxograma de Cadastro e Descadastro

![fluxograma cad-descad](/Assets/Fluxograma%20de%20Cadastro%20e%20Descadastro.svg)

## Roadmap

### O que foi feito

- Padronização do formato dos bytes dos cartões durante leitura
- Adição de tags por meio de tags mestre de cadastro e descadastro
- Feedback de sucesso e falha
- Sistema de arquivos locais no ESP32, usando o LittleFS
  
### O que falta ser feito

- Conectar com o Google Drive
  - Quando?
    1. Ao cadastrar e descadastrar usuários
    2. Periodicamente: De hora em hora? Diariamente?
    3. A cada X leituras
    4. Quando a planilha mudar (request da planilha para o ESP, se possível)
  - Como?
    1. Conectar ao Wifi local do GEDRE
    2. Criar um HTTPServer, para enviar requests ao Drive
    3. Expor endpoints na planilha
  - O que terá na planilha?
    1. Registro de tags de entrada, associadas à um nome(opcional), data de cadastro, etc...
    2. Aba com tags-mestre, para edição off-board
    3. Aba com variáveis de controle: Debug Mode, Buzzer OFF, LEDs OFF, etc...
    4. Logs?

- Planilha online no Drive com os dados de acesso
  - Fazer o script de recebimento de dados vindos do ESP32, reforçar a segurança
  - Implementar o sistema de timeout para evitar atualizações desnecessárias

- Terminal para os componentes
  - Projetar e imprimir invólucro para o leitor de cartões do lado externo do laboratório
  - Projetar e imprimir invólucro para botão interno de saída
  - Analisar e reforçar as conexões existentes do sistema da porta
