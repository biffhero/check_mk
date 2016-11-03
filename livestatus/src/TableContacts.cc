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

#include "TableContacts.h"
#include <cstdio>
#include "AttributelistColumn.h"
#include "CustomVarsDictColumn.h"
#include "CustomVarsNamesColumn.h"
#include "CustomVarsValuesColumn.h"
#include "OffsetIntColumn.h"
#include "OffsetStringColumn.h"
#include "OffsetTimeperiodColumn.h"
#include "Query.h"
#include "nagios.h"

using std::string;

extern contact *contact_list;

TableContacts::TableContacts(Logger *logger) : Table(logger) {
    addColumns(this, "", -1);
}

string TableContacts::name() const { return "contacts"; }

string TableContacts::namePrefix() const { return "contact_"; }

// static
void TableContacts::addColumns(Table *table, const string &prefix,
                               int indirect_offset) {
    contact ctc;
    char *ref = reinterpret_cast<char *>(&ctc);
    table->addColumn(new OffsetStringColumn(
        prefix + "name", "The login name of the contact person",
        reinterpret_cast<char *>(&ctc.name) - ref, indirect_offset));
    table->addColumn(new OffsetStringColumn(
        prefix + "alias", "The full name of the contact",
        reinterpret_cast<char *>(&ctc.alias) - ref, indirect_offset));
    table->addColumn(new OffsetStringColumn(
        prefix + "email", "The email address of the contact",
        reinterpret_cast<char *>(&ctc.email) - ref, indirect_offset));
    table->addColumn(new OffsetStringColumn(
        prefix + "pager", "The pager address of the contact",
        reinterpret_cast<char *>(&ctc.pager) - ref, indirect_offset));
    table->addColumn(new OffsetStringColumn(
        prefix + "host_notification_period",
        "The time period in which the contact will be notified about host "
        "problems",
        reinterpret_cast<char *>(&ctc.host_notification_period) - ref,
        indirect_offset));
    table->addColumn(new OffsetStringColumn(
        prefix + "service_notification_period",
        "The time period in which the contact will be notified about service "
        "problems",
        reinterpret_cast<char *>(&ctc.service_notification_period) - ref,
        indirect_offset));
    for (int i = 0; i < MAX_CONTACT_ADDRESSES; i++) {
        char b[32];
        snprintf(b, sizeof(b), "address%d", i + 1);
        table->addColumn(new OffsetStringColumn(
            prefix + b, (string("The additional field ") + b),
            reinterpret_cast<char *>(&ctc.address[i]) - ref, indirect_offset));
    }

    table->addColumn(new OffsetIntColumn(
        prefix + "can_submit_commands",
        "Wether the contact is allowed to submit commands (0/1)",
        reinterpret_cast<char *>(&ctc.can_submit_commands) - ref,
        indirect_offset));
    table->addColumn(new OffsetIntColumn(
        prefix + "host_notifications_enabled",
        "Wether the contact will be notified about host problems in general "
        "(0/1)",
        reinterpret_cast<char *>(&ctc.host_notifications_enabled) - ref,
        indirect_offset));
    table->addColumn(new OffsetIntColumn(
        prefix + "service_notifications_enabled",
        "Wether the contact will be notified about service problems in general "
        "(0/1)",
        reinterpret_cast<char *>(&ctc.service_notifications_enabled) - ref,
        indirect_offset));

    table->addColumn(new OffsetTimeperiodColumn(
        prefix + "in_host_notification_period",
        "Wether the contact is currently in his/her host notification period "
        "(0/1)",
        reinterpret_cast<char *>(&ctc.host_notification_period_ptr) - ref,
        indirect_offset));
    table->addColumn(new OffsetTimeperiodColumn(
        prefix + "in_service_notification_period",
        "Wether the contact is currently in his/her service notification "
        "period (0/1)",
        reinterpret_cast<char *>(&ctc.service_notification_period_ptr) - ref,
        indirect_offset));

    table->addColumn(new CustomVarsNamesColumn(
        prefix + "custom_variable_names",
        "A list of all custom variables of the contact",
        reinterpret_cast<char *>(&ctc.custom_variables) - ref,
        indirect_offset));
    table->addColumn(new CustomVarsValuesColumn(
        prefix + "custom_variable_values",
        "A list of the values of all custom variables of the contact",
        reinterpret_cast<char *>(&ctc.custom_variables) - ref,
        indirect_offset));
    table->addColumn(new CustomVarsDictColumn(
        prefix + "custom_variables", "A dictionary of the custom variables",
        reinterpret_cast<char *>(&ctc.custom_variables) - ref,
        indirect_offset));
    table->addColumn(new AttributelistColumn(
        prefix + "modified_attributes",
        "A bitmask specifying which attributes have been modified",
        reinterpret_cast<char *>(&ctc.modified_attributes) - ref,
        indirect_offset, false));
    table->addColumn(new AttributelistColumn(
        prefix + "modified_attributes_list",
        "A list of all modified attributes",
        reinterpret_cast<char *>(&ctc.modified_attributes) - ref,
        indirect_offset, true));
}

void TableContacts::answerQuery(Query *query) {
    for (contact *ct = contact_list; ct != nullptr; ct = ct->next) {
        if (!query->processDataset(ct)) {
            break;
        }
    }
}

void *TableContacts::findObject(const string &objectspec) {
    return find_contact(const_cast<char *>(objectspec.c_str()));
}
