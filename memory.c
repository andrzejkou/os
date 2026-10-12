
#include "memory.h"

#include <limits.h>
#include <stdlib.h>

int initMemory(MemoryManager *manager, size_t pageCountEX,
               size_t frameCountEX) {
  if (manager == NULL || pageCountEX == 0 || frameCountEX == 0 ||
      frameCountEX > INT_MAX || pageCountEX > SIZE_MAX / PAGE_SIZE ||
      frameCountEX > SIZE_MAX / PAGE_SIZE ||
      pageCountEX > SIZE_MAX / sizeof(Page) ||
      frameCountEX > SIZE_MAX / sizeof(int))
    return -1;

  manager->physicalMemory = NULL;
  manager->pageTable = NULL;
  manager->frameUsed = NULL;
  manager->pageCount = 0;
  manager->frameCount = 0;

  manager->physicalMemory = malloc(frameCountEX * PAGE_SIZE);
  manager->frameUsed = calloc(frameCountEX, sizeof(int));
  manager->pageTable = malloc(pageCountEX * sizeof(Page));

  if (manager->physicalMemory == NULL || manager->frameUsed == NULL ||
      manager->pageTable == NULL) {
    free(manager->physicalMemory);
    free(manager->frameUsed);
    free(manager->pageTable);
    manager->physicalMemory = NULL;
    manager->frameUsed = NULL;
    manager->pageTable = NULL;
    return -1;
  }

  manager->pageCount = pageCountEX;
  manager->frameCount = frameCountEX;

  for (size_t i = 0; i < pageCountEX; i++) {
    manager->pageTable[i].used = 0;
    manager->pageTable[i].frameNum = -1;
    manager->pageTable[i].blockStart = 0;
    manager->pageTable[i].amount = 0;
  }

  return 0;
}

size_t allocateMemory(MemoryManager *manager, size_t size) {
  if (manager == NULL || manager->pageTable == NULL ||
      manager->frameUsed == NULL || size == 0)
    return SIZE_MAX;

  size_t pagesNeed = size / PAGE_SIZE;

  if (size % PAGE_SIZE != 0)
    pagesNeed++;

  if (pagesNeed > manager->pageCount || pagesNeed > manager->frameCount)
    return SIZE_MAX;

  size_t freePages = 0;
  size_t startPage = 0;
  int found = 0;

  for (size_t i = 0; i < manager->pageCount; i++) {
    if (manager->pageTable[i].used == 0) {
      if (freePages == 0)
        startPage = i;

      freePages++;

      if (freePages == pagesNeed) {
        found = 1;
        break;
      }
    } else {
      freePages = 0;
    }
  }

  if (!found)
    return SIZE_MAX;

  size_t count = 0;

  for (size_t i = 0; i < manager->frameCount && count < pagesNeed; i++) {
    if (manager->frameUsed[i] == 0)
      count++;
  }

  if (count < pagesNeed)
    return SIZE_MAX;

  size_t currentPage = startPage;

  for (size_t i = 0;
       i < manager->frameCount && currentPage - startPage < pagesNeed; i++) {
    if (manager->frameUsed[i] == 0) {
      manager->frameUsed[i] = 1;
      manager->pageTable[currentPage].used = 1;
      manager->pageTable[currentPage].frameNum = (int)i;
      manager->pageTable[currentPage].blockStart = 0;
      manager->pageTable[currentPage].amount = 0;
      currentPage++;
    }
  }

  manager->pageTable[startPage].blockStart = 1;
  manager->pageTable[startPage].amount = size;

  return startPage * PAGE_SIZE;
}

int writeMemory(MemoryManager *manager, const void *dataEx, size_t adress,
                size_t size) {
  if (size == 0)
    return 0;

  if (manager == NULL || manager->pageTable == NULL ||
      manager->physicalMemory == NULL || dataEx == NULL)
    return -1;

  size_t pageNum = adress / PAGE_SIZE;
  size_t offset = adress % PAGE_SIZE;

  if (pageNum >= manager->pageCount || manager->pageTable[pageNum].used == 0)
    return -1;

  size_t startPage = pageNum;

  while (startPage > 0 && manager->pageTable[startPage].blockStart == 0) {
    if (manager->pageTable[startPage].used == 0)
      return -1;

    startPage--;
  }

  if (manager->pageTable[startPage].used == 0 ||
      manager->pageTable[startPage].blockStart == 0)
    return -1;

  size_t position = adress - startPage * PAGE_SIZE;
  size_t amount = manager->pageTable[startPage].amount;

  if (position >= amount || size > amount - position)
    return -1;

  const unsigned char *bytesData = dataEx;
  int frameNum = manager->pageTable[pageNum].frameNum;

  for (size_t i = 0; i < size; i++) {
    if (offset == PAGE_SIZE) {
      offset = 0;
      pageNum++;
      frameNum = manager->pageTable[pageNum].frameNum;
    }

    manager->physicalMemory[(size_t)frameNum * PAGE_SIZE + offset] =
        bytesData[i];

    offset++;
  }

  return 0;
}

int readMemory(MemoryManager *manager, void *dataEx, size_t adress,
               size_t size) {
  if (size == 0)
    return 0;

  if (manager == NULL || manager->pageTable == NULL ||
      manager->physicalMemory == NULL || dataEx == NULL)
    return -1;

  size_t pageNum = adress / PAGE_SIZE;
  size_t offset = adress % PAGE_SIZE;

  if (pageNum >= manager->pageCount || manager->pageTable[pageNum].used == 0)
    return -1;

  size_t startPage = pageNum;

  while (startPage > 0 && manager->pageTable[startPage].blockStart == 0) {
    if (manager->pageTable[startPage].used == 0)
      return -1;

    startPage--;
  }

  if (manager->pageTable[startPage].used == 0 ||
      manager->pageTable[startPage].blockStart == 0)
    return -1;

  size_t position = adress - startPage * PAGE_SIZE;
  size_t amount = manager->pageTable[startPage].amount;

  if (position >= amount || size > amount - position)
    return -1;

  unsigned char *bytesData = dataEx;
  int frameNum = manager->pageTable[pageNum].frameNum;

  for (size_t i = 0; i < size; i++) {
    if (offset == PAGE_SIZE) {
      offset = 0;
      pageNum++;
      frameNum = manager->pageTable[pageNum].frameNum;
    }

    bytesData[i] =
        manager->physicalMemory[(size_t)frameNum * PAGE_SIZE + offset];

    offset++;
  }

  return 0;
}

int freeMemory(MemoryManager *manager, size_t adress) {
  if (manager == NULL || manager->pageTable == NULL ||
      manager->frameUsed == NULL || manager->physicalMemory == NULL ||
      adress % PAGE_SIZE != 0)
    return -1;

  size_t pageNum = adress / PAGE_SIZE;

  if (pageNum >= manager->pageCount || manager->pageTable[pageNum].used == 0 ||
      manager->pageTable[pageNum].blockStart == 0)
    return -1;

  size_t size = manager->pageTable[pageNum].amount;
  size_t pagesNeed = size / PAGE_SIZE;

  if (size % PAGE_SIZE != 0)
    pagesNeed++;

  if (size == 0 || pagesNeed > manager->pageCount - pageNum)
    return -1;

  for (size_t i = pageNum; i < pageNum + pagesNeed; i++) {
    int frameNum = manager->pageTable[i].frameNum;

    if (manager->pageTable[i].used == 0 || frameNum < 0 ||
        (size_t)frameNum >= manager->frameCount)
      return -1;
  }

  for (size_t i = pageNum; i < pageNum + pagesNeed; i++) {
    int frameNum = manager->pageTable[i].frameNum;

    for (size_t j = 0; j < PAGE_SIZE; j++)
      manager->physicalMemory[(size_t)frameNum * PAGE_SIZE + j] = 0;

    manager->frameUsed[frameNum] = 0;
    manager->pageTable[i].frameNum = -1;
    manager->pageTable[i].blockStart = 0;
    manager->pageTable[i].amount = 0;
    manager->pageTable[i].used = 0;
  }

  return 0;
}

void destroyFrames(MemoryManager *manager) {
  if (manager == NULL)
    return;

  free(manager->physicalMemory);
  free(manager->pageTable);
  free(manager->frameUsed);

  manager->physicalMemory = NULL;
  manager->pageTable = NULL;
  manager->frameUsed = NULL;
  manager->pageCount = 0;
  manager->frameCount = 0;
}
