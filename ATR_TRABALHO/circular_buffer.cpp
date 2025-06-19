#include "circular_buffer.h"
#include <string.h>

CircularBuffer ferroviaBuffer;
CircularBuffer rodaBuffer;
HANDLE hMutexBufferFerrovia;
HANDLE hMutexBufferRoda;


void InitializeBuffers() {
    // Initialize ferrovia buffer
    ferroviaBuffer.head = 0;
    ferroviaBuffer.tail = 0;
    ferroviaBuffer.count = 0;
    ferroviaBuffer.isFull = FALSE;
    InitializeCriticalSection(&ferroviaBuffer.cs);
    ferroviaBuffer.hEventSpaceAvailable = CreateEvent(NULL, TRUE, TRUE, NULL);

    // Initialize roda buffer
    rodaBuffer.head = 0;
    rodaBuffer.tail = 0;
    rodaBuffer.count = 0;
    rodaBuffer.isFull = FALSE;
    InitializeCriticalSection(&rodaBuffer.cs);
    rodaBuffer.hEventSpaceAvailable = CreateEvent(NULL, TRUE, TRUE, NULL);
}

void DestroyBuffers() {
    DeleteCriticalSection(&ferroviaBuffer.cs);
    DeleteCriticalSection(&rodaBuffer.cs);
    CloseHandle(ferroviaBuffer.hEventSpaceAvailable);
    CloseHandle(rodaBuffer.hEventSpaceAvailable);
}

void WriteToFerroviaBuffer(const char* value) {
    WaitForSingleObject(hMutexBufferFerrovia, INFINITE);

    // Se o buffer está cheio, não escreve e espera
    while (ferroviaBuffer.count >= BUFFER_SIZE) {
        ferroviaBuffer.isFull = TRUE;
        ResetEvent(ferroviaBuffer.hEventSpaceAvailable);
        ReleaseMutex(hMutexBufferFerrovia); //Libera MUTEX

        // Espera por espaço disponível
        WaitForSingleObject(ferroviaBuffer.hEventSpaceAvailable, INFINITE);

        WaitForSingleObject(hMutexBufferFerrovia, INFINITE);
    }

    errno_t err = strncpy_s(ferroviaBuffer.data[ferroviaBuffer.tail],
        MAX_MSG_LENGTH,
        value,
        MAX_MSG_LENGTH - 1);

    if (err == 0) {
        ferroviaBuffer.tail = (ferroviaBuffer.tail + 1) % BUFFER_SIZE;
        ferroviaBuffer.count++;
        ferroviaBuffer.isFull = (ferroviaBuffer.count >= BUFFER_SIZE);
    }
    else {
        printf("Erro ao copiar mensagem para o buffer ferrovia: %d\n", err);
    }

    ReleaseMutex(hMutexBufferFerrovia); //Libera MUTEX
}

void WriteToRodaBuffer(const char* value) {
    WaitForSingleObject(hMutexBufferRoda, INFINITE);

    while (rodaBuffer.count >= BUFFER_SIZE) {
        rodaBuffer.isFull = TRUE;
        ResetEvent(rodaBuffer.hEventSpaceAvailable); // Garante que o evento não está sinalizado
        ReleaseMutex(hMutexBufferRoda); //Libera MUTEX

        // Espera por espaço disponível
        WaitForSingleObject(rodaBuffer.hEventSpaceAvailable, INFINITE);

        WaitForSingleObject(hMutexBufferRoda, INFINITE);
    }

    errno_t err = strncpy_s(rodaBuffer.data[rodaBuffer.tail],
        SMALL_MSG_LENGTH,
        value,
        SMALL_MSG_LENGTH - 1);

    if (err == 0) {
        rodaBuffer.tail = (rodaBuffer.tail + 1) % BUFFER_SIZE;
        rodaBuffer.count++;
        rodaBuffer.isFull = (rodaBuffer.count >= BUFFER_SIZE);
    }
    else {
        printf("Erro ao copiar mensagem para o buffer roda: %d\n", err);
    }

    ReleaseMutex(hMutexBufferRoda); //Libera MUTEX
}

int ReadFromFerroviaBuffer(char* output) {
    WaitForSingleObject(hMutexBufferFerrovia, INFINITE);

    int result = 0;
    if (ferroviaBuffer.count > 0) {
        errno_t err = strncpy_s(output,
            MAX_MSG_LENGTH,
            ferroviaBuffer.data[ferroviaBuffer.head],
            MAX_MSG_LENGTH - 1);

        if (err == 0) {
            ferroviaBuffer.head = (ferroviaBuffer.head + 1) % BUFFER_SIZE;
            ferroviaBuffer.count--;

            // Se estava cheio e agora tem espaço, sinaliza
            if (ferroviaBuffer.isFull && ferroviaBuffer.count < BUFFER_SIZE) {
                ferroviaBuffer.isFull = FALSE;
                SetEvent(ferroviaBuffer.hEventSpaceAvailable);
            }

            result = 1;
        }
        else {
            printf("Erro ao ler mensagem do buffer ferrovia: %d\n", err);
        }
    }

    ReleaseMutex(hMutexBufferFerrovia); //Libera MUTEX
    return result;
}

int ReadFromRodaBuffer(char* output) {
    WaitForSingleObject(hMutexBufferRoda, INFINITE);

    int result = 0;
    if (rodaBuffer.count > 0) {
        errno_t err = strncpy_s(output,
            SMALL_MSG_LENGTH,
            rodaBuffer.data[rodaBuffer.head],
            SMALL_MSG_LENGTH - 1);

        if (err == 0) {
            rodaBuffer.head = (rodaBuffer.head + 1) % BUFFER_SIZE;
            rodaBuffer.count--;

            // Se estava cheio e agora tem espaço, sinaliza
            if (rodaBuffer.isFull && rodaBuffer.count < BUFFER_SIZE) {
                rodaBuffer.isFull = FALSE;
                SetEvent(rodaBuffer.hEventSpaceAvailable);
            }

            result = 1;
        }
        else {
            printf("Erro ao ler mensagem do buffer roda: %d\n", err);
        }
    }
    
    ReleaseMutex(hMutexBufferRoda); //Libera MUTEX
    return result;
}

void PrintBuffers() {
    WaitForSingleObject(hMutexBufferFerrovia, INFINITE);
    WaitForSingleObject(hMutexBufferRoda, INFINITE);
    

    printf("=== Buffers ===\n");
    printf("Ferrovia [%d/%d] %s:\n", ferroviaBuffer.count, BUFFER_SIZE,
        ferroviaBuffer.isFull ? "(CHEIO)" : "");
    int index = ferroviaBuffer.head;
    for (int i = 0; i < ferroviaBuffer.count; i++) {
        printf("%s\n", ferroviaBuffer.data[index]);
        index = (index + 1) % BUFFER_SIZE;
    }

    printf("\nRoda [%d/%d] %s:\n", rodaBuffer.count, BUFFER_SIZE,
        rodaBuffer.isFull ? "(CHEIO)" : "");
    index = rodaBuffer.head;
    for (int i = 0; i < rodaBuffer.count; i++) {
        printf("%s\n", rodaBuffer.data[index]);
        index = (index + 1) % BUFFER_SIZE;
    }
    printf("\n");

    ReleaseMutex(hMutexBufferRoda); //Libera MUTEX
    ReleaseMutex(hMutexBufferFerrovia); //Libera MUTEX
}