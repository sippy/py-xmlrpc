/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 */


#include <assert.h>
#include "xmlrpc.h"
#include "rpcInternal.h"


static PyObject		*rpcFault__init__(PyObject *self, PyObject *args);
static PyObject		*rpcFault__str__(PyObject *self, PyObject *args);
static PyMethodDef	rpcFaultMethods[] = {
	{"__init__",  rpcFault__init__,  METH_VARARGS},
	{"__repr__",  rpcFault__str__,   METH_VARARGS},
	{"__str__",   rpcFault__str__,   METH_VARARGS},
	{  NULL,      NULL },
};


static PyObject *
rpcFault__init__(PyObject *self, PyObject *args)
{
	PyObject	*faultCode,
			*faultString;

	unless (PyArg_ParseTuple(args, "OOS", &self, &faultCode, &faultString))
		return (NULL);
	unless (PyInt_Check(faultCode))
		return setPyErr("faultCode must be an int");
	if ((PyObject_SetAttrString(self, "faultCode", faultCode))
	or  (PyObject_SetAttrString(self, "faultString", faultString))) {
		return (NULL);
	}
	Py_INCREF(Py_None);
	return (Py_None);
}


static PyObject *
rpcFault__str__(PyObject *self, PyObject *args)
{
	PyObject	*faultCode,
			*faultString,
			*tuple,
			*res;

	unless (PyArg_ParseTuple(args, "O", &self))
		return (NULL);
	faultCode = PyObject_GetAttrString(self, "faultCode");
	faultString = PyObject_GetAttrString(self, "faultString");
	if (faultCode == NULL || faultString == NULL)
		return (NULL);
	tuple = Py_BuildValue("(O,O)", faultCode, faultString);
	Py_DECREF(faultCode);
	Py_DECREF(faultString);
	if (tuple == NULL)
		return (NULL);
	res = PyObject_Str(tuple);
	Py_DECREF(tuple);
	return (res);
}


PyObject *
rpcFaultClass(void)
{
	PyObject	*klass,
			*dict,
			*func,
			*meth;
	PyMethodDef	*method;

	dict = PyDict_New();
	if (dict == NULL)
		return (NULL);
	klass = PyErr_NewException("xmlrpc.fault", NULL, dict);
	if (klass == NULL)
		return (NULL);
	for (method = rpcFaultMethods; method->ml_name != NULL; method++) {
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


bool
rpcFault_Extract(PyObject *fault, int *faultCode, char **faultString)
{
	PyObject	*pyFaultCode,
			*pyFaultString;

	assert(PyErr_GivenExceptionMatches(fault, rpcFault));
	pyFaultCode = PyObject_GetAttrString(fault, "faultCode");
	if (faultCode && PyInt_Check(pyFaultCode))
		*faultCode = (int)PyInt_AS_LONG(pyFaultCode);
	else {
		fprintf(rpcLogger, "invalid fault code... default to -1\n");
		*faultCode = -1;
	}
	pyFaultString = PyObject_GetAttrString(fault, "faultString");
	if (faultString && PyString_Check(pyFaultString)) {
		*faultString = alloc(PyString_GET_SIZE(pyFaultString) + 1);
		if (*faultString == NULL)
			return false;
		strcpy(*faultString, PyString_AS_STRING(pyFaultString));
	} else {
		fprintf(rpcLogger, "invalid fault string... default to 'unknown error'\n");
		*faultString = alloc(strlen("unknown error") + 1);
		if (*faultString == NULL)
			return false;
		strcpy(*faultString, "unknown error");
	}
	return true;
}


void
rpcFaultRaise_C(int faultCode, char *faultString)
{
	PyObject	*pyFaultCode,
			*pyFaultString;

	pyFaultCode = PyInt_FromLong((long)faultCode);
	pyFaultString = PyString_FromString(faultString);
	rpcFaultRaise(pyFaultCode, pyFaultString);
	Py_DECREF(pyFaultCode);
	Py_DECREF(pyFaultString);
}


void
rpcFaultRaise(PyObject *errCode, PyObject *errString)
{
	PyObject	*tuple;

	assert(PyInt_Check(errCode));
	assert(PyString_Check(errString));
	tuple = Py_BuildValue("(O,O)", errCode, errString);
	if (tuple == NULL)
		fprintf(rpcLogger, "Py_BuildValue failed in rpcFaultRaise");
	PyErr_SetObject(rpcFault, tuple);
	Py_DECREF(tuple);
}
