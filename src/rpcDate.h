/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 *
 * An object which stores information about a date and time
 */

#ifndef _RPCDATE_H_
#define _RPCDATE_H_


#include "rpcInclude.h"


PyTypeObject	rpcDateType;


/*
 * dateTime object
 */
typedef struct {
	PyObject_HEAD			/* python standard */
	PyObject	*value;		/* 6-tuple: (Y, M, D, H, M, S) */
} rpcDate;


PyObject 	*rpcDateNew(PyObject *tuple);


#endif /* _RPCDATE_H_ */
