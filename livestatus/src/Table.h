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

#ifndef Table_h
#define Table_h

#include "config.h"  // IWYU pragma: keep
#include <map>
#include <string>
#include <utility>
#include "nagios.h"  // IWYU pragma: keep
class Column;
class DynamicColumn;
class Logger;
class Query;

/// A table-like view for some underlying data, exposed via LQL.
class Table {
public:
    explicit Table(Logger *logger);
    virtual ~Table();

    void addColumn(Column *);
    void addDynamicColumn(DynamicColumn *);

    template <typename Predicate>
    bool any_column(Predicate pred) {
        for (auto &c : _columns) {
            if (pred(c.second)) {
                return true;
            }
        }
        return false;
    }

    /// The name of the table, as used in the GET command.
    virtual std::string name() const = 0;

    /// \brief An optional prefix for column names.
    ///
    /// \todo Due to the way multisite works, column names are sometimes
    /// prefixed by a variation of the table name (e.g. "hosts" => "host_"), but
    /// the logic for this really shouldn't live on the cmc side. Furthermore,
    /// multisite sometimes even seems to use a *sequence* of prefixes, which is
    /// yet another a bug. Instead of fixing it there, it is currently papered
    /// over on the cmc side. :-/
    virtual std::string namePrefix() const = 0;

    /// \brief Retrieve a column with a give name.
    ///
    /// If the name contains a ':' then we have a dynamic column with column
    /// arguments: The part before the colon is the column name of the dynamic
    /// column and the part after it is the name of the fresh, dynamically
    /// created column (up to the 2nd ':') and further arguments. This whole
    /// mechanism is e.g. used to access RRD metrics data.
    ///
    /// \todo This member function is virtual just because TableStateHistory and
    /// TableLog override it for some dubious reason: They first try the normal
    /// lookup, and if that didn't find a column, the lookup is retried with a
    /// "current_" prefix. This logic should probably not live in cmc at all.
    virtual Column *column(std::string colname);

    virtual void answerQuery(Query *query) = 0;
    virtual bool isAuthorized(contact *ctc, void *data);
    virtual void *findObject(const std::string &objectspec);

    Logger *const _logger;

private:
    Column *dynamicColumn(const std::string &name, const std::string &rest);

    std::map<std::string, Column *> _columns;
    std::map<std::string, DynamicColumn *> _dynamic_columns;
};

#endif  // Table_h
