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

#ifndef StatsColumn_h
#define StatsColumn_h

#include "config.h"  // IWYU pragma: keep
#include "Aggregator.h"
class Column;
class Filter;

class StatsColumn {
    Column *_column;
    Filter *_filter;
    StatsOperation _operation;

public:
    StatsColumn(Column *c, Filter *f, StatsOperation o)
        : _column(c), _filter(f), _operation(o) {}
    ~StatsColumn();
    Column *column() const { return _column; }
    StatsOperation operation() { return _operation; }
    Filter *stealFilter() {
        Filter *f = _filter;
        _filter = 0;
        return f;
    }
    Aggregator *createAggregator();
};

#endif  // StatsColumn_h
