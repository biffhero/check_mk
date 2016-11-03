#!/usr/bin/python
# -*- encoding: utf-8; py-indent-offset: 4 -*-
# +------------------------------------------------------------------+
# |             ____ _               _        __  __ _  __           |
# |            / ___| |__   ___  ___| | __   |  \/  | |/ /           |
# |           | |   | '_ \ / _ \/ __| |/ /   | |\/| | ' /            |
# |           | |___| | | |  __/ (__|   <    | |  | | . \            |
# |            \____|_| |_|\___|\___|_|\_\___|_|  |_|_|\_\           |
# |                                                                  |
# | Copyright Mathias Kettner 2014             mk@mathias-kettner.de |
# +------------------------------------------------------------------+
#
# This file is part of Check_MK.
# The official homepage is at http://mathias-kettner.de/check_mk.
#
# check_mk is free software;  you can redistribute it and/or modify it
# under the  terms of the  GNU General Public License  as published by
# the Free Software Foundation in version 2.  check_mk is  distributed
# in the hope that it will be useful, but WITHOUT ANY WARRANTY;  with-
# out even the implied warranty of  MERCHANTABILITY  or  FITNESS FOR A
# PARTICULAR PURPOSE. See the  GNU General Public License for more de-
# tails. You should have  received  a copy of the  GNU  General Public
# License along with GNU Make; see the file  COPYING.  If  not,  write
# to the Free Software Foundation, Inc., 51 Franklin St,  Fifth Floor,
# Boston, MA 02110-1301 USA.

# This module is needed only for SNMP based checks

import subprocess

import cmk.tty as tty

import cmk_base.agent_simulator
import cmk_base.console as console

OID_END              =  0  # Suffix-part of OID that was not specified
OID_STRING           = -1  # Complete OID as string ".1.3.6.1.4.1.343...."
OID_BIN              = -2  # Complete OID as binary string "\x01\x03\x06\x01..."
OID_END_BIN          = -3  # Same, but just the end part
OID_END_OCTET_STRING = -4  # yet same, but omit first byte (assuming that is the length byte)


# Wrapper to mark OIDs as being cached for regular checks, but not for discovery
def CACHED_OID(oid):
    return "cached", oid


def BINARY(oid):
    return "binary", oid


def strip_snmp_value(value, hex_plain = False):
    v = value.strip()
    if v.startswith('"'):
        v = v[1:-1]
        if len(v) > 2 and is_hex_string(v):
            return not hex_plain and convert_from_hex(v) or value
        else:
            # Fix for non hex encoded string which have been somehow encoded by the
            # netsnmp command line tools. An example:
            # Checking windows systems via SNMP with hr_fs: disk names like c:\
            # are reported as c:\\, fix this to single \
            return v.strip().replace('\\\\', '\\')
    else:
        return v

def is_hex_string(value):
    # as far as I remember, snmpwalk puts a trailing space within
    # the quotes in case of hex strings. So we require that space
    # to be present in order make sure, we really deal with a hex string.
    if value[-1] != ' ':
        return False
    hexdigits = "0123456789abcdefABCDEF"
    n = 0
    for x in value:
        if n % 3 == 2:
            if x != ' ':
                return False
        else:
            if x not in hexdigits:
                return False
        n += 1
    return True

def convert_from_hex(value):
    hexparts = value.split()
    r = ""
    for hx in hexparts:
        r += chr(int(hx, 16))
    return r

def oid_to_bin(oid):
    return u"".join([ unichr(int(p)) for p in oid.strip(".").split(".") ])

def extract_end_oid(prefix, complete):
    return complete[len(prefix):].lstrip('.')

# sort OID strings numerically
def oid_to_intlist(oid):
    if oid:
        return map(int, oid.split('.'))
    else:
        return []

def cmp_oids(o1, o2):
    return cmp(oid_to_intlist(o1), oid_to_intlist(o2))

def cmp_oid_pairs(pair1, pair2):
    return cmp(oid_to_intlist(pair1[0].lstrip('.')),
               oid_to_intlist(pair2[0].lstrip('.')))

def snmpv3_contexts_of_host(hostname):
    return host_extra_conf(hostname, snmpv3_contexts)

def snmpv3_contexts_of(hostname, check_type):
    for ty, rules in snmpv3_contexts_of_host(hostname):
        if ty == None or ty == check_type:
            return rules
    return [None]

def get_snmp_table(hostname, ip, check_type, oid_info, use_snmpwalk_cache):
    # oid_info is either ( oid, columns ) or
    # ( oid, suboids, columns )
    # suboids is a list if OID-infixes that are put between baseoid
    # and the columns and also prefixed to the index column. This
    # allows to merge distinct SNMP subtrees with a similar structure
    # to one virtual new tree (look into cmctc_temp for an example)
    if len(oid_info) == 2:
        oid, targetcolumns = oid_info
        suboids = [None]
    else:
        oid, suboids, targetcolumns = oid_info

    if not oid.startswith("."):
        raise MKGeneralException("OID definition '%s' does not begin with ." % oid)

    index_column = -1
    index_format = None
    info = []
    for suboid in suboids:
        colno = -1
        columns = []
        # Detect missing (empty columns)
        max_len = 0
        max_len_col = -1

        for column in targetcolumns:
            fetchoid, value_encoding = compute_fetch_oid(oid, suboid, column)

            # column may be integer or string like "1.5.4.2.3"
            colno += 1
            # if column is 0, we do not fetch any data from snmp, but use
            # a running counter as index. If the index column is the first one,
            # we do not know the number of entries right now. We need to fill
            # in later. If the column is OID_STRING or OID_BIN we do something
            # similar: we fill in the complete OID of the entry, either as
            # string or as binary UTF-8 encoded number string
            if column in [ OID_END, OID_STRING, OID_BIN, OID_END_BIN, OID_END_OCTET_STRING ]:
                if index_column >= 0 and index_column != colno:
                    raise MKGeneralException("Invalid SNMP OID specification in implementation of check. "
                        "You can only use one of OID_END, OID_STRING, OID_BIN, OID_END_BIN and OID_END_OCTET_STRING.")
                index_column = colno
                columns.append((fetchoid, [], "string"))
                index_format = column
                continue

            rowinfo = get_snmpwalk(hostname, ip, check_type, oid, fetchoid, column, use_snmpwalk_cache)

            columns.append((fetchoid, rowinfo, value_encoding))
            number_of_rows = len(rowinfo)
            if number_of_rows > max_len:
                max_len     = number_of_rows
                max_len_col = colno

        if index_column != -1:
            index_rows = []
            # Take end-oids of non-index columns as indices
            fetchoid, max_column, value_encoding  = columns[max_len_col]
            for o, _unused_value in max_column:
                if index_format == OID_END:
                    index_rows.append((o, extract_end_oid(fetchoid, o)))
                elif index_format == OID_STRING:
                    index_rows.append((o, o))
                elif index_format == OID_BIN:
                    index_rows.append((o, oid_to_bin(o)))
                elif index_format == OID_END_BIN:
                    index_rows.append((o, oid_to_bin(extract_end_oid(fetchoid, o))))
                else: # OID_END_OCTET_STRING:
                    index_rows.append((o, oid_to_bin(extract_end_oid(fetchoid, o))[1:]))

            columns[index_column] = fetchoid, index_rows, value_encoding


        # prepend suboid to first column
        if suboid and columns:
            fetchoid, first_column, value_encoding = columns[0]
            new_first_column = []
            for o, val in first_column:
                new_first_column.append((o, str(suboid) + "." + str(val)))
            columns[0] = fetchoid, new_first_column, value_encoding

        # Here we have to deal with a nasty problem: Some brain-dead devices
        # omit entries in some sub OIDs. This happens e.g. for CISCO 3650
        # in the interfaces MIB with 64 bit counters. So we need to look at
        # the OIDs and watch out for gaps we need to fill with dummy values.
        new_columns = sanitize_snmp_table_columns(columns)

        # From all SNMP data sources (stored walk, classic SNMP, inline SNMP) we
        # get normal python strings. But for Check_MK we need unicode strings now.
        # Convert them by using the standard Check_MK approach for incoming data
        sanitized_columns = sanitize_snmp_encoding(new_columns)

        info += construct_snmp_table_of_rows(sanitized_columns)

    return info


def get_snmpwalk(hostname, ip, check_type, oid, fetchoid, column, use_snmpwalk_cache):
    is_cachable = is_snmpwalk_cachable(column)
    rowinfo = None
    if is_cachable and use_snmpwalk_cache:
        # Returns either the cached SNMP walk or None when nothing is cached
        rowinfo = get_cached_snmpwalk(hostname, fetchoid)

    if rowinfo == None:
        if opt_use_snmp_walk or is_usewalk_host(hostname):
            rowinfo = get_stored_snmpwalk(hostname, fetchoid)
        else:
            rowinfo = perform_snmpwalk(hostname, ip, check_type, oid, fetchoid)

        if is_cachable:
            save_snmpwalk_cache(hostname, fetchoid, rowinfo)

    return rowinfo


def perform_snmpwalk(hostname, ip, check_type, base_oid, fetchoid):
    added_oids = set([])
    rowinfo = []
    if is_snmpv3_host(hostname):
        snmp_contexts = snmpv3_contexts_of(hostname, check_type)
    else:
        snmp_contexts = [None]

    for context_name in snmp_contexts:
        cpu_tracking.push_phase("snmp")
        if is_inline_snmp_host(hostname):
            rows = inline_snmpwalk_on_suboid(hostname, check_type, fetchoid, base_oid,
                                                                  context_name=context_name,
                                                                  ipaddress=ip)
        else:
            rows = snmpwalk_on_suboid(hostname, ip, fetchoid, context_name=context_name)
        cpu_tracking.pop_phase()

        # I've seen a broken device (Mikrotik Router), that broke after an
        # update to RouterOS v6.22. It would return 9 time the same OID when
        # .1.3.6.1.2.1.1.1.0 was being walked. We try to detect these situations
        # by removing any duplicate OID information
        if len(rows) > 1 and rows[0][0] == rows[1][0]:
            console.vverbose("Detected broken SNMP agent. Ignoring duplicate OID %s.\n" % rows[0][0])
            rows = rows[:1]

        for row_oid, val in rows:
            if row_oid in added_oids:
                console.vverbose("Duplicate OID found: %s (%s)\n" % (row_oid, val))
            else:
                rowinfo.append((row_oid, val))
                added_oids.add(row_oid)

    return rowinfo


def compute_fetch_oid(oid, suboid, column):
    fetchoid = oid
    value_encoding = "string"

    if suboid:
        fetchoid += "." + str(suboid)

    if column != "":
        if type(column) == tuple:
            fetchoid += "." + str(column[1])
            if column[0] == "binary":
                value_encoding = "binary"
        else:
            fetchoid += "." + str(column)

    return fetchoid, value_encoding


def sanitize_snmp_encoding(columns):
    for index, (column, value_encoding) in enumerate(columns):
        if value_encoding == "string":
            columns[index] = map(snmp_decode_string, column)
        else:
            columns[index] = map(snmp_decode_binary, column)
    return columns


def sanitize_snmp_table_columns(columns):
    # First compute the complete list of end-oids appearing in the output
    # by looping all results and putting the endoids to a flat list
    endoids = []
    for fetchoid, column, value_encoding in columns:
        for o, value in column:
            endoid = extract_end_oid(fetchoid, o)
            if endoid not in endoids:
                endoids.append(endoid)

    # The list needs to be sorted to prevent problems when the first
    # column has missing values in the middle of the tree.
    if not are_ascending_oids(endoids):
        endoids.sort(cmp = cmp_oids)
        need_sort = True
    else:
        need_sort = False

    # Now fill gaps in columns where some endois are missing
    new_columns = []
    for fetchoid, column, value_encoding in columns:
        # It might happen that end OIDs are not ordered. Fix the OID sorting to make
        # it comparable to the already sorted endoids list. Otherwise we would get
        # some mixups when filling gaps
        if need_sort:
            column.sort(cmp = cmp_oid_pairs)

        i = 0
        new_column = []
        # Loop all lines to fill holes in the middle of the list. All
        # columns check the following lines for the correct endoid. If
        # an endoid differs empty values are added until the hole is filled
        for o, value in column:
            eo = extract_end_oid(fetchoid, o)
            if len(column) != len(endoids):
                while i < len(endoids) and endoids[i] != eo:
                    new_column.append("") # (beginoid + '.' +endoids[i], "" ) )
                    i += 1
            new_column.append(value)
            i += 1

        # At the end check if trailing OIDs are missing
        while i < len(endoids):
            new_column.append("") # (beginoid + '.' +endoids[i], "") )
            i += 1
        new_columns.append((new_column, value_encoding))

    return new_columns


def are_ascending_oids(oid_list):
    for a in range(len(oid_list) - 1):
        if cmp_oids(oid_list[a], oid_list[a + 1]) > 0: # == 0 should never happen
            return False
    return True


def construct_snmp_table_of_rows(columns):
    if not columns:
        return []

    # Now construct table by swapping X and Y.
    new_info = []
    for index in range(len(columns[0])):
        row = [ c[index] for c in columns ]
        new_info.append(row)
    return new_info


# SNMP-Helper functions used in various checks

def check_snmp_misc(item, params, info):
    for line in info:
        if item == line[0]:
            value = savefloat(line[1])
            text = line[2]
            crit_low, warn_low, warn_high, crit_high = params
            # if value is negative, we have to swap >= and <=!
            perfdata=[ (item, line[1]) ]
            if not within_range(value, crit_low, crit_high):
                return (2, "CRIT - %.2f value out of crit range (%.2f .. %.2f)" % \
                        (value, crit_low, crit_high), perfdata)
            elif not within_range(value, warn_low, warn_high):
                return (2, "WARNING - %.2f value out of warning range (%.2f .. %.2f)" % \
                        (value, warn_low, warn_high), perfdata)
            else:
                return (0, "OK = %s (OK within %.2f .. %.2f)" % (text, warn_low, warn_high), perfdata)
    return (3, "Missing item %s in SNMP data" % item)

def inventory_snmp_misc(checkname, info):
    inventory = []
    for line in info:
        value = savefloat(line[1])
        params = "(%.1f, %.1f, %.1f, %.1f)" % (value*.8, value*.9, value*1.1, value*1.2)
        inventory.append( (line[0], line[2], params ) )
    return inventory

# Version with simple handling of target parameters: only
# the current value is OK, all other values are CRIT
def inventory_snmp_fixed(checkname, info):
    inventory = []
    for line in info:
        value = line[1]
        params = '"%s"' % (value,)
        inventory.append( (line[0], line[2], params ) )
    return inventory

def check_snmp_fixed(item, targetvalue, info):
    for line in info:
        if item == line[0]:
            value = line[1]
            if value != targetvalue:
                return (2, "CRIT - %s (should be %s)" % (value, targetvalue))
            else:
                return (0, "OK - %s" % (value,))
    return (3, "Missing item %s in SNMP data" % item)


def is_snmpwalk_cachable(column):
    return type(column) == tuple and column[0] == "cached"


def get_cached_snmpwalk(hostname, fetchoid):
    path = cmk.paths.var_dir + "/snmp_cache/" + hostname + "/" + fetchoid

    try:
        console.vverbose("  Loading %s from walk cache %s\n" % (fetchoid, path))
        return eval(file(path).read())
    except IOError:
        return None # don't print error when not cached yet
    except:
        if cmk.debug.enabled():
            raise
        console.verbose("Failed to read cached SNMP walk from %s, ignoring.\n" % path)
        return None


def save_snmpwalk_cache(hostname, fetchoid, rowinfo):
    base_dir = cmk.paths.var_dir + "/snmp_cache/" + hostname + "/"
    if not os.path.exists(base_dir):
        os.makedirs(base_dir)
    console.vverbose("  Caching walk of %s\n" % fetchoid)
    file(base_dir + fetchoid, "w").write("%r\n" % rowinfo)


g_walk_cache = {}
def get_stored_snmpwalk(hostname, oid):
    if oid.startswith("."):
        oid = oid[1:]

    if oid.endswith(".*"):
        oid_prefix = oid[:-2]
        dot_star = True
    else:
        oid_prefix = oid
        dot_star = False

    path = cmk.paths.snmpwalks_dir + "/" + hostname

    console.vverbose("  Loading %s from %s\n" % (oid, path))

    rowinfo = []

    # New implementation: use binary search
    def to_bin_string(oid):
        try:
            return tuple(map(int, oid.strip(".").split(".")))
        except:
            raise MKGeneralException("Invalid OID %s" % oid)

    def compare_oids(a, b):
        aa = to_bin_string(a)
        bb = to_bin_string(b)
        if len(aa) <= len(bb) and bb[:len(aa)] == aa:
            result = 0
        else:
            result = cmp(aa, bb)
        return result

    if hostname in g_walk_cache:
        lines = g_walk_cache[hostname]
    else:
        try:
            lines = file(path).readlines()
        except IOError:
            raise MKSNMPError("No snmpwalk file %s" % path)
        g_walk_cache[hostname] = lines

    begin = 0
    end = len(lines)
    hit = None
    while end - begin > 0:
        current = (begin + end) / 2
        parts = lines[current].split(None, 1)
        comp = parts[0]
        hit = compare_oids(oid_prefix, comp)
        if hit == 0:
            break
        elif hit == 1: # we are too low
            begin = current + 1
        else:
            end = current

    if hit != 0:
        return [] # not found


    def collect_until(index, direction):
        rows = []
        # Handle case, where we run after the end of the lines list
        if index >= len(lines):
            if direction > 0:
                return []
            else:
                index -= 1
        while True:
            line = lines[index]
            parts = line.split(None, 1)
            o = parts[0]
            if o.startswith('.'):
                o = o[1:]
            if o == oid or o.startswith(oid_prefix + "."):
                if len(parts) > 1:
                    try:
                        value = cmk_base.agent_simulator.process(parts[1])
                    except:
                        value = parts[1] # agent simulator missing in precompiled mode
                else:
                    value = ""
                # Fix for missing starting oids
                rows.append(('.'+o, strip_snmp_value(value)))
                index += direction
                if index < 0 or index >= len(lines):
                    break
            else:
                break
        return rows


    rowinfo = collect_until(current, -1)
    rowinfo.reverse()
    rowinfo += collect_until(current + 1, 1)

    if dot_star:
        return [ rowinfo[0] ]
    else:
        return rowinfo

def snmp_decode_string(text):
    encoding = get_snmp_character_encoding(g_hostname)
    if encoding:
        return text.decode(encoding)
    else:
        # Try to determine the current string encoding. In case a UTF-8 decoding fails, we decode latin1.
        try:
            return text.decode('utf-8')
        except:
            return text.decode('latin1')


def snmp_decode_binary(text):
    return map(ord, text)


#   .--Classic SNMP--------------------------------------------------------.
#   |        ____ _               _        ____  _   _ __  __ ____         |
#   |       / ___| | __ _ ___ ___(_) ___  / ___|| \ | |  \/  |  _ \        |
#   |      | |   | |/ _` / __/ __| |/ __| \___ \|  \| | |\/| | |_) |       |
#   |      | |___| | (_| \__ \__ \ | (__   ___) | |\  | |  | |  __/        |
#   |       \____|_|\__,_|___/___/_|\___| |____/|_| \_|_|  |_|_|           |
#   |                                                                      |
#   +----------------------------------------------------------------------+
#   | Non-inline SNMP handling code. Kept for compatibility.               |
#   '----------------------------------------------------------------------'

def snmpwalk_on_suboid(hostname, ip, oid, hex_plain = False, context_name = None):
    protospec = snmp_proto_spec(hostname)
    portspec = snmp_port_spec(hostname)
    command = snmp_walk_command(hostname)
    if context_name != None:
        command += [ "-n", context_name ]
    command += [ "-OQ", "-OU", "-On", "-Ot", "%s%s%s" % (protospec, ip, portspec), oid ]

    debug_cmd = [ "''" if a == "" else a for a in command ]
    console.vverbose("Running '%s'\n" % " ".join(debug_cmd))

    snmp_process = subprocess.Popen(command, close_fds=True, stdin=open(os.devnull),
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    # Ugly(1): in some cases snmpwalk inserts line feed within one
    # dataset. This happens for example on hexdump outputs longer
    # than a few bytes. Those dumps are enclosed in double quotes.
    # So if the value begins with a double quote, but the line
    # does not end with a double quote, we take the next line(s) as
    # a continuation line.
    rowinfo = []
    try:
        line_iter = snmp_process.stdout.xreadlines()
        while True:
            line = line_iter.next().strip()
            parts = line.split('=', 1)
            if len(parts) < 2:
                continue # broken line, must contain =
            oid = parts[0].strip()
            value = parts[1].strip()
            # Filter out silly error messages from snmpwalk >:-P
            if value.startswith('No more variables') or value.startswith('End of MIB') \
               or value.startswith('No Such Object available') or value.startswith('No Such Instance currently exists'):
                continue

            if value == '"' or (len(value) > 1 and value[0] == '"' and (value[-1] != '"')): # to be continued
                while True: # scan for end of this dataset
                    nextline = line_iter.next().strip()
                    value += " " + nextline
                    if value[-1] == '"':
                        break
            rowinfo.append((oid, strip_snmp_value(value, hex_plain)))

    except StopIteration:
        pass

    error = snmp_process.stderr.read()
    exitstatus = snmp_process.wait()
    if exitstatus:
        console.verbose(tty.red + tty.bold + "ERROR: " + tty.normal + "SNMP error: %s\n" % error.strip())
        raise MKSNMPError("SNMP Error on %s: %s (Exit-Code: %d)" % (ip, error.strip(), exitstatus))
    return rowinfo
