/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 *
 * An object which contains utilities to parse and encode objects in
 * xmlrpc.
 */


#ifndef _RPCUTILS_H_
#define _RPCUTILS_H_


#include "rpcInclude.h"


#define TYPE_REQ	0
#define TYPE_RESP	1


PyObject	*xmlEncode(PyObject *value);
PyObject	*xmlDecode(PyObject *string);
PyObject	*buildRequest(
			char *url,
			char *method,
			PyObject *params,
			PyObject *addInfo
		);
PyObject	*buildFault(
			int errCode,
			char *errStr,
			PyObject *addInfo
		);
PyObject	*buildResponse(PyObject *result, PyObject *addInfo);
PyObject	*parseRequest(PyObject *request);
PyObject	*parseResponse(PyObject *request);
bool		doKeepAlive(PyObject *header, int reqType);
bool		doKeepAliveFromDict(PyObject *addInfo);


#endif /* _RPCUTILS_H_ */
