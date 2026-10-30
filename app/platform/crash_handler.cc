// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "app/platform/crash_handler.h"

#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

#define NOMINMAX
#include <windows.h>

#include <dbghelp.h>

/* Nothing here allocates.

   This code runs after the process has already lost: the heap it would have
   used may be the thing that broke, a thread may hold the loader lock, and the
   stack may be the one that overflowed.  So every buffer is a fixed array,
   every path is a `wchar_t[kPathChars]`, and the only C++ in the file is the
   struct that holds the dbghelp entry points.  `std::string` and
   `std::filesystem` are the two things that would turn a crash into a silent
   exit.

   The one place that is allowed to be expensive is symbol lookup: it is what
   turns `Game.exe+0x1A2B3C` into a file and a line, and it happens after the
   minidump is already on disk. */

namespace {

constexpr std::size_t kPathChars = 512;
constexpr std::size_t kLineChars = 1024;

/* The dump is written on a thread of its own when the fault was a stack
   overflow: there is nothing left of the faulting stack to build a report on,
   and the report is a few thousand frames deep in the interpreter's recursion.
   A megabyte is far more than the writer needs. */
constexpr DWORD kWriterStackSize = 1024 * 1024;
constexpr DWORD kWriterTimeoutMs = 120 * 1000;

/* A recursion that ran out of stack is the crash this report most often has to
   explain, and it fills the whole walk with one address.  Repeated frames are
   folded into a count, which leaves room for the frames at the bottom -- the
   call that started it -- inside the same budget. */
constexpr int kMaxFrames = 512;

/* Application-defined codes, in the range Windows reserves for exactly this.
   The CRT paths that raise them have no exception record of their own, so they
   are given one: everything downstream -- the report, the dump, the debugger --
   then sees one kind of event instead of three. */
constexpr DWORD kExceptionAbort = 0xE0000001;
constexpr DWORD kExceptionPurecall = 0xE0000002;

/* MiniDumpWithFullMemory is opt-in (`URGE_CRASH_FULL=1`): a game with wgpu and
   the interpreter mapped is hundreds of megabytes, and the stacks plus the
   indirectly referenced memory are what actually answer "where did it die". */
constexpr DWORD kDumpType =
    MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithThreadInfo |
    MiniDumpWithUnloadedModules | MiniDumpWithIndirectlyReferencedMemory;
constexpr DWORD kFullDumpType = kDumpType | MiniDumpWithFullMemory |
                                MiniDumpWithFullMemoryInfo |
                                MiniDumpWithProcessThreadData;

wchar_t g_app_dir[kPathChars] = {};
wchar_t g_app_name[kPathChars] = {};
wchar_t g_dump_dir[kPathChars] = {};
wchar_t g_log_path[kPathChars] = {};
wchar_t g_note[kPathChars] = {};

bool g_enabled = false;

/* One report per process.  A fault inside the writer -- or a second thread
   dying while the first is still dumping -- must not start a second one. */
volatile LONG g_reporting = 0;

/* Whatever was on top of the unhandled-exception chain before this file got
   there.  It is asked first, and a real answer from it ends the matter: the
   interpreter's collector installs a filter of its own for the duration of a
   compaction, and an access violation it repairs is not a crash. */
LPTOP_LEVEL_EXCEPTION_FILTER g_previous_filter = nullptr;

struct DbgHelp {
  HMODULE module = nullptr;
  decltype(&::MiniDumpWriteDump) write_dump = nullptr;
  decltype(&::SymInitializeW) initialize = nullptr;
  decltype(&::SymSetOptions) set_options = nullptr;
  decltype(&::SymFromAddrW) from_addr = nullptr;
  decltype(&::SymGetLineFromAddrW64) line_from_addr = nullptr;
  decltype(&::SymFunctionTableAccess64) function_table = nullptr;
  decltype(&::SymGetModuleBase64) module_base = nullptr;
  decltype(&::StackWalk64) stack_walk = nullptr;
};

DbgHelp g_dbghelp;

bool ReadEnvironment(const wchar_t* name, wchar_t* value, DWORD capacity) {
  DWORD length = GetEnvironmentVariableW(name, value, capacity);
  return length > 0 && length < capacity;
}

bool EnvironmentFlag(const wchar_t* name) {
  wchar_t value[8];
  if (!ReadEnvironment(name, value, 8))
    return false;
  return value[0] != L'0' && value[0] != L'n' && value[0] != L'N';
}

bool EnsureDirectory(const wchar_t* path) {
  if (CreateDirectoryW(path, nullptr))
    return true;
  if (GetLastError() != ERROR_ALREADY_EXISTS)
    return false;
  DWORD attributes = GetFileAttributesW(path);
  return attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

/* dbghelp is loaded by hand rather than linked: the engine ships as one
   executable and a DLL it only needs after it has already died is a dependency
   worth not having at load time.  A newer `dbghelp.dll` dropped beside the
   executable wins over the one in `System32`, which is the usual way to get
   symbols out of a PDB a ten-year-old system DLL cannot read. */
void LoadDbgHelp() {
  g_dbghelp.module = LoadLibraryW(L"dbghelp.dll");
  if (!g_dbghelp.module)
    return;

  HMODULE module = g_dbghelp.module;
  g_dbghelp.write_dump = reinterpret_cast<decltype(&::MiniDumpWriteDump)>(
      GetProcAddress(module, "MiniDumpWriteDump"));
  g_dbghelp.initialize = reinterpret_cast<decltype(&::SymInitializeW)>(
      GetProcAddress(module, "SymInitializeW"));
  g_dbghelp.set_options = reinterpret_cast<decltype(&::SymSetOptions)>(
      GetProcAddress(module, "SymSetOptions"));
  g_dbghelp.from_addr = reinterpret_cast<decltype(&::SymFromAddrW)>(
      GetProcAddress(module, "SymFromAddrW"));
  g_dbghelp.line_from_addr =
      reinterpret_cast<decltype(&::SymGetLineFromAddrW64)>(
          GetProcAddress(module, "SymGetLineFromAddrW64"));
  g_dbghelp.function_table =
      reinterpret_cast<decltype(&::SymFunctionTableAccess64)>(
          GetProcAddress(module, "SymFunctionTableAccess64"));
  g_dbghelp.module_base = reinterpret_cast<decltype(&::SymGetModuleBase64)>(
      GetProcAddress(module, "SymGetModuleBase64"));
  g_dbghelp.stack_walk = reinterpret_cast<decltype(&::StackWalk64)>(
      GetProcAddress(module, "StackWalk64"));
}

void MakeStamp(wchar_t* out, std::size_t capacity) {
  SYSTEMTIME now;
  GetLocalTime(&now);
  _snwprintf_s(out, capacity, _TRUNCATE, L"%04u%02u%02u-%02u%02u%02u",
               now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
               now.wSecond);
}

const wchar_t* ExceptionName(DWORD code) {
  switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
      return L"ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
      return L"ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_BREAKPOINT:
      return L"BREAKPOINT";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
      return L"DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
      return L"FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INVALID_OPERATION:
      return L"FLT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW:
      return L"FLT_OVERFLOW";
    case EXCEPTION_GUARD_PAGE:
      return L"GUARD_PAGE";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
      return L"ILLEGAL_INSTRUCTION";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
      return L"INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:
      return L"INT_OVERFLOW";
    case EXCEPTION_INVALID_DISPOSITION:
      return L"INVALID_DISPOSITION";
    case EXCEPTION_IN_PAGE_ERROR:
      return L"IN_PAGE_ERROR";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
      return L"NONCONTINUABLE_EXCEPTION";
    case EXCEPTION_PRIV_INSTRUCTION:
      return L"PRIV_INSTRUCTION";
    case EXCEPTION_STACK_OVERFLOW:
      return L"STACK_OVERFLOW";
    case STATUS_DLL_INIT_FAILED:
      return L"DLL_INIT_FAILED";
    case STATUS_DLL_NOT_FOUND:
      return L"DLL_NOT_FOUND";
    case STATUS_ENTRYPOINT_NOT_FOUND:
      return L"ENTRYPOINT_NOT_FOUND";
    case STATUS_INVALID_CRUNTIME_PARAMETER:
      return L"INVALID_CRUNTIME_PARAMETER";
    case STATUS_STACK_BUFFER_OVERRUN:
      return L"STACK_BUFFER_OVERRUN";
    case kExceptionAbort:
      return L"ABORT";
    case kExceptionPurecall:
      return L"PURECALL";
    default:
      return nullptr;
  }
}

/* The report is UTF-16 with a byte-order mark, so a path with a Chinese game
   name opens in Notepad, in the editor the developer happens to use, and in
   `type` -- without this file ever converting anything. */
struct Report {
  HANDLE file = INVALID_HANDLE_VALUE;

  bool Open(const wchar_t* path) {
    file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
      return false;
    const WORD bom = 0xFEFF;
    DWORD written = 0;
    WriteFile(file, &bom, sizeof(bom), &written, nullptr);
    return true;
  }

  void Line(const wchar_t* format, ...) {
    if (file == INVALID_HANDLE_VALUE)
      return;

    wchar_t line[kLineChars];
    va_list args;
    va_start(args, format);
    int count = _vsnwprintf_s(line, kLineChars, _TRUNCATE, format, args);
    va_end(args);
    if (count < 0)
      return;
    if (static_cast<std::size_t>(count) + 2 >= kLineChars)
      count = static_cast<int>(kLineChars - 3);

    line[count] = L'\r';
    line[count + 1] = L'\n';

    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>((count + 2) * sizeof(wchar_t)),
              &written, nullptr);
  }

  void Blank() { Line(L""); }

  void Close() {
    if (file != INVALID_HANDLE_VALUE) {
      CloseHandle(file);
      file = INVALID_HANDLE_VALUE;
    }
  }
};

struct ModuleHit {
  wchar_t name[MAX_PATH] = {};
  DWORD64 offset = 0;
};

ModuleHit ResolveModule(DWORD64 address) {
  ModuleHit hit;
  HMODULE module = nullptr;
  if (!GetModuleHandleExW(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          reinterpret_cast<LPCWSTR>(static_cast<ULONG_PTR>(address)), &module))
    return hit;

  wchar_t path[MAX_PATH];
  if (!GetModuleFileNameW(module, path, MAX_PATH))
    return hit;

  const wchar_t* name = path;
  for (const wchar_t* cursor = path; *cursor; ++cursor)
    if (*cursor == L'\\' || *cursor == L'/')
      name = cursor + 1;

  wcsncpy_s(hit.name, name, _TRUNCATE);
  hit.offset =
      address - static_cast<DWORD64>(reinterpret_cast<ULONG_PTR>(module));
  return hit;
}

/* `Game.exe+0x1A2B3C` is the one address a developer can act on without the
   dump, so it is what every frame and the faulting instruction are printed
   as. */
void FormatAddress(wchar_t* out, std::size_t capacity, DWORD64 address) {
  ModuleHit hit = ResolveModule(address);
  if (hit.name[0]) {
    _snwprintf_s(out, capacity, _TRUNCATE, L"0x%016llX  %s+0x%llX", address,
                 hit.name, hit.offset);
  } else {
    _snwprintf_s(out, capacity, _TRUNCATE, L"0x%016llX", address);
  }
}

struct SymbolBuffer {
  union {
    SYMBOL_INFOW symbol;
    char storage[sizeof(SYMBOL_INFOW) + 256 * sizeof(wchar_t)];
  };
};

bool PrepareSymbols() {
  if (!g_dbghelp.initialize || !g_dbghelp.set_options || !g_dbghelp.from_addr)
    return false;

  /* Deferred loads keep the crash handler from stalling on every module that
     happens to be mapped -- wgpu, the CRT, the graphics driver -- and the
     critical-error flag suppresses the dialog dbghelp would otherwise put up
     for a module it has no symbols for. */
  g_dbghelp.set_options(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS |
                        SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
  return g_dbghelp.initialize(GetCurrentProcess(), nullptr, TRUE) != FALSE;
}

/* A frame's readable form: `module+offset  symbol+offset  (file:line)`, with
   each part dropped when it is not known. */
void FormatFrame(wchar_t* out,
                 std::size_t capacity,
                 DWORD64 address,
                 bool with_symbols) {
  ModuleHit hit = ResolveModule(address);
  int used = 0;
  if (hit.name[0]) {
    used = _snwprintf_s(out, capacity, _TRUNCATE, L"0x%016llX  %s+0x%llX",
                        address, hit.name, hit.offset);
  } else {
    used = _snwprintf_s(out, capacity, _TRUNCATE, L"0x%016llX", address);
  }
  if (used < 0 || !with_symbols)
    return;

  SymbolBuffer buffer{};
  buffer.symbol.SizeOfStruct = sizeof(SYMBOL_INFOW);
  buffer.symbol.MaxNameLen = 256;

  DWORD64 displacement = 0;
  if (!g_dbghelp.from_addr(GetCurrentProcess(), address, &displacement,
                           &buffer.symbol))
    return;

  used += _snwprintf_s(out + used, capacity - used, _TRUNCATE, L"  %s+0x%llX",
                       buffer.symbol.Name,
                       static_cast<unsigned long long>(displacement));

  if (used < 0 || !g_dbghelp.line_from_addr)
    return;

  IMAGEHLP_LINEW64 line{};
  line.SizeOfStruct = sizeof(IMAGEHLP_LINEW64);
  DWORD line_displacement = 0;
  if (g_dbghelp.line_from_addr(GetCurrentProcess(), address, &line_displacement,
                               &line))
    _snwprintf_s(out + used, capacity - used, _TRUNCATE, L"  (%s:%u)",
                 line.FileName, line.LineNumber);
}

/* Walks the faulting thread's stack from the context the fault was recorded
   with, so the frames below the handler are the game's own.  Without dbghelp --
   or when the unwind tables are missing -- the return addresses are still on
   the stack, and printing the first few of them raw is better than nothing. */
void WriteStack(Report& report,
                CONTEXT* context,
                HANDLE thread,
                bool with_symbols) {
  report.Blank();
  report.Line(L"stack");
  report.Line(L"-----");

  if (!g_dbghelp.stack_walk || !g_dbghelp.function_table ||
      !g_dbghelp.module_base || !with_symbols) {
    ULONG_PTR stack_pointer =
#if defined(_M_X64)
        context->Rsp;
#else
        context->Esp;
#endif
    const DWORD64* stack = reinterpret_cast<const DWORD64*>(stack_pointer);
    wchar_t text[kLineChars];
    for (int index = 0; index < 24; ++index) {
      FormatAddress(text, kLineChars, stack[index]);
      report.Line(L"#%02d %s", index, text);
    }
    return;
  }

  STACKFRAME64 frame{};
#if defined(_M_X64)
  constexpr DWORD kMachine = IMAGE_FILE_MACHINE_AMD64;
  frame.AddrPC.Offset = context->Rip;
  frame.AddrFrame.Offset = context->Rbp;
  frame.AddrStack.Offset = context->Rsp;
#else
  constexpr DWORD kMachine = IMAGE_FILE_MACHINE_I386;
  frame.AddrPC.Offset = context->Eip;
  frame.AddrFrame.Offset = context->Ebp;
  frame.AddrStack.Offset = context->Esp;
#endif
  frame.AddrPC.Mode = AddrModeFlat;
  frame.AddrFrame.Mode = AddrModeFlat;
  frame.AddrStack.Mode = AddrModeFlat;

  wchar_t text[kLineChars];
  DWORD64 previous = 0;
  int repeated = 0;
  int printed = 0;
  for (int index = 0; index < kMaxFrames; ++index) {
    if (!g_dbghelp.stack_walk(kMachine, GetCurrentProcess(), thread, &frame,
                              context, nullptr, g_dbghelp.function_table,
                              g_dbghelp.module_base, nullptr))
      break;
    if (!frame.AddrPC.Offset)
      break;

    if (frame.AddrPC.Offset == previous) {
      ++repeated;
      continue;
    }
    if (repeated > 0) {
      report.Line(L"     ... %d more identical frame(s)", repeated);
      repeated = 0;
    }

    previous = frame.AddrPC.Offset;
    FormatFrame(text, kLineChars, frame.AddrPC.Offset, true);
    report.Line(L"#%02d %s", printed++, text);
  }

  if (repeated > 0)
    report.Line(L"     ... %d more identical frame(s)", repeated);
}

void WriteRegisters(Report& report, CONTEXT* context) {
  report.Blank();
  report.Line(L"registers");
  report.Line(L"---------");
#if defined(_M_X64)
  report.Line(L"  rax %016llX  rbx %016llX  rcx %016llX  rdx %016llX",
              context->Rax, context->Rbx, context->Rcx, context->Rdx);
  report.Line(L"  rsi %016llX  rdi %016llX  rbp %016llX  rsp %016llX",
              context->Rsi, context->Rdi, context->Rbp, context->Rsp);
  report.Line(L"  r08 %016llX  r09 %016llX  r10 %016llX  r11 %016llX",
              context->R8, context->R9, context->R10, context->R11);
  report.Line(L"  r12 %016llX  r13 %016llX  r14 %016llX  r15 %016llX",
              context->R12, context->R13, context->R14, context->R15);
  report.Line(L"  rip %016llX  eflags %08X", context->Rip,
              static_cast<unsigned int>(context->EFlags));
#else
  report.Line(L"  (the register dump is x64 only)");
#endif
}

void WriteSystem(Report& report) {
  /* `GetVersionEx` reports whatever the executable's manifest claims -- 6.2 on
     everything from Windows 8.1 up -- so the real build comes from ntdll, which
     is what tells a developer whether this is a Windows 10 or a Windows 11
     driver. */
  wchar_t version[128] = L"Windows (unknown)";
  using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
  auto rtl_get_version = reinterpret_cast<RtlGetVersionFn>(
      GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
  if (rtl_get_version) {
    OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof(info);
    if (rtl_get_version(&info) == 0) {
      _snwprintf_s(version, 128, _TRUNCATE, L"Windows %lu.%lu.%lu",
                   info.dwMajorVersion, info.dwMinorVersion,
                   info.dwBuildNumber);
    }
  }

  report.Line(L"system     : %s (%s), %u logical CPUs", version,
#if defined(_M_X64)
              L"x64",
#else
              L"x86",
#endif
              GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
}

bool WriteDumpFile(const wchar_t* path,
                   EXCEPTION_POINTERS* info,
                   DWORD thread_id) {
  if (!g_dbghelp.write_dump)
    return false;

  HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return false;

  MINIDUMP_EXCEPTION_INFORMATION exception{};
  exception.ThreadId = thread_id;
  exception.ExceptionPointers = info;
  exception.ClientPointers = FALSE;

  const DWORD type =
      EnvironmentFlag(L"URGE_CRASH_FULL") ? kFullDumpType : kDumpType;

  BOOL written =
      g_dbghelp.write_dump(GetCurrentProcess(), GetCurrentProcessId(), file,
                           static_cast<MINIDUMP_TYPE>(type),
                           info ? &exception : nullptr, nullptr, nullptr);

  CloseHandle(file);
  if (!written)
    DeleteFileW(path);
  return written != FALSE;
}

/* The order below is the order of what survives a second fault: the header
   first, because it is the part that answers the question on its own; the dump
   next, because a debugger wants it and nothing else; the symbols last, because
   loading a PDB is the one step that can take a second or hang on a network
   symbol path. */
bool WriteArtifacts(EXCEPTION_POINTERS* info, DWORD thread_id) {
  wchar_t stamp[32];
  MakeStamp(stamp, 32);

  wchar_t base[kPathChars];
  _snwprintf_s(base, kPathChars, _TRUNCATE, L"%s\\%s_%s_%u", g_dump_dir,
               g_app_name, stamp, GetCurrentProcessId());

  wchar_t report_path[kPathChars];
  _snwprintf_s(report_path, kPathChars, _TRUNCATE, L"%s.txt", base);
  wchar_t dump_path[kPathChars];
  _snwprintf_s(dump_path, kPathChars, _TRUNCATE, L"%s.dmp", base);

  const EXCEPTION_RECORD* record = info ? info->ExceptionRecord : nullptr;
  const DWORD code = record ? record->ExceptionCode : 0;
  const DWORD64 address =
      record ? reinterpret_cast<DWORD64>(record->ExceptionAddress) : 0;
  const wchar_t* name = ExceptionName(code);

  Report report;
  const bool has_report = report.Open(report_path);

  SYSTEMTIME now;
  GetLocalTime(&now);
  wchar_t address_text[kLineChars];
  FormatAddress(address_text, kLineChars, address);
  wchar_t exe_path[kPathChars] = {};
  GetModuleFileNameW(nullptr, exe_path, kPathChars);

  report.Line(L"URGE crash report");
  report.Line(L"=================");
  report.Blank();
  report.Line(L"time       : %04u-%02u-%02u %02u:%02u:%02u.%03u", now.wYear,
              now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
              now.wMilliseconds);
  report.Line(L"process    : %s  (pid %u)", g_app_name, GetCurrentProcessId());
  report.Line(L"executable : %s", exe_path);
  report.Line(L"directory  : %s", g_app_dir);
  if (name)
    report.Line(L"exception  : 0x%08X  %s", code, name);
  else
    report.Line(L"exception  : 0x%08X", code);
  report.Line(L"address    : %s", address_text);
  report.Line(L"thread     : %u", thread_id);

  /* Only an access violation carries a faulting address, and the difference
     between a read and a write is most of the diagnosis: a read at 0 is a null
     dereference, a write past the end of a buffer is a heap overflow. */
  if (record && code == EXCEPTION_ACCESS_VIOLATION &&
      record->NumberParameters >= 2) {
    const wchar_t* kind = L"access to";
    switch (record->ExceptionInformation[0]) {
      case 0:
        kind = L"read of";
        break;
      case 1:
        kind = L"write to";
        break;
      case 8:
        kind = L"execute at";
        break;
      default:
        break;
    }
    report.Line(
        L"fault      : %s 0x%016llX", kind,
        static_cast<unsigned long long>(record->ExceptionInformation[1]));
  }

  if (g_note[0])
    report.Line(L"note       : %s", g_note);
  if (g_log_path[0])
    report.Line(L"log        : %s", g_log_path);
  WriteSystem(report);

  if (info && info->ContextRecord)
    WriteRegisters(report, info->ContextRecord);

  const bool has_dump = WriteDumpFile(dump_path, info, thread_id);
  report.Line(L"dump       : %s", has_dump ? dump_path : L"(not written)");

  if (info && info->ContextRecord) {
    const bool with_symbols = PrepareSymbols();
    HANDLE thread = OpenThread(
        THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
        FALSE, thread_id);
    if (!thread)
      thread = GetCurrentThread();
    WriteStack(report, info->ContextRecord, thread, with_symbols);
    if (thread != GetCurrentThread())
      CloseHandle(thread);
  }

  report.Close();

  /* The console is the one place a developer running the game from a shell will
     look, and it is gone the moment the process is: say where the files went
     while there is still somewhere to say it. */
  wchar_t summary[kLineChars];
  if (name)
    _snwprintf_s(summary, kLineChars, _TRUNCATE,
                 L"[URGE] crashed: 0x%08X %s at %s\r\n[URGE] report: %s\r\n",
                 code, name, address_text, report_path);
  else
    _snwprintf_s(summary, kLineChars, _TRUNCATE,
                 L"[URGE] crashed: 0x%08X at %s\r\n[URGE] report: %s\r\n", code,
                 address_text, report_path);
  char utf8[kLineChars * 2];
  int length =
      WideCharToMultiByte(CP_UTF8, 0, summary, -1, utf8,
                          static_cast<int>(sizeof(utf8)), nullptr, nullptr);
  if (length > 1) {
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), utf8,
              static_cast<DWORD>(length - 1), &written, nullptr);
  }

  return has_report || has_dump;
}

/* A message box is the only thing a player gets to see, and it has to stay out
   of the way of everything else.  A console this process shares with a shell --
   a developer running the game from a terminal, a probe, a CI job -- is already
   watching stderr, and a modal there would turn a crash into a hang.  A console
   nobody else is attached to is the one Windows gave the double-clicked game,
   and that one gets the box. */
bool ShouldShowDialog() {
  wchar_t value[8];
  if (ReadEnvironment(L"URGE_CRASH_DIALOG", value, 8))
    return value[0] != L'0' && value[0] != L'n' && value[0] != L'N';

  DWORD processes[8] = {};
  return GetConsoleProcessList(processes, 8) <= 1;
}

void ShowDialog(DWORD code, const wchar_t* address_text) {
  if (!ShouldShowDialog())
    return;

  const wchar_t* name = ExceptionName(code);
  wchar_t text[kLineChars * 2];
  _snwprintf_s(text, kLineChars * 2, _TRUNCATE,
               L"URGE has stopped.\n\n"
               L"%s (0x%08X)\nat %s\n\n"
               L"The crash report and a memory dump were written to:\n%s\n\n"
               L"Please send those files to the developer.",
               name ? name : L"Unknown exception", code, address_text,
               g_dump_dir);

  MessageBoxW(nullptr, text, L"URGE",
              MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST);
}

bool ProduceArtifacts(EXCEPTION_POINTERS* info, DWORD thread_id) {
  wchar_t address_text[kLineChars] = {};
  if (info && info->ExceptionRecord)
    FormatAddress(
        address_text, kLineChars,
        reinterpret_cast<DWORD64>(info->ExceptionRecord->ExceptionAddress));

  const bool written = WriteArtifacts(info, thread_id);
  if (written && info && info->ExceptionRecord)
    ShowDialog(info->ExceptionRecord->ExceptionCode, address_text);
  return written;
}

struct WriterRequest {
  EXCEPTION_POINTERS* info;
  DWORD thread_id;
  bool written;
};

/* The stack-overflow path: the faulting thread has no stack left, so the report
   is built on one that does.  It reads the faulting thread's stack through the
   process handle, which is why the walk still works from here. */
DWORD WINAPI WriterThread(void* parameter) {
  auto* request = static_cast<WriterRequest*>(parameter);
  request->written = ProduceArtifacts(request->info, request->thread_id);
  return 0;
}

LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* info) {
  /* A debugger was told about the fault before this ran, and it is the one that
     should decide what happens next. */
  if (IsDebuggerPresent())
    return EXCEPTION_CONTINUE_SEARCH;

  /* Whatever was here before gets the first refusal.  During a compaction the
     interpreter's collector repairs access violations by un-protecting the page
     it just moved and resuming -- a crash report for one of those would be a
     lie, and returning its answer keeps the run alive. */
  if (g_previous_filter) {
    LONG result = g_previous_filter(info);
    if (result != EXCEPTION_CONTINUE_SEARCH)
      return result;
  }

  if (InterlockedExchange(&g_reporting, 1) != 0)
    return EXCEPTION_CONTINUE_SEARCH;

  const DWORD thread_id = GetCurrentThreadId();
  bool written = false;

  if (info && info->ExceptionRecord &&
      info->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
    WriterRequest request{info, thread_id, false};
    HANDLE thread = CreateThread(nullptr, kWriterStackSize, &WriterThread,
                                 &request, 0, nullptr);
    if (thread) {
      WaitForSingleObject(thread, kWriterTimeoutMs);
      CloseHandle(thread);
      written = request.written;
    } else {
      written = ProduceArtifacts(info, thread_id);
    }
  } else {
    written = ProduceArtifacts(info, thread_id);
  }

  /* Nothing was written: hand the fault back so Windows reports it the usual
     way, instead of the process disappearing without a trace. */
  return written ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}

void NoteInvalidParameter(const wchar_t* expression,
                          const wchar_t* function,
                          const wchar_t* file,
                          unsigned int line,
                          uintptr_t) {
  /* This one is recorded, not raised.  The CRT calls it for parameters it then
     reports as an error and carries on from -- a bad file descriptor is one --
     so turning it into a crash would break runs that used to survive.  If the
     call does abort afterwards, the note is already in the report. */
  _snwprintf_s(g_note, kPathChars, _TRUNCATE,
               L"invalid parameter: %s in %s (%s:%u)",
               expression ? expression : L"?", function ? function : L"?",
               file ? file : L"?", line);

  char utf8[kPathChars * 2];
  int length =
      WideCharToMultiByte(CP_UTF8, 0, g_note, -1, utf8,
                          static_cast<int>(sizeof(utf8)), nullptr, nullptr);
  if (length > 1) {
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), utf8,
              static_cast<DWORD>(length - 1), &written, nullptr);
  }
}

void OnPurecall() {
  /* A pure virtual call has no recovery path: the vtable slot is the CRT's
     `_purecall`, which is already the end of the line. */
  RaiseException(kExceptionPurecall, EXCEPTION_NONCONTINUABLE, 0, nullptr);
}

void OnSignal(int signal_number) {
  /* `abort()` arrives here once `_CALL_REPORTFAULT` is cleared below.  It is
     the only signal wired up: the floating-point and illegal-instruction
     signals are only ever reached through `raise()`, since the hardware faults
     behind them are SEH exceptions and never pass through the CRT. */
  (void)signal_number;
  RaiseException(kExceptionAbort, EXCEPTION_NONCONTINUABLE, 0, nullptr);
}

void SplitExecutablePath(const wchar_t* path) {
  const wchar_t* name = path;
  for (const wchar_t* cursor = path; *cursor; ++cursor)
    if (*cursor == L'\\' || *cursor == L'/')
      name = cursor + 1;

  std::size_t length = static_cast<std::size_t>(name - path);
  if (length >= kPathChars)
    length = kPathChars - 1;

  /* The separator before the file name is not part of the directory, and
     `%s\\crash` would otherwise produce `...\\crash`.  `C:\` is three
     characters and must keep its own. */
  if (length > 3 && (path[length - 1] == L'\\' || path[length - 1] == L'/'))
    --length;

  wmemcpy(g_app_dir, path, length);
  g_app_dir[length] = L'\0';

  wcsncpy_s(g_app_name, name, _TRUNCATE);
  wchar_t* extension = wcsrchr(g_app_name, L'.');
  if (extension)
    *extension = L'\0';
  if (!g_app_name[0])
    wcscpy_s(g_app_name, L"Game");
}

bool ResolveDumpDirectory() {
  if (ReadEnvironment(L"URGE_CRASH_DIR", g_dump_dir, kPathChars))
    return EnsureDirectory(g_dump_dir);

  _snwprintf_s(g_dump_dir, kPathChars, _TRUNCATE, L"%s\\crash", g_app_dir);
  if (EnsureDirectory(g_dump_dir))
    return true;

  /* A game installed under `Program Files` cannot write beside its own
     executable.  The per-user directory is where the save data would go
     anyway, and it always exists once the folder is made. */
  wchar_t local[kPathChars];
  if (ReadEnvironment(L"LOCALAPPDATA", local, kPathChars)) {
    _snwprintf_s(g_dump_dir, kPathChars, _TRUNCATE, L"%s\\URGE", local);
    EnsureDirectory(g_dump_dir);
    _snwprintf_s(g_dump_dir, kPathChars, _TRUNCATE, L"%s\\URGE\\crash", local);
    if (EnsureDirectory(g_dump_dir))
      return true;
  }

  /* `GetTempPathW` always ends with a separator. */
  DWORD length = GetTempPathW(kPathChars, local);
  if (length > 0 && length < kPathChars) {
    _snwprintf_s(g_dump_dir, kPathChars, _TRUNCATE, L"%sURGE-crash", local);
    if (EnsureDirectory(g_dump_dir))
      return true;
  }

  return false;
}

bool Setup() {
  /* `URGE_CRASH_DUMP=0` is the off switch; any other value spells out the
     default.  Checked before anything else, so a run that does not want a
     handler does not even pay for the dbghelp load. */
  wchar_t switch_value[8];
  if (ReadEnvironment(L"URGE_CRASH_DUMP", switch_value, 8) &&
      (switch_value[0] == L'0' || switch_value[0] == L'n' ||
       switch_value[0] == L'N'))
    return false;

  wchar_t exe_path[kPathChars];
  if (!GetModuleFileNameW(nullptr, exe_path, kPathChars))
    return false;
  SplitExecutablePath(exe_path);

  if (!ResolveDumpDirectory())
    return false;

  LoadDbgHelp();

  /* Hard-error popups ("drive not ready", "cannot find DLL") would otherwise
     sit in front of the crash report with nobody to dismiss them.  The
     GP-fault box is what Windows puts up for an unhandled exception when it
     gets there first. */
  SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
               SEM_NOOPENFILEERRORBOX);

  /* `abort()` reports to Windows Error Reporting through `__fastfail` by
     default, which bypasses every handler in this file.  Clearing the flag
     leaves the CRT raising `SIGABRT` instead, which does not.

     `SIGSEGV` is deliberately left alone.  The CRT's own start-up filter
     (`seh_filter_exe`) turns an access violation into that signal *before* the
     unhandled-exception filter is ever consulted, so a handler here would take
     the fault away from the report and keep none of its detail -- no faulting
     address, no read-or-write, no exception record for the dump.  With the
     signal at its default, the filter returns "continue searching" and the
     violation arrives where it can still be described. */
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  _set_invalid_parameter_handler(&NoteInvalidParameter);
  _set_purecall_handler(&OnPurecall);
  signal(SIGABRT, &OnSignal);

  return true;
}

}  // namespace

namespace platform {
namespace crash {

void Install() {
  /* Installing twice would put this file's filter on top of itself, and the
     chain would call it back in an endless loop the first time anything
     faulted. */
  if (g_enabled)
    return;
  if (!Setup())
    return;

  g_enabled = true;
  g_previous_filter = SetUnhandledExceptionFilter(&OnUnhandledException);
}

void SetLogPath(const char* utf8_path) {
  if (!g_enabled || !utf8_path || !utf8_path[0])
    return;

  int length = MultiByteToWideChar(CP_UTF8, 0, utf8_path, -1, g_log_path,
                                   static_cast<int>(kPathChars));
  if (length <= 1)
    g_log_path[0] = L'\0';
}

}  // namespace crash
}  // namespace platform
