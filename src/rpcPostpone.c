/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 *
 * This class is raised from one of a server's handler functions to
 * indicate that a server's response should be gracefully delayed until
 * a later date.
 *
 * To finish the response queueResponse() or queueFault() should be called.
 *
 */


#include <assert.h>
#include "xmlrpc.h"
#include "rpcInternal.h"


static PyMethodDef	rpcPostponeMethods[] = {
	{  NULL,      NULL },
};


PyObject *
rpcPostponeClass(void)
{
	PyObject	*klass,
			*dict,
			*func,
			*meth;
	PyMethodDef	*method;

	dict = PyDict_New();
	if (dict == NULL)
		return (NULL);
	klass = PyErr_NewException("xmlrpc.postpone", NULL, dict);
	if (klass == NULL)
		return (NULL);
	for (method = rpcPostponeMethods; method->ml_name != NULL; method++) {
		func = PyCFunction_New(method, NULL);
		if (func == NULL)
			return (NULL);
		meth = PyMethod_New(func, NULL, klass);
		if (meth == NULL)
			return (NULL);
		if (PyDict_SetItemString(dict, method->ml_name, meth))
			return (NULL);
		Py_DECREF(meth);
		Py_DECREF(func);
	}
	return (klass);
}
