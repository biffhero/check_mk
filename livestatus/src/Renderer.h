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

#ifndef Renderer_h
#define Renderer_h

#include "config.h"  // IWYU pragma: keep
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include "OutputBuffer.h"
#include "data_encoding.h"
class CSVSeparators;
class Logger;

enum class OutputFormat { csv, broken_csv, json, python, python3 };

struct Null {};

struct PlainChar {
    char _ch;
};

struct HexEscape {
    char _ch;
};

class Renderer {
public:
    static std::unique_ptr<Renderer> make(
        OutputFormat format, OutputBuffer *output,
        OutputBuffer::ResponseHeader response_header, bool do_keep_alive,
        std::string invalid_header_message, const CSVSeparators &separators,
        int timezone_offset, Encoding data_encoding);

    virtual ~Renderer();

    void setError(OutputBuffer::ResponseCode code, const std::string &message);
    std::size_t size() const;

    template <typename T>
    void output(T value) {
        add(std::to_string(value));
    }

    void output(double value);
    void output(PlainChar value);
    void output(HexEscape value);
    void output(char16_t value);
    void output(char32_t value);
    void output(Null value);
    void output(const std::vector<char> &value);
    void output(const std::string &value);
    void output(std::chrono::system_clock::time_point value);

    // A whole query.
    virtual void beginQuery() = 0;
    virtual void separateQueryElements() = 0;
    virtual void endQuery() = 0;

    // Output a single row returned by lq.
    virtual void beginRow() = 0;
    virtual void beginRowElement() = 0;
    virtual void endRowElement() = 0;
    virtual void separateRowElements() = 0;
    virtual void endRow() = 0;

    // Output a list-valued column.
    virtual void beginList() = 0;
    virtual void separateListElements() = 0;
    virtual void endList() = 0;

    // Output a list-valued value within a list-valued column.
    virtual void beginSublist() = 0;
    virtual void separateSublistElements() = 0;
    virtual void endSublist() = 0;

    // Output a dictionary, see CustomVarsColumn.
    virtual void beginDict() = 0;
    virtual void separateDictElements() = 0;
    virtual void separateDictKeyValue() = 0;
    virtual void endDict() = 0;

protected:
    Renderer(OutputBuffer *output, OutputBuffer::ResponseHeader response_header,
             bool do_keep_alive, std::string invalid_header_message,
             int timezone_offset, Encoding data_encoding);

    void add(const std::string &str);
    void add(const std::vector<char> &value);

    void outputByteString(const std::string &prefix,
                          const std::vector<char> &value);
    void outputUnicodeString(const std::string &prefix, const char *start,
                             const char *end, Encoding data_encoding);

    const Encoding _data_encoding;

private:
    OutputBuffer *const _output;
    const int _timezone_offset;
    Logger *const _logger;

    void outputUTF8(const char *start, const char *end);
    void outputLatin1(const char *start, const char *end);
    void outputMixed(const char *start, const char *end);
    void truncatedUTF8();
    void invalidUTF8(unsigned char ch);

    virtual void outputNull() = 0;
    virtual void outputBlob(const std::vector<char> &value) = 0;
    virtual void outputString(const std::string &value) = 0;
};

class QueryRenderer {
public:
    class BeginEnd {
    public:
        explicit BeginEnd(QueryRenderer &query) : _query(query) {
            if (_query._first) {
                _query._first = false;
            } else {
                _query.renderer().separateQueryElements();
            }
        }

    private:
        QueryRenderer &_query;
    };

    explicit QueryRenderer(Renderer &rend) : _renderer(rend), _first(true) {
        renderer().beginQuery();
    }

    ~QueryRenderer() { renderer().endQuery(); }

    Renderer &renderer() const { return _renderer; }

    void setError(OutputBuffer::ResponseCode code, const std::string &message) {
        renderer().setError(code, message);
    }

    std::size_t size() const { return renderer().size(); }

private:
    Renderer &_renderer;
    bool _first;
};

class RowRenderer {
public:
    class BeginEnd {
    public:
        explicit BeginEnd(RowRenderer &row) : _row(row) {
            if (_row._first) {
                _row._first = false;
            } else {
                _row.renderer().separateRowElements();
            }
            _row.renderer().beginRowElement();
        }
        ~BeginEnd() { _row.renderer().endRowElement(); }

    private:
        RowRenderer &_row;
    };

    explicit RowRenderer(QueryRenderer &query)
        : _query(query), _be(query), _first(true) {
        renderer().beginRow();
    }

    ~RowRenderer() { renderer().endRow(); }

    Renderer &renderer() const { return _query.renderer(); }

    template <typename T>
    void output(T value) {
        BeginEnd be(*this);
        renderer().output(value);
    }

private:
    QueryRenderer &_query;
    QueryRenderer::BeginEnd _be;
    bool _first;
};

class ListRenderer {
public:
    class BeginEnd {
    public:
        explicit BeginEnd(ListRenderer &list) : _list(list) {
            if (_list._first) {
                _list._first = false;
            } else {
                _list.renderer().separateListElements();
            }
        }

    private:
        ListRenderer &_list;
    };

    explicit ListRenderer(RowRenderer &row)
        : _row(row), _be(row), _first(true) {
        renderer().beginList();
    }

    ~ListRenderer() { renderer().endList(); }

    Renderer &renderer() const { return _row.renderer(); }

    template <typename T>
    void output(T value) {
        BeginEnd be(*this);
        renderer().output(value);
    }

private:
    RowRenderer &_row;
    RowRenderer::BeginEnd _be;
    bool _first;
};

class SublistRenderer {
public:
    class BeginEnd {
    public:
        explicit BeginEnd(SublistRenderer &sublist) : _sublist(sublist) {
            if (_sublist._first) {
                _sublist._first = false;
            } else {
                _sublist.renderer().separateSublistElements();
            }
        }

    private:
        SublistRenderer &_sublist;
    };

    explicit SublistRenderer(ListRenderer &list)
        : _list(list), _be(list), _first(true) {
        renderer().beginSublist();
    }

    ~SublistRenderer() { renderer().endSublist(); }

    Renderer &renderer() const { return _list.renderer(); }

    template <typename T>
    void output(T value) {
        BeginEnd be(*this);
        renderer().output(value);
    }

private:
    ListRenderer &_list;
    ListRenderer::BeginEnd _be;
    bool _first;
};

class DictRenderer {
public:
    class BeginEnd {
    public:
        explicit BeginEnd(DictRenderer &dict) : _dict(dict) {
            if (_dict._first) {
                _dict._first = false;
            } else {
                _dict.renderer().separateDictElements();
            }
        }

    private:
        DictRenderer &_dict;
    };

    explicit DictRenderer(RowRenderer &row)
        : _row(row), _be(row), _first(true) {
        renderer().beginDict();
    }

    ~DictRenderer() { renderer().endDict(); }

    Renderer &renderer() const { return _row.renderer(); }

    void output(std::string key, std::string value) {
        BeginEnd be(*this);
        renderer().output(key);
        renderer().separateDictKeyValue();
        renderer().output(value);
    }

private:
    RowRenderer &_row;
    RowRenderer::BeginEnd _be;
    bool _first;
};

#endif  // Renderer_h
