###############################################################################
# Name:         misc/gdb/print.py
# Purpose:      pretty-printers for wx data structures: this file is meant to
#               be sourced from gdb using "source -p" (or, better, autoloaded
#               in the future...)
# Author:       Vadim Zeitlin, modified for Code::Blocks IDE plugin support
# Created:      2009-01-04
# Copyright:    (c) 2009 Vadim Zeitlin
# Licence:      wxWindows licence
###############################################################################
# version printv4.py with Gemini wxArrayString support (ph 26/06/15)

# Define wxFooPrinter class implementing (at least) to_string() method for each
# wxFoo class we want to pretty print. Then just add wxFoo to the types array
# in wxLookupFunction at the bottom of this file.

import datetime
import gdb
import itertools
import sys

if sys.version_info[0] > 2:
    # Python 3
    Iterator = object
    long = int
else:
    # Python 2, we need to make an adaptor, so we can use Python 3 iterator implementations.
    class Iterator:
        def next(self):
            return self.__next__()

# Pretty-printer for wxString
class wxStringPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return self.val['m_impl']['_M_dataplus']['_M_p']

    def display_hint(self):
        return 'string'

# Pretty-printer for the explicit, stable wxArrayString class
class wxArrayStringPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return f"wxArrayString of length {self.val['m_nCount']}, capacity {self.val['m_nSize']}"

    def children(self):
        count = int(self.val['m_nCount'])
        items = self.val['m_pItems']
        for i in range(count):
            yield (f'[{i}]', items[i])

    def display_hint(self):
        return 'array'

# Pretty-printer for wxDateTime
class wxDateTimePrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        msec = self.val['m_time'].cast(gdb.lookup_type('long long'))
        if msec == 0x8000000000000000:
            return 'NONE'
        sec = int(msec / 1000)
        return datetime.datetime.fromtimestamp(sec).isoformat(' ')

# Pretty-printer for wxFileName
class wxFileNamePrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return gdb.parse_and_eval('((wxFileName*)%s)->GetFullPath(0)' %
                                  self.val.address)

# Base geometry printer
class wxXYPrinterBase:
    def __init__(self, val):
        self.x = val['x']
        self.y = val['y']

class wxPointPrinter(wxXYPrinterBase):
    def to_string(self):
        return '(%d, %d)' % (self.x, self.y)

class wxSizePrinter(wxXYPrinterBase):
    def to_string(self):
        return '%d*%d' % (self.x, self.y)

class wxRectPrinter(wxXYPrinterBase):
    def __init__(self, val):
        wxXYPrinterBase.__init__(self, val)
        self.width = val['width']
        self.height = val['height']

    def to_string(self):
        return '(%d, %d) %d*%d' % (self.x, self.y, self.width, self.height)


# Robust lookup function targeting ONLY explicit, non-macro core classes
def wxLookupFunction(val):
    type_str = str(val.type.unqualified().strip_typedefs())

    types = ['wxString',
             'wxArrayString',
             'wxDateTime',
             'wxFileName',
             'wxPoint',
             'wxSize',
             'wxRect']

    for t in types:
        if t in type_str:
            return globals()[t + 'Printer'](val)

    return None

gdb.pretty_printers.append(wxLookupFunction)
