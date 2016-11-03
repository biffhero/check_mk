// +------------------------------------------------------------------+
// |             ____ _               _        __  __ _  __           |
// |            / ___| |__   ___  ___| | __   |  \/  | |/ /           |
// |           | |   | '_ \ / _ \/ __| |/ /   | |\/| | ' /            |
// |           | |___| | | |  __/ (__|   <    | |  | | . \            |
// |            \____|_| |_|\___|\___|_|\_\___|_|  |_|_|\_\           |
// |                                                                  |
// | Copyright Mathias Kettner 2016             mk@mathias-kettner.de |
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
// ails.  You should have  received  a copy of the  GNU  General Public
// License along with GNU Make; see the file  COPYING.  If  not,  write
// to the Free Software Foundation, Inc., 51 Franklin St,  Fifth Floor,
// Boston, MA 02110-1301 USA.

#ifndef SectionFileinfo_h
#define SectionFileinfo_h

#include "../Section.h"
#include "../Configurable.h"

class Configuration;

class SectionFileinfo : public Section {
    typedef std::vector<std::string> PathsT;

    ListConfigurable<PathsT, BlockMode::Nop<PathsT>,
                  AddMode::PriorityAppend<PathsT>>
        _fileinfo_paths;

public:
    SectionFileinfo(Configuration &config);

protected:
    virtual bool produceOutputInner(std::ostream &out,
                                    const Environment &env) override;
private:
    void outputFileinfos(std::ostream &out, const char *path);
    bool outputFileinfo(std::ostream &out, const char *basename,
                     WIN32_FIND_DATA *data);
};

#endif  // SectionFileinfo_h

