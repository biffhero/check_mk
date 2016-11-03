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

#include "DynamicLogwatchFileColumn.h"
#include <ostream>
#include "HostFileColumn.h"
#include "Logger.h"

using std::string;

// Replace \\ with \ and \s with space
string unescape_filename(string filename) {
    string filename_native;
    bool quote_active = false;
    for (auto c : filename) {
        if (quote_active) {
            if (c == 's') {
                filename_native += ' ';
            } else {
                filename_native += c;
            }
            quote_active = false;
        } else if (c == '\\') {
            quote_active = true;
        } else {
            filename_native += c;
        }
    }
    return filename_native;
}

Column *DynamicLogwatchFileColumn::createColumn(const std::string &name,
                                                const std::string &arguments) {
    // arguments contains a file name
    if (arguments.empty()) {
        Warning(_logger) << "Invalid arguments for column '" << _name
                         << "': missing file name";
        return nullptr;
    }

    if (arguments.find('/') != string::npos) {
        Warning(_logger) << "Invalid arguments for column '" << _name
                         << "': file name '" << arguments << "' contains slash";
        return nullptr;
    }

    return new HostFileColumn(name, "Contents of logwatch file", _logwatch_path,
                              "/" + unescape_filename(arguments),
                              _indirect_offset, _extra_offset);
}
