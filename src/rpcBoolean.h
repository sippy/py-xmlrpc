/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 *
 * An object which represents a single logical value
 *
 */


#ifndef _RPCBOOL_H_
#define _RPCBOOL_H_


#include "rpcInclude.h"


extern	PyTypeObject	rpcBoolType;


/*
 * boolean object
 */
typedef struct {
	PyObject_HEAD			/* python standard */
	bool		value;		/* true/false value */
} rpcBool;


PyObject	*rpcBoolNew(bool value);
bool		rpcBoolValue(PyObject *obj);


#endif /* _RPCBOOL_H_ */
