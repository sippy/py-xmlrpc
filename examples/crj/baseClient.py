#!/usr/bin/python -O
#
# baseClient.py
#	Description:	A basic client for baseserver.  Unless you need to do
#			something special, you should not need to write your
#			own client for your server.  This client automagically
#			knows what commands the server offers and lets the user
#			call them.  It is currently limited to four args for
#			the server functions, so if you need more, either
#			make a class that inherits this one and make a member
#			function there (easy), or hack Client to allow more
#			args.  BTW - this was not my choice... python's scoping
#			sucks with lambdas.
#
#	Requires: 	xmlrpc
#
#
#	Copyright:	LGPL
#	Created:	March 20, 2001
#	Author:		Chris Jensen - chris@sourcelight.com
#
#	Last Update:	04/25/2001
#
#####################################################################################

import xmlrpc

LOG_LEVEL	= 0
ClientError	= 'ClientError'
ERR_CLOSE	= 'Connection is closed.  Call reconnect.'
NONE		= []

# Client
#
# Client(host, port, timeout)
#	Constructor.  host and port are the server to connect to.  timeout is the
#	maximum time you should wait for the server to procedd your request.  A
#	value less than 0 means block until the server responds.
#
# reconnect()
#	Reconnect to the server if the connection is lost.
#
# The rest of the callable functions are provided by the server.  To get a command
# list, call the usage function, which SHOULD return a complete list of functions
# and thier usage.  Obviously, if you write the server, you know what's there.
#
class Client:
	def __init__(self, host, port, timeout=-1.0):
		self.host	= host			# host
		self.port	= port			# server port
		self.timeout	= timeout		# xmlrpc timeout
		self.client	= xmlrpc.client(host, port)
		xmlrpc.setLogLevel(LOG_LEVEL)
		self.cmds	= self.__talk__('getclient')
		if not self.cmds:
			raise ClientError('could not get command list')

	def __getattr__(self, fn):
		if fn not in self.cmds:
			raise AttributeError('%s instance has no attribute %s' % (
						self.__class__, fn))
		return (lambda a1=NONE,a2=NONE,a3=NONE,a4=NONE,x=fn,y=self.__talk__:
				y(x, a1, a2, a3, a4))

###
## special commands.
##
	# kill the server
	#
	def kill(self):
		if not self.client:
			raise ClientError(ERR_CLOSE)
		self.timeout	= 1.0
		try:	self.__talk__('kill')
		except:	pass
		self.client	= None

	# disconnect from the server
	#
	def leave(self):
		self.__talk__('leave')
		self.client	= None

	# reconnect to the server
	#
	def reconnect(self):
		self.client	= xmlrpc.client(self.host, self.port)

###
## helper functions
##
	# do a send and receive with the server
	#
	def __talk__(self, command, *args):
		args	= filter(lambda x,y=NONE: (x is not y), args)
		if not self.client:
			raise ClientError(ERR_CLOSE)
		try:
			return self.client.execute(command, args,
						   timeout=self.timeout)
		except:
			self.client	= xmlrpc.client(self.host, self.port)
			return self.client.execute(command, args,
						   timeout=self.timeout)
