#!/usr/local/bin/python -O
#
import sys

import exControl
import baseServer

usage	= '''
usage: %s [options]
	options:
	-l <log level>	0 to 9				(default 0)
	-p <port>	start a server on port <port>	(default 43434)
''' % (sys.argv[0])


def main():
	conf	= crack(len(sys.argv) - 1, sys.argv[1:])
	cont	= exControl.exControl()
	server	= baseServer.Server(conf['port'], conf['loglv'], cont)
	server.serv()

def crack(argc, argv):
	conf	= {
		'loglv'	: 0,
		'port'	: 43434,
		}
	i	= 0
	while (i < argc):
		if argv[i][0] == '-':
			if   argv[i][1] == 'p':
				try:	conf['port']	= int(argv[i+1])
				except:	baseServer.die(cmdusage)
				i = i + 1
			elif argv[i][1] == 'l':
				try:	conf['loglv']	= int(argv[i+1])
				except:	baseServer.die(usage)
				i = i + 1
			else:
				baseServer.die(usage)
		else:
			baseServer.die(usage)
		i = i + 1
	return conf

if __name__ == '__main__':
	main()
