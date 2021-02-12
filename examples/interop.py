#!/usr/bin/python -O
#
# A server which can ALMOST pass the compliancy test at jake.manilasites.com
#
#


import sys
sys.path.append('..')
import turbo_xmlrpc as xmlrpc
import traceback


PORT = 9999


def main():
	METHODS = {
		'interopEchoTests.echoString'		: echoStringTest,
		'interopEchoTests.echoInteger'		: echoIntegerTest,
		'interopEchoTests.echoFloat'		: echoFloatTest,
		'interopEchoTests.echoStruct'		: echoStructTest,
		'interopEchoTests.echoBase64'		: echoBase64Test,
		'interopEchoTests.echoDate'		: echoDateTest,
		'interopEchoTests.echoStringArray'	: echoStringArrayTest,
		'interopEchoTests.echoIntegerArray'	: echoIntegerArrayTest,
		'interopEchoTests.echoFloatArray'	: echoFloatArrayTest,
		'interopEchoTests.echoStructArray'	: echoStructArrayTest,
	}
	s = xmlrpc.server()
	s.addMethods(METHODS)
	s.bindAndListen(PORT)
	while 1:
		try:
			s.work()
		except:
			traceback.print_exc()


def echoStringTest(server, source, name, args):
	return args[0]


def echoIntegerTest(server, source, name, args):
	return args[0]


def echoFloatTest(server, source, name, args):
	return args[0]


def echoBase64Test(server, source, name, args):
	return args[0]


def echoDateTest(server, source, name, args):
	return args[0]


def echoStructTest(server, source, name, args):
	return args[0]


def echoStringArrayTest(server, source, name, args):
	return args[0]


def echoIntegerArrayTest(server, source, name, args):
	return args[0]


def echoFloatArrayTest(server, source, name, args):
	return args[0]


def echoStructArrayTest(server, source, name, args):
	return args[0]



main()
