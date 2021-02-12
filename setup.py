#!/usr/bin/python

from distutils.core import setup, Extension

import os
import sys

if sys.platform == 'win32':
	MACROS	= {
		'define'	: [('MSWINDOWS', None)],
		  }
	LIBS	= ['ws2_32']
else:
	MACROS	= {'define' : []}
	LIBS	= []

# I think that there are some unresolved which force me
# to go in this order...
#
SRC	= ['src/' + x for x in os.listdir('src') if x.endswith('.c')]

setup(	name		= 'py-turbo_xmlrpc',
	version		= '0.9.0b',
	description	= 'Fast XMLRPC implementation for Python',
	author		= 'Shilad Sen',
	author_email	= 'shilad.sen@sourcelight.com',
	license		= 'GNU Lesser General Public License',
	url		= "https://github.com/sippy/py-xmlrpc/",
	py_modules	= ['turbo_xmlrpc', 'pyxmlrpclib'],
	ext_modules	= [Extension(
				'_xmlrpc',
				SRC,
				define_macros=MACROS['define'],
				libraries=LIBS,
				)],
	)
