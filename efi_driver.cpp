#include "efi_driver.h"
#include <iostream>

static const LPCWSTR VARIABLE_NAME_CMD = L"StealthCmd";
static const LPCWSTR VARIABLE_NAME_RESULT = L"StealthResult";
static const LPCWSTR DUMMY_GUID = L"{00000000-0000-0000-0000-000000000000}";

void driver_t::ensureBuffer() {
    if (!pData) {
        pData = static_cast<PNULL_MEMORY>(malloc(sizeof(NULL_MEMORY)));
        if (pData) {
            memset(pData, 0, sizeof(NULL_MEMORY));
        }
    }
}

bool driver_t::acquirePrivilege() {
    if (m_privilegeAcquired) return true;

    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    TOKEN_PRIVILEGES tkp = {};
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    LookupPrivilegeValue(NULL, SE_SYSTEM_ENVIRONMENT_NAME, &tkp.Privileges[0].Luid);
    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);

    DWORD err = GetLastError();
    CloseHandle(hToken);

    m_privilegeAcquired = (err == ERROR_SUCCESS);
    return m_privilegeAcquired;
}

driver_t::~driver_t() {
    disconnect();
    if (pData) {
        free(pData);
        pData = nullptr;
    }
}

bool driver_t::connect() {
    return WaitForDriver(100);
}

void driver_t::disconnect() {
    // Stateless protocol, nothing to close
}

bool driver_t::WaitForDriver(int timeout_ms) {
    if (!acquirePrivilege()) {
        return false;
    }

    NULL_MEMORY Temp = {};

    if (GetFirmwareEnvironmentVariableW(VARIABLE_NAME_RESULT, DUMMY_GUID, &Temp, sizeof(Temp)) == 0) {
        return false;
    }

    return (Temp.magic == STEALTH_MAGIC);
}

LONG driver_t::send_instruction(INSTRUCTIONS instr) {
    ensureBuffer();
    if (!pData) return STATUS_UNSUCCESSFUL;

    pData->instruction = instr;
    pData->magic = STEALTH_MAGIC;
    pData->status = STATUS_PENDING;
    pData->completed = 0;
    pData->req_pending = 1;

    if (SetFirmwareEnvironmentVariableW(VARIABLE_NAME_CMD, DUMMY_GUID, pData, sizeof(NULL_MEMORY)) == 0) {
        return STATUS_UNSUCCESSFUL;
    }

    if (GetFirmwareEnvironmentVariableW(VARIABLE_NAME_RESULT, DUMMY_GUID, pData, sizeof(NULL_MEMORY)) == 0) {
        return STATUS_UNSUCCESSFUL;
    }

    return pData->status;
}

void driver_t::setup_cmd(INSTRUCTIONS instr, uint32_t pid, uint64_t address, uint64_t size) {
    ensureBuffer();
    if (!pData) return;
    pData->pid = pid;
    pData->address = address;
    pData->size = size;
}

bool driver_t::read_bytes(uint32_t pid, uint64_t address, void* buffer, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!buffer || size == 0 || !connect()) return false;

    size_t bytesRead = 0;
    while (bytesRead < size) {
        size_t chunkSize = (size - bytesRead > MAX_DATA_SIZE) ? MAX_DATA_SIZE : (size - bytesRead);

        setup_cmd(READ_PROCESS_MEMORY, pid, address + bytesRead, chunkSize);

        if (send_instruction(READ_PROCESS_MEMORY) != STATUS_SUCCESS) {
            return false;
        }

        memcpy(static_cast<uint8_t*>(buffer) + bytesRead, pData->data, chunkSize);
        bytesRead += chunkSize;
    }
    return true;
}

bool driver_t::write_bytes(uint32_t pid, uint64_t address, const void* buffer, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!buffer || size == 0 || !connect()) return false;

    size_t bytesWritten = 0;
    while (bytesWritten < size) {
        size_t chunkSize = (size - bytesWritten > MAX_DATA_SIZE) ? MAX_DATA_SIZE : (size - bytesWritten);

        setup_cmd(WRITE_PROCESS_MEMORY, pid, address + bytesWritten, chunkSize);
        memcpy(pData->data, static_cast<const uint8_t*>(buffer) + bytesWritten, chunkSize);

        if (send_instruction(WRITE_PROCESS_MEMORY) != STATUS_SUCCESS) {
            return false;
        }

        bytesWritten += chunkSize;
    }
    return true;
}

uint64_t driver_t::get_module_base(uint32_t pid, const char* name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!name || !connect()) return 0;

    ensureBuffer();
    if (!pData) return 0;

    pData->pid = pid;
    strcpy_s(pData->module_name_buffer, sizeof(pData->module_name_buffer), name);

    LONG status = send_instruction(GET_MOD_INFO);
    if (status == STATUS_SUCCESS) {
        return pData->BaseAddress;
    }
    return 0;
}
