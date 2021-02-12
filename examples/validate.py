#!/usr/bin/python -O
#
# A server which CAN pass the compliancy test at validator.xmlrpc.com
#
#


import sys
sys.path.append('..')
import turbo_xmlrpc as xmlrpc
import traceback


PORT = 23456


def main():
	METHODS = {
		'validator1.arrayOfStructsTest'		: arrayOfStructsTest,
		'validator1.countTheEntities'		: countTheEntities,
		'validator1.easyStructTest'		: easyStructTest,
		'validator1.echoStructTest'		: echoStructTest,
		'validator1.manyTypesTest'		: manyTypesTest,
		'validator1.moderateSizeArrayCheck'	: moderateSizeArrayCheck,
		'validator1.simpleStructReturnTest'	: simpleStructReturnTest,
		'validator1.nestedStructTest'		: nestedStructTest
	}
	s = xmlrpc.server()
	s.addMethods(METHODS)
	s.bindAndListen(PORT)
	while 1:
		try:
			s.work()
		except Exception not in (KeyboardInterrupt, SystemExit):
			traceback.print_exc()


def arrayOfStructsTest(server, source, name, args):
	s = 0
	for dict in args[0]:
		s = s + dict['curly']
	return s


def easyStructTest(server, source, name, args):
	dict = args[0]
	s = 0
	for key in ('moe', 'larry', 'curly'):
		s = s + dict[key]
	return s


def echoStructTest(server, source, name, args):
	return args[0]


def countTheEntities(server, source, name, args):
	specialChars = {
		'>'	: 'ctRightAngleBrackets',
		'<'	: 'ctLeftAngleBrackets',
		'&'	: 'ctAmpersands',
		'\''	: 'ctApostrophes',
		'"'	: 'ctQuotes'
	}
	sums = {}
	for k in specialChars.values():
		sums[k] = 0
	s = args[0]
	for c in s:
		if specialChars.has_key(c):
			tag = specialChars[c]
			sums[tag] = sums[tag] + 1
	return sums

def manyTypesTest(server, source, name, args):
	return args


def moderateSizeArrayCheck(server, source, name, args):
	array = args[0]
	return array[0] + array[-1]


def nestedStructTest(server, source, name, args):
	dict = args[0]['2000']['04']['01']
	return dict['moe'] + dict['curly'] + dict['larry']


def simpleStructReturnTest(server, source, name, args):
	num = args[0]
	return {
		'times10'	: num*10,
		'times100'	: num*100,
		'times1000'	: num*1000
	}


main()
