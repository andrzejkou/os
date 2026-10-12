#include "memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t assertionsPassed = 0;

#define TEST(condition)                                                        \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "\nОШИБКА: %s (строка %d)\n", #condition, __LINE__);     \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
    assertionsPassed++;                                                        \
  } while (0)

static void printMapping(MemoryManager *manager, size_t address, size_t size) {
  size_t firstPage = address / PAGE_SIZE;
  size_t lastPage = (address + size - 1) / PAGE_SIZE;

  for (size_t page = firstPage; page <= lastPage; page++) {
    printf("  Виртуальная страница %zu -> физический фрейм %d\n", page,
           manager->pageTable[page].frameNum);
  }
}

void testString(void) {
  puts("\n========================================");
  puts("ТЕСТ 1. Запись и чтение строки");
  puts("========================================");

  MemoryManager manager;
  TEST(initMemory(&manager, 16, 16) == 0);
  puts("Менеджер создан: 16 виртуальных страниц, 16 физических фреймов.");

  char data[] = "Hello World";
  char result[sizeof(data)] = {0};
  size_t size = sizeof(data);
  size_t address = allocateMemory(&manager, size);

  TEST(address != SIZE_MAX);
  printf("Запрошено байтов: %zu (включая завершающий \\0).\n", size);
  printf("Начальный виртуальный адрес: %zu\n", address);
  printMapping(&manager, address, size);

  TEST(writeMemory(&manager, data, address, size) == 0);
  printf("Записано: \"%s\"\n", data);

  TEST(readMemory(&manager, result, address, sizeof(result)) == 0);
  printf("Прочитано: \"%s\"\n", result);
  TEST(strcmp(data, result) == 0);
  puts("Результат сравнения: данные совпадают.");

  destroyFrames(&manager);
  puts("ИТОГ: PASS — строка записана и прочитана без изменений.");
}

void testSeveralPages(void) {
  puts("\n========================================");
  puts("ТЕСТ 2. Запись данных через три страницы");
  puts("========================================");

  MemoryManager manager;
  TEST(initMemory(&manager, 8, 8) == 0);

  const size_t size = PAGE_SIZE * 2 + 137;
  unsigned char *data = malloc(size);
  unsigned char *result = malloc(size);
  TEST(data != NULL && result != NULL);

  for (size_t i = 0; i < size; i++)
    data[i] = (unsigned char)(i % 251);

  size_t address = allocateMemory(&manager, size);
  TEST(address != SIZE_MAX);

  printf("Размер данных: %zu байт.\n", size);
  printf("Размер страницы: %d байт. Требуется страниц: %zu.\n", PAGE_SIZE,
         (size + PAGE_SIZE - 1) / PAGE_SIZE);
  printf("Начальный виртуальный адрес: %zu\n", address);
  printMapping(&manager, address, size);

  TEST(writeMemory(&manager, data, address, size) == 0);
  puts("Запись завершена.");
  TEST(readMemory(&manager, result, address, size) == 0);
  puts("Чтение завершено.");
  TEST(memcmp(data, result, size) == 0);
  printf("Сравнение %zu байтов: данные совпадают.\n", size);

  free(data);
  free(result);
  destroyFrames(&manager);
  puts("ИТОГ: PASS — корректная запись и чтение через три страницы.");
}

void testOffsetAndBounds(void) {
  puts("\n========================================");
  puts("ТЕСТ 3. Смещение, граница страницы и выход за границы блока");
  puts("========================================");

  MemoryManager manager;
  TEST(initMemory(&manager, 4, 4) == 0);

  size_t size = PAGE_SIZE + 10;
  size_t address = allocateMemory(&manager, size);
  TEST(address != SIZE_MAX);
  printf("Выделено: %zu байт, начальный адрес: %zu.\n", size, address);

  unsigned char data[20];
  unsigned char result[20] = {0};
  for (size_t i = 0; i < sizeof(data); i++)
    data[i] = (unsigned char)(i + 1);

  size_t crossingAddress = address + PAGE_SIZE - 10;
  printf("Записываем %zu байт с адреса %zu (смещение %zu в странице).\n",
         sizeof(data), crossingAddress, crossingAddress % PAGE_SIZE);

  TEST(writeMemory(&manager, data, crossingAddress, sizeof(data)) == 0);
  TEST(readMemory(&manager, result, crossingAddress, sizeof(result)) == 0);
  TEST(memcmp(data, result, sizeof(data)) == 0);
  puts("Запись и чтение через границу страницы прошли успешно.");

  int writeTooFar =
      writeMemory(&manager, data, address + PAGE_SIZE - 9, sizeof(data));
  printf("Попытка записать 20 байт за пределами блока: код %d (ожидался -1).\n",
         writeTooFar);
  TEST(writeTooFar == -1);

  int readTooFar = readMemory(&manager, result, address + PAGE_SIZE + 10, 1);
  printf("Попытка чтения сразу за концом блока: код %d (ожидался -1).\n",
         readTooFar);
  TEST(readTooFar == -1);

  int writeFromMiddle = writeMemory(&manager, data, address + 1, size);
  printf("Попытка записи размера всего блока со смещением +1: код %d (ожидался "
         "-1).\n",
         writeFromMiddle);
  TEST(writeFromMiddle == -1);

  int invalidAddress = readMemory(&manager, result, SIZE_MAX, 1);
  printf("Попытка чтения по недопустимому адресу SIZE_MAX: код %d (ожидался "
         "-1).\n",
         invalidAddress);
  TEST(invalidAddress == -1);

  destroyFrames(&manager);
  puts("ИТОГ: PASS — переход через страницу работает, выходы за границы "
       "отклоняются.");
}

void testFreeAndReuse(void) {
  puts("\n========================================");
  puts("ТЕСТ 4. Освобождение блоков и повторное использование фреймов");
  puts("========================================");

  MemoryManager manager;
  TEST(initMemory(&manager, 8, 8) == 0);

  size_t a = allocateMemory(&manager, PAGE_SIZE);
  size_t b = allocateMemory(&manager, PAGE_SIZE);
  size_t c = allocateMemory(&manager, PAGE_SIZE);
  TEST(a == 0 && b == PAGE_SIZE && c == PAGE_SIZE * 2);

  printf("Блок A: адрес %zu, фрейм %d\n", a,
         manager.pageTable[a / PAGE_SIZE].frameNum);
  printf("Блок B: адрес %zu, фрейм %d\n", b,
         manager.pageTable[b / PAGE_SIZE].frameNum);
  printf("Блок C: адрес %zu, фрейм %d\n", c,
         manager.pageTable[c / PAGE_SIZE].frameNum);

  char keep[] = "Соседний блок";
  TEST(writeMemory(&manager, keep, c, sizeof(keep)) == 0);
  puts("В блок C записан текст «Соседний блок».");

  TEST(freeMemory(&manager, b) == 0);
  printf("Блок B освобождён; статус его страницы: %d.\n",
         manager.pageTable[b / PAGE_SIZE].used);
  TEST(freeMemory(&manager, b) == -1);
  TEST(readMemory(&manager, keep, b, 1) == -1);
  puts("Повторное освобождение и чтение из освобождённого блока отклонены.");

  size_t d = allocateMemory(&manager, PAGE_SIZE * 2);
  TEST(d == PAGE_SIZE * 3);
  printf("Блок D (2 страницы): адрес %zu, фреймы %d и %d.\n", d,
         manager.pageTable[3].frameNum, manager.pageTable[4].frameNum);
  TEST(manager.pageTable[3].frameNum == 1);
  TEST(manager.pageTable[4].frameNum == 3);

  unsigned char payload[PAGE_SIZE * 2];
  unsigned char result[PAGE_SIZE * 2];
  for (size_t i = 0; i < sizeof(payload); i++)
    payload[i] = (unsigned char)(i % 193);

  TEST(writeMemory(&manager, payload, d, sizeof(payload)) == 0);
  TEST(readMemory(&manager, result, d, sizeof(result)) == 0);
  TEST(memcmp(payload, result, sizeof(payload)) == 0);
  printf("Проверка блока D: %zu байтов совпадают.\n", sizeof(payload));

  char neighbour[sizeof(keep)] = {0};
  TEST(readMemory(&manager, neighbour, c, sizeof(neighbour)) == 0);
  TEST(strcmp(keep, neighbour) == 0);
  printf("Соседний блок C после операций содержит: «%s».\n", neighbour);

  int interiorFree = freeMemory(&manager, d + PAGE_SIZE);
  printf("Попытка освободить D со второй страницы: код %d (ожидался -1).\n",
         interiorFree);
  TEST(interiorFree == -1);

  TEST(freeMemory(&manager, d) == 0);
  size_t e = allocateMemory(&manager, PAGE_SIZE);
  TEST(e == b);
  printf("После освобождения D новый блок E получил адрес %zu.\n", e);
  TEST(manager.physicalMemory[PAGE_SIZE] == 0);
  puts("Первый байт повторно используемого физического фрейма равен 0.");

  destroyFrames(&manager);
  puts("ИТОГ: PASS — фреймы возвращаются в пул и используются повторно.");
}

void testOutOfMemory(void) {
  puts("\n========================================");
  puts("ТЕСТ 5. Нехватка физической памяти");
  puts("========================================");

  MemoryManager manager;
  TEST(initMemory(&manager, 8, 2) == 0);
  puts(
      "Создан менеджер: 8 виртуальных страниц, но только 2 физических фрейма.");

  size_t a = allocateMemory(&manager, PAGE_SIZE * 2);
  TEST(a == 0);
  printf("Выделен блок размером %d байт по адресу %zu.\n", PAGE_SIZE * 2, a);

  size_t noMemory = allocateMemory(&manager, 1);
  printf("Следующее выделение 1 байта: %s.\n",
         noMemory == SIZE_MAX ? "отклонено — свободных фреймов нет"
                              : "неожиданный успех");
  TEST(noMemory == SIZE_MAX);

  size_t zeroSize = allocateMemory(&manager, 0);
  printf("Выделение блока нулевого размера: %s.\n",
         zeroSize == SIZE_MAX ? "отклонено" : "неожиданный успех");
  TEST(zeroSize == SIZE_MAX);

  TEST(freeMemory(&manager, a) == 0);
  puts("Первый блок освобождён.");

  size_t reused = allocateMemory(&manager, 1);
  printf("После освобождения выделение 1 байта: адрес %zu.\n", reused);
  TEST(reused == 0);

  destroyFrames(&manager);
  destroyFrames(&manager);
  puts("Повторный вызов destroyFrames() также обработан.");
  puts("ИТОГ: PASS — нехватка памяти и освобождение обработаны корректно.");
}

int main(void) {
  puts("ТЕСТИРОВАНИЕ СТРАНИЧНОГО МЕНЕДЖЕРА ПАМЯТИ");

  testString();
  testSeveralPages();
  testOffsetAndBounds();
  testFreeAndReuse();
  testOutOfMemory();

  puts("\n========================================");
  printf("ВСЕ ТЕСТЫ ПРОЙДЕНЫ. Успешных проверок: %zu.\n", assertionsPassed);
  puts("Вывод: основные сценарии выделения, записи, чтения и освобождения "
       "памяти работают.");
  puts("Также проверены переход через границу страницы, недопустимые обращения "
       "и нехватка фреймов.");
  puts("========================================");

  return 0;
}
