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

#include "HostSpecialIntColumn.h"
#include "mk_inventory.h"
#include "pnp4nagios.h"

using std::string;

int32_t HostSpecialIntColumn::getValue(void *row, contact * /* auth_user */) {
    void *data = shiftPointer(row);
    if (data == nullptr) {
        return 0;
    }

    host *hst = static_cast<host *>(data);
    switch (_type) {
        case HSIC_REAL_HARD_STATE:
            if (hst->current_state == 0) {
                return 0;
            } else if (hst->state_type == 1) {
                return hst->current_state;  // we have reached a hard state
            } else {
                return hst->last_hard_state;
            }

        case HSIC_PNP_GRAPH_PRESENT:
            return pnpgraph_present(hst->name);

        case HSIC_MK_INVENTORY_LAST: {
            extern char g_mk_inventory_path[];
            return mk_inventory_last(string(g_mk_inventory_path) + "/" +
                                     hst->name);
        }
    }
    // never reached, make -Wall happy
    return 0;
}
