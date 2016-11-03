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

#include "HostlistStateColumn.h"
#include "auth.h"

static inline bool hst_state_is_worse(int32_t state1, int32_t state2) {
    if (state1 == 0) {
        return false;  // UP is worse than nothing
    }
    if (state2 == 0) {
        return true;  // everything else is worse then UP
    }
    if (state2 == 1) {
        return false;  // nothing is worse than DOWN
    }
    if (state1 == 1) {
        return true;  // state1 is DOWN, state2 not
    }
    return false;  // both are UNREACHABLE
}

hostsmember *HostlistStateColumn::getMembers(void *data) {
    data = shiftPointer(data);
    if (data == nullptr) {
        return nullptr;
    }

    return *reinterpret_cast<hostsmember **>(reinterpret_cast<char *>(data) +
                                             _offset);
}

int32_t HostlistStateColumn::getValue(void *row, contact *auth_user) {
    int32_t result = 0;
    for (hostsmember *mem = getMembers(row); mem != nullptr; mem = mem->next) {
        host *hst = mem->host_ptr;
        if (auth_user == nullptr ||
            is_authorized_for(auth_user, hst, nullptr)) {
            switch (_logictype) {
                case HLSC_NUM_SVC_PENDING:
                case HLSC_NUM_SVC_OK:
                case HLSC_NUM_SVC_WARN:
                case HLSC_NUM_SVC_CRIT:
                case HLSC_NUM_SVC_UNKNOWN:
                case HLSC_NUM_SVC:
                    result += ServicelistStateColumn::getValue(
                        _logictype, hst->services, auth_user);
                    break;

                case HLSC_WORST_SVC_STATE: {
                    int state = ServicelistStateColumn::getValue(
                        _logictype, hst->services, auth_user);
                    if (ServicelistStateColumn::svcStateIsWorse(state,
                                                                result)) {
                        result = state;
                    }
                    break;
                }

                case HLSC_NUM_HST_UP:
                case HLSC_NUM_HST_DOWN:
                case HLSC_NUM_HST_UNREACH:
                    if (hst->has_been_checked != 0 &&
                        hst->current_state == _logictype - HLSC_NUM_HST_UP) {
                        result++;
                    }
                    break;

                case HLSC_NUM_HST_PENDING:
                    if (hst->has_been_checked == 0) {
                        result++;
                    }
                    break;

                case HLSC_NUM_HST:
                    result++;
                    break;

                case HLSC_WORST_HST_STATE:
                    if (hst_state_is_worse(hst->current_state, result)) {
                        result = hst->current_state;
                    }
                    break;
            }
        }
    }
    return result;
}
