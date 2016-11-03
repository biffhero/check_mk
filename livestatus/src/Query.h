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

#ifndef Query_h
#define Query_h

#include "config.h"  // IWYU pragma: keep
#include <cstdint>
#include <ctime>
#include <list>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>
#include "AndingFilter.h"
#include "OutputBuffer.h"
#include "Renderer.h"
#include "RendererBrokenCSV.h"
#include "VariadicFilter.h"
#include "data_encoding.h"
#include "nagios.h"  // IWYU pragma: keep
#include "opids.h"
class Aggregator;
class Column;
class Filter;
class Logger;
class StatsColumn;
class Table;

class Query {
public:
    Query(const std::list<std::string> &lines, Table *, Encoding data_encoding);
    ~Query();

    void process(OutputBuffer *output);

    bool processDataset(void *);

    bool timelimitReached();
    void invalidRequest(const std::string &message);

    contact *authUser() { return _auth_user; }
    int timezoneOffset() { return _timezone_offset; }

    const std::string *findValueForIndexing(const std::string &column_name);
    void findIntLimits(const std::string &column_name, int *lower, int *upper);
    void optimizeBitmask(const std::string &column_name, uint32_t *bitmask);
    AndingFilter *filter() { return &_filter; }
    std::unordered_set<Column *> *allColumns() { return &_all_columns; }

private:
    const Encoding _data_encoding;
    QueryRenderer *_renderer_query;
    OutputBuffer::ResponseHeader _response_header;
    bool _do_keepalive;
    std::string _invalid_header_message;
    Table *_table;
    AndingFilter _filter;
    contact *_auth_user;
    AndingFilter _wait_condition;
    unsigned _wait_timeout;
    struct trigger *_wait_trigger;
    void *_wait_object;
    CSVSeparators _separators;
    bool _show_column_headers;
    OutputFormat _output_format;
    int _limit;
    int _time_limit;
    time_t _time_limit_timeout;
    unsigned _current_line;
    int _timezone_offset;
    Logger *const _logger;

    // normal queries
    std::vector<Column *> _columns;
    // dynamically allocated. Must delete them.
    std::vector<Column *> _dummy_columns;

    // stats queries
    std::vector<StatsColumn *> _stats_columns;  // must also delete
    Aggregator **_stats_aggregators;

    typedef std::vector<std::string> _stats_group_spec_t;
    std::map<_stats_group_spec_t, Aggregator **> _stats_groups;

    std::unordered_set<Column *> _all_columns;

    // invalidHeader can be called during header parsing
    void invalidHeader(const std::string &message);

    void addColumn(Column *column);
    void *findTimerangeFilter(const char *columnname, time_t *, time_t *);
    void setResponseHeader(OutputBuffer::ResponseHeader r);
    void setDoKeepalive(bool d);

    bool doStats();
    void doWait();
    Aggregator **getStatsGroup(_stats_group_spec_t &groupspec);
    void computeStatsGroupSpec(_stats_group_spec_t &groupspec, void *data);
    Filter *createFilter(Column *column, RelationalOperator relOp,
                         const std::string &value);
    void parseFilterLine(char *line, VariadicFilter &filter);
    void parseStatsLine(char *line);
    void parseStatsGroupLine(char *line);
    void parseAndOrLine(char *line, LogicalOperator andor,
                        VariadicFilter &filter, const std::string &header);
    void parseNegateLine(char *line, VariadicFilter &filter,
                         const std::string &header);
    void parseStatsAndOrLine(char *line, LogicalOperator andor);
    void parseStatsNegateLine(char *line);
    void parseColumnsLine(char *line);
    void parseColumnHeadersLine(char *line);
    void parseLimitLine(char *line);
    void parseTimelimitLine(char *line);
    void parseSeparatorsLine(char *line);
    void parseOutputFormatLine(char *line);
    void parseKeepAliveLine(char *line);
    void parseResponseHeaderLine(char *line);
    void parseAuthUserHeader(char *line);
    void parseWaitTimeoutLine(char *line);
    void parseWaitTriggerLine(char *line);
    void parseWaitObjectLine(char *line);
    void parseLocaltimeLine(char *line);
    void start(QueryRenderer &q);
    void finish(QueryRenderer &q);
    Column *createDummyColumn(const char *name);
};

#endif  // Query_h
