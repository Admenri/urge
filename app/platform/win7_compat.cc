// Windows 7 compatibility link hooks.
//
// The static CRT, the static C++ library, SDL and the Rust parts of wgpu
// import a handful of functions that only exist from Windows 8 on.  Windows 7
// refuses to load the process before main() runs because those imports cannot
// be resolved:
//
//   api-ms-win-core-synch-l1-2-0.dll   WaitOnAddress, WakeByAddress{All,Single}
//   api-ms-win-core-registry-l1-1-0.dll  RegOpenKeyEx{A,W}, ...
//   api-ms-win-core-registry-l1-1-1.dll  RegSetKeyValueW, RegDeleteKeyValueW
//   bcryptprimitives.dll               ProcessPrng
//   KERNEL32.dll                       CreateFile2, CopyFile2,
//                                      GetSystemTimePreciseAsFileTime
//
// Each hook defines the `__imp_*` slot the compiler calls through, so the
// reference resolves inside the image and the import library is never pulled
// in.  The wrapper forwards to the real API when the running system has it
// (GetProcAddress), and otherwise implements a fallback using APIs Windows 7
// does have.  On a modern system this is a single cached indirect call; on
// Windows 7 it is the fallback.
//
// Linked into the Game executable only when URGE_WIN7_COMPAT is ON.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <bcrypt.h>

#include <cstring>

#pragma comment(lib, "bcrypt.lib")

namespace {

template <typename Fn>
Fn resolve(HMODULE module, const char *name)
{
#ifdef URGE_WIN7_FORCE_FALLBACK
    // The standalone fallback test compiles with this to run the Windows 7
    // code paths on a newer system.
    (void)module;
    (void)name;
    return nullptr;
#else
    return module ? reinterpret_cast<Fn>(GetProcAddress(module, name)) : nullptr;
#endif
}

HMODULE kernel32_module()
{
    static HMODULE module = GetModuleHandleW(L"kernel32.dll");
    return module;
}

HMODULE advapi32_module()
{
    static HMODULE module = GetModuleHandleW(L"advapi32.dll");
    if (!module) module = LoadLibraryW(L"advapi32.dll");
    return module;
}

// --- WaitOnAddress / WakeByAddress ----------------------------------------
//
// Windows 7 has condition variables and critical sections, so the fallback is
// a small address-keyed wait list guarded by one lock.  The waiter enlists
// before it re-checks the compared value, which closes the missed-wakeup race
// between a waker that finds no waiter and a waiter that has not enlisted yet.
// The real API is used whenever the system exports it.

using PFN_WaitOnAddress = BOOL(WINAPI *)(volatile VOID *, PVOID, SIZE_T, DWORD);
using PFN_WakeByAddress = void(WINAPI *)(PVOID);

PFN_WaitOnAddress real_wait_on_address()
{
    static PFN_WaitOnAddress fn =
        resolve<PFN_WaitOnAddress>(kernel32_module(), "WaitOnAddress");
    return fn;
}

PFN_WakeByAddress real_wake_by_address_single()
{
    static PFN_WakeByAddress fn =
        resolve<PFN_WakeByAddress>(kernel32_module(), "WakeByAddressSingle");
    return fn;
}

PFN_WakeByAddress real_wake_by_address_all()
{
    static PFN_WakeByAddress fn =
        resolve<PFN_WakeByAddress>(kernel32_module(), "WakeByAddressAll");
    return fn;
}

struct WaitBlock {
    const volatile void *address;
    CONDITION_VARIABLE condition;
    WaitBlock *next;
    bool woken;
};

CRITICAL_SECTION wait_lock;
INIT_ONCE wait_once = INIT_ONCE_STATIC_INIT;
WaitBlock *waiters = nullptr;

BOOL CALLBACK initialize_wait_lock(PINIT_ONCE, PVOID, PVOID *)
{
    InitializeCriticalSectionAndSpinCount(&wait_lock, 4000);
    return TRUE;
}

void enter_wait_lock()
{
    InitOnceExecuteOnce(&wait_once, initialize_wait_lock, nullptr, nullptr);
    EnterCriticalSection(&wait_lock);
}

void remove_wait_block(WaitBlock *block)
{
    WaitBlock **link = &waiters;
    while (*link && *link != block) link = &(*link)->next;
    if (*link == block) *link = block->next;
}

bool wait_by_address_polling(volatile void *address, void *compare, SIZE_T size,
                             DWORD milliseconds)
{
    WaitBlock block = {address, CONDITION_VARIABLE_INIT, nullptr, false};
    enter_wait_lock();

    // Enlist first: a waker that runs from here on sees this waiter.  A waker
    // that ran before this point has already changed the value, which the
    // check below observes.
    block.next = waiters;
    waiters = &block;

    if (std::memcmp(const_cast<const void *>(address), compare, size) != 0) {
        remove_wait_block(&block);
        LeaveCriticalSection(&wait_lock);
        return TRUE;
    }

    const ULONGLONG deadline =
        milliseconds == INFINITE ? 0 : GetTickCount64() + milliseconds;
    DWORD remaining = milliseconds;
    for (;;) {
        if (block.woken) break;

        if (!SleepConditionVariableCS(&block.condition, &wait_lock, remaining)) {
            const DWORD error = GetLastError();
            if (error != ERROR_TIMEOUT) {
                remove_wait_block(&block);
                LeaveCriticalSection(&wait_lock);
                SetLastError(error);
                return FALSE;
            }
            if (!block.woken) {
                remove_wait_block(&block);
                LeaveCriticalSection(&wait_lock);
                SetLastError(ERROR_TIMEOUT);
                return FALSE;
            }
            break;
        }

        if (milliseconds != INFINITE) {
            const ULONGLONG now = GetTickCount64();
            remaining = now >= deadline ? 0 : static_cast<DWORD>(deadline - now);
        }
    }

    remove_wait_block(&block);
    LeaveCriticalSection(&wait_lock);
    return TRUE;
}

void wake_by_address_polling(void *address, bool all)
{
    enter_wait_lock();
    for (WaitBlock *block = waiters; block; block = block->next) {
        if (block->address == address && !block->woken) {
            block->woken = true;
            WakeConditionVariable(&block->condition);
            if (!all) break;
        }
    }
    LeaveCriticalSection(&wait_lock);
}

} // namespace

extern "C" {

BOOL WINAPI UrgeWaitOnAddress(volatile VOID *address, PVOID compare, SIZE_T size,
                              DWORD milliseconds)
{
    if (const PFN_WaitOnAddress fn = real_wait_on_address()) {
        return fn(address, compare, size, milliseconds);
    }
    return wait_by_address_polling(address, compare, size, milliseconds);
}

void WINAPI UrgeWakeByAddressSingle(PVOID address)
{
    if (const PFN_WakeByAddress fn = real_wake_by_address_single()) {
        fn(address);
        return;
    }
    wake_by_address_polling(address, false);
}

void WINAPI UrgeWakeByAddressAll(PVOID address)
{
    if (const PFN_WakeByAddress fn = real_wake_by_address_all()) {
        fn(address);
        return;
    }
    wake_by_address_polling(address, true);
}

} // extern "C"

// --- ProcessPrng ----------------------------------------------------------
//
// Windows 10 1903 added bcryptprimitives!ProcessPrng.  BCryptGenRandom with
// the system-preferred RNG is the same source on every OS down to Vista.

namespace {

using PFN_ProcessPrng = BOOL(__cdecl *)(PUCHAR, ULONG);

PFN_ProcessPrng real_process_prng()
{
    static PFN_ProcessPrng fn = [] {
        HMODULE module = GetModuleHandleW(L"bcryptprimitives.dll");
        if (!module) module = LoadLibraryW(L"bcryptprimitives.dll");
        return resolve<PFN_ProcessPrng>(module, "ProcessPrng");
    }();
    return fn;
}

} // namespace

extern "C" BOOL __cdecl UrgeProcessPrng(PUCHAR data, ULONG size)
{
    if (const PFN_ProcessPrng fn = real_process_prng()) return fn(data, size);

    return BCryptGenRandom(nullptr, data, size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
}

// --- CreateFile2 / CopyFile2 ---------------------------------------------

namespace {

using PFN_CreateFile2 = HANDLE(WINAPI *)(LPCWSTR, DWORD, DWORD, DWORD,
                                        CREATEFILE2_EXTENDED_PARAMETERS *);
using PFN_CopyFile2 = HRESULT(WINAPI *)(PCWSTR, PCWSTR,
                                        COPYFILE2_EXTENDED_PARAMETERS *);

PFN_CreateFile2 real_create_file2()
{
    static PFN_CreateFile2 fn =
        resolve<PFN_CreateFile2>(kernel32_module(), "CreateFile2");
    return fn;
}

PFN_CopyFile2 real_copy_file2()
{
    static PFN_CopyFile2 fn =
        resolve<PFN_CopyFile2>(kernel32_module(), "CopyFile2");
    return fn;
}

} // namespace

extern "C" HANDLE WINAPI UrgeCreateFile2(LPCWSTR file_name, DWORD desired_access,
                                         DWORD share_mode, DWORD creation_disposition,
                                         CREATEFILE2_EXTENDED_PARAMETERS *parameters)
{
    if (const PFN_CreateFile2 fn = real_create_file2()) {
        return fn(file_name, desired_access, share_mode, creation_disposition, parameters);
    }

    DWORD attributes = FILE_ATTRIBUTE_NORMAL;
    DWORD flags = 0;
    LPSECURITY_ATTRIBUTES security = nullptr;
    HANDLE template_file = nullptr;
    if (parameters) {
        attributes = parameters->dwFileAttributes;
        flags = parameters->dwFileFlags;
        if (flags & SECURITY_SQOS_PRESENT) flags |= parameters->dwSecurityQosFlags;
        security = parameters->lpSecurityAttributes;
        template_file = parameters->hTemplateFile;
        if (attributes == 0 && flags == 0) attributes = FILE_ATTRIBUTE_NORMAL;
    }

    return CreateFileW(file_name, desired_access, share_mode, security,
                       creation_disposition, attributes | flags, template_file);
}

extern "C" HRESULT WINAPI UrgeCopyFile2(PCWSTR existing_file, PCWSTR new_file,
                                        COPYFILE2_EXTENDED_PARAMETERS *parameters)
{
    if (const PFN_CopyFile2 fn = real_copy_file2()) {
        return fn(existing_file, new_file, parameters);
    }

    DWORD flags = 0;
    if (parameters) {
        const DWORD requested = parameters->dwCopyFlags;
        if (requested & COPY_FILE_FAIL_IF_EXISTS) flags |= COPY_FILE_FAIL_IF_EXISTS;
        if (requested & COPY_FILE_ALLOW_DECRYPTED_DESTINATION) {
            flags |= COPY_FILE_ALLOW_DECRYPTED_DESTINATION;
        }
        const DWORD unsupported = requested & ~(COPY_FILE_FAIL_IF_EXISTS |
                                                COPY_FILE_ALLOW_DECRYPTED_DESTINATION);
        if (unsupported) return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    if (CopyFileExW(existing_file, new_file, nullptr, nullptr, nullptr, flags)) {
        return S_OK;
    }
    return HRESULT_FROM_WIN32(GetLastError());
}

// --- GetSystemTimePreciseAsFileTime --------------------------------------

namespace {

using PFN_GetSystemTimePreciseAsFileTime = void(WINAPI *)(LPFILETIME);

} // namespace

extern "C" void WINAPI UrgeGetSystemTimePreciseAsFileTime(LPFILETIME time)
{
    static PFN_GetSystemTimePreciseAsFileTime fn =
        resolve<PFN_GetSystemTimePreciseAsFileTime>(
            kernel32_module(), "GetSystemTimePreciseAsFileTime");
    if (fn) {
        fn(time);
        return;
    }
    GetSystemTimeAsFileTime(time);
}

// --- Registry ------------------------------------------------------------
//
// The registry API sets do not exist on Windows 7, but every function they
// forward to is exported by advapi32.dll there.  The wrappers are a cached
// GetProcAddress each; no import of the API set remains.

namespace {

using PFN_RegOpenKeyExA = LSTATUS(WINAPI *)(HKEY, LPCSTR, DWORD, REGSAM, PHKEY);
using PFN_RegOpenKeyExW = LSTATUS(WINAPI *)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
using PFN_RegQueryValueExA = LSTATUS(WINAPI *)(HKEY, LPCSTR, LPDWORD, LPDWORD,
                                              LPBYTE, LPDWORD);
using PFN_RegQueryValueExW = LSTATUS(WINAPI *)(HKEY, LPCWSTR, LPDWORD, LPDWORD,
                                              LPBYTE, LPDWORD);
using PFN_RegCloseKey = LSTATUS(WINAPI *)(HKEY);
using PFN_RegSetKeyValueW = LSTATUS(WINAPI *)(HKEY, LPCWSTR, LPCWSTR, DWORD,
                                             LPCVOID, DWORD);
using PFN_RegDeleteKeyValueW = LSTATUS(WINAPI *)(HKEY, LPCWSTR, LPCWSTR);

} // namespace

extern "C" {

LSTATUS WINAPI UrgeRegOpenKeyExA(HKEY key, LPCSTR sub_key, DWORD options,
                                 REGSAM desired, PHKEY result)
{
    static PFN_RegOpenKeyExA fn =
        resolve<PFN_RegOpenKeyExA>(advapi32_module(), "RegOpenKeyExA");
    return fn ? fn(key, sub_key, options, desired, result) : ERROR_PROC_NOT_FOUND;
}

LSTATUS WINAPI UrgeRegOpenKeyExW(HKEY key, LPCWSTR sub_key, DWORD options,
                                 REGSAM desired, PHKEY result)
{
    static PFN_RegOpenKeyExW fn =
        resolve<PFN_RegOpenKeyExW>(advapi32_module(), "RegOpenKeyExW");
    return fn ? fn(key, sub_key, options, desired, result) : ERROR_PROC_NOT_FOUND;
}

LSTATUS WINAPI UrgeRegQueryValueExA(HKEY key, LPCSTR value_name, LPDWORD reserved,
                                    LPDWORD type, LPBYTE data, LPDWORD size)
{
    static PFN_RegQueryValueExA fn =
        resolve<PFN_RegQueryValueExA>(advapi32_module(), "RegQueryValueExA");
    return fn ? fn(key, value_name, reserved, type, data, size) : ERROR_PROC_NOT_FOUND;
}

LSTATUS WINAPI UrgeRegQueryValueExW(HKEY key, LPCWSTR value_name, LPDWORD reserved,
                                    LPDWORD type, LPBYTE data, LPDWORD size)
{
    static PFN_RegQueryValueExW fn =
        resolve<PFN_RegQueryValueExW>(advapi32_module(), "RegQueryValueExW");
    return fn ? fn(key, value_name, reserved, type, data, size) : ERROR_PROC_NOT_FOUND;
}

LSTATUS WINAPI UrgeRegCloseKey(HKEY key)
{
    static PFN_RegCloseKey fn =
        resolve<PFN_RegCloseKey>(advapi32_module(), "RegCloseKey");
    return fn ? fn(key) : ERROR_PROC_NOT_FOUND;
}

LSTATUS WINAPI UrgeRegSetKeyValueW(HKEY key, LPCWSTR sub_key, LPCWSTR value_name,
                                   DWORD type, LPCVOID data, DWORD size)
{
    static PFN_RegSetKeyValueW fn =
        resolve<PFN_RegSetKeyValueW>(advapi32_module(), "RegSetKeyValueW");
    return fn ? fn(key, sub_key, value_name, type, data, size)
              : ERROR_PROC_NOT_FOUND;
}

LSTATUS WINAPI UrgeRegDeleteKeyValueW(HKEY key, LPCWSTR sub_key, LPCWSTR value_name)
{
    static PFN_RegDeleteKeyValueW fn =
        resolve<PFN_RegDeleteKeyValueW>(advapi32_module(), "RegDeleteKeyValueW");
    return fn ? fn(key, sub_key, value_name) : ERROR_PROC_NOT_FOUND;
}

} // extern "C"

// --- Linker bindings -----------------------------------------------------
//
// A dllimport call goes through the `__imp_*` slot, so defining that slot as
// selectany (COMDAT) data takes the reference away from the import library.
// The plain-name aliases cover the rarer caller that references the thunk
// symbol rather than the IAT slot.

#define URGE_HOOK_SLOT(name) \
    __declspec(selectany) void *__imp_##name = reinterpret_cast<void *>(&Urge##name)

extern "C" {

URGE_HOOK_SLOT(WaitOnAddress);
URGE_HOOK_SLOT(WakeByAddressSingle);
URGE_HOOK_SLOT(WakeByAddressAll);
URGE_HOOK_SLOT(ProcessPrng);
URGE_HOOK_SLOT(CreateFile2);
URGE_HOOK_SLOT(CopyFile2);
URGE_HOOK_SLOT(GetSystemTimePreciseAsFileTime);
URGE_HOOK_SLOT(RegOpenKeyExA);
URGE_HOOK_SLOT(RegOpenKeyExW);
URGE_HOOK_SLOT(RegQueryValueExA);
URGE_HOOK_SLOT(RegQueryValueExW);
URGE_HOOK_SLOT(RegCloseKey);
URGE_HOOK_SLOT(RegSetKeyValueW);
URGE_HOOK_SLOT(RegDeleteKeyValueW);

} // extern "C"

#pragma comment(linker, "/alternatename:WaitOnAddress=UrgeWaitOnAddress")
#pragma comment(linker, "/alternatename:WakeByAddressSingle=UrgeWakeByAddressSingle")
#pragma comment(linker, "/alternatename:WakeByAddressAll=UrgeWakeByAddressAll")
#pragma comment(linker, "/alternatename:ProcessPrng=UrgeProcessPrng")
#pragma comment(linker, "/alternatename:CreateFile2=UrgeCreateFile2")
#pragma comment(linker, "/alternatename:CopyFile2=UrgeCopyFile2")
#pragma comment(linker, "/alternatename:GetSystemTimePreciseAsFileTime=UrgeGetSystemTimePreciseAsFileTime")
#pragma comment(linker, "/alternatename:RegOpenKeyExA=UrgeRegOpenKeyExA")
#pragma comment(linker, "/alternatename:RegOpenKeyExW=UrgeRegOpenKeyExW")
#pragma comment(linker, "/alternatename:RegQueryValueExA=UrgeRegQueryValueExA")
#pragma comment(linker, "/alternatename:RegQueryValueExW=UrgeRegQueryValueExW")
#pragma comment(linker, "/alternatename:RegCloseKey=UrgeRegCloseKey")
#pragma comment(linker, "/alternatename:RegSetKeyValueW=UrgeRegSetKeyValueW")
#pragma comment(linker, "/alternatename:RegDeleteKeyValueW=UrgeRegDeleteKeyValueW")
