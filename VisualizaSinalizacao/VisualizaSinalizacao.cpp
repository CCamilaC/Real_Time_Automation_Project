#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <io.h>

// ############ DEFINIÇÕES GLOBAIS ############
#define ARQUIVO_DISCO "arquivo_sinalizacao.dat"
#define MAX_MSG_LENGTH 41
#define MAX_MENSAGENS_DISCO 200

//############# HANDLES #############
HANDLE evVISUFERROVIA_PauseResume;
HANDLE evVISUFERROVIA_Exit;
HANDLE evVISUFERROVIATemporizacao;
HANDLE evEncerraThreads;
HANDLE hEventMsgDiscoDisponivel;
HANDLE hEventEspacoDiscoDisponivel;
HANDLE hMutexArquivoDisco;
HANDLE hFile; // Handle do arquivo mapeado


//############# VARIÁVEIS GLOBAIS #############
OVERLAPPED overlapped;
const char* estados_texto[20] = { // 20 textos de estado
    "Desvio atuado",
    "Desvio nao atuado",
    "Sinaleiro em PARE",
    "Sinaleiro em VIA LIVRE",
    "Ocorrencia na via",
    "Sem ocorrencia na via",
    "Sensor ativo",
    "Sensor inativo",
    "Veiculo detectado",
    "Veiculo nao detectado",
    "Barreira abaixada",
    "Barreira levantada",
    "Desvio nao confirmado",
    "Desvio confirmado",
    "Via ocupada",
    "Via livre",
    "Alimentacao normal",
    "Alimentacao interrompida",
    "Sinaleiro apagado",
    "Sinaleiro aceso",

};

DWORD WINAPI ProcessarMensagens(const char* mensagem, DWORD tamanho) {
    // Processa cada mensagem
    int mensagens_processadas = 0;
    //long tamanho = strlen(lpimage);



    for (long i = 0; i < tamanho; i += MAX_MSG_LENGTH) {
        const char* mensagemAtual = mensagem + i;

        if (mensagemAtual[0] == '\0' || mensagemAtual[0] == ' ') {
            continue;
        }

        // Processamento da mensagem 
        char nseq[8] = { 0 }, tipo[3] = { 0 }, diag[2] = { 0 },
            remota[4] = { 0 }, id[9] = { 0 }, estado[2] = { 0 },
            timestamp[13] = { 0 };

        int parsed = sscanf_s(mensagem, "%7[^;];%2[^;];%1[^;];%3[^;];%8[^;];%1[^;];%12s",
            nseq, (unsigned)_countof(nseq),
            tipo, (unsigned)_countof(tipo),
            diag, (unsigned)_countof(diag),
            remota, (unsigned)_countof(remota),
            id, (unsigned)_countof(id),
            estado, (unsigned)_countof(estado),
            timestamp, (unsigned)_countof(timestamp));

        if (parsed == 7) {
            int estado_int = estado[0] - '0';
            if (estado_int >= 0 && estado_int < 20) {
                int indice_aleatorio;
                if (estado_int == 0) {
                    // Numero par aleatorio (0, 2, 4, ..., 18)
                    indice_aleatorio = (rand() % 10) * 2;
                }
                else if (estado_int == 1) {
                    // Numero Impar aleatorio (1, 3, 5, ..., 19)
                    indice_aleatorio = (rand() % 10) * 2 + 1;
                }
                const char* estadoTexto = estados_texto[indice_aleatorio];

                printf("%s NSEQ: %s REMOTA: %s SENSOR: %s ESTADO: %s\n",
                    timestamp, nseq, remota, id, estadoTexto);
                mensagens_processadas++;


                // APAGA A MENSAGEM PROCESSADA

                char apagado[MAX_MSG_LENGTH] = { 0 }; // Buffer preenchido com zeros
                DWORD offset = i * MAX_MSG_LENGTH;

                OVERLAPPED overlappedWrite = { 0 };
                overlappedWrite.Offset = offset;
                overlappedWrite.OffsetHigh = 0;

                DWORD bytesWritten = 0;

                BOOL bWrite = WriteFile(
                    hFile,
                    apagado,
                    MAX_MSG_LENGTH,
                    &bytesWritten,
                    &overlappedWrite
                );

                if (!bWrite) {
                    DWORD err = GetLastError();
                    if (err == ERROR_IO_PENDING) {
                        // Espera a escrita assíncrona completar
                        BOOL res = GetOverlappedResult(hFile, &overlappedWrite, &bytesWritten, TRUE);
                        if (res) {

                        }
                        else {
                            printf("[ERRO] Falha ao concluir escrita assíncrona para apagar. Erro %lu\n", GetLastError());
                        }
                    }
                    else {
                        printf("[ERRO] Falha na escrita assíncrona para apagar. Erro: %lu\n", err);
                    }
                }

            }
        }
    }
    return 0;

}

//############# FUNÇÃO DA THREAD DE VISUALIZAÇÃO DE SINALIZAÇÃO #############
DWORD WINAPI ThreadVisualizaSinalizacao(LPVOID) {
    // Inicialização das variaveis auxiliares e handles
    HANDLE eventos[2] = { evVISUFERROVIA_PauseResume, evEncerraThreads };
    HANDLE hArquivoDiscoMapping;

    BOOL pausado = FALSE;

    printf("Thread de Visualizacao de Sinalizacao iniciada\n");

    hFile = CreateFileA(
        ARQUIVO_DISCO,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,  // Permite escrita simultânea
        NULL,
        OPEN_EXISTING,  // ABRE o arquivo existente (não cria)
        FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            printf("[INFO] Arquivo ainda não existe. Aguardando...\n");
        }
        else {
            printf("[ERRO] Falha ao abrir arquivo: %d\n", err);
        }
    }
    else {
        printf("[INFO] Arquivo aberto com sucesso.\n");
    }


    while (1) {
        if (!pausado) {
            // Verifica os dois eventos simultaneamente (sem bloquear)
            DWORD result = WaitForMultipleObjects(2, eventos, FALSE, 0);

            switch (result) {
            case WAIT_OBJECT_0:  // evVISUFERROVIA_PauseResume
                pausado = !pausado;
                if (pausado) {
                    printf("\033[91m[Sinalizacao] Thread pausada. Aguardando retomada...\033[0m\n");
                }
                else {
                    printf("\033[91m[Sinalizacao] Retomando execucao.\033[0m\n");
                }
                ResetEvent(evVISUFERROVIA_PauseResume);
                break;

            case WAIT_OBJECT_0 + 1:  // evVISUFERROVIA_Exit
                printf("\033[91m[Sinalizacao] Evento de saida recebido. Encerrando thread.\033[0m\n");
                return 0;

            default:
                break; // Nenhum evento sinalizado
            }

            // Se pausado, entra em espera bloqueante ate algo acontecer
            while (pausado) {
                DWORD r = WaitForMultipleObjects(2, eventos, FALSE, INFINITE);
                if (r == WAIT_OBJECT_0) { // Toggle pausa
                    pausado = FALSE;
                    printf("\033[91m[Sinalizacao] Retomando execucao.\033[0m\n");
                    ResetEvent(evVISUFERROVIA_PauseResume);
                    break;
                }
                else if (r == WAIT_OBJECT_0 + 1) { // Evento de saida
                    printf("\033[91m[Sinalizacao] Evento de saida recebido. Encerrando thread.\033[0m\n");
                    return 0;
                }
            }

            // ####### CASO NÃO ESTEJA PAUSADO OU ENCERRADO ########

            DWORD waitResult = WaitForSingleObject(hEventMsgDiscoDisponivel, 0); //Espera que hajam mensagens escritas
            if (waitResult == WAIT_OBJECT_0 && !pausado) { // Se o evento foi sinalizado e a thread nao esta pausada

                WaitForSingleObject(hMutexArquivoDisco, INFINITE); //Garante acesso unico ao arquivo 
                ResetEvent(hEventMsgDiscoDisponivel); // Avisa a main que recebeu a mensagem

                DWORD fileSize = GetFileSize(hFile, NULL);
                char* buffer = (char*)malloc(fileSize + 1);

                BOOL bResult = ReadFile(
                    hFile,
                    buffer,
                    fileSize,
                    NULL,
                    &overlapped
                );

                if (!bResult && GetLastError() == ERROR_IO_PENDING) {
                    //WaitForSingleObject(overlapped.hEvent, INFINITE);
                    DWORD bytesRead;
                    bResult = GetOverlappedResult(hFile, &overlapped, &bytesRead, FALSE);

                    if (bResult) {
                        buffer[bytesRead] = '\0';
                        ProcessarMensagens(buffer, bytesRead); // Função para processar as mensagens
                    }
                }

                // Trunca o arquivo após a leitura
                SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
                SetEndOfFile(hFile);


                SetEvent(hEventEspacoDiscoDisponivel); // Sinaliza que novas mensagens foram processadas
                ReleaseMutex(hMutexArquivoDisco);
            }

        }

    }
    return 0;

}

// ######### FUNÇÃO MAIN DO SISTEMA #########
int main() {
    // Inicializacao das variaveis auxiliares e handles
    evVISUFERROVIA_PauseResume = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"EV_VISUFERROVIA_PAUSE");
    evVISUFERROVIA_Exit = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"EV_VISUFERROVIA_EXIT");
    evEncerraThreads = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"EV_ENCERRA_THREADS");
    evVISUFERROVIATemporizacao = CreateEvent(NULL, FALSE, FALSE, L"EV_VISUFERROVIA_TEMPORIZACAO"); // evento que nunca sera setado apenas para temporiza��o
    hEventEspacoDiscoDisponivel = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"EV_ESPACO_DISCO_DISPONIVEL");
    hMutexArquivoDisco = OpenMutex(MUTEX_ALL_ACCESS, FALSE, L"MUTEX_ARQUIVO_DISCO");
    hEventMsgDiscoDisponivel = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"EV_MSG_DISCO_DISPONIVEL");




    if (hEventMsgDiscoDisponivel == NULL) {
        printf("[Erro] Falha ao abrir EV_MSG_DISCO_DISPONIVEL: %lu\n", GetLastError());
        return 1;
    }
    if (hMutexArquivoDisco == NULL) {
        printf("[Erro] Falha MUTEX_ARQUIVO_DISCO: %lu\n", GetLastError());
        return 1;
    }
    HANDLE hThread = CreateThread(NULL, 0, ThreadVisualizaSinalizacao, NULL, 0, NULL);
    if (hThread == NULL) {
        printf("Erro ao criar a thread.\n");
        return 1;
    }

    WaitForSingleObject(hThread, INFINITE); // Espera a thread terminar


       // Fecha os handles de eventos e mutex
    CloseHandle(hThread);
    CloseHandle(evVISUFERROVIA_PauseResume);
    CloseHandle(evVISUFERROVIA_Exit);
    CloseHandle(evVISUFERROVIATemporizacao);
    CloseHandle(evEncerraThreads);


    return 0;
}



