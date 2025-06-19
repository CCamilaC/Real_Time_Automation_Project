#include <windows.h>
#include <stdio.h>
#include <string.h>

// ################ CRIA��O DE HANDLES ################
HANDLE evVISUHOTBOX_PauseResume;
HANDLE evVISUHOTBOX_Exit;
HANDLE evVISUHOTBOXTemporiza��o;
HANDLE evEncerraThreads;
HANDLE hPipeHotbox;

//################ THREAD DE VISUALIZA��O DE HOTBOXES ################

DWORD WINAPI ThreadVisualizaHotboxes(LPVOID) {
	// Inicializa��o das vari�veis e handles
    HANDLE eventos[2] = { evVISUHOTBOX_PauseResume, evEncerraThreads };
    BOOL pausado = FALSE;
    char buffer[128];
    DWORD bytesRead;

    printf("Thread de Visualizacao de Hotboxes iniciada\n");

    // Cria a Named Pipe (servidor)
    hPipeHotbox = CreateNamedPipe(
        TEXT("\\\\.\\pipe\\PIPE_HOTBOX"),
        PIPE_ACCESS_INBOUND,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1, 0, 0, 0, NULL);

	if (hPipeHotbox == INVALID_HANDLE_VALUE) { //Checa se a cria��o da pipe falhou
        printf("[Erro] Falha ao criar a pipe. Erro: %lu\n", GetLastError());
        return 1;
    }

    printf("[VisualizaHotboxes] Aguardando conex�o do produtor...\n");
    ConnectNamedPipe(hPipeHotbox, NULL); //Esse comando bloqueia indefinidamente at� que um cliente se conecte � pipe.
    printf("[VisualizaHotboxes] Conectado ao produtor.\n");

    while (1) {
        if (!pausado) {
            // Tenta ler da pipe
            BOOL success = ReadFile(hPipeHotbox, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
            if (success && bytesRead > 0) {
                buffer[bytesRead] = '\0';

                // Verifica se � uma mensagem de falha (DIAG == 1)
                if (strstr(buffer, ";00;") != NULL && strstr(buffer, ";1;") != NULL && strstr(buffer, "XXXXXXXX") != NULL) {
                    // Formato esperado: NSEQ;00;1;REMOTA;XXXXXXXX;0;HH:MM:SS:MS
                    char nseq[8], tipo[3], diag[2], remota[4], id[9], estado[2], timestamp[13];
                    sscanf_s(buffer, "%7[^;];%2[^;];%1[^;];%3[^;];%8[^;];%1[^;];%12s",
                        nseq, (unsigned int)sizeof(nseq),
                        tipo, (unsigned int)sizeof(tipo),
                        diag, (unsigned int)sizeof(diag),
                        remota, (unsigned int)sizeof(remota),
                        id, (unsigned int)sizeof(id),
                        estado, (unsigned int)sizeof(estado),
                        timestamp, (unsigned int)sizeof(timestamp));

                    printf("\033[31m %s NSEQ: %s REMOTA: %s FALHA DE HARDWARE\033[0m\n", timestamp, nseq, remota);
                }
                else {
                    // Hotbox comum: NSEQ;99;ID;ESTADO;HH:MM:SS:MS
                    char nseq[8], tipo[3], id[9], estado[2], timestamp[13];
                    sscanf_s(buffer, "%7[^;];%2[^;];%8[^;];%1[^;];%12s",
                        nseq, (unsigned int)sizeof(nseq),
                        tipo, (unsigned int)sizeof(tipo),
                        id, (unsigned int)sizeof(id),
                        estado, (unsigned int)sizeof(estado),
                        timestamp, (unsigned int)sizeof(timestamp));

                    printf("%s NSEQ: %s DETECTOR: %s \033[%dm%s\033[0m\n",
                        timestamp,
                        nseq,
                        id,
                        (estado[0] == '1') ? 31 : 32,  // 31=vermelho, 32=verde
                        (estado[0] == '1') ? "RODA QUENTE DETECTADA" : "TEMP. DENTRO DA FAIXA");
                }
            }

			WaitForSingleObject(evVISUHOTBOXTemporiza��o, 100); // Espera 100ms antes de tentar ler novamente
        }

		// ################ PAUSA/RETOMADA DA THREAD ################
        DWORD result = WaitForMultipleObjects(2, eventos, FALSE, 0);
        switch (result) {
		case WAIT_OBJECT_0: // evVISUHOTBOX_PauseResume
            pausado = !pausado;
            printf("\033[91mThread de visualiza��o de Hotboxes %s.\n", pausado ? "PAUSADA\033[0m" : "RETOMADA\033[0m");
            ResetEvent(evVISUHOTBOX_PauseResume);
            break;
		case WAIT_OBJECT_0 + 1: // evEncerraThreads
            printf("\033[91mThread Visualiza Hotboxes: Evento de sa�da recebido. Encerrando thread.\033[0m\n");
            CloseHandle(hPipeHotbox);
            return 0;
        default:
            break;
        }

        while (pausado) {
            DWORD r = WaitForMultipleObjects(2, eventos, FALSE, INFINITE);
			if (r == WAIT_OBJECT_0) { // evVISUHOTBOX_PauseResume
                pausado = FALSE;
                printf("\033[91mThread Visualiza Hotboxes: Retomando execu��o.\033[0m\n");
                ResetEvent(evVISUHOTBOX_PauseResume);
                break;
            }
			else if (r == WAIT_OBJECT_0 + 1) { // evEncerraThreads
                printf("\033[91mThread Visualiza Hotboxes: Evento de sa�da recebido. Encerrando thread.\033[91m\n");
                CloseHandle(hPipeHotbox);
                return 0;
            }
        }
    }
    return 0;
}

//################ FUN��O PRINCIPAL ################
int main() {
	// Cria��o dos eventos e threads
    evVISUHOTBOX_PauseResume = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"EV_VISUHOTBOX_PAUSE");
    evVISUHOTBOX_Exit = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"EV_VISUHOTBOX_EXIT");
    evEncerraThreads = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"EV_ENCERRA_THREADS");
    evVISUHOTBOXTemporiza��o = CreateEvent(NULL, FALSE, FALSE, L"EV_VISUHOTBOX_TEMPORIZACAO");

    HANDLE hThread = CreateThread(NULL, 0, ThreadVisualizaHotboxes, NULL, 0, NULL);

	if (hThread == NULL) { // Checagem de erro na cria��o da thread
        printf("Erro ao criar a thread.\n");
        return 1;
    }

	WaitForSingleObject(hThread, INFINITE); // Espera a thread terminar

	// Fechamento dos handles
    CloseHandle(hThread);
    if (evVISUHOTBOX_PauseResume != NULL)
        CloseHandle(evVISUHOTBOX_PauseResume);

    if (evVISUHOTBOX_Exit != NULL)
        CloseHandle(evVISUHOTBOX_Exit);

    if (evVISUHOTBOXTemporiza��o != NULL)
        CloseHandle(evVISUHOTBOXTemporiza��o);

    if (evEncerraThreads != NULL)
        CloseHandle(evEncerraThreads);

    return 0;
}
