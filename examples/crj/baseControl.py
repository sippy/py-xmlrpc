#!/usr/bin/python -O
#
# baseControl.py
#	Description:	Contains the most basic implementation of a control class
#			for my standard server architecture.  To use, make a class
#			the using baseControl as a base class.  Then add your
#			functions there.  Be sure to use a '_' as the first
#			character in the names of functions you do NOT want to be
#			externally callable.  The list of externally callable
#			functions is generated automagically.
#			A small example is given.  
#			Be carefule of one thing, None is not in the xmlrpc spec,
#			so xmlrpc does not support the None type. 
#
#	Requires:	xmlrpc
#
#
#       Copyright:      LGPL
#       Created:        March 20, 2001
#       Author:         Chris Jensen - chris@sourcelight.com
#
#	Last Update:	04/10/2001
#
#####################################################################################

import string
import sys

# baseControl
#
# baseControl(usage, logobj=sys.stderr)
#	Constructor.  usage is the usage string for your subclass. logobj is
#	an object that has a write method (ex. sys.stderr (default), file obj, etc.)
#
# dispatch(*args)
#	This only should be called by the xmlrpc stuff.  It is called when a command
#	comes into the xmlrpc server.
#
# link(cmdlist, cmduse)
#	This only needs to be called be the baseServer object on initialization.
#
# Nothing else is really public, except for the functions in a sub-class, which
# are called through xmlrpc.
#
class baseControl:
	def __init__(self, usage, logobj=None):
		self.closed	= 0
		self.usage	= usage				# command usage
		self.logobj	= (logobj or sys.stderr)	# log to logobj if
								# given, else stderr
		self.__loadmethods__()

	def __del__(self):
		if not self.closed:
			self.close()

	def __getattr__(self, name):
		if name == 'dispatch':
			return self._dispatch
		if name == 'link':
			return self._link
		raise AttributeError, "'%s' instance has no attribute '%s'" % (
							self.__class__, name)

	def _close(self):
		self.closed	= 1
		return 1

	def _dispatch(self, *args):
		(cmd, argv)	= (args[3], args[4])
		if self.closed:
			raise 'Access Error', 'Control object has been closed.'
		return apply(self.fnmap.get(cmd), tuple(argv))

	def _link(self, cmdlist, cmduse):
		cmduse	= cmduse + self.usage
		for i in self.fnmap.keys():
			cmdlist[i]	= self.dispatch
		return (cmdlist, cmduse)

	def _log(self, msg):
		self.logobj.write('%s\n' % string.strip(msg))

	def __loadmethods__(self):
		self.fnmap	= {}
		for i in dir(self.__class__) + dir(self.__class__.__bases__[0]):
			if ((i[0] == '_')
			or  (not callable(getattr(self, i)))):
				continue
			self.fnmap[i]	= getattr(self, i)


exusage	= '''
	echo	arg
		returns arg as a string
	echo2	arg1 arg2
		returns (arg1, arg2) as a string
'''

# example Control class
#
class exControl(baseControl):
	def __init__(self, logobj=None):
		baseControl.__init__(self, exusage, logobj)

	def echo(self, arg):
		return str(arg)

	def echo2(self, arg1, arg2):
		return str((arg1, arg2))
