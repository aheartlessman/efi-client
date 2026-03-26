#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <mutex>

#define STEALTH_MAGIC 0x544C41455453ULL // "STEALT"

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((LONG)0x00000000L)
#endif
#ifndef STATUS_UNSUCCESSFUL
#define STATUS_UNSUCCESSFUL ((LONG)0xC0000001L)
#endif
#ifndef STATUS_PENDING
#define STATUS_PENDING ((LONG)0x00000103L)
#endif
#ifndef STATUS_TIMEOUT
#define STATUS_TIMEOUT ((LONG)0x00000102L)
#endif

#define MAX_DATA_SIZE       1024
#define MAX_MODULE_NAME     256

#pragma pack(push, 1)
enum INSTRUCTIONS : ULONG
{
    WRITE_KERNEL_MEMORY = 0,
    WRITE_PROCESS_MEMORY,
    READ_KERNEL_MEMORY,
    READ_PROCESS_MEMORY,
    ALLOCATE_MEMORY,
    FREE_MEMORY,
    PROTECT_MEMORY,
    ATTACH_PROCESS,
    OPEN_PROCESS,
    READ_PROCESS_MEMORY64,
    WRITE_PROCESS_MEMORY64,
    GET_MOD_INFO
};

typedef struct _NULL_MEMORY
{
    volatile ULONG64 magic;
    volatile LONG status;
    volatile UCHAR req_pending;
    volatile UCHAR completed;
    UCHAR _padding[2];

    ULONG instruction;
    ULONG pid;
    ULONG64 address;
    ULONG64 size;
    ULONG64 buffer_address;
    ULONG64 allocate_base;
    ULONG protect;
    ULONG _padding2;

    ULONG64 BaseAddress;
    char module_name_buffer[MAX_MODULE_NAME];

    UCHAR data[MAX_DATA_SIZE];
} NULL_MEMORY, * PNULL_MEMORY;
#pragma pack(pop)

class driver_t {
private:
    PNULL_MEMORY pData = nullptr;
    std::mutex m_mutex;
    bool m_privilegeAcquired = false;

    void ensureBuffer();
    bool acquirePrivilege();

public:
    driver_t() = default;
    ~driver_t();

    driver_t(const driver_t&) = delete;
    driver_t& operator=(const driver_t&) = delete;

    static driver_t& singleton() {
        static driver_t instance;
        return instance;
    }

    bool connect();
    void disconnect();
    LONG send_instruction(INSTRUCTIONS instr);
    void setup_cmd(INSTRUCTIONS instr, uint32_t pid, uint64_t address, uint64_t size);

    template<typename T>
    T read(uint32_t pid, uint64_t address) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (sizeof(T) > MAX_DATA_SIZE || !connect()) {
            return T{};
        }

        setup_cmd(READ_PROCESS_MEMORY, pid, address, sizeof(T));

        if (send_instruction(READ_PROCESS_MEMORY) == STATUS_SUCCESS) {
            T buffer{};
            memcpy(&buffer, pData->data, sizeof(T));
            return buffer;
        }
        return T{};
    }

    template<typename T>
    bool write(uint32_t pid, uint64_t address, const T& value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (sizeof(T) > MAX_DATA_SIZE || !connect()) return false;

        setup_cmd(WRITE_PROCESS_MEMORY, pid, address, sizeof(T));
        memcpy(pData->data, &value, sizeof(T));

        return send_instruction(WRITE_PROCESS_MEMORY) == STATUS_SUCCESS;
    }

    bool read_bytes(uint32_t pid, uint64_t address, void* buffer, size_t size);
    bool write_bytes(uint32_t pid, uint64_t address, const void* buffer, size_t size);
    uint64_t get_module_base(uint32_t pid, const char* name);
    bool WaitForDriver(int timeout_ms = -1);
};

inline driver_t& get_driver() { return driver_t::singleton(); }
