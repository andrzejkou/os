
#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096

// вспомогательная структура для создания по сути массива индексов для фреймов,
// представленного "страницами", виртуальная память
typedef struct {
  int used;
  int frameNum;
  int blockStart;
  size_t amount;
} Page;

// менеджер значений
typedef struct {
  uint8_t *physicalMemory;
  Page *pageTable;

  // массив состояний ячеек (0 и 1)
  int *frameUsed;
  size_t pageCount;
  size_t frameCount;
} MemoryManager;

int initMemory(MemoryManager *manager, size_t pageCountEX, size_t frameCountEX);
size_t allocateMemory(MemoryManager *manager, size_t size);
int writeMemory(MemoryManager *manager, const void *dataEx, size_t adress,
                size_t size);
int readMemory(MemoryManager *manager, void *dataEx, size_t adress,
               size_t size);
int freeMemory(MemoryManager *manager, size_t adress);
void destroyFrames(MemoryManager *manager);

#endif
