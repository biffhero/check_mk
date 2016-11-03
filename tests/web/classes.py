#!/usr/bin/python
# call using
# > py.test -s -k test_html_generator.py

# enable imports from web directory
from testlib import cmk_path
import sys
sys.path.insert(0, "%s/web/htdocs" % cmk_path())

# external imports
import re
import json

# internal imports
from htmllib import html
from htmllib import HTMLGenerator, HTMLCheck_MK

#   .--Deprecated Renderer-------------------------------------------------.
#   |          ____                                _           _           |
#   |         |  _ \  ___ _ __  _ __ ___  ___ __ _| |_ ___  __| |          |
#   |         | | | |/ _ \ '_ \| '__/ _ \/ __/ _` | __/ _ \/ _` |          |
#   |         | |_| |  __/ |_) | | |  __/ (_| (_| | ||  __/ (_| |          |
#   |         |____/ \___| .__/|_|  \___|\___\__,_|\__\___|\__,_|          |
#   |                    |_|                                               |
#   |              ____                _                                   |
#   |             |  _ \ ___ _ __   __| | ___ _ __ ___ _ __                |
#   |             | |_) / _ \ '_ \ / _` |/ _ \ '__/ _ \ '__|               |
#   |             |  _ <  __/ | | | (_| |  __/ | |  __/ |                  |
#   |             |_| \_\___|_| |_|\__,_|\___|_|  \___|_|                  |
#   |                                                                      |
#   +----------------------------------------------------------------------+
#   | Rendering methods which are to be replaced by the new HTMLGenerator. |
#   | It borrows its write functionality from the OutputFunnel.            |
#   '----------------------------------------------------------------------'


class DeprecatedRenderer(object):


    def __init__(self):
        super(DeprecatedRenderer, self).__init__()

        self.header_sent = False
        self._default_stylesheets = ['check_mk', 'graphs']
        self._default_javascripts = [ "checkmk", "graphs" ]
        self.browser_reload = 0
        self.browser_redirect = ''
        self.render_headfoot = True
        self.output_format = "html"

        self.enable_debug = False
        self.screenshotmode = False
        self.have_help = False
        self.help_visible = False

        self.body_classes = ['main']
        self.link_target = None

        self.context_buttons_open = False
        self.keybindings_enabled = True
        self.keybindings = []


    def add_keybinding(self, keylist, jscode):
        self.keybindings.append([keylist, jscode])


    def header(self, title='', **args):
        if self.output_format == "html":
            if not self.header_sent:
                self.body_start(title, **args)
                self.header_sent = True
                if self.render_headfoot:
                    self.top_heading(title)


    def body_start(self, title='', **args):
        self.html_head(title, **args)
        self.write('<body class="%s">' % self._get_body_css_classes())


    def _get_body_css_classes(self):
        body_classes = self.body_classes
        if self.screenshotmode:
            body_classes.append("screenshotmode")
        return " ".join(body_classes)


    def add_custom_style_sheet(self):
        raise NotImplementedError()


    def css_filename_for_browser(self, css):
        raise NotImplementedError()


    def javascript_filename_for_browser(self, jsname):
        raise NotImplementedError()


    def html_foot(self):
        self.write("</html>\n")


    def top_heading(self, title):
        raise NotImplementedError()


    def top_heading_left(self, title):
        self.write('<table class=\"header\"><tr><td width="*" class=\"heading\">')
        self.write('<a href="#" onfocus="if (this.blur) this.blur();" '
                   'onclick="this.innerHTML=\'%s\'; document.location.reload();">%s</a></td>' %
                   (_("Reloading..."), self.attrencode(title)))


    def top_heading_right(self):
        cssclass = self.help_visible and "active" or "passive"
        self.icon_button(None, _("Toggle context help texts"), "help", id="helpbutton",
                         onclick="toggle_help()", style="display:none", ty="icon", cssclass=cssclass)

        self.write("%s</td></tr></table>" %
                   _("<a class=head_logo href=\"http://mathias-kettner.de\">"
                     "<img src=\"images/logo_cmk_small.png\"/></a>"))
        self.write("<hr class=\"header\">\n")
        if self.enable_debug:
            self._dump_get_vars()


    # Embed help box, whose visibility is controlled by a global
    # button in the page.
    def help(self, text):
        if text and text.strip():
            self.have_help = True
            self.write('<div class=help style="display: %s">' % (
                        not self.help_visible and "none" or "block"))
            self.write(text.strip())
            self.write('</div>')


    #
    # HTML form rendering
    #


    def detect_icon_path(self, icon_name):
        raise NotImplementedError()


    def icon(self, help, icon, **kwargs):

        # TODO:
        title = help

        self.write(self.render_icon(icon, title, **kwargs))


    def empty_icon(self):
        self.write(self.render_icon("images/trans.png"))


    def render_icon(self, icon_name, help="", middle=True, id=None, cssclass=None):

        #TODO
        title = help
        id_ = id

        align = middle and ' align=absmiddle' or ''
        title = title and ' title="%s"' % self.attrencode(title) or ""
        id_ = id_ and ' id="%s"' % id_ or ''
        cssclass = cssclass and (" " + cssclass) or ""

        if "/" in icon_name:
            icon_path = icon_name
        else:
            icon_path = self.detect_icon_path(icon_name)

        return '<img src="%s" class="icon%s"%s%s%s />' % (icon_path, cssclass, align, title, id_)



    def render_icon_button(self, url, help, icon, id="", onclick="",
                           style="", target="", cssclass="", ty="button"):

        # TODO:
        title = help
        id_ = id

        if id_:
            id_ = "id='%s' " % id_

        if onclick:
            onclick = 'onclick="%s" ' % onclick
            url = "javascript:void(0)"

        if style:
            style = 'style="%s" ' % style

        if target:
            target = 'target="%s" ' % target
        else:
            target = ""

        if cssclass:
            cssclass = 'class="%s" ' % cssclass

        # TODO: Can we clean this up and move all button_*.png to internal_icons/*.png?
        if ty == "button":
            icon = "images/button_" + icon + ".png"

        return '<a %s%s%s%s%sonfocus="if (this.blur) this.blur();" href="%s" title="%s">%s</a>' % \
                 (id_, onclick, style, target, cssclass, url, self.attrencode(title),
                  self.render_icon(icon, cssclass="iconbutton"))


    def icon_button(self, *args, **kwargs):
        self.write(self.render_icon_button(*args, **kwargs))


    def popup_trigger(self, *args, **kwargs):
        self.write(self.render_popup_trigger(*args, **kwargs))


    def render_popup_trigger(self, content, ident, what=None, data=None, url_vars=None,
                             style=None, menu_content=None, cssclass=None, onclose=None):
        style = style and (' style="%s"' % style) or ""
        src = '<div class="popup_trigger%s" id="popup_trigger_%s"%s>\n' % (cssclass and (" " + cssclass) or "", ident, style)
        onclick = "toggle_popup(event, this, \'%s\', %s, %s, %s, %s, %s);" % \
                    (ident, what and  "'"+what+"'" or 'null',
                     self.attrencode(json.dumps(data)) if data else 'null',
                     url_vars and "'"+self.urlencode_vars(url_vars)+"'" or 'null',
                     menu_content and "'"+self.attrencode(menu_content)+"'" or 'null',
                     onclose and "'%s'" % onclose.replace("'", "\\'") or 'null')
        src += '<a class="popup_trigger" href="javascript:void(0);" onclick="%s">\n' % onclick
        src += content
        src += '</a>'
        src += '</div>\n'
        return src



    #
    # Context Buttons
    #


    def begin_context_buttons(self):
        if not self.context_buttons_open:
            self.context_button_hidden = False
            self.write("<table class=contextlinks><tr><td>\n")
            self.context_buttons_open = True


    def end_context_buttons(self):
        if self.context_buttons_open:
            if self.context_button_hidden:
                self.write('<div title="%s" id=toggle class="contextlink short" '
                      % _("Show all buttons"))
                self._context_button_hover_code("_short")
                self.write("><a onclick='unhide_context_buttons(this);' href='#'>...</a></div>")
            self.write("</td></tr></table>\n")
        self.context_buttons_open = False


    def context_button(self, title, url, icon=None, hot=False, id=None, bestof=None, hover_title=None, fkey=None):
        #self.guitest_record_output("context_button", (title, url, icon))
        title = self.attrencode(title)
        display = "block"
        if bestof:
            counts = self.get_button_counts()
            weights = counts.items()
            weights.sort(cmp = lambda a,b: cmp(a[1],  b[1]))
            best = dict(weights[-bestof:])
            if id not in best:
                display="none"
                self.context_button_hidden = True

        if not self.context_buttons_open:
            self.begin_context_buttons()

        title = "<span>%s</span>" % self.attrencode(title)
        if icon:
            title = '%s%s' % (self.render_icon(icon, cssclass="inline", middle=False), title)

        if id:
            idtext = " id='%s'" % self.attrencode(id)
        else:
            idtext = ""
        self.write('<div%s style="display:%s" class="contextlink%s%s" ' %
            (idtext, display, hot and " hot" or "", (fkey and self.keybindings_enabled) and " button" or ""))
        self._context_button_hover_code(hot and "_hot" or "")
        self.write('>')
        self.write('<a href="%s"' % self.attrencode(url))
        if hover_title:
            self.write(' title="%s"' % self.attrencode(hover_title))
        if bestof:
            self.write(' onclick="count_context_button(this); " ')
        if fkey and self.keybindings_enabled:
            title += '<div class=keysym>F%d</div>' % fkey
            self.add_keybinding([html.F1 + (fkey - 1)], "document.location='%s';" % self.attrencode(url))
        self.write('>%s</a></div>\n' % title)


    def get_button_counts(self):
        raise NotImplementedError()


    def _context_button_hover_code(self, what):
        self.write(r'''onmouseover='this.style.backgroundImage="url(\"images/contextlink%s_hi.png\")";' ''' % what)
        self.write(r'''onmouseout='this.style.backgroundImage="url(\"images/contextlink%s.png\")";' ''' % what)




    # Only strip off some tags. We allow some simple tags like
    # <b>, <tt>, <i> to be part of the string. This is useful
    # for messages where we still want to have formating options.
    def permissive_attrencode(self, obj):
        msg = self.attrencode(obj)
        msg = re.sub(r'&lt;(/?)(h2|b|tt|i|br(?: /)?|pre|a|sup|p|li|ul|ol)&gt;', r'<\1\2>', msg)
        # Also repair link definitions
        return re.sub(r'&lt;a href=&quot;(.*?)&quot;&gt;', r'<a href="\1">', msg)


    # Encode HTML attributes: replace " with &quot;, also replace
    # < and >. This code is slow. Works on str and unicode without
    # changing the type. Also works on things that can be converted
    # with %s.
    def attrencode(self, value):
        ty = type(value)
        if ty == int:
            return str(value)
        elif isinstance(value, HTML):
            return value.value # This is HTML code which must not be escaped
        elif ty not in [str, unicode]: # also possible: type Exception!
            value = "%s" % value # Note: this allows Unicode. value might not have type str now

        return value.replace("&", "&amp;")\
                    .replace('"', "&quot;")\
                    .replace("<", "&lt;")\
                    .replace(">", "&gt;")

    #
    # Encoding and escaping
    #


    # Encode HTML attributes. Replace HTML syntax with HTML text.
    # For example: replace '"' with '&quot;', '<' with '&lt;'.
    # This code is slow. Works on str and unicode without changing
    # the type. Also works on things that can be converted with '%s'.
    def _escape_attribute(self, value):
        attr_type = type(value)
        if value is None:
            return ''
        elif attr_type == int:
            return str(value)
        elif isinstance(value, HTML):
            return value.value # This is HTML code which must not be escaped
        elif attr_type not in [str, unicode]: # also possible: type Exception!
            value = "%s" % value # Note: this allows Unicode. value might not have type str now
        return value.replace("&", "&amp;")\
                    .replace('"', "&quot;")\
                    .replace("<", "&lt;")\
                    .replace(">", "&gt;")


    # render HTML text.
    # We only strip od some tags and allow some simple tags
    # such as <h1>, <b> or <i> to be part of the string.
    # This is useful for messages where we want to keep formatting
    # options. (Formerly known as 'permissive_attrencode') """
    # for the escaping functions
    _unescaper_text = re.compile(r'&lt;(/?)(h2|b|tt|i|br(?: /)?|pre|a|sup|p|li|ul|ol)&gt;')
    _unescaper_href = re.compile(r'&lt;a href=&quot;(.*?)&quot;&gt;')
    def _escape_text(self, text):

        if isinstance(text, HTML):
            return text.value # This is HTML code which must not be escaped

        text = self._escape_attribute(text)
        text = self._unescaper_text.sub(r'<\1\2>', text)
        # Also repair link definitions
        text = self._unescaper_href.sub(r'<a href="\1">', text)
        return text


    # This function returns a str object, never unicode!
    # Beware: this code is crucial for the performance of Multisite!
    # Changing from the self coded urlencode to urllib.quote
    # is saving more then 90% of the total HTML generating time
    # on more complex pages!
    def urlencode_vars(self, vars):
        output = []
        for varname, value in sorted(vars):
            if type(value) == int:
                value = str(value)
            elif type(value) == unicode:
                value = value.encode("utf-8")

            try:
                # urllib is not able to encode non-Ascii characters. Yurks
                output.append(varname + '=' + urllib.quote(value))
            except:
                output.append(varname + '=' + self.urlencode(value)) # slow but working

        return '&'.join(output)


    def urlencode(self, value):
        if type(value) == unicode:
            value = value.encode("utf-8")
        elif value == None:
            return ""
        ret = ""
        for c in value:
            if c == " ":
                c = "+"
            elif ord(c) <= 32 or ord(c) > 127 or c in [ '#', '+', '"', "'", "=", "&", ":", "%" ]:
                c = "%%%02x" % ord(c)
            ret += c
        return ret


    # Escape a variable name so that it only uses allowed charachters for URL variables
    def varencode(self, varname):
        if varname == None:
            return "None"
        if type(varname) == int:
            return varname

        ret = ""
        for c in varname:
            if not c.isdigit() and not c.isalnum() and c != "_":
                ret += "%%%02x" % ord(c)
            else:
                ret += c
        return ret


    def u8(self, c):
        if ord(c) > 127:
            return "&#%d;" % ord(c)
        else:
            return c


    def utf8_to_entities(self, text):
        if type(text) != unicode:
            return text
        else:
            return text.encode("utf-8")


    # remove all HTML-tags
    def strip_tags(self, ht):
        if type(ht) not in [str, unicode]:
            return ht
        while True:
            x = ht.find('<')
            if x == -1:
                break
            y = ht.find('>', x)
            if y == -1:
                break
            ht = ht[0:x] + ht[y+1:]
        return ht.replace("&nbsp;", " ")


    def strip_scripts(self, ht):
        while True:
            x = ht.find('<script')
            if x == -1:
                break
            y = ht.find('</script>')
            if y == -1:
                break
            ht = ht[0:x] + ht[y+9:]
        return ht




    #
    # HTML low level rendering and writing functions. Only put most basic function in
    # this section which are really creating only some simple tags.
    #

    def heading(self, text):
        self.write("<h2>%s</h2>\n" % text)


    def rule(self):
        self.write("<hr/>")


    def p(self, content):
        self.write("<p>%s</p>" % self.attrencode(content))


    def render_javascript(self, code):
        return "<script language=\"javascript\">\n%s\n</script>\n" % code


    def javascript(self, code):
        self.write(self.render_javascript(code))


    def javascript_file(self, name):
        self.write('<script type="text/javascript" src="js/%s.js">\n</script>\n' % name)


    def play_sound(self, url):
        self.write("<audio src=\"%s\" autoplay>\n" % self.attrencode(url))


    #
    # HTML - All the common and more complex HTML rendering methods
    #


    def default_html_headers(self):
        self.write('<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />\n')
        self.write('<meta http-equiv="X-UA-Compatible" content="IE=edge" />\n')
        self.write('<link rel="shortcut icon" href="images/favicon.ico" type="image/ico">\n')


    def html_head(self, title, javascripts = [], stylesheets = ["pages"], force=False):
        if not self.header_sent or force:
            self.write('<!DOCTYPE HTML>\n'
                       '<html><head>\n')

            self.default_html_headers()
            self.write('<title>')
            self.write(self.attrencode(title))
            #self.guitest_record_output("page_title", title)
            self.write('</title>\n')

            # If the variable _link_target is set, then all links in this page
            # should be targetted to the HTML frame named by _link_target. This
            # is e.g. useful in the dash-board
            if self.link_target:
                self.write('<base target="%s">\n' % self.attrencode(self.link_target))

            # Load all specified style sheets and all user style sheets in htdocs/css
            for css in self._default_stylesheets + stylesheets + [ 'ie' ]:
                fname = self.css_filename_for_browser(css)
                if fname == None:
                    continue

                if css == 'ie':
                    self.write('<!--[if IE]>\n')
                self.write('<link rel="stylesheet" type="text/css" href="%s" />\n' % fname)
                if css == 'ie':
                    self.write('<![endif]-->\n')

            self.add_custom_style_sheet()

            for js in self._default_javascripts + javascripts:
                filename_for_browser = self.javascript_filename_for_browser(js)
                if filename_for_browser:
                    self.javascript_file(filename_for_browser)

            if self.browser_reload != 0:
                if self.browser_redirect != '':
                    self.javascript('set_reload(%s, \'%s\')' % (self.browser_reload, self.browser_redirect))
                else:
                    self.javascript('set_reload(%s)' % (self.browser_reload))

            self.write("</head>\n")
            self.header_sent = True


#
# A Class which can be used to simulate HTML generation in varios tests in tests/web/
class HTMLTester(object):

    def __init__(self):
        super(HTMLTester, self).__init__()
        self.plugged_text = ''


    def write(self, text):
        self.plugged_text += "%s" % text


    def plug(self):
        self.plugged_text = ''


    def drain(self):
            t = self.plugged_text
            self.plugged_text = ''
            return t


    def add_custom_style_sheet(self):
        #raise NotImplementedError()
        pass


    def css_filename_for_browser(self, css):
        #raise NotImplementedError()
        return "file/name/css_%s" % css


    def javascript_filename_for_browser(self, jsname):
        #raise NotImplementedError()
        return "file/name/js_%s" % jsname 


    def top_heading(self, title):
        self.top_heading_left(title)
        self.top_heading_right()


    def detect_icon_path(self, icon_name):
        return "test/%s.jpg" % icon_name


    def url_prefix(self):
        return "OMD"


    def get_button_counts(self):
        return {"1": 1, "2": 2, "3": 3,}


class GeneratorTester(HTMLTester, HTMLGenerator):
    def __init__(self):
        super(GeneratorTester, self).__init__()
        HTMLTester.__init__(self)
        HTMLGenerator.__init__(self)


class HTMLCheck_MKTester(HTMLTester, html):
    pass
#    def __init__(self):
#        super(HTMLCheck_MKTester, self).__init__()


class HTMLOrigTester(HTMLTester, DeprecatedRenderer):
    pass






