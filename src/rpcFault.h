/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 *
 * An object which stores information about a date and time
 */

#ifndef _RPCFAULT_H_
#define _RPCFAULT_H_


#include "rpcInclude.h"


extern	PyObject	*rpcFault;


PyObject 	*rpcFaultClass(void);
bool		rpcFault_Extract(
			PyObject	*fault,
			int		*faultCode,
			char		**faultString
		);
void		rpcFaultRaise(PyObject *faultCode, PyObject *faultString);
void		rpcFaultRaise_C(int faultCode, char *faultString);


#endif /* _RPCFAULT_H_ */
