#!/usr/local/bin/python2.0 -O
#
# an example showing the usage of baseClient to talk to the server

import sys
import baseClient


if len(sys.argv) != 3:
	sys.stderr.write('usage: %s host port\n' % sys.argv[0])
	sys.exit(0)

client = baseClient.Client(sys.argv[1], int(sys.argv[2]))

print 'testing usage:'
print client.usage()
print
print 'testing ping:'
print client.ping()
print
print 'testing echo:'
print client.echo(['this is', ['a test']])
print
print 'testing echo2:'
print client.echo2(312.4234, {'test' : [312, 4234]})
print
print 'testing ident:'
print client.ident({'test' : [312, 4234]})
