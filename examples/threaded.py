#!/usr/bin/python -O
#
# The follow is an example for using threads with py-turbo_xmlrpc.
#
# The normalPing method should take about 1/10 of a second per processed
# client request, with a linear degredation relative to the number of
# simultaneous clients.
#
# The threadPing method should take 1/10 of a second per client request,
# but the client request time should stay constant regardless of the
# number of simultaneous clients.
#
# I'm not very adept at threading, so I hope I didn't screw anything up.
#
#


import sys

sys.path.insert(0, '../')

import time
import thread
import traceback
import turbo_xmlrpc as xmlrpc


xmlrpc.setLogLevel(0)


PORT = 9998
TIME_WORK = 0.001
TIME_SLEEP = 0.1


def main():
	global responses
	global responseLock

	responses = []
	responseLock = thread.allocate_lock()

	server = xmlrpc.server()
	server.bindAndListen(PORT)
	server.addMethods({
		'threadPing' : threadPingMethod,
		'normalPing' : normalPingMethod
	})

	while 1:
		try:
			server.work(TIME_WORK)
			addResponses()
		except:
			e = sys.exc_info()
			if e[0] in (KeyboardInterrupt, SystemExit):
				raise e[0], e[1], e[2]
			traceback.print_exc()

# Process any responses in the response queue
#
def addResponses():
	global responses
	global responseLock

	while responses:
		try:
			responseLock.acquire()
			(serv, src, res) = responses[0]
			del(responses[0])
		finally:
			responseLock.release()
		serv.queueResponse(src, res)


# A normal, serial ping method.
#
def normalPingMethod(serv, src, uri, method, params):
	time.sleep(TIME_SLEEP)
	return params


# A parallel, threaded ping method
#
def threadPingMethod(serv, src, uri, method, params):
	thread.start_new_thread(doPingMethod,
	                        (serv, src, method, params))
	raise xmlrpc.postpone


# The actual ping method the thread executes
#
def doPingMethod(serv, src, method, params):
	time.sleep(TIME_SLEEP)
	try:
		responseLock.acquire()
		responses.append((serv, src, params))
	finally:
		responseLock.release()
	thread.exit()


if __name__ == '__main__':
	main()
