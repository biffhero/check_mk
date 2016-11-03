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

#include "ServicelistStateColumn.h"
#include "auth.h"

// return true if state1 is worse than state2
bool ServicelistStateColumn::svcStateIsWorse(int32_t state1, int32_t state2) {
    if (state1 == 0) {
        return false;  // OK is worse than nothing
    }
    if (state2 == 0) {
        return true;  // everything else is worse then OK
    }
    if (state2 == 2) {
        return false;  // nothing is worse than CRIT
    }
    if (state1 == 2) {
        return true;  // state1 is CRIT, state2 not
    }
    return (state1 > state2);  // both or WARN or UNKNOWN
}

servicesmember *ServicelistStateColumn::getMembers(void *data) {
    data = shiftPointer(data);
    if (data == nullptr) {
        return nullptr;
    }

    return *reinterpret_cast<servicesmember **>(reinterpret_cast<char *>(data) +
                                                _offset);
}

int32_t ServicelistStateColumn::getValue(int logictype, servicesmember *mem,
                                         contact *auth_user) {
    int32_t result = 0;

    for (; mem != nullptr; mem = mem->next) {
        service *svc = mem->service_ptr;
        if (auth_user == nullptr ||
            is_authorized_for(auth_user, svc->host_ptr, svc)) {
            int lt = logictype;
            int state;
            int has_been_checked;
            if (logictype >= 60) {
                state = svc->last_hard_state;
                lt -= 64;
            } else {
                state = svc->current_state;
            }

            has_been_checked = svc->has_been_checked;

            switch (lt) {
                case SLSC_WORST_STATE:
                    if (svcStateIsWorse(state, result)) {
                        result = state;
                    }
                    break;
                case SLSC_NUM:
                    result++;
                    break;
                case SLSC_NUM_PENDING:
                    if (has_been_checked == 0) {
                        result++;
                    }
                    break;
                default:
                    if ((has_been_checked != 0) && state == lt) {
                        result++;
                    }
                    break;
            }
        }
    }
    return result;
}

int32_t ServicelistStateColumn::getValue(void *row, contact *auth_user) {
    return getValue(_logictype, getMembers(row), auth_user);
}
