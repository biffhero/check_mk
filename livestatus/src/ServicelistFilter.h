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

#ifndef ServicelistFilter_h
#define ServicelistFilter_h

#include "config.h"  // IWYU pragma: keep
#include <string>
#include "ColumnFilter.h"
#include "ServicelistColumn.h"
#include "opids.h"

#ifdef CMC
#include "cmc.h"
#else
#include "nagios.h"
#endif

class ServicelistFilter : public ColumnFilter {
public:
    ServicelistFilter(ServicelistColumn *column, RelationalOperator relOp,
                      const std::string &value);
    bool accepts(void *row, contact *auth_user, int timezone_offset) override;
    ServicelistColumn *column() const override;

private:
    ServicelistColumn *_column;
    RelationalOperator _relOp;
    std::string _ref_host;
    std::string _ref_service;
};

#endif  // ServicelistFilter_h
