// +------------------------------------------------------------------+
// |             ____ _               _        __  __ _  __           |
// |            / ___| |__   ___  ___| | __   |  \/  | |/ /           |
// |           | |   | '_ \ / _ \/ __| |/ /   | |\/| | ' /            |
// |           | |___| | | |  __/ (__|   <    | |  | | . \            |
// |            \____|_| |_|\___|\___|_|\_\___|_|  |_|_|\_\           |
// |                                                                  |
// | Copyright Mathias Kettner 2014             mk@mathias-kettner.de |
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
// tails. You should have  received  a copy of the  GNU  General Public
// License along with GNU Make; see the file  COPYING.  If  not,  write
// to the Free Software Foundation, Inc., 51 Franklin St,  Fifth Floor,
// Boston, MA 02110-1301 USA.

#ifndef LogCache_h
#define LogCache_h

#include "config.h"  // IWYU pragma: keep
#include <ctime>
#include <map>
#include <mutex>
#include "nagios.h"  // IWYU pragma: keep
class Column;
class CommandsHolder;
#ifdef CMC
class Core;
#endif
class Logfile;
class Logger;

typedef std::map<time_t, Logfile *> _logfiles_t;

class LogCache {
    const CommandsHolder &_commands_holder;
    unsigned long _max_cached_messages;
    unsigned long _num_at_last_check;
    Logger *const _logger;
    _logfiles_t _logfiles;

public:
    std::mutex _lock;

    LogCache(Logger *logger, const CommandsHolder &commands_holder,
             unsigned long max_cached_messages);
    ~LogCache();
#ifdef CMC
    void setMaxCachedMessages(unsigned long m);
#endif
    time_t _last_index_update;

    const char *name() { return "log"; }
    const char *namePrefix() { return "logs"; }
    void handleNewMessage(Logfile *logfile, time_t since, time_t until,
                          unsigned logclasses);
    Column *column(
        const char *colname);  // override in order to handle current_
    _logfiles_t *logfiles() { return &_logfiles; };
    void forgetLogfiles();
    void updateLogfileIndex();

    bool logCachePreChecks(
#ifdef CMC
        Core *core
#endif
        );

private:
    void scanLogfile(char *path, bool watch);
    _logfiles_t::iterator findLogfileStartingBefore(time_t);
};

#endif  // LogCache_h
