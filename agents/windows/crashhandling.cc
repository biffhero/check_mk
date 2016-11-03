// +------------------------------------------------------------------+
// |             ____ _               _        __  __ _  __           |
// |            / ___| |__   ___  ___| | __   |  \/  | |/ /           |
// |           | |   | '_ \ / _ \/ __| |/ /   | |\/| | ' /            |
// |           | |___| | | |  __/ (__|   <    | |  | | . \            |
// |            \____|_| |_|\___|\___|_|\_\___|_|  |_|_|\_\           |
// |                                                                  |
// | Copyright Mathias Kettner 2016             mk@mathias-kettner.de |
// +------------------------------------------------------------------+
//
// This file is part of Check_MK.
// The official homepage is at http://mathias-kettner.de/check_mk.
//
// check_mk is free software;  you can redistribute it and/or modify it
// under the  terms of the  GNU General Public License  as published by
// the Free Software Foundation in version 2.  check_mk is  distributed
// in the hope that it will be useful, but WITHOUT ANY WARRANTY;  with-
// out even the implied warranty of  MERCHANTABILITY  or  FITNESS FOR A
// PARTICULAR PURPOSE. See the  GNU General Public License for more de-
// ails.  You should have  received  a copy of the  GNU  General Public
// License along with GNU Make; see the file  COPYING.  If  not,  write
// to the Free Software Foundation, Inc., 51 Franklin St,  Fifth Floor,
// Boston, MA 02110-1301 USA.

#include "crashhandling.h"
#include <dbghelp.h>
#include <windows.h>
#include <winnt.h>
#include "logging.h"
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#ifdef __x86_64

static void dump_registers(CONTEXT *c) {
    crash_log("rax %016" PRIx64 " rbx %016" PRIx64 " rcx %016" PRIx64
              " rdx %016" PRIx64,
              c->Rax, c->Rbx, c->Rcx, c->Rdx);
    crash_log("rsp %016" PRIx64 " rbp %016" PRIx64 " rsi %016" PRIx64
              " rdi %016" PRIx64,
              c->Rsp, c->Rbp, c->Rsi, c->Rdi);
    crash_log("r8  %016" PRIx64 " r9  %016" PRIx64 " r10 %016" PRIx64
              " r11 %016" PRIx64,
              c->R8, c->R9, c->R10, c->R11);
    crash_log("r12 %016" PRIx64 " r13 %016" PRIx64 " r14 %016" PRIx64
              " r15 %016" PRIx64,
              c->R12, c->R13, c->R14, c->R15);
}

/**
 * converts instruction pointer to "filename (line)"
 **/
static std::string resolve(ULONG64 rip) {
    std::string result;

    HANDLE process = ::GetCurrentProcess();
    DWORD64 symbol_offset = 0;

    {  // Get file / line of source code.
        IMAGEHLP_LINE64 line_str = {0};
        line_str.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

        if (::SymGetLineFromAddr64(process, (DWORD64)rip,
                                   (DWORD *)&symbol_offset, &line_str)) {
            result = line_str.FileName;
            result += "(";
            result += std::to_string((uint64_t)line_str.LineNumber).c_str();
            result += "): ";
        }
    }

    {  // get symbol name
        struct {
            union {
                SYMBOL_INFO symbol;
                char buf[sizeof(SYMBOL_INFO) + 1024];
            } u;
        } image_symbol = {0};

        image_symbol.u.symbol.SizeOfStruct = sizeof(SYMBOL_INFO);
        image_symbol.u.symbol.Name[0] = 0;
        image_symbol.u.symbol.MaxNameLen =
            sizeof(image_symbol) - sizeof(SYMBOL_INFO);

        // Successor of SymGetSymFromAddr64.
        if (::SymFromAddr(process, (DWORD64)rip, &symbol_offset,
                          &image_symbol.u.symbol)) {
            result += image_symbol.u.symbol.Name;
        }
    }

    return result;
}

// display backtrace. with mingw this will resolve only symbols from
// windows dlls, not our own code. we can use addr2line on the unstripped
// exe to resolve those.
static void log_backtrace(PVOID exc_address) {
    CONTEXT context;
    context.ContextFlags = CONTEXT_ALL;
    ::RtlCaptureContext(&context);

    // the backtrace includes all the stack frames from the exception handler
    // itself. Only start outputting with the frame the exception occured in
    int exc_frame = -1;

    for (int i = 0;; ++i) {
        ULONG64 rip = context.Rip;
        ULONG64 image_base;
        PRUNTIME_FUNCTION entry =
            ::RtlLookupFunctionEntry(rip, &image_base, nullptr);

        if (entry == nullptr) break;

        if (rip == reinterpret_cast<ULONG64>(exc_address)) {
            exc_frame = i;
        }

        if (exc_frame != -1) {
            crash_log("#%d %016" PRIx64 " %s", i - exc_frame, rip,
                      resolve(rip).c_str());
            dump_registers(&context);
        }

        PVOID handler_data;
        ULONG64 establisher_frame;
        ::RtlVirtualUnwind(0, image_base, rip, entry, &context, &handler_data,
                           &establisher_frame, nullptr);
    }
}

#endif

LONG WINAPI exception_handler(LPEXCEPTION_POINTERS ptrs) {
    crash_log("windows exception 0x%" PRIx32 " from address 0x%p (revision %s)",
              static_cast<unsigned int>(ptrs->ExceptionRecord->ExceptionCode),
              ptrs->ExceptionRecord->ExceptionAddress, VCS_REV);

#ifdef __x86_64

    HANDLE proc = ::GetCurrentProcess();
    ::SymInitialize(proc, nullptr, TRUE);

    ::SymSetOptions(SymGetOptions() | SYMOPT_DEFERRED_LOADS |
                    SYMOPT_NO_IMAGE_SEARCH);

    log_backtrace(ptrs->ExceptionRecord->ExceptionAddress);

    ::SymCleanup(proc);
#else   // __x86_64
// on x86 the backtrace can't be implemented in the same way
#endif  // __x86_64

    return EXCEPTION_CONTINUE_SEARCH;
}
