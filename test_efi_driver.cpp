#include <iostream>
#include <vector>
#include "efi_driver.h"

int main() {
    printf("[*] EFI Memory Client (Shared Memory Mode)\n");

    auto& driver = get_driver();

    printf("[*] Connecting...\n");
    // Wait forever for driver to find us
    if (!driver.WaitForDriver(-1)) {
        printf("[-] Failed to connect.\n");
        return 1;
    }

    printf("[+] Connected! Testing Physical Memory Read using PID=0...\n");

    // Test: Read first 8 bytes of physical memory (usually legacy standard)
    // We use PID 0 for physical read as per our driver implementation convention
    // (Wait, driver logic handles READ_PROCESS_MEMORY by checking PID?)
    // Our driver stub calls ReadProcessMemory -> checks PID==0 -> Calls ReadPhysical.
    // So we use PID 0.
    
    UINT64 val = driver.read<UINT64>(0, 0x1000); // Read physical address 0x1000
    printf("[*] Read [0x1000]: 0x%llX\n", val);

    printf("[*] Press Enter to exit...\n");
    std::cin.get();

    return 0;
}
