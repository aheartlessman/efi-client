# EFI Client

Windows-клиент для взаимодействия с EFI Memory Driver через Firmware Environment Variables.

## Как это работает

Клиент использует Windows API `SetFirmwareEnvironmentVariable` / `GetFirmwareEnvironmentVariable` для отправки команд драйверу, установленному через EFI Loader.

```
efi_client.exe (этот проект)
     |
     |  SetFirmwareEnvironmentVariableW("StealthCmd", ...)   --> Отправка команды
     |  GetFirmwareEnvironmentVariableW("StealthResult", ...) --> Получение результата
     v
Windows Kernel --> EFI Runtime Services --> EfiMemoryDriver
```

Протокол синхронный: отправил команду через `SetVariable`, сразу читаешь результат через `GetVariable`.

## Связанные репозитории

| Репозиторий | Описание |
|---|---|
| **efi-memory-driver** | UEFI драйвер (должен быть установлен) |
| **efi-loader** | UEFI загрузчик |
| **efi-client** | Windows клиент (этот репо) |

## Требования

- Windows 10/11 x64
- Visual Studio 2022 (v143 toolset)
- Права администратора для запуска
- Установленный EFI Memory Driver (см. efi-memory-driver)

## Структура

```
efi_client/
├── efi_driver.h           # API: класс driver_t, структуры данных
├── efi_driver.cpp         # Реализация коммуникации с драйвером
├── test_efi_driver.cpp    # Тестовое приложение
└── efi_client.vcxproj     # Visual Studio проект
```

## Сборка

### Visual Studio
Открыть `efi_client.vcxproj` в Visual Studio и собрать (Release x64).

### Командная строка
```bash
# Из VS Developer Command Prompt
cl.exe /EHsc /O2 /Fe:efi_client.exe test_efi_driver.cpp efi_driver.cpp
```

## Использование

```cpp
#include "efi_driver.h"

int main() {
    auto& driver = get_driver();

    // Подключение к драйверу (проверяет наличие)
    if (!driver.WaitForDriver(-1)) {
        printf("Driver not found\n");
        return 1;
    }

    // Чтение физической памяти (PID=0 = физический адрес)
    uint64_t value = driver.read<uint64_t>(0, 0x1000);

    // Запись в физическую память
    driver.write<uint64_t>(0, 0x2000, 0xDEADBEEF);

    // Чтение блока данных (автоматическая разбивка по 1024 байт)
    uint8_t buffer[4096];
    driver.read_bytes(0, 0x1000, buffer, sizeof(buffer));

    // Запись блока данных
    driver.write_bytes(0, 0x2000, buffer, sizeof(buffer));

    return 0;
}
```

## API

### `driver_t` (singleton)

| Метод | Описание |
|---|---|
| `get_driver()` | Получить singleton экземпляр |
| `WaitForDriver(timeout_ms)` | Проверить наличие драйвера. `-1` = ждать бесконечно |
| `read<T>(pid, address)` | Прочитать значение типа T |
| `write<T>(pid, address, value)` | Записать значение типа T |
| `read_bytes(pid, addr, buf, size)` | Прочитать блок байт (chunked) |
| `write_bytes(pid, addr, buf, size)` | Записать блок байт (chunked) |
| `get_module_base(pid, name)` | Получить базовый адрес модуля (TODO) |

### Конвенция PID

| PID | Режим |
|---|---|
| `0` | Прямой доступ к физической памяти |
| `> 0` | Память процесса через CR3 (пока не реализовано) |

## Важно

- Приложение должно запускаться **от имени администратора**
- Автоматически запрашивает привилегию `SE_SYSTEM_ENVIRONMENT_NAME`
- Thread-safe: все операции защищены мьютексом
- Максимальный размер одной операции: 1024 байт (автоматический chunking для больших запросов)
