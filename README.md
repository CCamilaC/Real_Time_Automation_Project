# Monitoramento Ferroviário em Tempo Real - ELT127

Projeto final da disciplina **Automação em Tempo Real (ELT127)** - UFMG - 2025/1  
Prof. Luiz Themystokliz S. Mendes

## Descrição do Projeto

Este sistema simula um ambiente de **monitoramento ferroviário em tempo real**, com foco em **segurança operacional** por meio da **detecção de rodas quentes** e do **monitoramento da sinalização ferroviária**.  

Sensores distribuídos ao longo da via enviam dados de temperatura e estado da sinalização para CLPs (Controladores Lógicos Programáveis), os quais são lidos e processados por uma aplicação multithread em C/C++ desenvolvida com a API Win32.

##  Funcionalidade Geral

A aplicação é composta por múltiplas **threads** e **processos**, que se comunicam via **buffers circulares, arquivos e IPC** (pipes nomeados). Cada tarefa é responsável por uma parte do sistema:

### Tarefas implementadas:

1. **Leitura dos CLPs**
   - Gera mensagens simuladas de sinalização (intervalo aleatório entre 100 e 2000 ms) e hotboxes (fixo de 500 ms).
   - Armazena em buffer circular na RAM.
   - Bloqueia se o buffer estiver cheio.

2. **Captura de Dados da Sinalização**
   - Lê o buffer e envia mensagens com falha de hardware para a visualização de rodas quentes.
   - Outras mensagens são salvas em arquivo circular no disco (200 posições).
   - Bloqueia se o arquivo estiver cheio.

3. **Captura de Dados dos Hotboxes**
   - Lê o buffer circular e encaminha mensagens via IPC para a visualização de rodas quentes.

4. **Exibição da Sinalização**
   - Lê do arquivo circular e exibe mensagens em um terminal exclusivo, com interpretações de estado como “Desvio atuado” ou “Sinaleiro em PARE”.

5. **Visualização de Rodas Quentes**
   - Exibe mensagens de falhas de hardware e de rodas quentes em console separada.

6. **Leitura do Teclado**
   - Permite controlar o estado (ativo/bloqueado) de cada thread com as teclas `c`, `d`, `h`, `s`, `q`.
   - A tecla `ESC` encerra todo o sistema de forma segura.

## Comunicação entre Tarefas

- **Buffer circular em RAM** para comunicação entre a leitura dos CLPs e as threads de captura.
- **Arquivo circular em disco** para armazenamento das mensagens de sinalização.
- **Pipes Nomeados** para comunicação entre threads de captura e visualização de rodas quentes.
- **Eventos (WinAPI)** para sincronização entre as threads e controle via teclado.

## Demonstração em vídeo

[![Assista à Demonstração](https://img.youtube.com/vi/4lq4ou5mtMc/0.jpg)](https://youtu.be/4lq4ou5mtMc)



## Créditos

Desenvolvido como trabalho final para a disciplina ELT127 - Automação em Tempo Real  
Curso de Engenharia de Controle e Automação - UFMG  
Autoras: Camila Chagas Carvalho e Luiza Calheiros Lei
