/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 *
 * An xmlrpc client which can make rpc calls to a remote xmlrpc server
 */

#ifndef _RPCCLIENT_H_
#define _RPCCLIENT_H_


#include "rpcInclude.h"
#include "rpcSource.h"
#include "rpcDispatch.h"
#include "rpcServer.h"


extern	PyTypeObject	rpcClientType;



/*
 * A new xmlrpc client
 */
typedef struct {
	PyObject_HEAD			/* python standard */
	char		*host;
	char		*url;
	int		port;
	rpcDisp		*disp;
	rpcSource	*src;
	bool		execing;
} rpcClient;


rpcClient	*rpcClientNew(char *host, int port, char *url);
rpcClient	*rpcClientNewFromServer(
			char		*host,
			int		port,
			char		*url,
			rpcServer	*servp
		);
void		rpcClientDealloc(rpcClient *cp);
bool		rpcClientNbExecute(
			rpcClient	*cp,
			char		*method,
			PyObject	*params,
			bool		(*func) (
				rpcClient *,
				PyObject *,
				PyObject *
			),
			PyObject	*args,
			char		*name,
			char		*pass
		);
PyObject	*rpcClientExecute(
			rpcClient	*cp,
			char		*method,
			PyObject	*params,
			double		timeout,
			char		*name,
			char		*pass
		);
void		rpcClientClose(rpcClient *cp);


#endif /* _RPCCLIENT_H_ */
