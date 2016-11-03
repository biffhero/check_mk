#!/usr/biin/python
# call using
# > py.test -s -k test_html_generator.py

# enable imports from web directory
from testlib import cmk_path
import sys
sys.path.insert(0, "%s/web/htdocs" % cmk_path())

# external imports
import re
import difflib
import warnings
import traceback  # for tracebacks
try:
    import dill.source # for displaying lambda functional code
except:
    print "Cannot import dill.source" in tests/web/tools.py

import time
from bs4 import BeautifulSoup as bs
from bs4 import NavigableString

# internal imports
from htmllib import HTML


class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


def timeit(method):
    def timed(*args, **kw):
        ts = time.time()
        result = method(*args, **kw)
        te = time.time()
        print '%r %2.2f sec' % \
              (method.__name__, te-ts)
        return result
    return timed


def prettify(html_text):
    if isinstance(html_text, HTML):
        txt = bs(html_text.value, 'html5lib').prettify()
    else:
        txt = bs(html_text, 'html5lib').prettify()
    return re.sub('\n{2,}', '\n', re.sub('>', '>\n', txt))


class HTMLCode(object):

    def __init__(self, value):
        self.value = value

    def prettify(self):
        return bs(self.value, 'html5lib').prettify()


def encode_attribute(value):
    if isinstance(value, list):
        return map(encode_attribute, value)

    return value.replace("&", "&amp;")\
                .replace('"', "&quot;")\
                .replace("<", "&lt;")\
                .replace(">", "&gt;")


def undo_encode_attribute(value):
    if isinstance(value, list):
        return map(undo_encode_attribute, value)

    return value.replace("&quot;", '"')\
                .replace("&lt;", '<')\
                .replace("&gt;", '>')\
                .replace("&amp;", '&')\


def subber(value):
    if isinstance(value, list):
        return map(subber, value)

    return re.sub('>', ' ',\
           re.sub('<', ' ',\
           re.sub('\\\\', '',\
           re.sub("'", '&quot;',\
           re.sub('"', '&quot;',\
           re.sub('\n', '', value))))))

def compare_soup(html1, html2):

    if isinstance(html1, HTML):
       html1 = html1.value

    if isinstance(html2, HTML):
       html2 = html2.value

    s1 = bs(prettify(html1), 'html5lib')
    s2 = bs(prettify(html2), 'html5lib')

    children_1 = list(s1.recursiveChildGenerator())
    children_2 = list(s2.recursiveChildGenerator())

    unify_attrs = lambda x: encode_attribute(undo_encode_attribute(subber(x)))

    for d1, d2 in zip(children_1, children_2):

        assert type(d1) == type(d2), "\n%s\n%s" % (type(d1), type(d2))

        if isinstance(d1, NavigableString):
            set1 = set(filter(lambda x: x, subber(d1).split(' ')))
            set2 = set(filter(lambda x: x, subber(d2).split(' ')))
            assert set1 == set2, "\n%s\n%s\n" % (set1, set2)

        else:
            assert len(list(d1.children)) == len(list(d2.children)), '%s\n%s' % (s1.prettify(), s2.prettify())
            attrs1 = {k: filter(lambda x: x != '', (v)) for k, v in d1.attrs.iteritems() if len(v) > 0}
            attrs2 = {k: filter(lambda x: x != '', (v)) for k, v in d2.attrs.iteritems() if len(v) > 0}

            for key in attrs1.keys():
                assert key in attrs2, '%s\n%s\n\n%s' % (key, d1, d2)
                if key.startswith("on") or key == "style":
                    val1 = filter(lambda x: x, map(lambda x: unify_attrs(x).strip(' '), attrs1.pop(key, '').split(';')))
                    val2 = filter(lambda x: x, map(lambda x: unify_attrs(x).strip(' '), attrs2.pop(key, '').split(';')))
                    assert val1 == val2, '\n%s\n%s' % (val1, val2)

            assert attrs1 == attrs2, '\n%s\n%s' % (d1, d2)


def compare_html(html1, html2):

    if isinstance(html1, HTML):
       html1 = html1.value

    if isinstance(html2, HTML):
       html2 = html2.value


    # compare tags
    opening_1 = re.findall(r'<[^<]*>', html1)
    opening_2 = re.findall(r'<[^<]*>', html2)
    closing_1 = re.findall(r'</\s*\w+\s*>', html1)
    closing_2 = re.findall(r'</\s*\w+\s*>', html2)

    map(lambda x: compare_soup(x[0], x[1]), zip(opening_1, opening_2))
    assert closing_1 == closing_2, '\n%s\n%s' % (closing_1, closing_2)

    # compare soup structure
    compare_soup(html1, html2)

    return True


def test_compare_soup():

    html1 = '<tag> <div class=\"test\"> Test <img src = \"Yo!\" /> </div> Hallo Welt! </tag>'
    compare_soup(html1, html1)


def compare_attributes(old, new):

    vars_old = {var: getattr(old, var) for var in dir(old) if not callable(getattr(old,var))\
                                                              and not var.startswith("__")}
    vars_new = {var: getattr(new, var) for var in dir(new) if not callable(getattr(new,var))\
                                                              and not var.startswith("__")}
    # compare variables
    exclusives_old = {var for var in vars_old if var not in vars_new}
    exclusives_new = {var for var in vars_new if var not in vars_old}

    # compare variable_content
    for var in vars_old:
         if var not in exclusives_old and var not in ['start_time', 'last_measurement', 'plugged_text', 'indent_level']:
            assert getattr(old, var) == getattr(new, var),\
                "Values for attribute %s differ: %s %s" % (var, getattr(old, var), getattr(new, var))


# For classes using the drain() functionality
def compare_and_empty(old, new):

    # compare html code
    compare_html(old.plugged_text, new.plugged_text)

    # compare attribute values
    compare_attributes(old, new)

    # empty plugged_text
    old.drain()
    new.drain()


def _html_generator_test(old, new, fun, reinit=True):

#     try:

        old.plug()
        new.plug()

        fun(old)
        fun(new)

        # compare html code
        compare_html(old.plugged_text, new.plugged_text)

        # compare attribute values
        compare_attributes(old, new)

#     except (NotImplementedError) as err:
# 
#         trace = traceback.format_exc().splitlines()
#         print bcolors.WARNING + "WARNING: " + bcolors.ENDC\
#               + trace[-1] + " in class " + bcolors.OKBLUE\
#               + ''.join(s.strip('(').strip(')') for s in re.findall(r'\(.*\)', trace[2]))\
#               + bcolors.ENDC
#         print '\n'.join(trace[5:-1])
# 
#     finally:
# 
        if reinit:
            old.__init__()
            new.__init__()

        old.plug()
        new.plug()


# Try to render and write the html using the function fun. (e.g. old.open_head())
# Resets the whole element
def html_generator_test(old, new, fun, attributes=None, reinit=True):

    if attributes and not isinstance(attributes, list):
        attributes=[attributes]


    if reinit:
        try:
            print bcolors.HEADER + "TESTING" + bcolors.ENDC + dill.source.getsource(fun)
        except:
            print "Cannot import dill.source" in tests/web/tools.py


    if attributes:
        attr = attributes.pop(0)

        setattr(old, attr, True)
        setattr(new, attr, True)
        html_generator_test(old, new, fun, attributes=attributes, reinit=False)

        setattr(old, attr, False)
        setattr(new, attr, False)
        html_generator_test(old, new, fun, attributes=attributes, reinit=False)

    else:
        _html_generator_test(old, new, fun, reinit=reinit)


def gentest(old, new, fun, **attrs):
    html_generator_test(old, new, fun, **attrs)


