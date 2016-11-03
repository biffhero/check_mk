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

#ifndef MonitoringCore_h
#define MonitoringCore_h

#include "config.h"  // IWYU pragma: keep
#include <string>
class Logger;

/// An abstraction layer for the monitoring core (nagios or cmc)
class MonitoringCore {
public:
    class Contact;
    class ContactGroup;
    class Host;

    virtual ~MonitoringCore() = default;

    virtual Host *getHostByDesignation(const std::string &designation) = 0;
    virtual ContactGroup *find_contactgroup(const std::string &name) = 0;

    virtual bool host_has_contact(Host *host, Contact *contact) = 0;
    virtual bool is_contact_member_of_contactgroup(ContactGroup *group,
                                                   Contact *contact) = 0;

    virtual bool mkeventdEnabled() = 0;
    virtual std::string mkeventdSocketPath() = 0;

    virtual Logger *loggerLivestatus() = 0;
};

#endif  // MonitoringCore_h
