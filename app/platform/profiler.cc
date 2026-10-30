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

#include "app/platform/profiler.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/common/logger.h"
#include "core/render/render_stats.h"

#define NOMINMAX
#include <windows.h>

#include <dbghelp.h>
#include <tlhelp32.h>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

/* How the sampler stays out of the engine's way.

   The thread that runs the game is stopped with `SuspendThread`, its stack is
   read out of the frozen context, and it is resumed.  That is only safe if
   nothing the sampler does while the thread is stopped can block on something
   that thread holds -- and the heap lock is the one it holds most often.  So
   the whole sampling path is allocation free: the frames of a stack are walked
   into a buffer on the sampler's own stack, then copied into a tape that was
   reserved before the run started.  Everything expensive -- dbghelp, symbol
   loading, string building, the report itself -- happens in `WriteReport`,
   after sampling has stopped and nothing is suspended.

   The stack walk is the system's own.  A PE image carries a table that says how
   to undo each function's prologue (`RUNTIME_FUNCTION`),
   `RtlLookupFunctionEntry` finds the entry for an address, and
   `RtlVirtualUnwind` applies it to a context.  A leaf function has no such
   entry -- it touches no non-volatile register, so no unwinding is needed: its
   return address is the first thing on the stack.  Both cases are handled
   below, and the stack pointer is checked against the thread's stack limits
   before a byte is read, because the one thing that would turn a sample into a
   crash is dereferencing a guard page. */

namespace platform {
namespace profiler {
namespace {

constexpr int kMaxDepthLimit = 192;
constexpr std::size_t kMaxSamples = 1u << 20;
constexpr std::uint32_t kModuleRefreshSamples = 4096;
constexpr int kHotkeyVirtualKey = VK_F4;

constexpr std::size_t kMaxTreeNodes = 200000;
constexpr int kTreeLines = 220;
constexpr double kTreeThreshold = 0.4;
constexpr int kTopRows = 40;

/* A module that ships without a PDB -- wgpu, the video driver -- still answers
   `SymFromAddr`, but only with the nearest *export*, which for most addresses
   is a symbol hundreds of kilobytes away and has nothing to do with the code
   that ran.  A real function's displacement is inside the function; past this
   distance the answer is a guess and `module + offset` is the honest one. */
constexpr DWORD64 kDisplacementLimit = 0x4000;
constexpr DWORD64 kExportDisplacementLimit = 0x100;

struct SampleRecord {
  std::uint32_t offset = 0;
  std::uint16_t depth = 0;
  std::uint8_t phase = 0;
  std::uint8_t reserved = 0;
};

struct ModuleRange {
  std::uint64_t base = 0;
  std::uint64_t end = 0;
  std::string name;
};

struct Recorder {
  std::mutex lock;
  std::vector<std::uint64_t> tape;
  std::vector<SampleRecord> samples;
  std::vector<ModuleRange> modules;
  std::uint64_t dropped = 0;
};

Recorder g_recorder;

HANDLE g_target = nullptr;
HANDLE g_thread = nullptr;
HANDLE g_stop = nullptr;
std::atomic<bool> g_sampling{false};
std::atomic<bool> g_busy{false};

std::uint64_t g_stack_low = 0;
std::uint64_t g_stack_high = 0;
int g_interval_us = 2000;
int g_max_depth = 64;
std::uint32_t g_max_frames = 0;
std::uint64_t g_started = 0;
std::uint64_t g_stopped = 0;
std::atomic<std::uint64_t> g_sampled{0};
std::atomic<std::uint64_t> g_truncated{0};

std::string g_output_dir;
std::string g_prefix;

std::vector<ModuleRange> g_modules;
std::uint64_t g_exe_base = 0;

using RtlLookupFunctionEntryFn =
    PRUNTIME_FUNCTION(WINAPI*)(DWORD64, PDWORD64, PUNWIND_HISTORY_TABLE);
using RtlVirtualUnwindFn =
    PEXCEPTION_ROUTINE(WINAPI*)(DWORD,
                                DWORD64,
                                DWORD64,
                                PRUNTIME_FUNCTION,
                                PCONTEXT,
                                PVOID*,
                                PDWORD64,
                                PKNONVOLATILE_CONTEXT_POINTERS);

RtlLookupFunctionEntryFn g_lookup_entry = nullptr;
RtlVirtualUnwindFn g_virtual_unwind = nullptr;

std::uint64_t NowTicks() {
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return static_cast<std::uint64_t>(counter.QuadPart);
}

std::uint64_t TickFrequency() {
  static const std::uint64_t frequency = [] {
    LARGE_INTEGER value;
    QueryPerformanceFrequency(&value);
    return static_cast<std::uint64_t>(value.QuadPart);
  }();
  return frequency;
}

double TicksToSeconds(std::uint64_t ticks) {
  const std::uint64_t frequency = TickFrequency();
  return frequency ? static_cast<double>(ticks) / static_cast<double>(frequency)
                   : 0.0;
}

std::string Narrow(const wchar_t* text) {
  if (!text)
    return {};

  const int length =
      WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  if (length <= 1)
    return {};

  std::string out(static_cast<std::size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), length, nullptr,
                      nullptr);
  out.resize(static_cast<std::size_t>(length) - 1);
  return out;
}

std::wstring Widen(const std::string& text) {
  if (text.empty())
    return {};

  const int length = MultiByteToWideChar(
      CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0)
    return {};

  std::wstring out(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      out.data(), length);
  return out;
}

char LowerAscii(char value) {
  return (value >= 'A' && value <= 'Z') ? static_cast<char>(value - 'A' + 'a')
                                        : value;
}

bool LoadUnwinder() {
  HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
  if (!kernel)
    return false;

  g_lookup_entry = reinterpret_cast<RtlLookupFunctionEntryFn>(
      GetProcAddress(kernel, "RtlLookupFunctionEntry"));
  g_virtual_unwind = reinterpret_cast<RtlVirtualUnwindFn>(
      GetProcAddress(kernel, "RtlVirtualUnwind"));
  return g_lookup_entry && g_virtual_unwind;
}

const ModuleRange* FindModule(const std::vector<ModuleRange>& modules,
                              std::uint64_t address) {
  auto it = std::upper_bound(modules.begin(), modules.end(), address,
                             [](std::uint64_t value, const ModuleRange& range) {
                               return value < range.base;
                             });
  if (it == modules.begin())
    return nullptr;

  --it;
  return address < it->end ? &*it : nullptr;
}

/* Enumerated on the sampler thread while the engine is running, so the
   allocation a toolhelp snapshot does cannot land on a thread that is holding
   the heap lock.  A module that loads later is picked up by the next refresh;
   a frame inside it is dropped until then. */
void RefreshModules() {
  std::vector<ModuleRange> modules;

  HANDLE snapshot = CreateToolhelp32Snapshot(
      TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
  if (snapshot != INVALID_HANDLE_VALUE) {
    MODULEENTRY32W entry;
    entry.dwSize = sizeof(entry);

    if (Module32FirstW(snapshot, &entry)) {
      do {
        ModuleRange range;
        range.base = reinterpret_cast<std::uint64_t>(entry.modBaseAddr);
        range.end = range.base + static_cast<std::uint64_t>(entry.modBaseSize);
        if (range.end <= range.base)
          continue;
        range.name = Narrow(entry.szModule);
        modules.push_back(std::move(range));
      } while (Module32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
  }

  std::sort(modules.begin(), modules.end(),
            [](const ModuleRange& left, const ModuleRange& right) {
              return left.base < right.base;
            });

  g_modules = modules;

  std::lock_guard guard(g_recorder.lock);
  g_recorder.modules = std::move(modules);
}

/* Walks one stack.  The whole walk is inside a `__try`: reading another
   thread's stack is inherently a guess about memory that the other thread is
   allowed to change the instant it is resumed, and a frame that has been popped
   and its page released in between is a fault, not a bug.  A fault ends the
   walk with the frames collected so far, which is exactly the answer a profiler
   wants from a stack it cannot finish reading.

   Every frame is checked against the table before it is used: the address has
   to be inside a module this process has loaded, the unwinder has to return an
   entry whose image base is that module, and the entry has to actually cover
   the address.  An address that fails any of them is not unwindable -- it is a
   return address that landed in padding, or a module that went away -- and the
   only thing left to do is pop one return address off the stack, which is what
   the leaf case below does. */
int WalkStack(const CONTEXT& start, std::uint64_t* frames) {
  CONTEXT context = start;
  int depth = 0;

  __try {
    while (depth < g_max_depth) {
      const std::uint64_t rip = context.Rip;
      if (!rip)
        break;

      const ModuleRange* module = FindModule(g_modules, rip);
      if (module || depth == 0)
        frames[depth++] = rip;
      if (!module)
        break;

      DWORD64 image_base = 0;
      PRUNTIME_FUNCTION entry = g_lookup_entry(rip, &image_base, nullptr);
      const bool unwindable = entry && image_base == module->base &&
                              image_base + entry->BeginAddress <= rip &&
                              rip < image_base + entry->EndAddress;

      if (!unwindable) {
        const std::uint64_t stack_pointer = context.Rsp;
        if (stack_pointer < g_stack_low + 0x1000 ||
            stack_pointer + 0x10 > g_stack_high)
          break;

        const std::uint64_t next =
            *reinterpret_cast<const std::uint64_t*>(stack_pointer);
        context.Rip = next;
        context.Rsp = stack_pointer + 8;
        if (!next)
          break;
      } else {
        PVOID handler_data = nullptr;
        DWORD64 establisher_frame = 0;
        KNONVOLATILE_CONTEXT_POINTERS pointers{};
        g_virtual_unwind(UNW_FLAG_NHANDLER, image_base, rip, entry, &context,
                         &handler_data, &establisher_frame, &pointers);

        if (!context.Rip)
          break;
        if (context.Rsp < g_stack_low + 0x1000 || context.Rsp > g_stack_high)
          break;
      }
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }

  return depth;
}

void SampleOnce() {
  if (SuspendThread(g_target) == static_cast<DWORD>(-1))
    return;

  CONTEXT context;
  std::memset(&context, 0, sizeof(context));
  context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
  const bool captured = GetThreadContext(g_target, &context) != FALSE;

  std::uint64_t frames[kMaxDepthLimit];
  int depth = 0;
  if (captured)
    depth = WalkStack(context, frames);

  const auto phase = static_cast<std::uint8_t>(urge::CurrentFramePhase());

  ResumeThread(g_target);

  std::lock_guard guard(g_recorder.lock);

  if (g_recorder.samples.size() >= kMaxSamples ||
      g_recorder.tape.size() + static_cast<std::size_t>(depth) >
          g_recorder.tape.capacity()) {
    ++g_recorder.dropped;
    return;
  }

  SampleRecord record;
  record.offset = static_cast<std::uint32_t>(g_recorder.tape.size());
  record.depth = static_cast<std::uint16_t>(depth);
  record.phase = phase;

  g_recorder.tape.insert(g_recorder.tape.end(), frames, frames + depth);
  g_recorder.samples.push_back(record);

  if (depth >= g_max_depth)
    ++g_truncated;

  ++g_sampled;
}

void WaitInterval(HANDLE timer) {
  if (!timer) {
    Sleep(1);
    return;
  }

  LARGE_INTEGER due;
  due.QuadPart = -static_cast<LONGLONG>(g_interval_us) * 10;
  if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
    WaitForSingleObject(timer, INFINITE);
  else
    Sleep(1);
}

void Start() {
  if (!g_thread)
    return;
  if (g_sampling.exchange(true))
    return;

  {
    std::lock_guard guard(g_recorder.lock);
    g_recorder.tape.clear();
    g_recorder.samples.clear();
    g_recorder.dropped = 0;
  }

  g_sampled.store(0);
  g_truncated.store(0);
  g_started = NowTicks();
  g_stopped = 0;

  /* The frame budget in the report has to describe the window the samples came
     from, not the whole session: a game that is profiled from the middle of a
     level would otherwise report an average over everything that ran before the
     profiler was switched on. */
  urge::GetRenderStats().run.Reset();

  LOGGER_INFO("[Profile] sampling started ({} us interval)", g_interval_us);
}

DWORD WINAPI SamplerThread(void*) {
  HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr,
                                        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                        TIMER_ALL_ACCESS);
  if (!timer)
    timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);

  RefreshModules();

  bool hotkey_down = false;
  std::uint32_t since_refresh = 0;

  for (;;) {
    if (WaitForSingleObject(g_stop, 0) == WAIT_OBJECT_0)
      break;

    const bool down = (GetAsyncKeyState(kHotkeyVirtualKey) & 0x8000) != 0;
    if (down && !hotkey_down)
      Toggle();
    hotkey_down = down;

    if (!g_sampling.load(std::memory_order_relaxed)) {
      WaitForSingleObject(g_stop, 8);
      continue;
    }

    g_busy.store(true);
    SampleOnce();
    g_busy.store(false);

    if (++since_refresh >= kModuleRefreshSamples) {
      since_refresh = 0;
      RefreshModules();
    }

    WaitInterval(timer);
  }

  g_busy.store(false);

  if (timer)
    CloseHandle(timer);
  return 0;
}

/* ------------------------------------------------------------------------- */
/* Symbol resolution and the report.                                          */
/* ------------------------------------------------------------------------- */

struct DbgHelpApi {
  HMODULE module = nullptr;
  decltype(&::SymInitializeW) initialize = nullptr;
  decltype(&::SymSetOptions) set_options = nullptr;
  decltype(&::SymFromAddrW) from_addr = nullptr;
  decltype(&::SymGetLineFromAddrW64) line_from_addr = nullptr;
  decltype(&::SymGetModuleInfoW64) module_info = nullptr;
  decltype(&::SymGetTypeInfo) type_info = nullptr;
  decltype(&::SymCleanup) cleanup = nullptr;
};

struct SymbolInfo {
  std::string name;
  std::string module;
};

struct Resolved {
  std::uint32_t symbol = 0;
  std::uint32_t file = 0;
  std::uint32_t line = 0;
};

class Resolver {
 public:
  bool Open() {
    api_.module = LoadLibraryW(L"dbghelp.dll");
    if (!api_.module)
      return false;

    api_.initialize = reinterpret_cast<decltype(&::SymInitializeW)>(
        GetProcAddress(api_.module, "SymInitializeW"));
    api_.set_options = reinterpret_cast<decltype(&::SymSetOptions)>(
        GetProcAddress(api_.module, "SymSetOptions"));
    api_.from_addr = reinterpret_cast<decltype(&::SymFromAddrW)>(
        GetProcAddress(api_.module, "SymFromAddrW"));
    api_.line_from_addr = reinterpret_cast<decltype(&::SymGetLineFromAddrW64)>(
        GetProcAddress(api_.module, "SymGetLineFromAddrW64"));
    api_.module_info = reinterpret_cast<decltype(&::SymGetModuleInfoW64)>(
        GetProcAddress(api_.module, "SymGetModuleInfoW64"));
    api_.type_info = reinterpret_cast<decltype(&::SymGetTypeInfo)>(
        GetProcAddress(api_.module, "SymGetTypeInfo"));
    api_.cleanup = reinterpret_cast<decltype(&::SymCleanup)>(
        GetProcAddress(api_.module, "SymCleanup"));

    if (!api_.initialize || !api_.set_options || !api_.from_addr)
      return false;

    api_.set_options(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS |
                     SYMOPT_LOAD_LINES | SYMOPT_NO_PROMPTS |
                     SYMOPT_FAIL_CRITICAL_ERRORS);
    return api_.initialize(GetCurrentProcess(), nullptr, TRUE) != FALSE;
  }

  void Close() {
    if (api_.cleanup)
      api_.cleanup(GetCurrentProcess());
  }

  std::uint64_t resolved() const { return resolved_count_; }
  std::uint64_t attempted() const { return attempted_count_; }
  std::uint64_t exe_resolved() const { return exe_resolved_count_; }
  std::uint64_t exe_attempted() const { return exe_attempted_count_; }

  const Resolved& Resolve(std::uint64_t address,
                          const std::vector<ModuleRange>& modules) {
    auto cached = cache_.find(address);
    if (cached != cache_.end())
      return cached->second;

    ++attempted_count_;

    Resolved result;
    const ModuleRange* range = FindModule(modules, address);
    const std::string module = range ? range->name : std::string();

    const bool in_exe = range && range->base == g_exe_base;
    if (in_exe)
      ++exe_attempted_count_;

    alignas(SYMBOL_INFOW)
        std::array<std::byte, sizeof(SYMBOL_INFOW) + 512 * sizeof(wchar_t)>
            storage{};
    auto* symbol = reinterpret_cast<SYMBOL_INFOW*>(storage.data());
    symbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
    symbol->MaxNameLen = 512;

    DWORD64 displacement = 0;
    std::string name;
    std::uint64_t start = address;

    const bool named = api_.from_addr(GetCurrentProcess(), address,
                                      &displacement, symbol) != FALSE;
    if (named) {
      name = Narrow(symbol->Name);
      start = address - displacement;
    }

    std::uint32_t file = 0;
    std::uint32_t line_number = 0;
    if (api_.line_from_addr) {
      IMAGEHLP_LINEW64 line;
      std::memset(&line, 0, sizeof(line));
      line.SizeOfStruct = sizeof(line);

      DWORD line_displacement = 0;
      if (api_.line_from_addr(GetCurrentProcess(), address, &line_displacement,
                              &line)) {
        file = FileId(Narrow(line.FileName));
        line_number = line.LineNumber;
      }
    }

    if (named && (Plausible(range, symbol, displacement) || file)) {
      ++resolved_count_;
      if (in_exe)
        ++exe_resolved_count_;
    } else {
      name.clear();
      start = address;
    }

    if (name.empty()) {
      name = module.empty()
                 ? std::format("<unknown> 0x{:X}", address)
                 : std::format("{} + 0x{:X}", module,
                               range ? address - range->base : address);
    }

    auto existing = functions_.find(start);
    if (existing != functions_.end()) {
      result.symbol = existing->second;
    } else {
      result.symbol = static_cast<std::uint32_t>(symbols_.size());
      functions_.emplace(start, result.symbol);
      symbols_.push_back(SymbolInfo{std::move(name), module});
    }

    result.file = file;
    result.line = line_number;

    auto inserted = cache_.emplace(address, result);
    return inserted.first->second;
  }

  const std::vector<SymbolInfo>& symbols() const { return symbols_; }
  const std::vector<std::string>& files() const { return files_; }

 private:
  /* dbghelp answers `SymFromAddr` with the nearest symbol *at or below* the
     address, whatever the symbol is -- which is the right answer for a module
     with a debug file and a wrong one for a module that only has an export
     table, where the answer is some export hundreds of kilobytes away.  The
     debug file knows how long the function is, so the displacement can be
     checked against it; where it does not, a fixed distance is the best guess
     left, and past it the address is reported as `module + offset`. */
  bool Plausible(const ModuleRange* range,
                 const SYMBOL_INFOW* symbol,
                 DWORD64 displacement) {
    if (!displacement)
      return true;
    if (!range)
      return displacement < kDisplacementLimit;
    if (ExportOnly(range))
      return displacement < kExportDisplacementLimit;
    if (!api_.type_info)
      return displacement < kDisplacementLimit;

    ULONG64 length = 0;
    if (!api_.type_info(GetCurrentProcess(), range->base, symbol->TypeIndex,
                        TI_GET_LENGTH, &length) ||
        !length)
      return displacement < kDisplacementLimit;

    return displacement < length;
  }

  bool ExportOnly(const ModuleRange* range) {
    auto found = export_only_.find(range->base);
    if (found != export_only_.end())
      return found->second;

    IMAGEHLP_MODULEW64 info;
    std::memset(&info, 0, sizeof(info));
    info.SizeOfStruct = sizeof(info);

    bool export_only = false;
    if (api_.module_info &&
        api_.module_info(GetCurrentProcess(), range->base, &info))
      export_only = info.SymType == SymExport;

    export_only_.emplace(range->base, export_only);
    return export_only;
  }

  std::uint32_t FileId(std::string path) {
    if (path.empty())
      return 0;

    auto found = file_ids_.find(path);
    if (found != file_ids_.end())
      return found->second;

    const auto id = static_cast<std::uint32_t>(files_.size() + 1);
    file_ids_.emplace(path, id);
    files_.push_back(std::move(path));
    return id;
  }

  DbgHelpApi api_;
  std::vector<SymbolInfo> symbols_;
  std::vector<std::string> files_;
  std::unordered_map<std::uint64_t, std::uint32_t> functions_;
  std::unordered_map<std::uint64_t, bool> export_only_;
  std::unordered_map<std::string, std::uint32_t> file_ids_;
  std::unordered_map<std::uint64_t, Resolved> cache_;
  std::uint64_t resolved_count_ = 0;
  std::uint64_t attempted_count_ = 0;
  std::uint64_t exe_resolved_count_ = 0;
  std::uint64_t exe_attempted_count_ = 0;
};

struct TreeNode {
  std::uint32_t symbol = 0;
  std::uint64_t count = 0;
  std::vector<std::uint32_t> children;
};

constexpr std::uint32_t kRootSymbol = 0xFFFFFFFFu;

/* Source paths arrive as the compiler saw them, which is an absolute path on
   whichever machine built the tree -- and the tree was not necessarily built on
   this one.  The prefix every file shares is stripped so a line reads
   `core/scene/sprite.cc:214` instead of the whole path.

   The prefix is taken within one root only.  The compiler's own startup files
   live on another drive entirely (`D:\a\_work\...`), and letting one of them
   into the common prefix would collapse it to nothing for every other file. */
std::string RootOf(const std::string& path) {
  const std::size_t separator = path.find_first_of("\\/");
  if (separator == std::string::npos)
    return {};
  return path.substr(0, separator + 1);
}

std::string StripCommonPrefix(const std::vector<std::string>& files) {
  if (files.empty())
    return {};

  std::unordered_map<std::string, std::size_t> roots;
  for (const std::string& file : files)
    ++roots[RootOf(file)];

  std::string root;
  std::size_t best = 0;
  for (const auto& [candidate, count] : roots) {
    if (count > best) {
      best = count;
      root = candidate;
    }
  }
  if (root.empty())
    return {};

  std::string prefix;
  bool first = true;
  for (const std::string& file : files) {
    if (file.compare(0, root.size(), root) != 0)
      continue;

    if (first) {
      prefix = file;
      first = false;
      continue;
    }

    std::size_t index = root.size();
    while (index < prefix.size() && index < file.size() &&
           LowerAscii(prefix[index]) == LowerAscii(file[index]))
      ++index;
    prefix.resize(index);
  }

  if (prefix.size() <= root.size())
    return root;

  const std::size_t separator = prefix.find_last_of("\\/");
  if (separator == std::string::npos)
    return root;
  return prefix.substr(0, separator + 1);
}

std::string Timestamp() {
  SYSTEMTIME time;
  GetLocalTime(&time);
  return std::format("{:04}{:02}{:02}-{:02}{:02}{:02}", time.wYear, time.wMonth,
                     time.wDay, time.wHour, time.wMinute, time.wSecond);
}

std::string ReadableTime() {
  SYSTEMTIME time;
  GetLocalTime(&time);
  return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", time.wYear,
                     time.wMonth, time.wDay, time.wHour, time.wMinute,
                     time.wSecond);
}

std::string ExecutablePath() {
  wchar_t buffer[MAX_PATH * 2];
  const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH * 2);
  return length ? Narrow(std::wstring(buffer, length).c_str()) : std::string();
}

bool WriteTextFile(const std::wstring& path, const std::string& text) {
  std::FILE* file = _wfopen(path.c_str(), L"wb");
  if (!file)
    return false;

  std::fwrite(text.data(), 1, text.size(), file);
  std::fclose(file);
  return true;
}

double Percent(std::uint64_t part, std::uint64_t total) {
  return total ? static_cast<double>(part) * 100.0 / static_cast<double>(total)
               : 0.0;
}

struct FrameStats {
  std::uint64_t frames = 0;
  double average_ms = 0.0;
  double p50 = 0.0;
  double p95 = 0.0;
  double p99 = 0.0;
  double worst = 0.0;
  std::uint64_t overflow = 0;
  double phase_ms[urge::kFramePhaseCount] = {};
  bool valid = false;
};

FrameStats CaptureFrameStats() {
  FrameStats stats;
  const urge::FrameTimeTotals& run = urge::GetRenderStats().run;
  if (!run.frames)
    return stats;

  stats.valid = true;
  stats.frames = run.frames;
  stats.average_ms = run.average_ms();
  stats.p50 = run.Percentile(0.50);
  stats.p95 = run.Percentile(0.95);
  stats.p99 = run.Percentile(0.99);
  stats.worst = run.worst_ms;
  stats.overflow = run.overflow;
  for (std::size_t i = 0; i < urge::kFramePhaseCount; ++i)
    stats.phase_ms[i] = run.average_phase(static_cast<urge::FramePhase>(i));
  return stats;
}

void WriteReport() {
  std::vector<std::uint64_t> tape;
  std::vector<SampleRecord> samples;
  std::vector<ModuleRange> modules;
  std::uint64_t dropped = 0;
  {
    std::lock_guard guard(g_recorder.lock);
    tape = g_recorder.tape;
    samples = g_recorder.samples;
    modules = g_recorder.modules;
    dropped = g_recorder.dropped;
  }

  const std::uint64_t total = samples.size();
  if (!total) {
    LOGGER_INFO("[Profile] nothing was sampled");
    return;
  }

  const FrameStats frames = CaptureFrameStats();
  const double duration = TicksToSeconds(g_stopped - g_started);

  Resolver resolver;
  const bool have_symbols = resolver.Open();

  std::vector<std::uint64_t> self_counts;
  std::vector<std::uint64_t> inclusive_counts;
  std::vector<std::uint32_t> symbol_stamp;
  std::unordered_map<std::uint64_t, std::uint64_t> line_counts;
  std::array<std::uint64_t, urge::kFramePhaseCount> phase_counts{};
  std::uint64_t stacked = 0;
  std::uint32_t epoch = 0;

  std::vector<TreeNode> tree;
  tree.reserve(4096);
  tree.emplace_back();
  tree.front().symbol = kRootSymbol;

  for (std::uint64_t index = 0; index < total; ++index) {
    const SampleRecord& sample = samples[index];
    if (!sample.depth)
      continue;

    ++stacked;
    phase_counts[std::min<std::size_t>(sample.phase,
                                       urge::kFramePhaseCount - 1)] += 1;

    std::uint32_t ids[kMaxDepthLimit];
    std::uint32_t leaf_file = 0;
    std::uint32_t leaf_line = 0;

    for (int depth = 0; depth < sample.depth; ++depth) {
      const Resolved& resolved =
          resolver.Resolve(tape[sample.offset + depth], modules);
      ids[depth] = resolved.symbol;

      if (!depth) {
        leaf_file = resolved.file;
        leaf_line = resolved.line;
      }
    }

    /* Only the leaf: this table answers "which line was the processor on", and
       a caller's line is on every stack below it whether or not it did any
       work.  Counting the whole stack would rank every function by how deep it
       sits rather than by how much time it spends. */
    if (leaf_file && leaf_line && line_counts.size() < 400000)
      ++line_counts[(static_cast<std::uint64_t>(leaf_file) << 32) | leaf_line];

    const std::size_t needed = resolver.symbols().size();
    if (self_counts.size() < needed) {
      self_counts.resize(needed, 0);
      inclusive_counts.resize(needed, 0);
      symbol_stamp.resize(needed, 0);
    }

    ++self_counts[ids[0]];

    /* A function that calls itself -- the interpreter does, through `eval` --
       appears several times in one stack.  Its inclusive time is the share of
       samples it is anywhere in, so it is counted once per stack, not once per
       frame; without this a recursive stack reports more than 100% of the run.
     */
    ++epoch;
    for (int depth = 0; depth < sample.depth; ++depth) {
      const std::uint32_t symbol = ids[depth];
      if (symbol_stamp[symbol] == epoch)
        continue;
      symbol_stamp[symbol] = epoch;
      ++inclusive_counts[symbol];
    }

    std::uint32_t node = 0;
    for (int depth = sample.depth - 1; depth >= 0; --depth) {
      const std::uint32_t symbol = ids[depth];
      std::uint32_t next = kRootSymbol;
      for (std::uint32_t child : tree[node].children) {
        if (tree[child].symbol == symbol) {
          next = child;
          break;
        }
      }
      if (next == kRootSymbol) {
        if (tree.size() >= kMaxTreeNodes)
          break;
        next = static_cast<std::uint32_t>(tree.size());
        tree.push_back(TreeNode{symbol, 0, {}});
        tree[node].children.push_back(next);
      }
      node = next;
      ++tree[node].count;
    }
  }

  const std::vector<SymbolInfo>& symbols = resolver.symbols();
  const std::vector<std::string>& files = resolver.files();
  const std::string prefix = StripCommonPrefix(files);

  auto location_of = [&](std::uint32_t file_id, std::uint32_t line) {
    if (!file_id || file_id > files.size())
      return std::string();
    const std::string& path = files[file_id - 1];
    const std::size_t skip =
        path.compare(0, prefix.size(), prefix) == 0 ? prefix.size() : 0;
    return std::format("{}:{}", path.substr(skip), line);
  };

  auto append_symbol = [&](std::string& line, const SymbolInfo& symbol) {
    if (!symbol.module.empty() && symbol.module != "Game.exe")
      line += std::format("   [{}]", symbol.module);
  };

  std::string out;
  out.reserve(1 << 18);

  out += "URGE performance profile\n";
  out += "========================\n\n";

  out += "host\n";
  out += std::format("  executable   {}\n", ExecutablePath());
  out += std::format("  captured     {}\n", ReadableTime());
  out += std::format("  run time     {:.2f} s\n", duration);
  if (frames.valid)
    out += std::format(
        "  frames       {}  ({:.2f} fps average)\n", frames.frames,
        duration > 0 ? static_cast<double>(frames.frames) / duration : 0.0);
  out += "\n";

  if (frames.valid) {
    const double phase_total = frames.average_ms > 0 ? frames.average_ms : 1.0;
    out += "frame budget (the sampled window, engine's own counters)\n";
    out += std::format("  average      {:.2f} ms\n", frames.average_ms);
    out += std::format("  p50          {:.2f} ms\n", frames.p50);
    out += std::format("  p95          {:.2f} ms\n", frames.p95);
    out += std::format("  p99          {:.2f} ms\n", frames.p99);
    out += std::format("  worst        {:.2f} ms", frames.worst);
    if (frames.overflow)
      out += std::format("  ({} frames over {} ms)", frames.overflow,
                         urge::FrameTimeTotals::kBucketCount);
    out += "\n";
    for (std::size_t i = 0; i < urge::kFramePhaseCount; ++i)
      out += std::format("  {:<11}  {:6.2f} ms  {:5.1f}%\n",
                         urge::FramePhaseName(static_cast<urge::FramePhase>(i)),
                         frames.phase_ms[i],
                         frames.phase_ms[i] * 100.0 / phase_total);
    out += "\n";
  }

  {
    const urge::RenderFrameCounters& counters = urge::GetRenderStats().last;
    out += "render (last frame)\n";
    out += std::format("  submits      {} frame, {} bitmap\n",
                       counters.frame_submits, counters.bitmap_submits);
    out += std::format("  bitmap ops   {}\n", counters.bitmap_ops);
    out += std::format(
        "  bitmap time  upload {:.2f} ms  record {:.2f} ms  finish {:.2f} ms  "
        "submit {:.2f} ms\n",
        counters.bitmap_upload_ms, counters.bitmap_record_ms,
        counters.bitmap_finish_ms, counters.bitmap_submit_ms);
    out += "\n";
  }

  out += "cpu profile\n";
  out += std::format(
      "  samples      {}  ({} us interval, ~{:.0f} Hz)\n", total, g_interval_us,
      duration > 0 ? static_cast<double>(total) / duration : 0.0);
  out += std::format("  dropped      {}\n", dropped);
  out += std::format("  truncated    {}\n", g_truncated.load());
  if (have_symbols)
    out += std::format(
        "  symbols      {} resolved of {} addresses ({} of {} in the exe)\n",
        resolver.resolved(), resolver.attempted(), resolver.exe_resolved(),
        resolver.exe_attempted());
  else
    out += "  symbols      none (dbghelp failed; addresses only)\n";
  out += "\n";

  if (have_symbols && resolver.exe_attempted() &&
      resolver.exe_resolved() * 2 < resolver.exe_attempted())
    out +=
        "  ! Less than half of the engine's own addresses resolved. Build\n"
        "    RelWithDebInfo and keep Game.pdb beside Game.exe.\n\n";

  out += "time by phase (of the stacks sampled)\n";
  for (std::size_t i = 0; i < urge::kFramePhaseCount; ++i) {
    if (!phase_counts[i])
      continue;
    out += std::format("  {:<11}  {:7}  {:5.1f}%\n",
                       urge::FramePhaseName(static_cast<urge::FramePhase>(i)),
                       phase_counts[i], Percent(phase_counts[i], stacked));
  }
  out += "\n";

  auto rank = [&](const std::vector<std::uint64_t>& counts) {
    std::vector<std::uint32_t> order(counts.size());
    for (std::uint32_t i = 0; i < order.size(); ++i)
      order[i] = i;
    std::sort(order.begin(), order.end(),
              [&](std::uint32_t left, std::uint32_t right) {
                if (counts[left] != counts[right])
                  return counts[left] > counts[right];
                return symbols[left].name < symbols[right].name;
              });
    if (order.size() > kTopRows)
      order.resize(kTopRows);
    return order;
  };

  out += "self time -- the leaf frame of every sample\n";
  out += "    #    self%       n  function\n";
  {
    std::uint32_t row = 0;
    for (std::uint32_t index : rank(self_counts)) {
      if (!self_counts[index])
        break;
      std::string line = std::format("  {:3}  {:6.2f}%  {:6}  {}", ++row,
                                     Percent(self_counts[index], total),
                                     self_counts[index], symbols[index].name);
      append_symbol(line, symbols[index]);
      out += line;
      out += "\n";
    }
  }
  out += "\n";

  out += "inclusive time -- every frame of every sample, once per sample\n";
  out += "    #     incl%       n  function\n";
  {
    std::uint32_t row = 0;
    for (std::uint32_t index : rank(inclusive_counts)) {
      if (!inclusive_counts[index])
        break;
      std::string line =
          std::format("  {:3}  {:6.2f}%  {:6}  {}", ++row,
                      Percent(inclusive_counts[index], total),
                      inclusive_counts[index], symbols[index].name);
      append_symbol(line, symbols[index]);
      out += line;
      out += "\n";
    }
  }
  out += "\n";

  if (!line_counts.empty()) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> lines(
        line_counts.begin(), line_counts.end());
    std::sort(lines.begin(), lines.end(),
              [](const auto& left, const auto& right) {
                return left.second > right.second;
              });
    if (lines.size() > kTopRows)
      lines.resize(kTopRows);

    out += "hot source lines -- where the leaf frame landed\n";
    out += "    #    self%       n  location\n";
    std::uint32_t row = 0;
    for (const auto& [key, count] : lines) {
      const auto file_id = static_cast<std::uint32_t>(key >> 32);
      const auto line = static_cast<std::uint32_t>(key & 0xFFFFFFFFu);
      out +=
          std::format("  {:3}  {:6.2f}%  {:6}  {}\n", ++row,
                      Percent(count, total), count, location_of(file_id, line));
    }
    out += "\n";
  }

  for (TreeNode& node : tree) {
    std::sort(node.children.begin(), node.children.end(),
              [&](std::uint32_t left, std::uint32_t right) {
                if (tree[left].count != tree[right].count)
                  return tree[left].count > tree[right].count;
                return tree[left].symbol < tree[right].symbol;
              });
  }

  out += "call tree -- inclusive share, children sorted by weight\n";
  {
    int budget = kTreeLines;

    std::vector<std::pair<std::uint32_t, int>> stack;
    for (auto it = tree[0].children.rbegin(); it != tree[0].children.rend();
         ++it)
      stack.emplace_back(*it, 1);

    while (!stack.empty() && budget > 0) {
      const auto [node, depth] = stack.back();
      stack.pop_back();

      const double share = Percent(tree[node].count, stacked);
      std::uint64_t children_count = 0;
      for (std::uint32_t child : tree[node].children)
        children_count += tree[child].count;
      const std::uint64_t self = tree[node].count - children_count;

      if (share >= kTreeThreshold || depth <= 2) {
        const SymbolInfo& symbol = symbols[tree[node].symbol];
        std::string line = std::format(
            "  {:>6.2f}%  {:>6.2f}%  {}{}", share, Percent(self, total),
            std::string(static_cast<std::size_t>(depth) * 2, ' '), symbol.name);
        append_symbol(line, symbol);
        out += line;
        out += "\n";
        --budget;
      }

      if (share < kTreeThreshold && depth > 2)
        continue;

      for (auto it = tree[node].children.rbegin();
           it != tree[node].children.rend(); ++it)
        stack.emplace_back(*it, depth + 1);
    }
  }

  const std::string stamp = Timestamp();
  const std::wstring base =
      Widen(g_output_dir) + L"\\" +
      Widen(std::format("{}-profile-{}", g_prefix, stamp));

  if (WriteTextFile(base + L".txt", out))
    LOGGER_INFO("[Profile] written: {}-profile-{}.txt", g_prefix, stamp);
  else
    LOGGER_WARN("[Profile] could not write {}-profile-{}.txt", g_prefix, stamp);

  std::string json;
  json.reserve(1 << 20);
  json +=
      "{\n  \"$schema\": "
      "\"https://www.speedscope.app/file-format-schema.json\",\n";
  json += "  \"shared\": {\n    \"frames\": [\n";
  for (std::size_t index = 0; index < symbols.size(); ++index) {
    json += std::format("      {{\"name\": \"{}\", \"file\": \"{}\"}}{}\n",
                        symbols[index].name, symbols[index].module,
                        index + 1 < symbols.size() ? "," : "");
  }
  json += "    ]\n  },\n  \"profiles\": [{\n";
  json += "    \"type\": \"sampled\",\n";
  json += std::format("    \"name\": \"{}\",\n", g_prefix);
  json += "    \"unit\": \"seconds\",\n";
  json += "    \"startValue\": 0,\n";
  json += std::format("    \"endValue\": {:.6f},\n", duration);
  json += "    \"samples\": [\n";

  for (std::uint64_t index = 0; index < total; ++index) {
    const SampleRecord& sample = samples[index];
    json += "      [";
    for (int depth = sample.depth - 1; depth >= 0; --depth) {
      const Resolved& resolved =
          resolver.Resolve(tape[sample.offset + depth], modules);
      json += std::format("{}{}", resolved.symbol, depth ? ", " : "");
    }
    json += std::format("]{}", index + 1 < total ? "," : "");
    json += "\n";
  }

  json += "    ],\n    \"weights\": [\n";
  for (std::uint64_t index = 0; index < total; ++index)
    json += std::format("      {:.6f}{}\n", g_interval_us / 1'000'000.0,
                        index + 1 < total ? "," : "");
  json += "    ]\n  }],\n  \"exporter\": \"URGE profiler\"\n}\n";

  if (WriteTextFile(base + L".json", json))
    LOGGER_INFO("[Profile] written: {}-profile-{}.json", g_prefix, stamp);

  resolver.Close();
}

}  // namespace

bool Install(const std::string& utf8_output_dir,
             const std::string& utf8_prefix,
             const Options& options,
             bool start_now) {
  if (g_thread)
    return true;

  if (!LoadUnwinder())
    return false;

  g_output_dir = utf8_output_dir;
  g_prefix = utf8_prefix.empty() ? std::string("Game") : utf8_prefix;
  g_interval_us = options.interval_us > 0 ? options.interval_us : 2000;
  g_max_depth = options.max_depth > 0 ? options.max_depth : 96;
  if (g_max_depth > kMaxDepthLimit)
    g_max_depth = kMaxDepthLimit;
  g_max_frames = options.max_frames;

  /* A real handle, not the pseudo-handle `GetCurrentThread` hands out: that one
     means "the calling thread", so the sampler would suspend itself instead of
     the engine.  `OpenThread` also pins the rights down to the two calls the
     sampler makes. */
  g_exe_base = reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));

  g_target = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE,
                        GetCurrentThreadId());
  if (!g_target)
    return false;

  /* The stack limits of the thread that will be sampled, read from its own TEB.
     `GetCurrentThreadStackLimits` would say the same thing but only exists from
     Windows 8 on, and its declaration depends on `_WIN32_WINNT`, which this
     tree does not pin. */
  const auto* teb = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
  g_stack_low = reinterpret_cast<std::uint64_t>(teb->StackLimit);
  g_stack_high = reinterpret_cast<std::uint64_t>(teb->StackBase);

  {
    std::lock_guard guard(g_recorder.lock);
    g_recorder.tape.reserve(g_max_frames);
    g_recorder.samples.reserve(kMaxSamples);
  }

  g_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!g_stop)
    return false;

  g_thread =
      CreateThread(nullptr, 256 * 1024, &SamplerThread, nullptr, 0, nullptr);
  if (!g_thread) {
    CloseHandle(g_stop);
    g_stop = nullptr;
    return false;
  }

  LOGGER_INFO("[Profile] installed ({} us interval, F4 toggles)",
              g_interval_us);

  if (start_now)
    Toggle();
  return true;
}

void Toggle() {
  if (g_sampling.load())
    Stop();
  else
    Start();
}

bool Running() {
  return g_sampling.load();
}

void Stop() {
  if (!g_sampling.exchange(false))
    return;

  g_stopped = NowTicks();

  for (int attempt = 0; attempt < 2000 && g_busy.load(); ++attempt)
    Sleep(1);

  LOGGER_INFO("[Profile] stopping, {} samples", g_sampled.load());
  WriteReport();
}

void Uninstall() {
  if (!g_thread)
    return;

  g_sampling.store(false);
  SetEvent(g_stop);
  WaitForSingleObject(g_thread, 5000);
  CloseHandle(g_thread);
  g_thread = nullptr;

  if (g_stop) {
    CloseHandle(g_stop);
    g_stop = nullptr;
  }

  if (g_target) {
    CloseHandle(g_target);
    g_target = nullptr;
  }

  std::lock_guard guard(g_recorder.lock);
  g_recorder.tape = {};
  g_recorder.samples = {};
  g_recorder.modules = {};
  g_recorder.tape.shrink_to_fit();
  g_recorder.samples.shrink_to_fit();
}

}  // namespace profiler
}  // namespace platform
