#ifndef __DBG_HELP_H__
#define __DBG_HELP_H__

#include <windows.h>


#define SYMOPT_LOAD_LINES 0x00000010


typedef struct _IMAGEHLP_LINE64 {
  DWORD   SizeOfStruct;
  PVOID   Key;
  DWORD   LineNumber;
  PTSTR   FileName;
  DWORD64 Address;
} IMAGEHLP_LINE64, *PIMAGEHLP_LINE64;

typedef enum {
  AddrMode1616,
  AddrMode1632,
  AddrModeReal,
  AddrModeFlat
} ADDRESS_MODE;

typedef struct _tagADDRESS64 {
  DWORD64      Offset;
  WORD         Segment;
  ADDRESS_MODE Mode;
} ADDRESS64, *LPADDRESS64;

typedef struct _KDHELP64 {
  DWORD64 Thread;
  DWORD   ThCallbackStack;
  DWORD   ThCallbackBStore;
  DWORD   NextCallback;
  DWORD   FramePointer;
  DWORD64 KiCallUserMode;
  DWORD64 KeUserCallbackDispatcher;
  DWORD64 SystemRangeStart;
  DWORD64 KiUserExceptionDispatcher;
  DWORD64 StackBase;
  DWORD64 StackLimit;
  DWORD64 Reserved[5];
} KDHELP64, *PKDHELP64;

typedef struct _tagSTACKFRAME64 {
  ADDRESS64 AddrPC;
  ADDRESS64 AddrReturn;
  ADDRESS64 AddrFrame;
  ADDRESS64 AddrStack;
  ADDRESS64 AddrBStore;
  PVOID     FuncTableEntry;
  DWORD64   Params[4];
  BOOL      Far;
  BOOL      Virtual;
  DWORD64   Reserved[3];
  KDHELP64  KdHelp;
} STACKFRAME64, *LPSTACKFRAME64;

typedef BOOL CALLBACK PREAD_PROCESS_MEMORY_ROUTINE64(
    HANDLE hProcess, DWORD64 lpBaseAddress, PVOID lpBuffer,
    DWORD nSize, LPDWORD lpNumberOfBytesRead );
typedef PVOID CALLBACK PFUNCTION_TABLE_ACCESS_ROUTINE64(
    HANDLE hProcess, DWORD64 AddrBase );
typedef DWORD64 CALLBACK PGET_MODULE_BASE_ROUTINE64(
    HANDLE hProcess, DWORD64 Address );
typedef DWORD64 CALLBACK PTRANSLATE_ADDRESS_ROUTINE64(
    HANDLE hProcess, HANDLE hThread, LPADDRESS64 lpaddr );


DWORD WINAPI SymSetOptions( DWORD SymOptions );

BOOL WINAPI SymInitialize(
    HANDLE hProcess, LPCTSTR UserSearchPath, BOOL fInvadeProcess );

PVOID WINAPI SymFunctionTableAccess64(
    HANDLE hProcess, DWORD64 AddrBase );

DWORD64 WINAPI SymGetModuleBase64(
    HANDLE hProcess, DWORD64 dwAddr );

BOOL WINAPI StackWalk64(
  DWORD MachineType, HANDLE hProcess, HANDLE hThread,
  LPSTACKFRAME64 StackFrame, PVOID ContextRecord,
  PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
  PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
  PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
  PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress );

BOOL WINAPI SymGetLineFromAddr64(
  HANDLE hProcess, DWORD64 dwAddr, PDWORD pdwDisplacement,
  PIMAGEHLP_LINE64 Line );

BOOL WINAPI SymCleanup( HANDLE hProcess );

#endif
