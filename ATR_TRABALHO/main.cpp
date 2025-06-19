#include <windows.h>
#include <stdio.h>
#define HAVE_STRUCT_TIMESPEC // tive que colocar isso devido a um problema que estava tendo sobre a biblioteca Pthread e o Vscode
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include "circular_buffer.h"
#include <process.h>
#include <conio.h>
#include <sstream>
#include <wchar.h>
#include <tchar.h>
#define ARQUIVO_DISCO "arquivo_sinalizacao.dat" //nome do arquivo usado para armazenar as mensagens de sinalização
#define MAX_MENSAGENS_DISCO 200
#define ARQUIVO_TAMANHO_MAXIMO (MAX_MENSAGENS_DISCO * MAX_MSG_LENGTH) // 8200 bytes

//############ DEFINIÇÕES GLOBAIS ############
#define __WIN32_WINNT 0X0500
#define HAVE_STRUCT_TIMESPEC
#define _CRT_SECURE_NO_WARNINGS
#define BUFFER_SIZE 200  // Tamanho fixo da lista circular
#define _CHECKERROR 1
typedef unsigned (WINAPI* CAST_FUNCTION)(LPVOID);
typedef unsigned* CAST_LPDWORD;

//############# HANDLES #############
HANDLE hBufferRodaCheio;  // Evento para sinalizar espaço no buffer

//handles para a tarefa de leitura do teclado
HANDLE evCLPFerrovia_PauseResume, evCLPHotbox_PauseResume, evFERROVIA_PauseResume, evHOTBOX_PauseResume, evTemporização;
HANDLE evVISUFERROVIA_PauseResume, evVISUHOTBOX_PauseResume;
HANDLE evEncerraThreads=NULL;
HANDLE hPipeHotbox = INVALID_HANDLE_VALUE; //handle global para o pipe
DWORD WINAPI hCLPThreadFerrovia(LPVOID);
DWORD WINAPI hCLPThreadRoda(LPVOID);

//handles para os arquivos em disco:
HANDLE hMutexArquivoDisco;
HANDLE hEventEspacoDiscoDisponivel;
HANDLE hEventMsgDiscoDisponivel;
HANDLE hArquivoDisco; 
HANDLE hMutexPipeHotbox;
HANDLE hArquivoDiscoMapping;
HANDLE hEventSemMsgNovas;

//######### STRUCT MENSAGEM FERROVIA ##########
typedef struct {
    int32_t nseq;       // Número sequencial (1-9999999)
    char tipo[3];       // Sempre "00"
    int8_t diag;        // Diagnóstico (0-9)
    int16_t remota;     // Remota (000-999)
    char id[9];         // ID do sensor
    int8_t estado;      // Estado (1 ou 2)
    char timestamp[13]; // HH:MM:SS:MS 
} mensagem_ferrovia;

//######### STRUCT MENSAGEM RODA ##########
typedef struct {
    int32_t nseq;       // Número sequencial (1-9999999)
    char tipo[3];       // Sempre "00"
    int8_t diag;        // Diagnóstico (0-9)
    int16_t remota;     // Remota (000-999)
    char id[9];         // ID do sensor 
    int8_t estado;      // Estado (1 ou 2)
    char timestamp[13]; // HH:MM:SS:MS 
} mensagem_roda;

////############ VARIAVEIS GLOBAIS ##########
int disco_posicao_escrita = 0;
char* lpimage;			// Apontador para imagem local
int gcounter_ferrovia = 0; //contador para mensagem de ferrovia
int gcounter_roda = 0; //contador para mensagem de roda

//########## FUNÇÃO PARA TIMESTAMP HH:MM:SS:MS ################
void gerar_timestamp(char* timestamp) {
    SYSTEMTIME time;
    GetLocalTime(&time);
    // Usando sprintf_s com tamanho do buffer (13 para HH:MM:SS:MS)
    sprintf_s(timestamp, 13, "%02d:%02d:%02d:%03d",
        time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
}

//############# FUNÇÃO FORMATA MSG FERROVIA ###################
void formatar_msg_ferrovia(char* buffer, size_t buffer_size, const mensagem_ferrovia* msg) {
    // Assumindo que buffer_size é o tamanho total do buffer
    sprintf_s(buffer, buffer_size, "%07d;%s;%d;%03d;%s;%d;%s",
        msg->nseq,
        msg->tipo,
        msg->diag,
        msg->remota,
        msg->id,
        msg->estado,
        msg->timestamp);
}

//############# FUNÇÃO FORMATA MSG RODA QUENTE ###################
void formatar_msg_roda(char* buffer, size_t buffer_size, const mensagem_roda* msg) {
    // Formata a mensagem no padrão especificado: NNNNNNN;NN;AAAAAAAA;N;HH:MM:SS:MS
    sprintf_s(buffer, buffer_size, "%07d;%s;%s;%d;%s",
        msg->nseq,      
        msg->tipo,      
        msg->id,        
        msg->estado,    
        msg->timestamp  
    );
}

//############# FUNÇÃO DE CRIAÇÃO DE MSG FERROVIA ################
void cria_msg_ferrovia() {
    mensagem_ferrovia msg;
    char buffer[MAX_MSG_LENGTH]; // Buffer para armazenar a mensagem formatada

    // Preenche a mensagem com dados simulados
    msg.nseq = ++gcounter_ferrovia; // Incrementa o número sequencial
    strcpy_s(msg.tipo, sizeof(msg.tipo), "00");

    //Mensagem recebida de um dos 100 sensores aleatoriamente
    int numero = 1 + (rand() % 100); // Gera número entre 1 e 100
    char sensor[9];
    char letras[3];
    for (int i = 0; i < 3; i++) {
        letras[i] = 'A' + rand() % 26; // Gera letras maiúsculas aleatórias
    }

    int numeros = rand() % 1000; // Gera números de 0 a 999

    sprintf_s(sensor, sizeof(sensor), "%.3s-%03d", letras, numeros);
    strcpy_s(msg.id, sizeof(msg.id), sensor);

    // Diagnóstico
    msg.diag = rand() % 2;
    if (msg.diag == 1) { //Caso haja falha na remota
        strcpy_s(msg.id, sizeof(msg.id), "XXXXXXXX");
        msg.estado = 0;
    }

    //Remota
    int remota = rand() % 1000; // Gera um número entre 0 e 999
    std::stringstream ss;
    if (remota < 10) {
        ss << "00" << remota;       // Ex: 5 → 005
    }
    else if (remota < 100) {
        ss << "0" << remota;        // Ex: 58 → 058
    }
    else {
        ss << remota;               // Ex: 123 → 123
    }
    msg.remota = remota;

    msg.estado = rand() % 2; // Estado 0 ou 1 aleatoriamente
    gerar_timestamp(msg.timestamp); // Gera o timestamp

    // Formata a mensagem completa
    formatar_msg_ferrovia(buffer, sizeof(buffer), &msg);

    // Escreve no buffer circular
    WriteToFerroviaBuffer(buffer);
    printf("\033[34m[THREAD CLP FERROVIA]\033[0m Mensagem Ferrovia criada: %s\033[0m\n", buffer);
}

//############# FUNÇÃO DE CRIAÇÃO DE MSG RODA QUENTE ################
void cria_msg_roda() {
    mensagem_roda msg;
    char buffer[SMALL_MSG_LENGTH]; // Buffer para armazenar a mensagem formatada

    // Preenche a mensagem com dados simulados
    msg.nseq = ++gcounter_roda; // Incrementa o número sequencial
    strcpy_s(msg.tipo, sizeof(msg.tipo), "99");
    char sensor[9];

    //Mensagem recebida de um dos 100 sensores aleatoriamente
    char letras[3];
    for (int i = 0; i < 3; i++) {
        letras[i] = 'A' + rand() % 26; // Gera letras maiúsculas aleatórias
    }

    int numeros = rand() % 1000; // Gera números de 0 a 999

    sprintf_s(sensor, sizeof(sensor), "%.3s-%03d", letras, numeros);
    //int numero = 1 + (rand() % 100); // Gera número entre 1 e 100
    
    strcpy_s(msg.id, sizeof(msg.id), sensor);

    //Estado
    msg.estado = rand() % 2; // Estado 0 ou 1 aleatoriamente

    gerar_timestamp(msg.timestamp); // Gera o timestamp

    // Formata a mensagem completa
    formatar_msg_roda(buffer, sizeof(buffer), &msg);

    // Escreve no buffer circular
    WriteToRodaBuffer(buffer);
    printf("\033[36m[THREAD CLP HOTBOX]\033[0m  Hotbox criada: %s\033[0m\n", buffer);
}

//############# THREAD CRIA MENSAGENS DE FERROVIA CLP #############
DWORD WINAPI CLPMsgFerrovia(LPVOID) {
    BOOL pausado = FALSE;
    HANDLE eventos[2] = { evEncerraThreads, evCLPFerrovia_PauseResume };

    while (1) {
        // Verifica buffer ferrovia
        WaitForSingleObject(hMutexBufferFerrovia, INFINITE); //Conquista MUTEX
        BOOL ferroviaCheia = ferroviaBuffer.isFull;
        ReleaseMutex(hMutexBufferFerrovia); //Libera MUTEX

        if (ferroviaCheia) {
            WaitForSingleObject(ferroviaBuffer.hEventSpaceAvailable, INFINITE);
        }

        // Verifica eventos sem bloquear 
        DWORD ret = WaitForMultipleObjects(2, eventos, FALSE, 0);

        switch (ret) {
        case WAIT_OBJECT_0: // evEncerraThreads
            return 0;

        case WAIT_OBJECT_0 + 1: // evCLPFerrovia_PauseResume
            pausado = !pausado;
            printf("Thread Ferrovia %s\n", pausado ? "PAUSADA" : "RETOMADA");
            ResetEvent(evCLPFerrovia_PauseResume);
            break;

        case WAIT_TIMEOUT:

            break;

        default:
            printf("Erro: %d\n", GetLastError());
            return 1;
        }

        if (!pausado) { //Se a thread estiver permitida de rodar

            int tempo_ferrovia = 100 + (rand() % 1901); // 100-2000ms

            WaitForSingleObject(evTemporização, tempo_ferrovia); // evento que nunca será setado apenas para bloquear a thread 
            cria_msg_ferrovia();
        }
        else {
            // Se pausado, verifica eventos mais frequentemente
			WaitForSingleObject(evTemporização, 100); // evento que nunca será setado apenas para bloquear a thread
        }
    }
    return 0;
}

//############# THREAD CRIA MENSAGENS DE RODA QUENTE CLP #############
DWORD WINAPI CLPMsgRodaQuente(LPVOID) {
    BOOL pausado = FALSE;
    HANDLE eventos[2] = { evEncerraThreads, evCLPHotbox_PauseResume};

    while (1) {
        // Verifica buffer roda
        WaitForSingleObject(hMutexBufferRoda, INFINITE);//Conquista MUTEX
        BOOL rodaCheia = rodaBuffer.isFull;
        ReleaseMutex(hMutexBufferRoda); //Libera MUTEX

        if (rodaCheia) {
            WaitForSingleObject(rodaBuffer.hEventSpaceAvailable, INFINITE);
        }

        // Verifica eventos sem bloquear 
        DWORD ret = WaitForMultipleObjects(2, eventos, FALSE, 0);

        switch (ret) {
        case WAIT_OBJECT_0: // evEncerraThreads
            return 0;   

        case WAIT_OBJECT_0 + 1: // evCLPHotbox_PauseResume
            pausado = !pausado;
            printf("Thread Hotbox %s\n", pausado ? "PAUSADA" : "RETOMADA");
            ResetEvent(evCLPHotbox_PauseResume);
            break;

        case WAIT_TIMEOUT:

            break;

        default:
            printf("Erro: %d\n", GetLastError());
            return 1;
        }

        if (!pausado) { //Se a thread estiver permitida de rodar
            
            WaitForSingleObject(evTemporização, 500); // evento que nunca será setado apenas para bloquear a thread
            cria_msg_roda();
        }
        else {
            // Se pausado, verifica eventos mais frequentemente
            WaitForSingleObject(evTemporização, 100); // evento que nunca será setado apenas para bloquear a thread
        }
    }
    return 0;
}

//######### FUNÇÃO PARA ESCRITA NO ARQUIVO EM DISCO ##################
BOOL EscreveMensagemDisco(const char* mensagem) {
    WaitForSingleObject(hMutexArquivoDisco, INFINITE);

	// Escreve a mensagem no arquivo circular
    errno_t err = strcpy_s(lpimage, MAX_MSG_LENGTH, mensagem);
    if (err != 0) {
        // Tratamento de erro na cópia
        ReleaseMutex(hMutexArquivoDisco);
        return FALSE;
    }

    // Sinaliza~ção que há nova mensagem disponível encontra-se na função original
    ResetEvent(hEventSemMsgNovas);
    ReleaseMutex(hMutexArquivoDisco);

    return TRUE;
}

//############## FUNÇÃO DA THREAD DE CAPTURA DE RODA QUENTE###############
DWORD WINAPI CapturaHotboxThread(LPVOID) {
    //Inicialização das bariáveis auxiliares e handles
    char mensagem[SMALL_MSG_LENGTH];
    HANDLE eventos[2] = { evHOTBOX_PauseResume, evEncerraThreads };
    BOOL pausado = FALSE;

    printf("[Hotbox-Captura] Thread iniciada.\n");

    while (1) {
        // Verifica eventos de pausa/encerramento
        DWORD status = WaitForMultipleObjects(2, eventos, FALSE, 0);

        switch (status) {
        case WAIT_OBJECT_0: // evHOTBOX_PauseResume
            pausado = !pausado;
            printf("[Hotbox-Captura] Thread %s.\n", pausado ? "pausada" : "retomada");
            ResetEvent(evHOTBOX_PauseResume);
            break;
        case WAIT_OBJECT_0 + 1: // evEncerraThreads
            printf("[Hotbox-Captura] Thread encerrada.\n");
            return 0;
        }

        // Espera enquanto estiver pausado
        while (pausado) {
            DWORD r = WaitForMultipleObjects(2, eventos, FALSE, INFINITE);
            if (r == WAIT_OBJECT_0) {
                pausado = FALSE;
                printf("[Hotbox-Captura] Retomando execução.\n");
                ResetEvent(evHOTBOX_PauseResume);
                break;
            }
            else if (r == WAIT_OBJECT_0 + 1) {
                printf("[Hotbox-Captura] Thread encerrada.\n");
                return 0;
            }
        }

        WaitForSingleObject(hMutexBufferRoda, INFINITE); //Espera por acesso esclusivo a lista circular

        if (ReadFromRodaBuffer(mensagem)) {
            if (mensagem[8] == '9' && mensagem[9] == '9') {
                //printf("\033[94m[THREAD CAPTURA DADOS HOTBOX]\033[0m Mensagem lida de Hotbox: %s\n", mensagem);


                if (hPipeHotbox != INVALID_HANDLE_VALUE) {
                    DWORD bytesWritten;

                    WaitForSingleObject(hMutexPipeHotbox, INFINITE);

                    BOOL success = WriteFile(hPipeHotbox, mensagem, (DWORD)strlen(mensagem), &bytesWritten, NULL);

                    ReleaseMutex(hMutexPipeHotbox);

                    if (!success) { //Checagem de erro para escrita no pipe
                        printf("[Erro] Falha ao escrever na pipe de hotbox.\n");
                        CloseHandle(hPipeHotbox);
                        hPipeHotbox = INVALID_HANDLE_VALUE;
                    }
                }
            }
        }
        else {
			WaitForSingleObject(evTemporização, 100); // Espera 100ms antes de tentar ler novamente
        }
        ReleaseMutex(hMutexBufferRoda);
    }

    return 0;
}


//############## FUNÇÃO DA THREAD DE CAPTURA DE SINALIZAÇÃO FERROVIÁRIA ###############
DWORD WINAPI CapturaSinalizacaoThread(LPVOID) {
	// Inicialização das variáveis auxiliares e handles
    char mensagem[MAX_MSG_LENGTH];
    HANDLE eventos[2] = { evFERROVIA_PauseResume, evEncerraThreads };
    BOOL pausado = FALSE;

    printf("[Ferrovia-Captura] Thread iniciada.\n");

    while (1) {
        DWORD status = WaitForMultipleObjects(2, eventos, FALSE, 0);

		if (status == WAIT_OBJECT_0) { // evFERROVIA_PauseResume
            pausado = !pausado;
            printf("Thread Ferrovia %s\n", pausado ? "PAUSADA" : "RETOMADA");
            ResetEvent(evFERROVIA_PauseResume);
        }
		else if (status == WAIT_OBJECT_0 + 1) { // evEncerraThreads
            printf("[Ferrovia-Captura] Thread encerrada.\n");
            return 0;
        }

		if (pausado) { // Aguarda 100ms para verificar eventos
            WaitForSingleObject(evTemporização, 100);
            continue;
        }

		WaitForSingleObject(hMutexBufferFerrovia, INFINITE); //Espera por acesso exclusivo a lista circular

		if (ReadFromFerroviaBuffer(mensagem)) { // Se conseguiu ler uma mensagem do buffer
            
            //printf("\033[92m[THREAD CAPTURA FERROVIA]\033[0m Mensagem capturada: '%s'\n", mensagem);

            char nseq[8], tipo[3], diag[2], remota[4], id[9], estado[2], timestamp[13];
            sscanf_s(mensagem, "%7[^;];%2[^;];%1[^;];%3[^;];%8[^;];%1[^;];%12s",
                nseq, (unsigned int)sizeof(nseq),
                tipo, (unsigned int)sizeof(tipo),
                diag, (unsigned int)sizeof(diag),
                remota, (unsigned int)sizeof(remota),
                id, (unsigned int)sizeof(id),
                estado, (unsigned int)sizeof(estado),
                timestamp, (unsigned int)sizeof(timestamp));

            if (diag[0] == '1') {
                // Envia para VisualizaFerrovia via pipe
                if (hPipeHotbox != INVALID_HANDLE_VALUE) {
                    DWORD bytesWritten;
                    WaitForSingleObject(hMutexPipeHotbox, INFINITE);
                    BOOL success = WriteFile(hPipeHotbox, mensagem, (DWORD)strlen(mensagem), &bytesWritten, NULL);
                    ReleaseMutex(hMutexPipeHotbox);

                    if (!success) {
                        printf("[Erro] Falha ao escrever na pipe Ferrovia.\n");
                        CloseHandle(hPipeHotbox);
                        hPipeHotbox = INVALID_HANDLE_VALUE;
                    }
                }
            }
            else {
                // Escreve no arquivo circular
                BOOL gravado = EscreveMensagemDisco(mensagem);

                if (!gravado) {
                    printf("\033[33m[FERROVIA] Arquivo cheio. Aguardando espaço...\033[0m\n");
                    WaitForSingleObject(hEventEspacoDiscoDisponivel, INFINITE);
                }
                else {
                    SetEvent(hEventMsgDiscoDisponivel); // Notifica VisualizaSinalizacao
                }
            }
        }
        else {
			WaitForSingleObject(evTemporização, 100); // Espera 100ms antes de tentar ler novamente
        }

        ReleaseMutex(hMutexBufferFerrovia);
    }

    return 0;
}


//############# FUNÇÃO MAIN DO SISTEMA ################
int main() {
	InitializeBuffers(); // Inicializa os buffers circulares 

	//Criação de handles
    HANDLE hCLPThreadFerrovia = NULL;
    HANDLE hCLPThreadRoda = NULL;
    HANDLE hCapturaHotboxThread = NULL;
    HANDLE hCapturaSinalizacaoThread = NULL;
    DWORD dwThreadId;
    

    // Eventos de pausa/retomada
    evCLPHotbox_PauseResume = CreateEvent(NULL, FALSE, FALSE, L"EV_CLPH_PAUSE");
	evTemporização = CreateEvent(NULL, FALSE, FALSE, L"EV_TEMPORIZACAO"); // evento que nunca sera setado apenas para temporização
    evCLPFerrovia_PauseResume = CreateEvent(NULL, FALSE, FALSE, L"EV_CLPF_PAUSE");
    evFERROVIA_PauseResume = CreateEvent(NULL, TRUE, FALSE, L"EV_FERROVIA_PAUSE");
    evHOTBOX_PauseResume = CreateEvent(NULL, TRUE, FALSE, L"EV_HOTBOX_PAUSE"); 
    evVISUFERROVIA_PauseResume = CreateEvent(NULL, TRUE, FALSE, L"EV_VISUFERROVIA_PAUSE");
    evVISUHOTBOX_PauseResume = CreateEvent(NULL, TRUE, FALSE, L"EV_VISUHOTBOX_PAUSE");
    hMutexPipeHotbox = CreateMutex(NULL, FALSE, NULL);

 
    // Eventos de término
    evEncerraThreads = CreateEvent(NULL, TRUE, FALSE, L"EV_ENCERRA_THREADS"    );

    // Eventos para memória circular em disco
    hEventMsgDiscoDisponivel = CreateEvent(NULL, TRUE, FALSE, L"EV_MSG_DISCO_DISPONIVEL");
    hEventEspacoDiscoDisponivel = CreateEvent(NULL, TRUE, FALSE, L"EV_ESPACO_DISCO_DISPONIVEL");
    hMutexArquivoDisco = CreateMutex(NULL, FALSE, L"MUTEX_ARQUIVO_DISCO");
	hEventSemMsgNovas = CreateEvent(NULL, TRUE, FALSE, L"EV_SEM_MSG_NOVAS");

	//############ CRIAÇÃO DO ARQUIVO EM DISCO ############
	hArquivoDisco = CreateFile(L"arquivo_sinalizacao.dat", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    // Criação do mapeamento do arquivo
	hArquivoDiscoMapping = CreateFileMapping(
		hArquivoDisco, // Handle do arquivo
		NULL,          // Atributos de segurança
		PAGE_READWRITE,// Acesso de leitura e escrita
		0,             // Tamanho máximo alto (0 para tamanho baixo)
		ARQUIVO_TAMANHO_MAXIMO, // Tamanho máximo do mapeamento
        L"MAPEAMENTO"           // Nome do mapeamento (NULL para anônimo)
	);
    if (hArquivoDiscoMapping == NULL) { //Checagem de erro do mapeamento
        DWORD erro = GetLastError();
        printf("Erro ao criar o mapeamento de arquivo. Código de erro: %d\n", erro);
        // Você pode tratar o erro aqui (fechar handles, retornar, etc.)
    }
    else {
        printf("Mapeamento de arquivo criado com sucesso!\n");
    }

    // Criação do visão de mapeamento
    lpimage = (char*)MapViewOfFile(hArquivoDiscoMapping, FILE_MAP_WRITE, 0, 0, MAX_MENSAGENS_DISCO);

    //############ CRIAÇÃO DE THREADS ############
    
    // Cria a thread CLP que escreve no buffer ferrovia
    hCLPThreadFerrovia = (HANDLE)_beginthreadex(
        NULL,
        0,
        (CAST_FUNCTION)CLPMsgFerrovia,
        NULL,
        0,
        (CAST_LPDWORD)&dwThreadId
    );

    if (hCLPThreadFerrovia) {
        printf("Thread CLP criada com ID=0x%x\n", dwThreadId);
    }

    // Cria a thread CLP que escreve no buffer roda
    hCLPThreadRoda = (HANDLE)_beginthreadex(
        NULL,
        0,
        (CAST_FUNCTION)CLPMsgRodaQuente,
        NULL,
        0,
        (CAST_LPDWORD)&dwThreadId
    );

    if (hCLPThreadRoda) {
        printf("Thread CLP criada com ID=0x%x\n", dwThreadId);
    }

    // Cria a thread de Captura de Dados dos HotBoxes
    hCapturaHotboxThread = (HANDLE)_beginthreadex(
        NULL,
        0,
        (CAST_FUNCTION)CapturaHotboxThread,
        NULL,
        0,
        (CAST_LPDWORD)&dwThreadId
    );
    if (hCapturaHotboxThread) {
        printf("Thread CapturaHotbox criada com ID=0x%x\n", dwThreadId);
    }

    // Cria a thread de Captura de Dados da Sinalização Ferroviária
    hCapturaSinalizacaoThread = (HANDLE)_beginthreadex(
        NULL,
        0,
        (CAST_FUNCTION)CapturaSinalizacaoThread,
        NULL,
        0,
        (CAST_LPDWORD)&dwThreadId
    );

    if (hCapturaSinalizacaoThread) {
        printf("Thread CapturaSinalizacao criada com ID=0x%x\n", dwThreadId);
    }


    //  CRIAÇÃO DO PROCESSO VISUALIZA_HOTBOXES.EXE COM NOVO CONSOLE 
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));


    //############ COLOCA UM PATH GERAL ############
    WCHAR exePath[MAX_PATH];
    WCHAR hotboxesPath[MAX_PATH];
    WCHAR sinalizacaoPath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    WCHAR* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) {
        *lastSlash = L'\0';  // Trunca o caminho, removendo o nome do .exe
    }
    swprintf(hotboxesPath, MAX_PATH, L"%s\\VisualizaHotboxes.exe", exePath);
    swprintf(sinalizacaoPath, MAX_PATH, L"%s\\VisualizaSinalizacao.exe", exePath);


    //###### Criação de processo separado com novo console para Hotboxes #########
    if (CreateProcess(
        hotboxesPath, // Nome do executável do processo filho
        NULL,                      // Argumentos da linha de comando
        NULL,                      // Atributos de segurança do processo
        NULL,                      // Atributos de segurança da thread
        FALSE,                     // Herança de handles
        CREATE_NEW_CONSOLE,        // Cria nova janela de console
        NULL,                      // Ambiente padrão
        NULL,                      // Diretório padrão
        &si,                       // Informações de inicialização
        &pi                        // Informações sobre o processo criado
    )) {
        printf("Processo VisualizaHotboxes.exe criado com sucesso!\n");
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else {
        printf("Erro ao criar processo VisualizaHotboxes.exe. Código do erro: %lu\n", GetLastError());
    }

    //##### Criação de processo separado com novo console para Sinalização Ferroviária #####
    if (CreateProcess(
        sinalizacaoPath, // Nome do executável do processo filho
        NULL,                      // Argumentos da linha de comando
        NULL,                      // Atributos de segurança do processo
        NULL,                      // Atributos de segurança da thread
        FALSE,                     // Herança de handles
        CREATE_NEW_CONSOLE,       // Cria nova janela de console
        NULL,                      // Ambiente padrão
        NULL,                      // Diretório padrão
        &si,                       // Informações de inicialização
        &pi                        // Informações sobre o processo criado
    )) {
        printf("Processo VisualizaSinalizacao.exe criado com sucesso!\n");
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else {
        printf("Erro ao criar processo VisualizaSinalizacao.exe. Código do erro: %lu\n", GetLastError());
    }


    //############ Criação do pipe nomeado para o IPC entre as threads captura e visualização Hotboxes ############
    int tentativas = 0;
    while (tentativas < 10) {
        hPipeHotbox = CreateFile(
            TEXT("\\\\.\\pipe\\PIPE_HOTBOX"),
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if (hPipeHotbox != INVALID_HANDLE_VALUE) {
            printf("[OK] Pipe para VisualizaHotboxes conectada com sucesso.\n");
            break;
        }

        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PIPE_BUSY) {
            printf("[INFO] Esperando conexão com VisualizaHotboxes... Tentativa %d\n", tentativas + 1);
            Sleep(500);
            tentativas++;
        }
        else {
            printf("[ERRO] Falha inesperada ao abrir pipe. Código: %lu\n", err);
            break;
        }
    }

    if (hPipeHotbox == INVALID_HANDLE_VALUE) {
        printf("[ERRO] Não foi possível conectar ao VisualizaHotboxes.\n");
    }

    //############ LEITURA DO TECLADO ################
    BOOL executando = TRUE;
    BOOL clp_pausado = FALSE;
    BOOL ferrovia_pausado = FALSE;
    BOOL hotbox_pausado = FALSE;
    BOOL visuFerrovia_pausado = FALSE;
    BOOL visuHotbox_pausado = FALSE;
    while (executando) {
        
        if (_kbhit()) {
            int tecla = _getch();

            switch (tecla) {
            case 'c':
                clp_pausado = !clp_pausado;
                SetEvent(evCLPFerrovia_PauseResume); 
                SetEvent(evCLPHotbox_PauseResume);
                printf("CLP %s\n", clp_pausado ? "PAUSADO" : "RETOMADO");
                break;

            case 'd':
                ferrovia_pausado = !ferrovia_pausado;
                SetEvent(evFERROVIA_PauseResume);
                printf("Captura Ferrovia %s\n", ferrovia_pausado ? "PAUSADA" : "RETOMADA");
                break;

            case 'h':
                hotbox_pausado = !hotbox_pausado;
                SetEvent(evHOTBOX_PauseResume);
                printf("Captura Hotbox %s\n", hotbox_pausado ? "PAUSADA" : "RETOMADA");
                break;

            case 's':
                visuFerrovia_pausado = !visuFerrovia_pausado;
                SetEvent(evVISUFERROVIA_PauseResume);
                printf("Visualização Ferrovia %s\n", visuFerrovia_pausado ? "PAUSADA" : "RETOMADA");
                break;

            case 'q':
                visuHotbox_pausado = !visuHotbox_pausado;
                SetEvent(evVISUHOTBOX_PauseResume);
                printf("Visualização Hotbox %s\n", visuHotbox_pausado ? "PAUSADA" : "RETOMADA");
                break;

            case 27: // ESC
                printf("Encerrando todas as tarefas...\n");
                SetEvent(evEncerraThreads);

                executando = FALSE;
                break;
            }
        }
        WaitForSingleObject(evTemporização, 1000); // evento que nunca será setado apenas para bloquear a thread
    }

    // Limpeza
    if (hCLPThreadFerrovia != NULL) {
        WaitForSingleObject(hCLPThreadFerrovia, INFINITE);
        printf("Thread CLP Ferrovia terminou\n");
        CloseHandle(hCLPThreadFerrovia);
    }
    if (hCLPThreadRoda != NULL) {
        WaitForSingleObject(hCLPThreadRoda, INFINITE);
        printf("Thread CLP roda terminou\n");
        CloseHandle(hCLPThreadRoda);
    }

    if (hCapturaHotboxThread != NULL) {
        WaitForSingleObject(hCapturaHotboxThread, INFINITE);
        CloseHandle(hCapturaHotboxThread);
    }

    if (hCapturaSinalizacaoThread != NULL) {
        WaitForSingleObject(hCapturaSinalizacaoThread, INFINITE);
        CloseHandle(hCapturaSinalizacaoThread);
    }

    // Desmapeia o arquivo
    BOOL status;
    status = UnmapViewOfFile(lpimage);
        
	if (!status) {// Checagem de erro do desmapeamento
        DWORD erro = GetLastError();
        LPVOID mensagemErro = NULL;

        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            erro,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&mensagemErro,
            0,
            NULL
        );

        printf("Falha ao desmapear a view do arquivo. Erro %d: %s\n",
            erro, (char*)mensagemErro);

        LocalFree(mensagemErro);
        return FALSE;
    }

    // Fecha handles de eventos
    CloseHandle(hBufferRodaCheio);
    CloseHandle(evCLPFerrovia_PauseResume);
    CloseHandle(evCLPHotbox_PauseResume);
    CloseHandle(evFERROVIA_PauseResume);
    CloseHandle(evHOTBOX_PauseResume);
    CloseHandle(evVISUFERROVIA_PauseResume);
    CloseHandle(evVISUHOTBOX_PauseResume);
    CloseHandle(evEncerraThreads);
    CloseHandle(hMutexPipeHotbox);
    DestroyBuffers();
    CloseHandle(hMutexBufferRoda);
    CloseHandle(hMutexBufferFerrovia);
    CloseHandle(hMutexArquivoDisco);
    CloseHandle(hEventMsgDiscoDisponivel);
    CloseHandle(hEventEspacoDiscoDisponivel);
	CloseHandle(hArquivoDisco);
    CloseHandle(hEventSemMsgNovas);

    return 0;
}