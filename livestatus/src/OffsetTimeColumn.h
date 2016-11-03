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

#ifndef OffsetTimeColumn_h
#define OffsetTimeColumn_h

#include "config.h"  // IWYU pragma: keep
#include <string>
#include "Column.h"
#include "OffsetIntColumn.h"
#include "opids.h"
class Filter;
class RowRenderer;

#ifdef CMC
#include "cmc.h"
#else
#include "nagios.h"
#endif

/* We are using IntColumn in order to implement a column of type time. This does
   almost the same as the time column, but applies a timezone offset stored in
   the Query. */

class OffsetTimeColumn : public OffsetIntColumn {
public:
    OffsetTimeColumn(const std::string &name, const std::string &description,
                     int offset, int indirect_offset = -1,
                     int extra_offset = -1)
        : OffsetIntColumn(name, description, offset, indirect_offset,
                          extra_offset) {}
    ColumnType type() override { return ColumnType::time; }
    void output(void *row, RowRenderer &r, contact *auth_user) override;
    Filter *createFilter(RelationalOperator relOp,
                         const std::string &value) override;
};

#endif  // OffsetTimeColumn_h
