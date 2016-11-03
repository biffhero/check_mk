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

#include "auth.h"

namespace {
bool host_has_contact(host *hst, contact *ctc) {
    return is_contact_for_host(hst, ctc) != 0 ||
           is_escalated_contact_for_host(hst, ctc) != 0;
}

bool service_has_contact(host *hst, service *svc, contact *ctc) {
    return is_contact_for_service(svc, ctc) != 0 ||
           is_escalated_contact_for_service(svc, ctc) != 0 ||
           (g_service_authorization == AUTH_LOOSE &&
            host_has_contact(hst, ctc));
}
}  // namespace

bool is_authorized_for(contact *ctc, host *hst, service *svc) {
    return ctc != UNKNOWN_AUTH_USER &&
           (svc == nullptr ? host_has_contact(hst, ctc)
                           : service_has_contact(hst, svc, ctc));
}
