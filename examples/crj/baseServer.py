#!/usr/bin/python -O
#
# baseServer.py
#	Description:	baseServer is a basic server that has only basic
#			functionality.  To actually use it, you write a Controller
#			class that inherits from baseControl and use it in the
#			server.  When a command comes in for that controller, is
#			properly routed to it.
#
#	Requires:	xmlrpc
#
#       Copyright:      LGPL
#       Created:        March 20, 2001
#       Author:         Chris Jensen - chris@sourcelight.com
#
#	Last Update:	4/25/2001
#
#####################################################################################

import string
import sys

import xmlrpc

USAGE	= '''
usage:
	getclient
		get a list of commands

	kill
		kill the server

	leave
		disconnect from server

	ping
		ping server

	usage
		get the usage string
'''

LOGLEVEL	= 0


## Server object
#
# Server(port, loglevel, *controls)
#	Constructor... port is the tcp/ip port to serv on, loglevel is the verbosity
#	of the logs (0-9 = no logs - extremly verbose), controls are classes that
#	inherit from baseControl and actually provide functionality for the server.
#
# serv()
#	Start handling requests.
#
# _cleanup()
#	Override this if your sub-class needs to so cleanup on exit
#
# If you run the server in an xterm (in the foreground), you can hit q and enter to
# kill the server instead of having to connect and send a kill command.  This does
# NOT work on windows... aparently you cannot select on stdin in python for windows.
#
class Server:
	def __init__(self, port, loglevel, *controls):
		global	LOGLEVEL
		LOGLEVEL	= loglevel	# need to set for the debug function
		self.port	= port		# port to serv on
		xmlrpc.setLogLevel(loglevel)	# set the module logging level
		usage		= USAGE		# usage string
		self.killed	= 0		# true if we've been killed
		self.contrl	= []		# list of our control objects
		self.cmdlist	= {		# our command list
			'getclient'	: self.cgetclient,
			'kill'		: self.ckill,
			'leave'		: self.cleave,
			'ping'		: self.cping,
			'usage'		: self.cusage,
			}
		for cont in controls:
			(self.cmdlist, usage) = cont.link(self.cmdlist, usage)
			self.contrl.append(cont)
		self.usage	= usage
		self.server	= xmlrpc.server()
		self.server.addMethods(self.cmdlist)
		if sys.platform[:5] != 'linux':
			return			# cause this doesn't work on windows
		self.stdin	= xmlrpc.source(sys.stdin.fileno())
		self.stdin.setCallback(self.cstdin, xmlrpc.ACT_INPUT, self.server)
		self.server.addSource(self.stdin)

	def serv(self):
		self.server.bindAndListen(self.port)
		debug(1, 'Listening on port %d' % self.port)
		while 1:
			try:		self.server.work()
			except:		pass
			if self.killed:	break
		self._cleanup()

	# this doesn't do anything, but can be useful if a sub-class needs to
	# clean up on exiting (ex. closing a database)
	def _cleanup(self):
		pass

	## standard command functions
	##

	# get a list of commands
	#
	def cgetclient(self, *args):
		keys	= self.cmdlist.keys()
		keys.sort()
		return keys

	# kill the server remotely
	#
	def ckill(self, *args):
		self.server.exit()
		self.killed	= 1
		return 'killed'

	# leave this session (close connection)
	#
	def cleave(self, *args):
		return 'bye'

	# return pong
	#
	def cping(self, *args):
		return 'pong'

	# return command usage
	#
	def cusage(self, *args):
		return self.usage

	# quit if q entered on stdin... not on windows
	#
	def cstdin(self, *args):
		if sys.stdin.readline()[0] == 'q':
			self.server.exit()
			self.killed	= 1
		return 1


def debug(level, msg):
	if level <= LOGLEVEL:
		sys.stderr.write(string.strip(msg) + '\n')

def die(msg):
	debug(0, msg)
	sys.exit(0)
