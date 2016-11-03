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

#include "ServicelistColumn.h"
#include <cstring>
#include "Renderer.h"
#include "ServicelistFilter.h"
#include "TimeperiodsCache.h"
#include "auth.h"
#include "opids.h"

extern TimeperiodsCache *g_timeperiods_cache;

using std::string;

servicesmember *ServicelistColumn::getMembers(void *data) {
    data = shiftPointer(data);
    if (data == nullptr) {
        return nullptr;
    }

    return *reinterpret_cast<servicesmember **>(reinterpret_cast<char *>(data) +
                                                _offset);
}

void ServicelistColumn::output(void *row, RowRenderer &r, contact *auth_user) {
    ListRenderer l(r);
    for (servicesmember *mem = getMembers(row); mem != nullptr;
         mem = mem->next) {
        service *svc = mem->service_ptr;
        if ((auth_user == nullptr) ||
            is_authorized_for(auth_user, svc->host_ptr, svc)) {
            // show only service name => no sublist
            if (!_show_host && _info_depth == 0) {
                l.output(string(svc->description));
            } else {
                SublistRenderer s(l);
                if (_show_host) {
                    s.output(string(svc->host_name));
                }
                s.output(string(svc->description));
                if (_info_depth >= 1) {
                    s.output(svc->current_state);
                    s.output(svc->has_been_checked);
                }
                if (_info_depth >= 2) {
                    s.output(svc->plugin_output == nullptr
                                 ? ""
                                 : string(svc->plugin_output));
                }
                if (_info_depth >= 3) {
                    s.output(svc->last_hard_state);
                    s.output(svc->current_attempt);
                    s.output(svc->max_attempts);
                    s.output(svc->scheduled_downtime_depth);
                    s.output(svc->problem_has_been_acknowledged);
                    s.output(inCustomTimeperiod(svc, "SERVICE_PERIOD"));
                }
            }
        }
    }
}

Filter *ServicelistColumn::createFilter(RelationalOperator relOp,
                                        const string &value) {
    return new ServicelistFilter(this, relOp, value);
}

int ServicelistColumn::inCustomTimeperiod(service *svc, const char *varname) {
    for (customvariablesmember *cvm = svc->custom_variables; cvm != nullptr;
         cvm = cvm->next) {
        if (strcmp(cvm->variable_name, varname) == 0) {
            return static_cast<int>(
                g_timeperiods_cache->inTimeperiod(cvm->variable_value));
        }
    }
    return 1;  // assume 7X24
}
