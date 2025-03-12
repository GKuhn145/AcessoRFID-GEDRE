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

## Fluxograma de controle de acesso

<img src="Assets/fluxograma AcessoRFID GEDRE.svg" height="500">

## Roadmap

### O que foi feito

- Padronização do formato dos bytes dos cartões durante leitura
- Adição de cartões/tags mestre de cadastro e descadastro
- Feedback de sucesso e falha
  
### O que falta ser feito

- Sistema de arquivos no ESP32 para controle de acesso
  - Usar a biblioteca LittleFS para guardar internamente os dados de acesso de cada cartão
  - Sincronizar periodicamente com o banco de dados na planilha do Drive

- Planilha online no Drive com os dados de acesso
  - Fazer o script de recebimento de dados vindos do ESP32, reforçar a segurança
  - Implementar o sistema de timeout para evitar atualizações desnecessárias

- Terminal para os componentes
  - Projetar e imprimir invólucro para o leitor de cartões do lado externo do laboratório
  - Projetar e imprimir invólucro para botão interno de saída
  - Analisar e reforçar as conexões existes do sistema da porta
