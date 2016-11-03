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

#include "OringFilter.h"
#include "Filter.h"

using std::string;

bool OringFilter::accepts(void *row, contact *auth_user, int timezone_offset) {
    for (auto filter : _subfilters) {
        if (filter->accepts(row, auth_user, timezone_offset)) {
            return true;
        }
    }
    return false;
}

bool OringFilter::optimizeBitmask(const string &column_name, uint32_t *mask,
                                  int timezone_offset) const {
    // We can only optimize, if *all* subfilters are filters for the
    // same column.
    uint32_t m = 0;

    for (auto filter : _subfilters) {
        uint32_t mm = 0xffffffff;
        if (!filter->optimizeBitmask(column_name, &mm, timezone_offset)) {
            return false;  // wrong column
        }
        m |= mm;
    }
    *mask &= m;
    return true;
}
