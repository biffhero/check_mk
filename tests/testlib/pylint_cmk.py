#!/usr/bin/python
# Library for pylint checks of Check_MK

import os
import sys
import glob
import shutil
import subprocess
import tempfile

from pylint.reporters.text import ColorizedTextReporter, ParseableTextReporter
from pylint.utils import Message

from testlib import repo_path


def ordered_module_files():
    ordered_modules = [
        "modules/check_mk_base.py",
        "modules/check_mk.py",
        "modules/config.py",
        "modules/discovery.py",
        "modules/snmp.py",
        "modules/notify.py",
        "modules/events.py",
        "modules/nagios.py",
        "modules/automation.py",
        "modules/inventory.py",
        "../cmc/modules/real_time_checks.py",
        "../cmc/modules/alert_handling.py",
        "../cmc/modules/keepalive.py",
        "../cmc/modules/cmc.py",
        "../cmc/modules/inline_snmp.py",
        "../cmc/modules/agent_bakery.py",
        "../cmc/modules/cap.py",
        "../cmc/modules/rrd.py",
    ]

    modules = [ os.path.realpath(repo_path() + "/" + p)
                for p in ordered_modules ]

    # Add modules which are not specified above
    for path in module_files():
        if path not in modules:
            modules.append(path)

    return modules


def module_files():
    modules = []
    for base_path in [ repo_path() + "/modules",
                       os.path.realpath(repo_path() + "/../cmc/modules") ]:

        modules += [ base_path + "/" + f for f in os.listdir(base_path)
                     if not f.startswith(".") ]
    return sorted(modules)


def check_files(base_dir):
    filelist = sorted([ base_dir + "/" + f for f in os.listdir(base_dir)
                         if not f.startswith(".") ])

    # Sort: first includes, then other
    filelist = [ f for f in filelist if f.endswith(".include") ] + \
               [ f for f in filelist if not f.endswith(".include") ]

    return filelist


def add_file(f, path):
    # Change path to be relative to "workdir" /home/git or the workdir
    # in the build system.
    relpath = os.path.relpath(os.path.realpath(path),
                              os.path.dirname(os.path.dirname(os.getcwd())))
    f.write("\n")
    f.write("#\n")
    f.write("# ORIG-FILE: " + relpath + "\n")
    f.write("#\n")
    f.write("\n")
    f.write(file(path).read())


def get_test_dir():
    base_path = os.environ.get("WORKDIR")
    if base_path:
        base_path += "/" + os.path.basename(sys.argv[0])
        if not os.path.exists(base_path):
            os.makedirs(base_path)
    else:
        base_path = tempfile.mkdtemp(prefix="cmk_pylint")

    print("Prepare check in %s ..." % base_path)
    return base_path


def run_pylint(base_path, check_files=None, cleanup_test_dir=False):
    pylint_args = os.environ.get("PYLINT_ARGS", "")
    if pylint_args:
        pylint_args += " "
    pylint_output = os.environ.get("PYLINT_OUTPUT")

    pylint_cfg = repo_path() + "/pylintrc"

    check_files = get_pylint_files(base_path, "*")
    if not check_files:
        print "Nothing to do..."
        return 0 # nothing to do

    os.putenv("TEST_PATH", repo_path() + "/tests")
    cmd = "pylint --rcfile=\"%s\" %s%s" % (pylint_cfg, pylint_args, " ".join(check_files))
    print("Running pylint with: %s" % cmd)
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                         shell=True, cwd=base_path)
    stdout = p.communicate()[0]

    if stdout.strip():
        if pylint_output:
            file(pylint_output, "a").write(stdout)
        else:
            print(stdout)

    exit_code = p.returncode
    print("Finished with exit code: %d" % exit_code)

    if exit_code == 0 and cleanup_test_dir:
        # Don't remove directory when specified via WORKDIR env
        if not os.environ.get("WORKDIR"):
            print("Removing build path...")
            shutil.rmtree(base_path)

    return exit_code


def get_pylint_files(base_path, file_pattern):
    files = []
    for path in glob.glob("%s/%s" % (base_path, file_pattern)):
        f = path[len(base_path)+1:]

        if is_python_file(path):
            files.append(f)

    return files


def is_python_file(path):
    if not os.path.isfile(path) or os.path.islink(path):
        return False

    if path.endswith(".py"):
        return True

    # Only add python files
    shebang = file(path, "r").readline()
    if shebang.startswith("#!") and "python" in shebang:
        return True

    return False


# Check_MK currently uses a packed version of it's files to
# run the pylint tests because it's not well structured in
# python modules. This custom reporter rewrites the found
# messages to tell the users the original location in the
# python sources
class CMKFixFileMixin(object):
    def handle_message(self, msg):
        new_path, new_line = self._orig_location_from_compiled_file(msg)

        if new_path == None:
            new_path = self._change_path_to_repo_path(msg)

        if new_path != None:
            msg = msg._replace(path=new_path)
        if new_line != None:
            msg = msg._replace(line=new_line)

        super(CMKFixFileMixin, self).handle_message(msg)


    def _change_path_to_repo_path(self, msg):
        abspath = os.path.join(os.getcwd(), msg.abspath)
        parts = abspath.split("/")
        while parts and parts[0] not in ["check_mk", "cmc"]:
            parts.pop(0)

        if parts:
            return "/".join(parts)


    def _orig_location_from_compiled_file(self, msg):
        lines = file(msg.abspath).readlines()
        line_nr = msg.line
        orig_file, went_back = None, -3
        while line_nr > 0:
            line_nr -= 1
            went_back += 1
            line = lines[line_nr]
            if line.startswith("# ORIG-FILE: "):
                orig_file = line.split(": ", 1)[1].strip()
                break

        if orig_file == None:
            went_back = None

        return orig_file, went_back



class CMKColorizedTextReporter(CMKFixFileMixin, ColorizedTextReporter):
    name = "cmk_colorized"



class CMKParseableTextReporter(CMKFixFileMixin, ParseableTextReporter):
    name = "cmk_parseable"


def verify_pylint_version():
    import pylint
    if tuple(map(int, pylint.__version__.split("."))) < (1, 5, 5):
        raise Exception("You need to use at least pylint 1.5.5. Run \"make setup\" in "
                        "pylint directory to get the current version.")


# Is called by pylint to load this plugin
def register(linter):
    verify_pylint_version()

    linter.register_reporter(CMKColorizedTextReporter)
    linter.register_reporter(CMKParseableTextReporter)
