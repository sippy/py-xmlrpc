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

setup(	name		= 'py-xmlrpc',
	version		= '0.8.8.4b',
	description	= 'xmlrpc for Python',
	author		= 'Shilad Sen',
	author_email	= 'shilad.sen@sourcelight.com',
	license		= 'GNU Lesser General Public License',
	url		= "http://sourceforge.net/projects/py-xmlrpc/",
	py_modules	= ['xmlrpc', 'pyxmlrpclib'],
	ext_modules	= [Extension(
				'_xmlrpc',
				SRC,
				define_macros=MACROS['define'],
				libraries=LIBS,
				)],
	)
