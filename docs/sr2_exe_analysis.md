# SR2_pc.exe Analysis

## Overview

- File size: 12,480,512 bytes
- Architecture: 32-bit
- Image base: 0x00400000
- Entry point: 0x031592ED

## Sections

| Name | Virtual Address | Virtual Size | Raw Size | Characteristics |
|------|-----------------|--------------|----------|----------------|
| .text | 0x00001000 | 0x009B61F4 | 0x009B7000 | 0x60000020 |
| .rdata | 0x009B8000 | 0x000C7C28 | 0x000C8000 | 0x40000040 |
| .data | 0x00A80000 | 0x0262639C | 0x00061000 | 0xC0000040 |
| .tls | 0x030A7000 | 0x00001085 | 0x00002000 | 0xC0000040 |
| .rsrc | 0x030A9000 | 0x0000B9C0 | 0x0000C000 | 0x40000040 |
| .reloc | 0x030B5000 | 0x000A32FC | 0x000A4000 | 0x42000040 |
| .bind | 0x03159000 | 0x00054000 | 0x00054000 | 0x60000040 |

## Imports

### DSOUND.dll

- Ordinal_11

### WSOCK32.dll

- Ordinal_101
- Ordinal_13
- Ordinal_1
- Ordinal_113
- Ordinal_57
- Ordinal_52
- Ordinal_14
- Ordinal_9
- Ordinal_51
- Ordinal_22
- Ordinal_111
- Ordinal_16
- Ordinal_112
- Ordinal_4
- Ordinal_6
- Ordinal_2
- Ordinal_11
- Ordinal_116
- Ordinal_115
- Ordinal_151
- Ordinal_18
- Ordinal_7
- Ordinal_21
- Ordinal_12
- Ordinal_3
- Ordinal_17
- Ordinal_23
- Ordinal_20
- Ordinal_8
- Ordinal_15
- Ordinal_10
- Ordinal_19

### KERNEL32.dll

- CompareStringA
- GetCurrentDirectoryA
- GetFullPathNameA
- GetCurrentProcessId
- GetEnvironmentStringsW
- FreeEnvironmentStringsW
- GetEnvironmentStrings
- FreeEnvironmentStringsA
- SetStdHandle
- IsValidCodePage
- TlsGetValue
- GetCurrentThreadId
- ReadFile
- SetLastError
- GetOverlappedResult
- GetLastError
- CloseHandle
- CreateFileA
- InterlockedExchangeAdd
- VirtualAlloc
- InterlockedIncrement
- InterlockedDecrement
- CreateThread
- ExitProcess
- SetCurrentDirectoryA
- Sleep
- GetCommandLineA
- CreateDirectoryA
- SetPriorityClass
- GetCurrentProcess
- EnterCriticalSection
- LeaveCriticalSection
- InitializeCriticalSection
- InterlockedCompareExchange
- DeleteFileA
- GetTickCount
- OutputDebugStringA
- InitializeCriticalSectionAndSpinCount
- DeleteCriticalSection
- TryEnterCriticalSection
- GetLocaleInfoA
- GetSystemTime
- DebugBreak
- WaitForSingleObject
- CreateSemaphoreA
- ReleaseSemaphore
- TerminateThread
- QueryPerformanceFrequency
- QueryPerformanceCounter
- GetTempPathA
- GetCurrentDirectoryW
- GetOEMCP
- GetACP
- SetHandleCount
- FlushFileBuffers
- GetConsoleMode
- CompareStringW
- SetConsoleCtrlHandler
- GetModuleFileNameA
- GetStdHandle
- GetTimeFormatA
- GetDateFormatA
- GetUserDefaultLCID
- EnumSystemLocalesA
- IsValidLocale
- GetStringTypeA
- GetStringTypeW
- GetLocaleInfoW
- FatalAppExitA
- HeapCreate
- HeapDestroy
- HeapSize
- GetCurrentThread
- GetCPInfo
- LCMapStringW
- TlsAlloc
- TlsFree
- TlsSetValue
- LoadLibraryA
- GetProcAddress
- FreeLibrary
- GetExitCodeThread
- GetFileAttributesA
- FindFirstFileA
- FindNextFileA
- FindClose
- LCMapStringA
- RaiseException
- GetDriveTypeA
- TerminateProcess
- InterlockedExchange
- GlobalMemoryStatus
- GetModuleHandleW
- CreateMutexA
- SetThreadPriority
- GetThreadPriority
- VirtualFree
- CreateEventW
- SetEvent
- ResetEvent
- SetFilePointer
- WriteFile
- GetTimeZoneInformation
- CreateMutexW
- ReleaseMutex
- RtlUnwind
- GetStartupInfoA
- GetProcessHeap
- GetVersionExA
- GetFileType
- PeekNamedPipe
- GetFileInformationByHandle
- HeapReAlloc
- GetSystemTimeAsFileTime
- SetEndOfFile
- FileTimeToLocalFileTime
- FileTimeToSystemTime
- GetLocalTime
- GetModuleHandleA
- IsDebuggerPresent
- SetUnhandledExceptionFilter
- UnhandledExceptionFilter
- HeapAlloc
- WriteConsoleA
- GetConsoleOutputCP
- HeapFree
- MultiByteToWideChar
- WideCharToMultiByte
- WriteConsoleW
- SetEnvironmentVariableA
- CreateFileW
- GetConsoleCP

### USER32.dll

- SetFocus
- ClipCursor
- SetCursorPos
- PostMessageW
- FindWindowA
- ClientToScreen
- GetWindowRect
- DispatchMessageW
- TranslateMessage
- PeekMessageW
- SetWindowTextA
- SetForegroundWindow
- SetActiveWindow
- ShowWindow
- ShowCursor
- CreateWindowExA
- GetSystemMetrics
- RegisterClassExA
- ToUnicodeEx
- DefWindowProcA
- GetAsyncKeyState
- OffsetRect
- SetRect
- SetWindowPos
- AdjustWindowRect
- MapVirtualKeyW
- CallNextHookEx
- GetKeyboardLayout
- MapVirtualKeyExW
- GetKeyState
- ToAsciiEx
- wsprintfW
- MessageBoxA
- GetCursorPos
- GetClientRect
- LoadCursorW
- SetCursor
- GetKeyNameTextW
- LoadIconW
- SystemParametersInfoW

### ADVAPI32.dll

- RegCloseKey
- RegSetValueExA
- RegQueryValueExW
- RegOpenKeyExW
- RegOpenKeyExA
- RegQueryValueExA
- RegCreateKeyExA

### SHELL32.dll

- SHGetFolderPathA
- SHGetFolderPathW

### ole32.dll

- CoSetProxyBlanket
- CoInitializeEx
- CoCreateInstance
- CoTaskMemFree

### binkw32.dll

- _BinkOpenDirectSound@4
- _BinkOpenWaveOut@4
- _BinkSetSoundSystem@8
- _BinkPause@8
- _BinkGetSummary@8
- _BinkNextFrame@4
- _BinkWait@4
- _BinkSetVolume@12
- _BinkRegisterFrameBuffers@8
- _BinkGetFrameBuffersInfo@8
- _BinkControlBackgroundIO@8
- _BinkSetSoundOnOff@8
- _BinkSetSoundTrack@8
- _BinkOpen@8
- _BinkClose@4
- _BinkGoto@12
- _BinkDoFrame@4

### DfEngine.dll

- CreateDFEngine

### X3DAudio1_4.dll

- X3DAudioCalculate
- X3DAudioInitialize

### XINPUT1_3.dll

- Ordinal_5
- Ordinal_2
- Ordinal_3

### d3dx9_39.dll

- D3DXCreateTextureFromFileW
- D3DXCreateFontA
- D3DXCreateTextureFromFileInMemory
- D3DXSaveSurfaceToFileA
- D3DXCompileShaderFromFileA

### IMM32.dll

- ImmGetCompositionStringW
- ImmGetContext
- ImmIsUIMessageW
- ImmSetCompositionStringW

### d3d9.dll

- Direct3DCreate9

### DINPUT8.dll

- DirectInput8Create

### GDI32.dll

- GetStockObject

### OLEAUT32.dll

- Ordinal_6
- Ordinal_2


## Known Addresses (from Monkey Patch)

| Address | Description | Notes |
|---------|-------------|-------|
| 0x00520ba0 | WinMain function | Signature: `0x83ec8b55` (push ebp; mov ebp, esp; sub esp) |
| 0x00c9e1c0 | CRT startup hook point | Used to redirect to Hook_WinMain |

## Engine Analysis

### DFEngine.dll
The game uses a custom engine accessed via `DFEngine.dll`. The only exported function is `CreateDFEngine`,
suggesting an object-oriented engine architecture. This DLL is the injection point used by Monkey Patch.

### Rendering
- Direct3D 9 (`d3d9.dll`)
- DirectX shader compilation (`D3DXCompileShaderFromFileA`)
- Shader files: `.fxo_pc` (compiled), `.pso_pc`/`.vso_pc` (pixel/vertex shaders)

### Audio
- DirectSound (`DSOUND.dll`)
- Bink video/audio (`binkw32.dll`) - RAD Game Tools
- X3DAudio for 3D spatial audio

### Input
- DirectInput 8 for gamepad/keyboard
- XInput 1.3 for Xbox controller support

### Timing Bug
The infamous "speedup bug" is caused by `QueryPerformanceFrequency` returning values that
aren't multiples of 1,000,000, causing floating-point precision errors in the game's
timing calculations. The Monkey Patch fixes this by hooking both QPF and QPC to normalize
values to 10,000,000 Hz.

## File Formats Reference

| Extension | Description | Status |
|-----------|-------------|--------|
| .vpp_pc | Volition packfile archive | Documented, parser complete |
| .str2_pc | Streaming archive | Similar to VPP |
| .asm_pc | Asset assembler/streaming index | Needs analysis |
| .xtbl | XML table data | Text-based, readable |
| .vint_doc | UI document definition | Text-based |
| .fxo_pc | Compiled shader effects | D3D9 shader bytecode |
| .pso_pc / .vso_pc | Pixel/Vertex shaders | D3D9 shader bytecode |
| .chunk_pc | World chunk data | Needs analysis |
| .cpeg_pc / .cvbm_pc | Texture packages | Needs analysis |
| .ccmesh_pc | Character mesh | Needs analysis |
| .vf3_pc | Font data | Needs analysis |
