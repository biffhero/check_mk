#!/usr/bin/env python
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

import sys
import logging as _logging

# Users should be able to set log levels without importing "logging"

CRITICAL = _logging.CRITICAL
ERROR    = _logging.ERROR
WARNING  = _logging.WARNING
INFO     = _logging.INFO
DEBUG    = _logging.DEBUG


# We need an additional log level between INFO and DEBUG to reflect the
# verbose() and vverbose() mechanisms of Check_MK.

VERBOSE = 15

class CMKLogger(_logging.getLoggerClass()):
    def __init__(self, name, level=_logging.NOTSET):
        super(CMKLogger, self).__init__(name, level)

        _logging.addLevelName(VERBOSE, "VERBOSE")

    def verbose(self, msg, *args, **kwargs):
        if self.is_verbose():
            self._log(VERBOSE, msg, args, **kwargs)


    def is_verbose(self):
        return self.isEnabledFor(VERBOSE)


    def is_very_verbose(self):
        return self.isEnabledFor(DEBUG)


_logging.setLoggerClass(CMKLogger)


# Set default logging handler to avoid "No handler found" warnings.
# Python 2.7+
logger = _logging.getLogger("cmk")
logger.addHandler(_logging.NullHandler())
logger.setLevel(INFO)


def get_logger(name):
    """This function provides the logging object for client code.

    It returns a child logger of the "cmk" main logger, identified
    by the given name. The name of the child logger will be prefixed
    with "cmk.", for example "cmk.mkeventd" in case of "mkeventd".
    """
    return logger.getChild(name)


def setup_console_logging():
    """This method enables all log messages to be written to the console
    without any additional information like date/time, logger-name. Just
    the log line is written.

    This can be used for existing command line applications which were
    using sys.stdout.write() or print() before.
    """

    handler = _logging.StreamHandler(stream=sys.stdout)

    formatter = _logging.Formatter("%(message)s")
    handler.setFormatter(formatter)

    logger.addHandler(handler)


def set_verbosity(verbosity):
    """Values for "verbosity":

      0: enables INFO and above
      1: enables VERBOSE and above
      2: enables DEBUG and above (ALL messages)
    """
    if verbosity == 0:
    	logger.setLevel(INFO)

    elif verbosity == 1:
    	logger.setLevel(VERBOSE)

    elif verbosity == 2:
    	logger.setLevel(DEBUG)

    else:
        raise NotImplementedError()


# TODO: Experimental. Not yet used.
class LogMixin(object):
    """Inherit from this class to provide logging support.

    Makes a logger available via "self.logger" for objects and
    "self.cls_logger" for the class.
    """
    _logger     = None
    _cls_logger = None

    @property
    def logger(self):
        if not self._logger:
            self._logger = _logging.getLogger('.'.join([__name__, self.__class__.__name__]))
        return self._logger


    @classmethod
    def cls_logger(cls):
        if not cls._cls_logger:
            cls._cls_logger = _logging.getLogger('.'.join([__name__, cls.__name__]))
        return cls._cls_logger
