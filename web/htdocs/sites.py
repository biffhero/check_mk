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

import config
import livestatus

#   .--API-----------------------------------------------------------------.
#   |                             _    ____ ___                            |
#   |                            / \  |  _ \_ _|                           |
#   |                           / _ \ | |_) | |                            |
#   |                          / ___ \|  __/| |                            |
#   |                         /_/   \_\_|  |___|                           |
#   |                                                                      |
#   +----------------------------------------------------------------------+
#   |  Functions und names for the public                                  |
#   '----------------------------------------------------------------------'

def live():
    """Get Livestatus connection object matching the current site configuration
       and user settings. On the first call the actual connection is being made."""
    if _live == None:
        _connect()
    return _live


# Accessor for the status of a single site
def state(site_id, deflt=None):
    """Get the status of a certain site. Returns a dictionary with various
       entries. deflt is being returned in case the specified site doe not
       exist or has no state."""
    if _live == None:
        _connect()
    return _site_status.get(site_id, deflt)


def states():
    """Returns dictionary of all known site states."""
    if _live == None:
        _connect()
    return _site_status


def disconnect():
    """Actively closes all Livestatus connections."""
    global _live, _site_status
    _live = None
    _site_status = None


#.
#   .--Internal------------------------------------------------------------.
#   |                ___       _                        _                  |
#   |               |_ _|_ __ | |_ ___ _ __ _ __   __ _| |                 |
#   |                | || '_ \| __/ _ \ '__| '_ \ / _` | |                 |
#   |                | || | | | ||  __/ |  | | | | (_| | |                 |
#   |               |___|_| |_|\__\___|_|  |_| |_|\__,_|_|                 |
#   |                                                                      |
#   +----------------------------------------------------------------------+
#   |  Internal functiions and variables                                   |
#   '----------------------------------------------------------------------'

# The global livestatus object. This is initialized automatically upon first access
# to the accessor function live()
_live = None

# _site_status keeps a dictionary for each site with the following keys:
# "state"              --> "online", "disabled", "down", "unreach", "dead" or "waiting"
# "exception"          --> An error exception in case of down, unreach, dead or waiting
# "status_host_state"  --> host state of status host (0, 1, 2 or None)
# "livestatus_version" --> Version of sites livestatus if "online"
# "program_version"    --> Version of Nagios if "online"
_site_status = None


# Build up a connection to livestatus to either a single site or multiple sites.
def _connect():
    _init_site_status()
    _connect_multiple_sites()
    _set_livestatus_auth()


def _connect_multiple_sites():
    global _live
    enabled_sites, disabled_sites = _get_enabled_and_disabled_sites()
    _set_initial_site_states(enabled_sites, disabled_sites)
    _live = livestatus.MultiSiteConnection(enabled_sites, disabled_sites)

    # Fetch status of sites by querying the version of Nagios and livestatus
    # This may be cached by a proxy for up to the next configuration reload.
    _live.set_prepend_site(True)
    for site_id, v1, v2, ps, num_hosts, num_services in _live.query(
          "GET status\n"
          "Cache: reload\n"
          "Columns: livestatus_version program_version program_start num_hosts num_services"):
        _update_site_status(site_id, {
            "state"              : "online",
            "livestatus_version" : v1,
            "program_version"    : v2,
            "program_start"      : ps,
            "num_hosts"          : num_hosts,
            "num_services"       : num_services,
            "core"               : v2.startswith("Check_MK") and "cmc" or "nagios",
        })
    _live.set_prepend_site(False)

    # Get exceptions in case of dead sites
    for site_id, deadinfo in _live.dead_sites().items():
        shs = deadinfo.get("status_host_state")
        _update_site_status(site_id, {
            "exception"         : deadinfo["exception"],
            "status_host_state" : shs,
            "state"             : _status_host_state_name(shs),
        })


def _get_enabled_and_disabled_sites():
    enabled_sites, disabled_sites = {}, {}

    for site_id, site in config.allsites().items():
        siteconf = config.user.siteconf.get(site_id, {})
        if siteconf.get("disabled", False):
            disabled_sites[site_id] = site
        else:
            enabled_sites[site_id] = site

    return enabled_sites, disabled_sites



def _status_host_state_name(shs):
    if shs == None:
        return "dead"
    else:
        return { 1:"down", 2:"unreach", 3:"waiting", }.get(shs, "unknown")


def _init_site_status():
    global _site_status
    _site_status = {}


def _set_initial_site_states(enabled_sites, disabled_sites):
    for site_id, site in enabled_sites.items():
        _set_site_status(site_id, {
            "state" : "dead",
            "site" : site
        })

    for site_id, site in disabled_sites.items():
        _set_site_status(site_id, {
            "state" : "disabled",
            "site" : site
        })


def _set_site_status(site_id, status):
    _site_status[site_id] = status


def _update_site_status(site_id, status):
    _site_status[site_id].update(status)


# If Multisite is retricted to data the user is a contact for, we need to set an
# AuthUser: header for livestatus.
def _set_livestatus_auth():
    user_id = _livestatus_auth_user()
    if user_id != None:
        _live.set_auth_user('read',   user_id)
        _live.set_auth_user('action', user_id)

    # May the user see all objects in BI aggregations or only some?
    if not config.user.may("bi.see_all"):
        _live.set_auth_user('bi', user_id)

    # May the user see all Event Console events or only some?
    if not config.user.may("mkeventd.seeall"):
        _live.set_auth_user('ec', user_id)

    # Default auth domain is read. Please set to None to switch off authorization
    _live.set_auth_domain('read')


# Returns either None when no auth user shal be set or the name of the user
# to be used as livestatus auth user
def _livestatus_auth_user():
    if not config.user.may("general.see_all"):
        return config.user.id

    force_authuser = html.var("force_authuser")
    if force_authuser == "1":
        return config.user.id
    elif force_authuser == "0":
        return None
    elif force_authuser:
        return force_authuser # set a different user

    # TODO: Remove this with 1.5.0/1.6.0
    if html.output_format != 'html' \
       and config.user.get_attribute("force_authuser_webservice"):
        return config.user.id

    if config.user.get_attribute("force_authuser"):
        return config.user.id

    return None


