/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 */


#include "xmlrpc.h"
#include "rpcInternal.h"


static	void		rpcBoolDealloc(rpcBool *bp);
static	int		rpcBoolLength(rpcBool *bp);
static	int		rpcBoolCompare(rpcBool *b1, rpcBool *b2);
static	PyObject	*rpcBoolRepr(rpcBool *bp);


/*
 * create a new edb boolean object
 */
PyObject *
rpcBoolNew(bool value)
{
	rpcBool	*bp;

	bp = PyObject_NEW(rpcBool, &rpcBoolType);
	if (bp == NULL)
		return NULL;
	bp->value = value;
	return (PyObject *)bp;
}


/*
 * get the value (true or false) of a boolean rpc object
 */
bool
rpcBoolValue(PyObject *obj)
{
	return ((rpcBool *)obj)->value;
}


/*
 * free resources associated with a boolean object
 */
static void
rpcBoolDealloc(rpcBool *bp)
{
	PyObject_DEL(bp);
}


/*
 * tell whether a boolean object is true or false
 */
static int
rpcBoolLength(rpcBool *bp)
{
	if (bp->value)
		return 1;
	else
		return 0;
}


/*
 * bool object to dictionary conversion
 */
static PyMappingMethods rpcBoolAsMapping = {
	(inquiry)rpcBoolLength,	/* mapping length */
	(binaryfunc)NULL,		/* mapping subscript */
	(objobjargproc)NULL,		/* mapping associate subscript */
};


/*
 * boolean comparison
 */
static int
rpcBoolCompare(rpcBool *b1, rpcBool *b2)
{
	if (not b1->value and not b2->value)
		return 0;
	else if (b1->value and b2->value)
		return 0;
	else
		return 1;
}


/*
 * represent a boolean xml object
 */
static PyObject *
rpcBoolStr(rpcBool *bp)
{
	if (bp->value)
		return PyString_FromString("<xmlrpc boolean true>");
	else
		return PyString_FromString("<xmlrpc boolean false>");
}


/*
 * represent a boolean xml object
 */
static PyObject *
rpcBoolRepr(rpcBool *bp)
{
	if (bp->value)
		return PyString_FromString("boolean(1)");
	else
		return PyString_FromString("boolean(0)");
}


/*
 * map characterstics of a boolean
 */
PyTypeObject rpcBoolType = {
	PyObject_HEAD_INIT(0)
	0,
	"rpcBoolean",
	sizeof(rpcBool),
	0,
	(destructor)rpcBoolDealloc,		/* tp_dealloc */
	0,					/* tp_print */
	0,					/* tp_getattr */
	0,					/* tp_setattr */
	(cmpfunc)rpcBoolCompare,		/* tp_compare */
	(reprfunc)rpcBoolRepr,			/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	&rpcBoolAsMapping,			/* tp_as_mapping */
	0,					/* tp_hash */
	0,					/* tp_call */
	(reprfunc)rpcBoolStr,			/* tp_str */
	0,					/* tp_getattro */
	0,					/* tp_setattro */
	0,					/* tp_as_buffer */
	0,					/* tp_xxx4 */
	0,					/* tp_doc */
};
