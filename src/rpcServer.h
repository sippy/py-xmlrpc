/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 *
 * An object which processes xmlrpc requests
 */


#ifndef _RPCSERVER_H_
#define _RPCSERVER_H_


#include "rpcInclude.h"
#include "rpcSource.h"
#include "rpcDispatch.h"


extern	PyTypeObject	rpcServerType;



/*
 * A new xmlrpc server
 */
typedef struct _server {
	PyObject_HEAD			/* python standard */
	rpcDisp		*disp;
	rpcSource	*src;
	PyObject	*comtab;
	bool		keepAlive;
	PyObject	*authFunc;	/* authentication function */
} rpcServer;


rpcServer	*rpcServerNew(void);
void		rpcServerDealloc(rpcServer *sp);
void		rpcServerClose(rpcServer *sp);
bool		rpcServerAddPyMethods(rpcServer *sp, PyObject *toAdd);
bool		rpcServerAddCMethod(
			rpcServer 	*servp,
			char		*method,
			PyObject	*(*func) (
				rpcServer *,
				rpcSource *,
				char *,
				char *,
				PyObject *,
				PyObject *
			)
		);
bool		rpcServerBindAndListen(
			rpcServer	*sp,
			int		port,
			int 		queue
		);
void		rpcServerSetAuth(rpcServer *sp, PyObject *authFunc);


#endif /* _RPCSERVER_H_ */

