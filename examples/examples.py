#!/usr/bin/python -O
#
# Examples for the xmlrpc library
#
#


import sys
sys.path.append('../')
import xmlrpc
import traceback
import select
import string


PORT		= 23456
TIMEOUT		= 1.0
LOGLEVEL	= 3		# this is the default log level
TEST_NAME	= 'shilad'
TEST_PASS	= 'shilad'


def main():
	global TESTMAP

	TESTMAP	= {
		'authClient'	: exampleAuthClient,
		'activeFds'	: exampleActiveFds,
		'base64'	: exampleBase64,
		'build'		: exampleBuild,
		'callback'	: exampleCallback,
		'client'	: exampleClient,
		'date'		: exampleDate,
		'encode'	: exampleEncode,
		'fault'		: exampleFault,
		'error'		: exampleError,
		'nbClient'	: exampleNbClient,
		'server'	: exampleServer
	}

	xmlrpc.setLogLevel(LOGLEVEL)
	if len(sys.argv) <> 2:
		sys.stderr.write('Exactly one example must be specified\n')
		usage()
	example = sys.argv[1]
	if not TESTMAP.has_key(example):
		sys.stderr.write('Unknown example: %s\n' % example)
		usage()

	TESTMAP[example]()


def usage():
	global TESTMAP

	validKeys = map(lambda s: ('[%s]' % s), TESTMAP.keys())
	validKeys = string.join(validKeys)
	sys.stderr.write('usage: examples.py %s\n' % validKeys)

	sys.exit(1)


def exampleClient():
	c = xmlrpc.client('localhost', PORT, '/blah')
	print c.execute('echo', ['Hello, World!'])


# shows how to explicitly catch a xmlrpc fault raised by the server
#
def exampleFault():
	try:
		c = xmlrpc.client('localhost', PORT, '/blah')
		print c.execute('fault', ['Hello, World!'])
	except xmlrpc.fault:
		f = sys.exc_value
		sys.stderr.write('xmlrpc fault occurred:\n')
		sys.stderr.write('faultCode is: %s\n' % f.faultCode)
		sys.stderr.write('faultString is: %s\n' % f.faultString)
		traceback.print_exc()
		sys.exit(1)


def exampleAuthClient():
	c = xmlrpc.client('localhost', PORT, '/blah')
	print c.execute('echo', ['Hello, World!'], 1.0, TEST_NAME, TEST_PASS)


# you may uncomment the 'setAuth()' line to use the example
# authentication function 'authenticate()' supplied below
#
def exampleServer():
	global exitFlag

	exitFlag = 0
	s = xmlrpc.server()
#	s.setAuth(authenticate)
	s.addMethods({
		'echo' : echoMethod,
		'exit' : exitMethod,
		'fault' : faultMethod
	})
	s.bindAndListen(PORT)
	while 1:
		try:
			s.work()	# you could set a timeout if desired
		except:
			e = sys.exc_info()
			if e[0] in (KeyboardInterrupt, SystemExit):
				raise e[0], e[1], e[2]
			traceback.print_exc()
		if exitFlag:
			break


def authenticate(uri, name, password):
	if name == TEST_NAME and password == TEST_PASS:
		return (1, 'a domain')
	else:
		return (0, 'a domain')



# there are no registered methods, so any method should raise an "unknown
# command" error.  This will be caught in errorHandler, and the work()
# operation will continue without an error being raised.
#
def exampleError():
	s = xmlrpc.server()
	s.bindAndListen(PORT)
	s.setOnErr(errorHandler)
	while 1:
		try:
			s.work()	# you could set a timeout if desired
		except:
			e = sys.exc_info()
			if e[0] in (KeyboardInterrupt, SystemExit):
				raise e[0], e[1], e[2]
			traceback.print_exc()

def errorHandler(src, exc):
	sys.stderr.write('source %s got error: ' % `src`)
	traceback.print_exception(exc[0], exc[1], exc[2])

	return xmlrpc.ONERR_KEEP_WORK

	# if you wanted to raise an error using the default error handler
	# return xmlrpc.ONERR_KEEP_WORK | xmlrpc.ONERR_KEEP_DEF


def faultMethod(servp, srcp, uri, method, params):
	raise xmlrpc.fault, (23, 'blah')


def echoMethod(servp, srcp, uri, method, params):
	print 'params are', params
	return params


def exitMethod(servp, srcp, uri, method, params):
	global exitFlag
	exitFlag = 1
	servp.exit()
	return 'okay'


def exampleActiveFds():
	i = 0
	c = xmlrpc.client('localhost', PORT, '/blah')
	c.nbExecute('echo', ['hello', 'world'], activeFdCall, i)
	c.work(0.0)
	while 1:
		(inFd, outFd, excFd) = c.activeFds()
		select.select(inFd, outFd, excFd)
		c.work(0.0)


def activeFdCall(client, response, i):
	print 'result is', xmlrpc.parseResponse(response)
	client.nbExecute('echo', ['hello', 'world'], activeFdCall, i + 1)
	client.work(0.0)
	if i == 1000:
		sys.exit(0)


def errHandler(*l):
	print 'in err handler', `l`
	return


def exampleNbClient():
	c = xmlrpc.client('localhost', PORT)
	r = c.nbExecute('echo', ['hello', 'world'], callback, 'Extra Args')
	c.work(10)


def callback(client, response, extArgs=None):
	print 'callback got response', `response`
	print 'result is', xmlrpc.parseResponse(response)


def exampleCallback():
	src = xmlrpc.source(sys.stdin.fileno())
	print dir(src)
	serv = xmlrpc.server()
	serv.bindAndListen(2343)
	src.setCallback(stdinHandler, xmlrpc.ACT_INPUT, serv)
	serv.addSource(src)
	serv.work()


def stdinHandler(src, action, serv):
	l = sys.stdin.readline()
	print 'read', `l`, 'from stdin'
	if l == 'exit\n':
		serv.exit()
		return 0		# remove callback
	else:
		return 1		# reinstall callback


def exampleBuild():
	request = xmlrpc.buildRequest('/uri', 'method', ['Hello, World!'], {})
	print 'request is:', `request`
	print
	print 'parsed request is',  xmlrpc.parseRequest(request)
	print

	response = xmlrpc.buildResponse(['Hello, World!'], {})
	print 'response is:', `response`
	print
	print 'parsed response is', xmlrpc.parseResponse(response)
	print


def exampleEncode():
	r = ['hum', 3242, 'de']
	print 'object is', r
	print 'encoded is:\n', xmlrpc.encode(r)
	print 'decoded is:', xmlrpc.decode(xmlrpc.encode(r))


def exampleDate():
	d = xmlrpc.dateTime(1999, 6, 12, 4, 32, 34)
	print 'date is', d
	print 'encoded date is', xmlrpc.encode(d)
	print 'decoded date is', xmlrpc.decode(xmlrpc.encode(d))


def exampleBase64():
	b = xmlrpc.base64('Hello, world!')
	print 'base64 is', b
	print 'encoded base64 is', xmlrpc.encode(b)
	print 'decoded base64 is', xmlrpc.decode(xmlrpc.encode(b))


if __name__ == '__main__':
	main()
