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

#include "SectionPS.h"
#include "../types.h"
#include "../dynamic_func.h"
#include "../PerfCounter.h"
#include "../logging.h"
#include <windows.h>
#include <tlhelp32.h>

SectionPS::SectionPS(Configuration &config)
    : Section("ps")
    , _use_wmi(config, "ps", "use_wmi", false)
    , _full_commandline(config, "ps", "full_path", false)
{
    withSeparator('\t');
}

SectionPS::process_entry_t SectionPS::getProcessPerfdata() {
    process_entry_t process_info;

    PerfCounterObject counterObject(230);  // process base number

    if (!counterObject.isEmpty()) {
        LARGE_INTEGER Frequency;
        QueryPerformanceFrequency(&Frequency);

        std::vector<PERF_INSTANCE_DEFINITION *> instances =
            counterObject.instances();

        std::vector<process_entry> entries(
            instances.size());  // one instance = one process

        // gather counters
        for (const PerfCounter &counter : counterObject.counters()) {
            std::vector<ULONGLONG> values = counter.values(instances);
            for (std::size_t i = 0; i < values.size(); ++i) {
                switch (counter.offset()) {
                    case 40:
                        entries.at(i).virtual_size = values[i];
                        break;
                    case 56:
                        entries.at(i).working_set_size = values[i];
                        break;
                    case 64:
                        entries.at(i).pagefile_usage = values[i];
                        break;
                    case 104:
                        entries.at(i).process_id = values[i];
                        break;
                }
            }
        }

        for (const process_entry &entry : entries) {
            process_info[entry.process_id] = entry;
        }
    }
    return process_info;
}

bool SectionPS::ExtractProcessOwner(HANDLE hProcess_i, std::string &csOwner_o) {
    // Get process token
    WinHandle hProcessToken;
    if (!OpenProcessToken(hProcess_i, TOKEN_READ, hProcessToken.ptr()) ||
        !hProcessToken)
        return false;

    // First get size needed, TokenUser indicates we want user information from
    // given token
    DWORD dwProcessTokenInfoAllocSize = 0;
    GetTokenInformation(hProcessToken, TokenUser, NULL, 0,
                        &dwProcessTokenInfoAllocSize);

    // Call should have failed due to zero-length buffer.
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        // Allocate buffer for user information in the token.
        PTOKEN_USER pUserToken = reinterpret_cast<PTOKEN_USER>(
            new BYTE[dwProcessTokenInfoAllocSize]);
        if (pUserToken != NULL) {
            // Now get user information in the allocated buffer
            if (GetTokenInformation(hProcessToken, TokenUser, pUserToken,
                                    dwProcessTokenInfoAllocSize,
                                    &dwProcessTokenInfoAllocSize)) {
                // Some vars that we may need
                SID_NAME_USE snuSIDNameUse;
                WCHAR szUser[MAX_PATH] = {0};
                DWORD dwUserNameLength = MAX_PATH;
                WCHAR szDomain[MAX_PATH] = {0};
                DWORD dwDomainNameLength = MAX_PATH;

                // Retrieve user name and domain name based on user's SID.
                if (LookupAccountSidW(NULL, pUserToken->User.Sid, szUser,
                                      &dwUserNameLength, szDomain,
                                      &dwDomainNameLength, &snuSIDNameUse)) {
                    char info[1024];
                    csOwner_o = "\\\\";
                    WideCharToMultiByte(CP_UTF8, 0, (WCHAR *)&szDomain, -1,
                                        info, sizeof(info), NULL, NULL);
                    csOwner_o += info;

                    csOwner_o += "\\";
                    WideCharToMultiByte(CP_UTF8, 0, (WCHAR *)&szUser, -1, info,
                                        sizeof(info), NULL, NULL);
                    csOwner_o += info;

                    delete[] pUserToken;
                    return true;
                }
            }
            delete[] pUserToken;
        }
    }
    return false;
}

bool SectionPS::produceOutputInner(std::ostream &out, const Environment&) {
    if (*_use_wmi) {
        return outputWMI(out);
    } else {
        return outputNative(out);
    }
}

void SectionPS::outputProcess(std::ostream &out, ULONGLONG virtual_size,
                              ULONGLONG working_set_size,
                              ULONGLONG pagefile_usage, ULONGLONG usermode_time,
                              ULONGLONG kernelmode_time, DWORD process_id,
                              DWORD process_handle_count, DWORD thread_count,
                              const std::string &user, LPCSTR exe_file) {
    // Note: CPU utilization is determined out of usermodetime and
    // kernelmodetime
    out << "(" << user << "," << virtual_size / 1024 << ","
        << working_set_size / 1024 << ",0"
        << "," << process_id << "," << pagefile_usage / 1024 << ","
        << usermode_time << "," << kernelmode_time << ","
        << process_handle_count << "," << thread_count << ")\t" << exe_file
        << "\n";
}

bool SectionPS::outputWMI(std::ostream &out) {
    if (_helper.get() == nullptr) {
        _helper.reset(new wmi::Helper(L"Root\\cimv2"));
    }

    wmi::Result result;
    try {
        result = _helper->getClass(L"Win32_Process");
        bool more = result.valid();

        while (more) {
            int processId = result.get<int>(L"ProcessId");

            WinHandle process(OpenProcess(
                PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId));
            std::string user = "SYSTEM";
            ExtractProcessOwner(process, user);
            std::wstring process_name;

            if (*_full_commandline && result.contains(L"ExecutablePath")) {
                process_name = result.get<std::wstring>(L"ExecutablePath");
            } else {
                process_name = result.get<std::wstring>(L"Caption");
            }

            if (*_full_commandline && result.contains(L"CommandLine")) {
                int argc;
                LPWSTR *argv = CommandLineToArgvW(
                    result.get<std::wstring>(L"CommandLine").c_str(), &argc);
                for (int i = 1; i < argc; ++i) {
                    process_name += std::wstring(L" ") + argv[i];
                }
                LocalFree(argv);
            }

            outputProcess(
                out, std::stoull(result.get<std::string>(L"VirtualSize")),
                std::stoull(result.get<std::string>(L"WorkingSetSize")),
                result.get<int>(L"PagefileUsage"),
                std::stoull(result.get<std::wstring>(L"UserModeTime")),
                std::stoull(result.get<std::wstring>(L"KernelModeTime")),
                processId, result.get<int>(L"HandleCount"),
                result.get<int>(L"ThreadCount"), user,
                to_utf8(process_name.c_str()).c_str());

            more = result.next();
        }
        return true;
    } catch (const wmi::ComException &e) {
        // the most likely cause is that the wmi query fails, i.e. because the
        // service is currently offline.
        crash_log("Exception: %s", e.what());
    } catch (const wmi::ComTypeException &e) {
        crash_log("Exception: %s", e.what());
        std::wstring types;
        std::vector<std::wstring> names;
        for (std::vector<std::wstring>::const_iterator iter = names.begin();
             iter != names.end(); ++iter) {
            types += *iter + L"=" +
                     std::to_wstring(result.typeId(iter->c_str())) + L", ";
        }
        crash_log(
            "Data types are different than expected, please report this and "
            "include the following: %ls",
            types.c_str());
    }
    return false;
}

bool SectionPS::outputNative(std::ostream &out) {
    PROCESSENTRY32 pe32;

    process_entry_t process_perfdata = getProcessPerfdata();

    WinHandle hProcessSnap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return false;
    }
    pe32.dwSize = sizeof(PROCESSENTRY32);

    bool more = Process32First(hProcessSnap, &pe32);

    // GetProcessHandleCount is only available winxp upwards
    typedef BOOL WINAPI (*GetProcessHandleCount_type)(HANDLE, PDWORD);
    GetProcessHandleCount_type GetProcessHandleCount_dyn =
        DYNAMIC_FUNC(GetProcessHandleCount, L"kernel32.dll");

    while (more) {
        std::string user = "unknown";
        DWORD dwAccess = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;
        WinHandle hProcess(OpenProcess(dwAccess, FALSE, pe32.th32ProcessID));

        // TODO the following isn't really necessary. We need the process
        // handle only to determine process owner and handle count,
        // the process list could still be useful without that.
        if (hProcess != nullptr) {
            // Process times
            FILETIME createTime, exitTime, kernelTime, userTime;
            ULARGE_INTEGER kernelmodetime, usermodetime;
            if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime,
                                &userTime) != -1) {
                kernelmodetime.LowPart = kernelTime.dwLowDateTime;
                kernelmodetime.HighPart = kernelTime.dwHighDateTime;
                usermodetime.LowPart = userTime.dwLowDateTime;
                usermodetime.HighPart = userTime.dwHighDateTime;
            }

            DWORD processHandleCount = 0;

            if (GetProcessHandleCount_dyn != nullptr) {
                GetProcessHandleCount_dyn(hProcess, &processHandleCount);
            }

            // Process owner
            ExtractProcessOwner(hProcess, user);

            // Memory levels
            ULONGLONG working_set_size = 0;
            ULONGLONG virtual_size = 0;
            ULONGLONG pagefile_usage = 0;
            process_entry_t::iterator it_perf =
                process_perfdata.find(pe32.th32ProcessID);
            if (it_perf != process_perfdata.end()) {
                working_set_size = it_perf->second.working_set_size;
                virtual_size = it_perf->second.virtual_size;
                pagefile_usage = it_perf->second.pagefile_usage;
            }

            // Note: CPU utilization is determined out of usermodetime and
            // kernelmodetime
            outputProcess(out, virtual_size, working_set_size, pagefile_usage,
                          usermodetime.QuadPart, kernelmodetime.QuadPart,
                          pe32.th32ProcessID, processHandleCount,
                          pe32.cntThreads, user, pe32.szExeFile);
        }
        more = Process32Next(hProcessSnap, &pe32);
    }
    process_perfdata.clear();

    // The process snapshot doesn't show the system idle process (used to
    // determine the number of cpu cores)
    // We simply fake this entry..
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    outputProcess(out, 0, 0, 0, 0, 0, 0, 0, sysinfo.dwNumberOfProcessors,
                  "SYSTEM", "System Idle Process");
    return true;
}
