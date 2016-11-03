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

#include "TableHostsByGroup.h"
#include "Query.h"
#include "TableHostgroups.h"
#include "TableHosts.h"
#include "auth.h"

using std::string;

extern host *host_list;
extern hostgroup *hostgroup_list;
extern char g_mk_inventory_path[];

namespace {
struct hostbygroup {
    host _host;
    hostgroup *_hostgroup;
};
}  // namespace

TableHostsByGroup::TableHostsByGroup(
    const DowntimesOrComments &_downtimes_holder,
    const DowntimesOrComments &_comments_holder, Logger *logger)
    : Table(logger) {
    struct hostbygroup ref;
    TableHosts::addColumns(this, "", -1, -1, _downtimes_holder,
                           _comments_holder);
    TableHostgroups::addColumns(this, "hostgroup_",
                                reinterpret_cast<char *>(&(ref._hostgroup)) -
                                    reinterpret_cast<char *>(&ref));
}

string TableHostsByGroup::name() const { return "hostsbygroup"; }

string TableHostsByGroup::namePrefix() const { return "host_"; }

void TableHostsByGroup::answerQuery(Query *query) {
    // When g_group_authorization is set to AUTH_STRICT we need to pre-check if
    // every host of this group is visible to the _auth_user
    bool requires_precheck =
        query->authUser() != nullptr && g_group_authorization == AUTH_STRICT;

    for (hostgroup *hg = hostgroup_list; hg != nullptr; hg = hg->next) {
        bool show_host_group = true;
        if (requires_precheck) {
            for (hostsmember *m = hg->members; m != nullptr; m = m->next) {
                if (!is_authorized_for(query->authUser(), m->host_ptr,
                                       nullptr)) {
                    show_host_group = false;
                    break;
                }
            }
        }

        if (show_host_group) {
            for (hostsmember *m = hg->members; m != nullptr; m = m->next) {
                hostbygroup hbg = {*m->host_ptr, hg};
                if (!query->processDataset(&hbg)) {
                    break;
                }
            }
        }
    }
}

bool TableHostsByGroup::isAuthorized(contact *ctc, void *data) {
    return is_authorized_for(ctc, static_cast<host *>(data), nullptr);
}

void *TableHostsByGroup::findObject(const string &objectspec) {
    return find_host(const_cast<char *>(objectspec.c_str()));
}
