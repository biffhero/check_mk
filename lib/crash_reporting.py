#!/usr/bin/python
# -*- encoding: utf-8; py-indent-offset: 4 -*-
# +------------------------------------------------------------------+
# |             ____ _               _        __  __ _  __           |
# |            / ___| |__   ___  ___| | __   |  \/  | |/ /           |
# |           | |   | '_ \ / _ \/ __| |/ /   | |\/| | ' /            |
# |           | |___| | | |  __/ (__|   <    | |  | | . \            |
# |            \____|_| |_|\___|\___|_|\_\___|_|  |_|_|\_\           |
# |                                                                  |
# | Copyright Mathias Kettner 2016             mk@mathias-kettner.de |
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

"""This module contains functions that can be used in all Check_MK components
to produce crash reports in a generic format which can then be sent to Check_MK
developers for analyzing the crashes."""

import base64
import inspect
import os
import pprint
import sys
import time
import traceback

try:
    import simplejson as json
except ImportError:
    import json

from . import __version__

# The default JSON encoder raises an exception when detecting unknown types. For the crash
# reporting it is totally ok to have some string representations of the objects.
class RobustJSONEncoder(json.JSONEncoder):
    # Are there cases where no __str__ is available? if so, we should do something like %r
    # pylint: disable=method-hidden
    def default(self, o):
        return "%s" % o


def crash_info_to_string(crash_info):
    return json.dumps(crash_info, cls=RobustJSONEncoder)


# The top level keys of the crash info dict are standardized
def create_crash_info(crash_type, details=None, version=None):
    if details == None:
        details = {}

    if version == None:
        version = __version__

    exc_type, exc_value, exc_traceback = sys.exc_info()

    return {
        "crash_type"    : crash_type,
        "time"          : time.time(),
        "os"            : get_os_info(),
        "version"       : version,
        "python_version": sys.version,
        "python_paths"  : sys.path,
        "exc_type"      : exc_type.__name__,
        "exc_value"     : "%s" % exc_value,
        "exc_traceback" : traceback.extract_tb(exc_traceback),
        "local_vars"    : get_local_vars_of_last_exception(),
        "details"       : details,
    }


def get_os_info():
    if "OMD_ROOT" in os.environ:
        return file(os.environ["OMD_ROOT"]+"/share/omd/distro.info").readline().split("=", 1)[1].strip()
    elif os.path.exists("/etc/redhat-release"):
        return file("/etc/redhat-release").readline().strip()
    elif os.path.exists("/etc/SuSE-release"):
        return file("/etc/SuSE-release").readline().strip()
    else:
        info = {}
        for f in [ "/etc/os-release", "/etc/lsb-release" ]:
            if os.path.exists(f):
                for line in file(f).readlines():
                    if "=" in line:
                        k, v = line.split("=", 1)
                        info[k.strip()] = v.strip().strip("\"")
                break

        if "PRETTY_NAME" in info:
            return info["PRETTY_NAME"]
        elif info:
            return info
        else:
            return "UNKNOWN"


def get_local_vars_of_last_exception():
    local_vars = {}
    try:
        for key, val in inspect.trace()[-1][0].f_locals.items():
            local_vars[key] = format_var_for_export(val)
    except IndexError:
        # please don't crash in the attempt to report a crash.
        # Don't know why inspect.trace() causes an IndexError but it does happen
        pass

    # This needs to be encoded as the local vars might contain binary data which can not be
    # transported using JSON.
    return base64.b64encode(format_var_for_export(pprint.pformat(local_vars), maxsize=5*1024*1024))


def format_var_for_export(val, maxdepth=4, maxsize=1024*1024):
    if maxdepth == 0:
        return "Max recursion depth reached"

    if isinstance(val, dict):
        val = val.copy()
        for item_key, item_val in val.items():
            val[item_key] = format_var_for_export(item_val, maxdepth-1)

    elif isinstance(val, list):
        val = val[:]
        for index, item in enumerate(val):
            val[index] = format_var_for_export(item, maxdepth-1)

    elif isinstance(val, tuple):
        new_val = ()
        for item in val:
            new_val += (format_var_for_export(item, maxdepth-1),)
        val = new_val

    # Check and limit size
    if type(val) in (str, unicode):
        size = len(val)
        if size > maxsize:
            val = val[:maxsize] + "... (%d bytes stripped)" % (size - maxsize)

    return val
