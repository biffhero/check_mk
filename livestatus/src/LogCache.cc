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

#include "LogCache.h"
#include <dirent.h>
#include <unistd.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <ostream>
#include <string>
#include <utility>
#include "Logfile.h"
#include "Logger.h"

#ifdef CMC
#include <chrono>
#include "Core.h"
using std::chrono::system_clock;
#else
extern time_t last_log_rotation;
#endif  // CMC

#define CHECK_MEM_CYCLE 1000 /* Check memory every N'th new message */

// watch nagios' logfile rotation
extern char *log_archive_path;
extern char *log_file;

int num_cached_log_messages = 0;

LogCache::LogCache(Logger *logger, const CommandsHolder &commands_holder,
                   unsigned long max_cached_messages)
    : _commands_holder(commands_holder)
    , _max_cached_messages(max_cached_messages)
    , _num_at_last_check(0)
    , _logger(logger) {
    updateLogfileIndex();
}

#ifdef CMC
void LogCache::setMaxCachedMessages(unsigned long m) {
    if (m != _max_cached_messages) {
        Notice(_logger)
            << "changing maximum number of messages for log file cache to "
            << m;
        _max_cached_messages = m;
    }
}
#endif

LogCache::~LogCache() { forgetLogfiles(); }

bool LogCache::logCachePreChecks(
#ifdef CMC
    Core *core
#endif
    ) {
    // Do we have any logfiles (should always be the case, but we don't want to
    // crash...
    if (_logfiles.empty()) {
        Informational(_logger) << "no log file found, not even " << log_file;
        return false;
    }
// Has Nagios rotated logfiles? => Update our file index. And delete all
// memorized log messages.
#ifdef CMC
    if (core->_last_logfile_rotation >
        system_clock::from_time_t(_last_index_update)) {
#else
    if (last_log_rotation > _last_index_update) {
#endif
        Informational(_logger)
            << "core has rotated log files, rebuilding log file index";
        forgetLogfiles();
        updateLogfileIndex();
    }
    return true;
}

void LogCache::forgetLogfiles() {
    Informational(_logger) << "flushing complete log file cache.";
    for (auto &logfile : _logfiles) {
        delete logfile.second;
    }
    _logfiles.clear();
    num_cached_log_messages = 0;
}

void LogCache::updateLogfileIndex() {
    _last_index_update = time(nullptr);
    // We need to find all relevant logfiles. This includes
    // directory.
    // the current nagios.log and all files in the archive
    scanLogfile(log_file, true);

    DIR *dir = opendir(log_archive_path);

    if (dir != nullptr) {
        char abspath[4096];
        struct dirent *ent, *result;
        int len = offsetof(struct dirent, d_name) +
                  pathconf(log_archive_path, _PC_NAME_MAX) + 1;
        ent = static_cast<struct dirent *>(malloc(len));

        while (0 == readdir_r(dir, ent, &result) && result != nullptr) {
            if (ent->d_name[0] != '.') {
                snprintf(abspath, sizeof(abspath), "%s/%s", log_archive_path,
                         ent->d_name);
                scanLogfile(abspath, false);
            }
            // ent = result;
        }
        free(ent);
        closedir(dir);
    } else {
        Informational(_logger) << "cannot open log archive "
                               << log_archive_path;
    }
}

void LogCache::scanLogfile(char *path, bool watch) {
    auto logfile = new Logfile(_logger, _commands_holder, path, watch);
    time_t since = logfile->since();
    if (since != 0) {
        // make sure that no entry with that 'since' is existing yet.
        // under normal circumstances this never happens. But the
        // user might have copied files around.
        if (_logfiles.find(since) == _logfiles.end()) {
            _logfiles.emplace(since, logfile);
        } else {
            Warning(_logger) << "ignoring duplicate log file " << path;
            delete logfile;
        }
    } else {
        delete logfile;
    }
}

/* This method is called each time a log message is loaded
   into memory. If the number of messages loaded in memory
   is to large, memory will be freed by flushing logfiles
   and message not needed by the current query.

   The parameters to this method reflect the current query,
   not the messages that just has been loaded.
 */
void LogCache::handleNewMessage(Logfile *logfile, time_t /*unused*/,
                                time_t /*unused*/, unsigned logclasses) {
    if (static_cast<unsigned long>(++num_cached_log_messages) <=
        _max_cached_messages) {
        return;  // current message count still allowed, everything ok
    }

    /* Memory checking an freeing consumes CPU ressources. We save
       ressources, by avoiding to make the memory check each time
       a new message is loaded when being in a sitation where no
       memory can be freed. We do this by suppressing the check when
       the number of messages loaded into memory has not grown
       by at least CHECK_MEM_CYCLE messages */
    if (static_cast<unsigned long>(num_cached_log_messages) <
        _num_at_last_check + CHECK_MEM_CYCLE) {
        return;  // Do not check this time
    }

    // [1] Begin by deleting old logfiles
    // Begin deleting with the oldest logfile available
    _logfiles_t::iterator it;
    for (it = _logfiles.begin(); it != _logfiles.end(); ++it) {
        Logfile *log = it->second;
        if (log == logfile) {
            // Do not touch the logfile the Query is currently accessing
            break;
        }
        if (log->numEntries() > 0) {
            num_cached_log_messages -= log->numEntries();
            log->flush();  // drop all messages of that file
            if (static_cast<unsigned long>(num_cached_log_messages) <=
                _max_cached_messages) {
                // remember the number of log messages in cache when
                // the last memory-release was done. No further
                // release-check shall be done until that number changes.
                _num_at_last_check = num_cached_log_messages;
                return;
            }
        }
    }
    // The end of this loop must be reached by 'break'. At least one logfile
    // must be the current logfile. So now 'it' points to the current logfile.
    // We save that pointer for later.
    auto queryit = it;

    // [2] Delete message classes irrelevent to current query
    // Starting from the current logfile (wo broke out of the
    // previous loop just when 'it' pointed to that)
    for (; it != _logfiles.end(); ++it) {
        Logfile *log = it->second;
        if (log->numEntries() > 0 && (log->classesRead() & ~logclasses) != 0) {
            Debug(_logger) << "freeing classes " << ~logclasses << " of file "
                           << log->path();
            long freed = log->freeMessages(~logclasses);  // flush only messages
                                                          // not needed for
                                                          // current query
            num_cached_log_messages -= freed;
            if (static_cast<unsigned long>(num_cached_log_messages) <=
                _max_cached_messages) {
                _num_at_last_check = num_cached_log_messages;
                return;
            }
        }
    }

    // [3] Flush newest logfiles
    // If there are still too many messages loaded, continue
    // flushing logfiles from the oldest to the newest starting
    // at the file just after (i.e. newer than) the current logfile
    for (it = ++queryit; it != _logfiles.end(); ++it) {
        Logfile *log = it->second;
        if (log->numEntries() > 0) {
            Debug(_logger) << "flush newer log, " << log->numEntries()
                           << " number of entries";
            num_cached_log_messages -= log->numEntries();
            log->flush();
            if (static_cast<unsigned long>(num_cached_log_messages) <=
                _max_cached_messages) {
                _num_at_last_check = num_cached_log_messages;
                return;
            }
        }
    }
    _num_at_last_check = num_cached_log_messages;
    // If we reach this point, no more logfiles can be unloaded,
    // despite the fact that there are still too many messages
    // loaded.

    Debug(_logger) << "cannot unload more messages, still "
                   << num_cached_log_messages << " loaded (max is "
                   << _max_cached_messages << ")";
}
