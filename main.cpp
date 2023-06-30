/*  UltimateAnticheat.cpp : This file contains the 'main' function. Program execution begins and ends there. main.cpp contains testing of functionality

    U.A.C. is an 'in-development'/educational anti-cheat written in C++ for x64 platforms (and can be easily ported to x86)
 
    Feature list: 
    1. Anti-dll injection (multiple methods including authenticode enforcement and mitigation policy)
    2. Anti-debugging (multiple methods)
    3. Anti-tamper  (multiple methods including image remapping)
    4. PEB modification
    5. Server-generated shellcode execution (self-unpacking + containing a key in each message which is required to be sent back, ensuring the shellcode was executed), plus cipher-chaining
    6. Client-server heartbeats, version checking, licensing, APIs
    7. Modification of modules: changing loaded module names, symbol names (exports and imports)
    8. TLS callback for anti-debugging + anti-dll injection and thread management
    9. WINAPI calls via 'symbolic hashes' -> stores a list of pointers such that we can call winapi routines by a numeric hash instead of its symbol name

    ... and hopefully soon many more techniques!

    Author: Alex S. ,  github: alsch092 .

*/

#pragma comment(linker, "/ALIGN:0x10000") //for image remapping

#define DLL_PROCESS_DETACH 0
#define DLL_PROCESS_ATTACH 1
#define DLL_THREAD_ATTACH 2
#define DLL_THREAD_DETACH 3

#include "API/API.hpp"

void NTAPI __stdcall TLSCallback(PVOID DllHandle, DWORD dwReason, PVOID Reserved); //in a commercial setting our AC would be in a .dll and the game/process would have the Tls callback
                                                                                   //todo: find way to insert bogus Tls callback into an EXE from a DLL at runtime (modify the directory ptrs to callbacks?)
#ifdef _M_IX86
#pragma comment (linker, "/INCLUDE:__tls_used")
#pragma comment (linker, "/INCLUDE:__tls_callback")
#else
#pragma comment (linker, "/INCLUDE:_tls_used")
#pragma comment (linker, "/INCLUDE:_tls_callback")
#endif
EXTERN_C
#ifdef _M_X64
#pragma const_seg (".CRT$XLB")
const
#else
#pragma data_seg (".CRT$XLB")
#endif

PIMAGE_TLS_CALLBACK _tls_callback = TLSCallback;
#pragma data_seg ()
#pragma const_seg ()

using namespace std;

extern "C" uint64_t shellxor(); //test routine for generating shellcode, can be removed when we are done messing around with shellcode

AntiCheat* g_AC = new AntiCheat(); //global single instance of our AC class

void NTAPI __stdcall TLSCallback(PVOID DllHandle, DWORD dwReason, PVOID Reserved)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
            printf("New process attached\n");
            break;

        case DLL_THREAD_ATTACH:
            printf("New thread spawned!\n");
            //ExitThread(0); //we can stop DLL injecting + DLL debuggers this way, but make sure you're handling your threads carefully.. uncomment line for added anti-debug + injection method!
            break;

        case DLL_THREAD_DETACH:
            printf("Thread detached!\n");
            break;
    };
}

void TestFunction() //called by our 'rogue'/SymLink CreateThread. WINAPI is not directly called for this!
{
    printf("Hello! this thread was made without calling CreateThread directly!\n");
}

bool TestMemoryIntegrity(AntiCheat* AC)
{
    uint64_t module = (uint64_t)GetModuleHandleW(L"UltimateAnticheat.exe");

    if (!module)
    {
        printf("Failed to get current module! %d\n", GetLastError());
        return false;
    }

    DWORD moduleSize = AC->GetProcessObject()->GetMemorySize();

    AC->GetIntegrityChecker()->SetMemoryHashList(AC->GetIntegrityChecker()->GetMemoryHash((uint64_t)module, 0x1000)); //cache the list of hashes we get from the process .text section

    MessageBoxA(0, "Patch over '.text' section memory here to test integrity checking!", 0, 0);

    if (AC->GetIntegrityChecker()->Check((uint64_t)module, 0x1000, AC->GetIntegrityChecker()->GetMemoryHashList()))
    {
        printf("Hashes match! Program appears genuine! Remember to put this inside a TLS callback (and then make sure TLS callback isn't hooked) to ensure we get hashes before memory is tampered.\n");
    }
    else
    {
        printf("Program is modified!\n");
        return true;
    }

    return false;
}

void TestNetworkHeartbeat()
{
    //normally the server would send this packet to us as a heartbeat. the first packet has no 'added' encryption on it, the ones after it do.
    //the first packet sent has no additional encryption, the ones sent after will be encrypted with the secret key of the last request
    BYTE shellcode[] = { 0x54,0x48,0x81,0xEC,0x80,0x00,0x00,0x00,0x51,0xB0,0x08,0x48,0xC7,0xC1,0x01,0x02,0x03,0x04,0x48,0xC7,0xC2,0x37,0x13,0x00,0x00,0x48,0x33,0xCA,0x48,0x81,0xC2,0x34,0x12,0x00,0x00,0x84,0xC0,0xFE,0xC8,0x75,0xF0,0x48,0x8B,0xC1,0x59,0x48,0x81,0xC4,0x80,0x00,0x00,0x00,0x5C,0xC3 };
    PacketWriter* p = new PacketWriter(Packets::Opcodes::SC_HEARTBEAT, shellcode, sizeof(shellcode)); //write opcode onto packet, then buffer

    if (!g_AC->GetNetworkClient()->UnpackAndExecute(p)) //so that we don't need a server running, just simulate a packet. every heartbeat is encrypted using the hash of the last heartbeat/some server gen'd key to prevent emulation
    {
        PacketWriter* packet_1 = new PacketWriter(Packets::Opcodes::SC_HEARTBEAT);
        uint64_t hash = g_AC->GetNetworkClient()->GetResponseHashList().back();

        for (int i = 0; i < sizeof(shellcode); i++) //this time we should xor our 'packet' by the last hash to simulate real environment, if we don't then we will get execution error        
            packet_1->Write<byte>(shellcode[i] ^ (BYTE)hash);
        
        if (!g_AC->GetNetworkClient()->UnpackAndExecute(packet_1)) //we call this a 2nd time to demonstrate how encrypting using the last hash works        
            printf("secret key gen failed: No server is present?\n");
        

        delete packet_1;
    }

    delete p;
}

void TestFunctionalities()
{  
    if (Exports::ChangeFunctionName("KERNEL32.DLL", "LoadLibraryA", "ANTI-INJECT1") &&   ///prevents DLL injection from any method relying on calling LoadLibrary in the host process.
        Exports::ChangeFunctionName("KERNEL32.DLL", "LoadLibraryW", "ANTI-INJECT2") &&
        Exports::ChangeFunctionName("KERNEL32.DLL", "LoadLibraryExA", "ANTI-INJECT3") &&
        Exports::ChangeFunctionName("KERNEL32.DLL", "LoadLibraryExW", "ANTI-INJECT4"))
            printf("Wrote over LoadLibrary export names successfully!\n");
    
    if (Integrity::IsUnknownDllPresent()) //authenticode winapis
    {
        printf("Found unsigned/rogue dll: We only want verified, signed dlls in our application (which is still subject to spoofing)!\n");
        //exit(Error::ROGUE_DLL);
    }

    //can we somehow create inline assembly in x64? this might be possible using macros -> make some routine with extra junk instructions/enough space for our asm, write over the junk instructions @ runtime with custom ASM -> should work but is not flexible and doesnt scale well 

    ULONG_PTR ImageBase = (ULONG_PTR)GetModuleHandle(NULL);

    if (!API::Initialize("LICENSE-123456789", L"explorer.exe")) //license server checks for whitelisted IPs to disallow others from hi-jacking service
    {
        printf("Initializing failed!\n");
        //exit(Error::LICENSE_UNKNOWN);
    }

    TestNetworkHeartbeat();

    g_AC->GetProcessObject()->SetElevated(Process::IsProcessElevated()); //this checks+sets our variable, it does not set our process to being elevated

    if (!g_AC->GetProcessObject()->GetProgramSections("UltimateAnticheat.exe")) //we can stop a routine like this from working if we patch NumberOfSections to 0    
        printf("Failed to parse program sections?\n");
        
    if (!Process::CheckParentProcess(g_AC->GetProcessObject()->GetParentName())) //parent process check, the parent process would normally be set using our API methods
    {
        printf("Parent process was not explorer.exe! hekker detected!\n"); //sometimes people will launch a game from their own process, which we can easily detect if they haven't spoofed it
        //exit(Error::PARENT_PROCESS_MISMATCH);
    }

    TestMemoryIntegrity(g_AC);

    SymbolicHash::CreateThread_Hash(0, 0, (LPTHREAD_START_ROUTINE)&TestFunction, 0, 0, 0); //shows how we can call CreateThread without directly calling winapi, we call our pointer instead which then invokes createthread

    g_AC->GetAntiDebugger()->StartAntiDebugThread();

    std::wstring newModuleName = L"new_name";

    if (Process::ChangeModuleName((wchar_t*)L"UltimateAnticheat.exe", (wchar_t*)newModuleName.c_str())) //in addition to changing export function names, we can also modify the names of loaded modules/libraries.
    {
        wprintf(L"Changed module name to %s!\n", newModuleName.c_str());
    }

    if (AntiCheat::IsVTableHijacked((void*)g_AC))
    {
        printf("VTable of Anticheat has been compromised/hooked.\n");
    }

    if (!g_AC->GetProcessObject()->ProtectProcess()) //todo: find way to stop process attaching or OpenProcess
    {
        printf("Could not protect process.\n");
    }
   
    if (ImageBase)
    {
        if (!RmpRemapImage(ImageBase)) //re-mapping of image to stop patching, and of course we can easily detect if someone bypasses this
        {
            printf("RmpRemapImage failed.\n");
        }
        else
        {
            //check page protections, if they're writable then some cheater has re-mapped our image to make it write-friendly and we need to ban them!
            MEMORY_BASIC_INFORMATION mbi = {};
            
            if (VirtualQueryEx(GetCurrentProcess(), (LPCVOID)ImageBase, &mbi, sizeof(mbi)))
            {
                if (mbi.AllocationProtect != PAGE_READONLY && mbi.State == MEM_COMMIT && mbi.Type == MEM_MAPPED)
                {
                    printf("Cheater! Change back the protections NOW!\n");
                    exit(Error::PAGE_PROTECTIONS_MISMATCH);
                }
            }
        }
    }
    else
    {
        printf("Imagebase was NULL!\n");
        exit(Error::NULL_MEMORY_REFERENCE);
    }

    delete g_AC->GetAntiDebugger(); //clean up all sub-objects
    delete g_AC->GetProcessObject();
    delete g_AC->GetIntegrityChecker();
    delete g_AC;
}

int main(int argc, char** argv)
{
    //  _MessageBox();
    TestFunctionalities();
    return 0;
}
