/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 *
 * An object which contains binary information which can be encoded
 * and decoded using Base64 encoding.
 */

#ifndef _RPCBASE64_H_
#define _RPCBASE64_H_


#include "rpcInclude.h"


PyTypeObject	rpcBase64Type;


/*
 * binary object
 */
typedef struct {
	PyObject_HEAD			/* python standard */
	PyObject	*value;		/* binary string of data */
} rpcBase64;


PyObject 	*rpcBase64New(PyObject *self);
char		*rpcBase64Encode(PyObject *self);
PyObject	*rpcBase64Decode(PyObject *str);


#endif /* _RPCBASE64_H_ */
