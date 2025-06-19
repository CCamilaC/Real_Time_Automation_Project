#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <io.h>

#define ARQUIVO_DISCO "arquivo_sinalizacao.dat"
#define MAX_MSG_LENGTH 41
#define MAX_MENSAGENS_DISCO 200

HANDLE evVISUFERROVIA_PauseResume;
HANDLE evVISUFERROVIA_Exit;
HANDLE evVISUFERROVIATemporização;
HANDLE evEncerraThreads;
HANDLE hEventMsgDiscoDisponivel;
HANDLE hEventEspacoDiscoDisponivel;
HANDLE hMutexArquivoDisco;
HANDLE hEventSemMsgNovas;

char* lpimage;// Apontador para imagem local

// 20 textos de estado
const char* estados_texto[20] = {
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

DWORD WINAPI ThreadVisualizaSinalizacao(LPVOID) {
    HANDLE eventos[2] = { evVISUFERROVIA_PauseResume, evEncerraThreads };
    HANDLE espera[2] = { hEventMsgDiscoDisponivel, evEncerraThreads };
    HANDLE hArquivoDiscoMapping;
    BOOL pausado = FALSE;

    printf("Thread de Visualizacao de Sinalizacao iniciada\n");

    // Abre o arquivo em modo leitura na mesma visão que a main.cpp
    hArquivoDiscoMapping = OpenFileMapping(FILE_MAP_READ, FALSE, L"MAPEAMENTO");

    if (hArquivoDiscoMapping == NULL) {
        DWORD erro = GetLastError();
        LPVOID mensagemErro;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, erro, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&mensagemErro, 0, NULL);
        printf("Erro ao abrir o mapeamento de arquivo. Codigo: %d - Mensagem: %s\n", erro, (char*)mensagemErro);
        LocalFree(mensagemErro);
    }
    else {
        printf("Mapeamento aberto com sucesso!\n");
    }

    lpimage = (char*)MapViewOfFile(hArquivoDiscoMapping, FILE_MAP_READ, 0, 0, MAX_MENSAGENS_DISCO);
    if (lpimage == NULL) {
        DWORD erro = GetLastError();
        LPVOID mensagemErro = NULL;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, erro, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&mensagemErro, 0, NULL);
        printf("Falha ao mapear a view do arquivo. Erro %d: %s\n", erro, (char*)mensagemErro);
        LocalFree(mensagemErro);
        return FALSE;
    }

    while (1) {
        // Aguarda nova mensagem OU encerramento
        DWORD waitResult = WaitForMultipleObjects(2, espera, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) {  // hEventMsgDiscoDisponivel
            if (!pausado) {
                WaitForSingleObject(hMutexArquivoDisco, INFINITE);
                ResetEvent(hEventMsgDiscoDisponivel);

                long tamanho = strlen(lpimage);
                for (long i = 0; i < tamanho; i += MAX_MSG_LENGTH) {
                    char mensagem[MAX_MSG_LENGTH + 1] = { 0 };
                    size_t copy_size = (tamanho - i) < MAX_MSG_LENGTH ? (tamanho - i) : MAX_MSG_LENGTH;
                    memcpy_s(mensagem, MAX_MSG_LENGTH, lpimage + i, copy_size);
                    mensagem[copy_size] = '\0';

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
                            int indice_aleatorio = (estado_int == 0) ?
                                (rand() % 10) * 2 : (rand() % 10) * 2 + 1;
                            const char* estadoTexto = estados_texto[indice_aleatorio];

                            printf("%s NSEQ: %s REMOTA: %s SENSOR: %s ESTADO: %s\n",
                                timestamp, nseq, remota, id, estadoTexto);
                        }
                    }
                }

                SetEvent(hEventSemMsgNovas);
                ReleaseMutex(hMutexArquivoDisco);
            }
        }
        else if (waitResult == WAIT_OBJECT_0 + 1) {  // evEncerraThreads
            printf("[Sinalizacao] Evento de saida recebido. Encerrando thread.\n");
            return 0;
        }

        // Verifica eventos de pausa/retomada sem bloquear
        DWORD result = WaitForMultipleObjects(2, eventos, FALSE, 0);
        switch (result) {
        case WAIT_OBJECT_0:  // Pause/resume
            pausado = !pausado;
            printf("[Sinalizacao] Thread %s.\n", pausado ? "pausada" : "retomada");
            ResetEvent(evVISUFERROVIA_PauseResume);
            break;

        case WAIT_OBJECT_0 + 1:  // Encerrar
            printf("[Sinalizacao] Evento de saida recebido. Encerrando thread.\n");
            return 0;
        }

        // Se pausado, entra em espera bloqueante até retomada ou encerramento
        while (pausado) {
            DWORD r = WaitForMultipleObjects(2, eventos, FALSE, INFINITE);
            if (r == WAIT_OBJECT_0) {
                pausado = FALSE;
                printf("[Sinalizacao] Retomando execução.\n");
                ResetEvent(evVISUFERROVIA_PauseResume);
                break;
            }
            else if (r == WAIT_OBJECT_0 + 1) {
                printf("[Sinalizacao] Evento de saida recebido. Encerrando thread.\n");
                return 0;
            }
        }
    }

    return 0;
}


int main() {
    evVISUFERROVIA_PauseResume = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"EV_VISUFERROVIA_PAUSE");
    evVISUFERROVIA_Exit = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"EV_VISUFERROVIA_EXIT");
    evEncerraThreads = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"EV_ENCERRA_THREADS");
    evVISUFERROVIATemporização = CreateEvent(NULL, FALSE, FALSE, L"EV_VISUFERROVIA_TEMPORIZACAO"); // evento que nunca será setado apenas para temporização
    hEventEspacoDiscoDisponivel = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"EV_ESPACO_DISCO_DISPONIVEL");
    hMutexArquivoDisco = OpenMutex(MUTEX_ALL_ACCESS, FALSE, L"MUTEX_ARQUIVO_DISCO");
    hEventMsgDiscoDisponivel = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"EV_MSG_DISCO_DISPONIVEL");
    hEventSemMsgNovas = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"EV_SEM_MSG_NOVAS");

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

    WaitForSingleObject(hThread, INFINITE);

    BOOL status;
    status = UnmapViewOfFile(lpimage);
    // Checagem de erro
    if (!status) {
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
    }

    CloseHandle(hThread);
    CloseHandle(evVISUFERROVIA_PauseResume);
    CloseHandle(evVISUFERROVIA_Exit);
    CloseHandle(evVISUFERROVIATemporização);
    CloseHandle(evEncerraThreads);
    CloseHandle(hEventSemMsgNovas);

    return 0;
}



