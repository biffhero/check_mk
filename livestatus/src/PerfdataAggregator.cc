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

#include "PerfdataAggregator.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>
#include "Renderer.h"
#include "StringColumn.h"
#include "strutil.h"

using std::string;
using std::vector;

void PerfdataAggregator::consume(void *row, contact * /* auth_user */,
                                 int /* timezone_offset */) {
    string perf_data = _column->getValue(row);
    vector<char> perf_data_vec(perf_data.begin(), perf_data.end());
    perf_data_vec.push_back('\0');
    char *scan = &perf_data_vec[0];

    while (char *entry = next_field(&scan)) {
        char *start_of_varname = entry;
        char *place_of_equal = entry;
        while ((*place_of_equal != 0) && *place_of_equal != '=') {
            place_of_equal++;
        }
        if (*place_of_equal == 0) {
            continue;  // ignore invalid perfdata
        }
        *place_of_equal = 0;  // terminate varname
        char *start_of_number = place_of_equal + 1;
        char *end_of_number = start_of_number;
        while ((*end_of_number != 0) &&
               ((isdigit(*end_of_number) != 0) || *end_of_number == '.')) {
            end_of_number++;
        }
        if (start_of_number == end_of_number) {
            continue;  // empty number
        }
        *end_of_number = 0;  // terminate number
        double value = strtod(start_of_number, nullptr);
        consumeVariable(start_of_varname, value);
    }
}

void PerfdataAggregator::consumeVariable(const string &varname, double value) {
    auto it = _aggr.find(varname);
    if (it == _aggr.end()) {  // first entry
        perf_aggr new_entry;
        new_entry._aggr = value;
        new_entry._count = 1;
        new_entry._sumq = value * value;
        _aggr.emplace(varname, new_entry);
    } else {
        it->second._count++;
        switch (getOperation()) {
            case StatsOperation::sum:
            case StatsOperation::avg:
                it->second._aggr += value;
                break;

            case StatsOperation::suminv:
            case StatsOperation::avginv:
                it->second._aggr += 1.0 / value;
                break;

            case StatsOperation::min:
                if (value < it->second._aggr) {
                    it->second._aggr = value;
                }
                break;

            case StatsOperation::max:
                if (value > it->second._aggr) {
                    it->second._aggr = value;
                }
                break;

            case StatsOperation::std:
                it->second._aggr += value;
                it->second._sumq += value * value;
                break;
            case StatsOperation::count:
                break;
        }
    }
}

void PerfdataAggregator::output(RowRenderer &r) {
    string perf_data;
    bool first = true;
    for (const auto &entry : _aggr) {
        double value;
        switch (getOperation()) {
            case StatsOperation::sum:
            case StatsOperation::min:
            case StatsOperation::max:
            case StatsOperation::suminv:
                value = entry.second._aggr;
                break;

            case StatsOperation::avg:
            case StatsOperation::avginv:
                if (entry.second._count == 0) {
                    value = 0.00;
                } else {
                    value = entry.second._aggr / entry.second._count;
                }
                break;

            case StatsOperation::std:
                if (entry.second._count <= 1) {
                    value = 0.0;
                } else {
                    value = sqrt((entry.second._sumq -
                                  (entry.second._aggr * entry.second._aggr) /
                                      entry.second._count) /
                                 (entry.second._count - 1));
                }
                break;
            default:
                value = 0;  // should never happen, but the real problem is that
                // _operation should beetter be a scoped enumeration.
                break;
        }
        char format[64];
        snprintf(format, sizeof(format), "%s=%.8f", entry.first.c_str(), value);
        if (first) {
            first = false;
        } else {
            perf_data += " ";
        }
        perf_data += format;
    }
    r.output(perf_data);
}
