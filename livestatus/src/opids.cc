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

#include "opids.h"
#include <unordered_map>
#include <utility>

using std::ostream;
using std::string;
using std::unordered_map;

namespace {
unordered_map<string, RelationalOperator> fromString = {
    {"=", RelationalOperator::equal},
    {"!=", RelationalOperator::not_equal},
    {"~", RelationalOperator::matches},
    {"!~", RelationalOperator::doesnt_match},
    {"=~", RelationalOperator::equal_icase},
    {"!=~", RelationalOperator::not_equal_icase},
    {"~~", RelationalOperator::matches_icase},
    {"!~~", RelationalOperator::doesnt_match_icase},
    {"<", RelationalOperator::less},
    {"!<", RelationalOperator::greater_or_equal},
    {">=", RelationalOperator::greater_or_equal},
    {"!>=", RelationalOperator::less},
    {">", RelationalOperator::greater},
    {"!>", RelationalOperator::less_or_equal},
    {"<=", RelationalOperator::less_or_equal},
    {"!<=", RelationalOperator::greater}};
}  // namespace

ostream &operator<<(ostream &os, const RelationalOperator &relOp) {
    // Slightly inefficient, but this doesn't matter for our purposes. We could
    // use Boost.Bimap or use 2 maps if really necessary.
    for (const auto &strAndOp : fromString) {
        if (strAndOp.second == relOp) {
            return os << strAndOp.first;
        }
    }
    return os;
}

bool relationalOperatorForName(const string &name, RelationalOperator &relOp) {
    auto it = fromString.find(name);
    if (it == fromString.end()) {
        return false;
    }
    relOp = it->second;
    return true;
}
