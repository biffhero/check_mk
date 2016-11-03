#!/usr/bin/python
# encoding: utf-8

import os
import glob
from testlib import cmk_path, cmc_path

check_paths = [
    "bin",
    "modules",
    "lib",
    "checks",
    "inventory",
    "notifications",
    "doc/treasures/active_checks",
    # CMC specific
    "agents/bakery",
    # TODO: Update all agent plugins to use sys.stdout.write instead of print
    "agents/plugins",
]

def test_find_debug_code():
    scanned = 0
    for base_path in [ cmk_path(), cmc_path() ]:
        for dir_path in check_paths:
            path = "%s/%s" % (base_path, dir_path)
            if not os.path.exists(path):
                continue

            for dirpath, dirnames, filenames in os.walk(path):
                scanned += 1
                for filename in filenames:
                    file_path = "%s/%s" % (dirpath, filename)

                    for nr, line in enumerate(open(file_path)):
                        if nr == 0 and ("bash" in line or "php" in line):
                            break # skip non python files

                        l = line.lstrip()
                        assert not l.startswith("print("), \
                            "Found \"print(...)\" call in %s:%d" % \
                                                    (file_path, nr+1)
                        assert not l.startswith("print "), \
                            "Found \"print ...\" call in %s:%d" % \
                                                    (file_path, nr+1)

    assert scanned > 0
