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

#include "TableEventConsoleHistory.h"
#include <string>
#include "MonitoringCore.h"
#include "TableEventConsoleEvents.h"

using std::string;

#ifdef CMC
TableEventConsoleHistory::TableEventConsoleHistory(
    MonitoringCore *mc, const Downtimes &downtimes_holder,
    const Comments &comments_holder, std::recursive_mutex &holder_lock,
    Core *core)
    : TableEventConsole(mc)
#else
TableEventConsoleHistory::TableEventConsoleHistory(
    MonitoringCore *mc, const DowntimesOrComments &downtimes_holder,
    const DowntimesOrComments &comments_holder)
    : TableEventConsole(mc)
#endif
{
    addColumn(new IntEventConsoleColumn(
        "history_line", "The line number of the event in the history file"));
    addColumn(new TimeEventConsoleColumn("history_time",
                                         "Time when the event was written into "
                                         "the history file (Unix timestamp)"));
    addColumn(new StringEventConsoleColumn(
        "history_what",
        "What happened (one of "
        "ARCHIVED/AUTODELETE/CANCELLED/CHANGESTATE/COUNTFAILED/COUNTREACHED/"
        "DELAYOVER/DELETE/EMAIL/EXPIRED/NEW/NOCOUNT/ORPHANED/SCRIPT/UPDATE)"));
    addColumn(new StringEventConsoleColumn(
        "history_who", "The user who triggered the command"));
    addColumn(new StringEventConsoleColumn(
        "history_addinfo",
        "Additional information, like email recipient/subject or action ID"));
    TableEventConsoleEvents::addColumns(this, downtimes_holder, comments_holder
#ifdef CMC
                                        ,
                                        holder_lock, core
#endif
                                        );
}

string TableEventConsoleHistory::name() const { return "eventconsolehistory"; }

string TableEventConsoleHistory::namePrefix() const {
    return "eventconsolehistory_";
}

// TODO(sp) This is copy-n-pasted in TableEventConsoleEvents.
// TODO(sp) Remove evil casts below.
bool TableEventConsoleHistory::isAuthorized(contact *ctc, void *data) {
    if (MonitoringCore::Host *hst = static_cast<Row *>(data)->_host) {
        return _core->host_has_contact(
            hst, reinterpret_cast<MonitoringCore::Contact *>(ctc));
    }

    ListEventConsoleColumn *col =
        static_cast<ListEventConsoleColumn *>(column("event_contact_groups"));
    if (col->isNone(data)) {
        return true;
    }

    for (const auto &name : col->getValue(data)) {
        if (_core->is_contact_member_of_contactgroup(
                _core->find_contactgroup(name),
                reinterpret_cast<MonitoringCore::Contact *>(ctc))) {
            return true;
        }
    }
    return false;
}
