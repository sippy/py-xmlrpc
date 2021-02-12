#!/usr/local/bin/python2.0 -O

import sys
import turbo_xmlrpc as xmlrpc

if len(sys.argv) != 3:
	sys.stderr.write('usage: %s host port\n' % sys.argv[0])
	sys.exit(0)

host	= sys.argv[1]
port	= int(sys.argv[2])

c	= xmlrpc.client(host, port)
xmlrpc.setLogLevel(0)

print c.execute('kill', [])
c	= None
