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

#include "CustomVarsDictColumn.h"
#include "Renderer.h"

using std::string;

CustomVarsDictColumn::CustomVarsDictColumn(string name, string description,
                                           int offset, int indirect_offset,
                                           int extra_offset)
    : CustomVarsColumn(name, description, offset, indirect_offset,
                       extra_offset) {}

ColumnType CustomVarsDictColumn::type() { return ColumnType::dict; }

void CustomVarsDictColumn::output(void *row, RowRenderer &r,
                                  contact * /* auth_user */) {
    DictRenderer d(r);
    for (customvariablesmember *cvm = getCVM(row); cvm != nullptr;
         cvm = cvm->next) {
        d.output(cvm->variable_name, cvm->variable_value);
    }
}

bool CustomVarsDictColumn::contains(void *row, const string &value) {
    for (customvariablesmember *cvm = getCVM(row); cvm != nullptr;
         cvm = cvm->next) {
        if (value.compare(cvm->variable_value) == 0) {
            return true;
        }
    }
    return false;
}
