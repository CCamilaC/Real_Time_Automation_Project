#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 200
#define MAX_MSG_LENGTH 41  // 40 chars + null terminator (para a mensagem maior)
#define SMALL_MSG_LENGTH 35 // 34 chars + null terminator

// Estrutura da lista circular
typedef struct {
    char data[BUFFER_SIZE][MAX_MSG_LENGTH];
    int head;
    int tail;
    int count;
    BOOL isFull;
    CRITICAL_SECTION cs;  // Seção crítica para proteger o buffer
	HANDLE hEventSpaceAvailable; // Evento para sinalizar espaço disponível
} CircularBuffer;

// Variável global do buffer
extern CircularBuffer ferroviaBuffer;
extern CircularBuffer rodaBuffer;

//Definição dos mutex de proteção dos buffers
extern HANDLE hMutexBufferFerrovia;
extern HANDLE hMutexBufferRoda;

void InitializeBuffers();
void DestroyBuffers();
void WriteToFerroviaBuffer(const char* value);
void WriteToRodaBuffer(const char* value);
int ReadFromFerroviaBuffer(char* output);
int ReadFromRodaBuffer(char* output);
void PrintBuffers();

#endif // CIRCULAR_BUFFER_H