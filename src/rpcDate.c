/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 */


#include <assert.h>
#include "xmlrpc.h"
#include "rpcInternal.h"


static void		xmlrpcDateDealloc(rpcDate *dp);
static PyObject		*xmlrpcDateRepr(rpcDate *dp);
static PyObject		*xmlrpcDateStr(rpcDate *dp);
PyObject		*xmlrpcDateGetAttr(rpcDate *dp, char *name);
static	int		xmlrpcDateSetAttr(rpcDate *dp, char *name, PyObject *value);
static PyObject		*xmlrpcDateGet(PyObject *self, PyObject *args);


/*
 * map characteristics of the date edb object 
*/
PyTypeObject rpcDateType = {
	PyObject_HEAD_INIT(0)
#if PY_MAJOR_VERSION < 3
	0,
#endif
	.tp_name = "xmlrpcdateTime",
	.tp_basicsize = sizeof(rpcDate),
	.tp_itemsize = 0,
	.tp_dealloc = (destructor)xmlrpcDateDealloc,
	.tp_print = NULL,
	.tp_getattr = (getattrfunc)xmlrpcDateGetAttr,
	.tp_setattr = (setattrfunc)xmlrpcDateSetAttr,
#if PY_MAJOR_VERSION < 3
	.tp_compare = NULL,
#endif
	.tp_repr = (reprfunc)xmlrpcDateRepr,
	.tp_as_number = NULL,
	.tp_as_sequence = NULL,
	.tp_as_mapping = NULL,
	.tp_hash = NULL,
	.tp_call = NULL,
	.tp_str = (reprfunc)xmlrpcDateStr,
	.tp_getattro = NULL,
	.tp_setattro = NULL,
	.tp_as_buffer = NULL,
	.tp_flags = 0,
	.tp_doc = NULL
};

/*
 * create a new edb date object
 * error checking should already be done.
 */

PyObject *
rpcDateNew(PyObject *tuple)
{
	rpcDate		*dp;

	assert(PyTuple_Check(tuple));
	assert(PyObject_Length(tuple) == 6);
	dp = PyObject_NEW(rpcDate, &rpcDateType);
	unless (dp)
		return (NULL);
	dp->value = PyTuple_GetSlice(tuple, 0, 6);
	return (PyObject *)dp;
}


/*
 * free resources associated with a date object
*/

static void
xmlrpcDateDealloc(rpcDate *dp)
{
	if (dp->value) {
		Py_DECREF(dp->value);
	}
	PyObject_DEL(dp);
}


/*
 * convert a date object to a string
 */
static PyObject *
xmlrpcDateStr(rpcDate *dp)
{
	return PyString_FromString("<dateTime.iso8601 object>");
}


/*
 * represent a date object
 */
static PyObject *
xmlrpcDateRepr(rpcDate *dp)
{
	char	buff[256];

	assert(PyTuple_Check(dp->value));
	assert(PyTuple_GET_SIZE(dp->value) == 6);
	snprintf(buff, sizeof(buff)-1, "dateTime(%ld,%ld,%ld,%ld,%ld,%ld)",
		PyInt_AS_LONG(PyTuple_GET_ITEM(dp->value, 0)),
		PyInt_AS_LONG(PyTuple_GET_ITEM(dp->value, 1)),
		PyInt_AS_LONG(PyTuple_GET_ITEM(dp->value, 2)),
		PyInt_AS_LONG(PyTuple_GET_ITEM(dp->value, 3)),
		PyInt_AS_LONG(PyTuple_GET_ITEM(dp->value, 4)),
		PyInt_AS_LONG(PyTuple_GET_ITEM(dp->value, 5)));
	buff[sizeof(buff)-1] = EOS;		/* just to be safe */
	return (PyString_FromString(buff));
}




static PyMethodDef xmlrpcDateMethods[] = {
	{ "date", (PyCFunction)xmlrpcDateGet,		1,	0 },
	{  NULL,  NULL },
};



static PyObject *
xmlrpcDateGet(PyObject *self, PyObject *args)
{
	PyObject		*dp;

	dp = ((rpcDate *)self)->value;
	return PyTuple_GetSlice(dp, 0, PyTuple_Size(dp));
}


PyObject *
xmlrpcDateGetAttr(rpcDate *dp, char *name)
{
	assert(dp != NULL);
	assert(name != NULL);
	if (strcmp("value", name) == 0) {
		Py_INCREF(dp->value);
		return dp->value;
	}
	return (Py_FindMethod(xmlrpcDateMethods, (PyObject *)dp, name));
}
	
static	int
xmlrpcDateSetAttr(rpcDate *dp, char *name, PyObject *value)
{
	assert(dp != NULL);
	assert(name != NULL);
	PyErr_SetString(PyExc_AttributeError, "unknown attribute");
	return -1;
}
