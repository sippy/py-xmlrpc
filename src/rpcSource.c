/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 */


#include <assert.h>
#include <string.h>
#include "xmlrpc.h"
#include "rpcInternal.h"

#ifndef MSWINDOWS
	#include <unistd.h>
#endif /* MSWINDOWS */


static	PyObject	*rpcSourceGetAttr(rpcSource *cp, char *name);
static	bool		pyMarshaller(
				rpcDisp		*dp,
				rpcSource	*srcp,
				int		actions,
				PyObject	*params
			);


rpcSource *
rpcSourceNew(int fd)
{
	rpcSource	*sp;

	sp = PyObject_NEW(rpcSource, &rpcSourceType);
	if (sp == NULL)
		return NULL;
	sp->fd = fd;
	sp->id = -1;
	sp->actOcc = 0;
	sp->actImp = 0;
	sp->desc = NULL;
	sp->func = NULL;
	sp->params = NULL;
	sp->onErrType = ONERR_TYPE_DEF;
	sp->onErr = NULL;
	sp->doClose = false;

	return sp;
}


void
rpcSourceDealloc(rpcSource *srcp)
{
	if (srcp->doClose)
		close(srcp->fd);
	if (srcp->desc) {
		free(srcp->desc);
		srcp->desc = NULL;
	}
	if (srcp->params) {
		Py_DECREF(srcp->params);
	}
	if (srcp->onErr and srcp->onErrType == ONERR_TYPE_PY) {
		Py_DECREF((PyObject *)srcp->onErr);
	}
	PyMem_DEL(srcp);
}


void
rpcSourceSetParams(rpcSource *sp, PyObject *params)
{
	if (sp->params) {
		Py_DECREF(params);
	}
	sp->params = params;
	Py_INCREF(params);
}


void
rpcSourceSetOnErr(rpcSource *srcp, int funcType, void *func)
{
	if (srcp->onErrType == ONERR_TYPE_PY and srcp->onErr != NULL) {
		Py_DECREF((PyObject *)srcp->onErr);
	}
	switch (funcType) {
	case ONERR_TYPE_DEF:
		srcp->onErr = NULL;
		break;
	case ONERR_TYPE_C:
		srcp->onErr = func;
		break;
	case ONERR_TYPE_PY:
		srcp->onErr = func;
		Py_INCREF((PyObject *)func);
		break;
	}
	srcp->onErrType = funcType;
}


/*
 * Set a handler for errors on the client
 */
static PyObject *
pyRpcSourceSetOnErr(PyObject *self, PyObject *args)
{
	PyObject	*func;
	rpcSource	*srcp;

	srcp = (rpcSource *)self;
	unless (PyArg_ParseTuple(args, "O", &func))
		return NULL;
	unless (PyCallable_Check(func)) {
		PyErr_SetString(rpcError, "error handler must be callable");
		return NULL;
	}
	if (PyObject_Compare(func, Py_None))
		rpcSourceSetOnErr(srcp, ONERR_TYPE_PY, func);
	else
		rpcSourceSetOnErr(srcp, ONERR_TYPE_DEF, NULL);

	Py_INCREF(Py_None);
	return Py_None;
}


/*
 * Get the description associated with the source
 */
static PyObject *
pyRpcSourceGetDesc(PyObject *self, PyObject *args)
{
	rpcSource	*srcp;

	srcp = (rpcSource *)self;
	unless (PyArg_ParseTuple(args, ""))
		return NULL;

	if (srcp->desc)
		return PyString_FromString(srcp->desc);
	else {
		char	buff[100];
		sprintf(buff, "fd %d", srcp->fd);
		return PyString_FromString(buff);
	}
	return PyInt_FromLong((long)srcp->fd);
}


/*
 * Get the description associated with the source
 */
static PyObject *
pyRpcSourceSetDesc(PyObject *self, PyObject *args)
{
	rpcSource	*srcp;
	char		*desc;

	srcp = (rpcSource *)self;
	unless (PyArg_ParseTuple(args, "s", &desc))
		return NULL;

	if (srcp->desc)
		free(srcp->desc);
	srcp->desc = alloc(strlen(desc) + 1);
	if (srcp->desc == NULL)
		return NULL;
	strcpy(srcp->desc, desc);

	Py_INCREF(Py_None);
	return Py_None;
}


/*
 * Set the callback for a specific object
 */
static PyObject *
pySetCallback(PyObject *self, PyObject *args)
{
	rpcSource	*srcp;
	int		actions;
	PyObject	*func,
			*params,
			*realParams;

	srcp = (rpcSource *)self;
	unless (PyArg_ParseTuple(args, "OiO", &func, &actions, &params))
		return NULL;
	unless (PyCallable_Check(func))
		return setPyErr("Callback must be a callable object");
	realParams = Py_BuildValue("(O,O)", func, params);
	if (realParams == NULL)
		return NULL;
	srcp->actImp = actions;
	srcp->func = pyMarshaller;
	srcp->params = realParams;

	Py_INCREF(Py_None);
	return Py_None;
}


/*
 * Marshall a python callback
 */
static bool
pyMarshaller(rpcDisp *dp, rpcSource *srcp, int acts, PyObject *params)
{
	PyObject	*res,
			*pyFunc,
			*realArgs;

	assert(PyTuple_Check(params));
	assert(PyTuple_GET_SIZE(params) == 2);
	pyFunc = PyTuple_GET_ITEM(params, 0);
	realArgs = PyTuple_GET_ITEM(params, 1);
	assert(PyCallable_Check(pyFunc));

	res = PyObject_CallFunction(pyFunc, "(O,i,O)", srcp, acts, realArgs);
	if (res == NULL)
		return false;
	unless (PyInt_Check(res)) {
		fprintf(stderr, "callback returned ");
		PyObject_Print(res, stderr, 0);
		fprintf(stderr, "; removing handler\n");
	} else if (PyInt_AsLong(res)) {
		srcp->params = params;
		srcp->actImp = acts;
		srcp->func = pyMarshaller;
		Py_INCREF(params);
		unless (rpcDispAddSource(dp, srcp))
			return false;
	}
	return true;
}


/*
 * member functions for source object
 */
static PyMethodDef rpcSourceMethods[] = {
	{ "setOnErr",	(PyCFunction)pyRpcSourceSetOnErr,	1,	0 },
	{ "getDesc",	(PyCFunction)pyRpcSourceGetDesc,	1,	0 },
	{ "setDesc",	(PyCFunction)pyRpcSourceSetDesc,	1,	0 },
	{ "setCallback",(PyCFunction)pySetCallback,		1,	0 },
	{ NULL,		NULL},
};


/*
 * return an attribute for a source object
 */
static PyObject *
rpcSourceGetAttr(rpcSource *cp, char *name)
{
	return Py_FindMethod(rpcSourceMethods, (PyObject *)cp, name);
}


/*
 * represent an xmlrpc source object
 */
static PyObject *
rpcSourceRepr(rpcSource *srcp)
{
	if (srcp->desc) {
		PyObject	*pystr;
		char		*buff;
		int		dlen;

		dlen = strlen(srcp->desc);
		buff = alloc(dlen + 100 + dlen);
		if (buff == NULL)
			return NULL;
		sprintf(buff, "<xmlrpc Source object, fd %d, %s at %p>",
			srcp->fd, srcp->desc, srcp);
		pystr = PyString_FromString(buff);
		free(buff);
		return pystr;
	} else {
		char		buff[100];

		sprintf(buff, "<xmlrpc Source object, fd %d, at %p>",
			srcp->fd, srcp);
		return PyString_FromString(buff);
	}
}


/*
 * map characterstics of an edb object
 */
PyTypeObject rpcSourceType = {
	PyObject_HEAD_INIT(0)
	0,
	"rpcSource",
	sizeof(rpcSource),
	0,
	(destructor)rpcSourceDealloc,		/* tp_dealloc */
	0,					/* tp_print */
	(getattrfunc)rpcSourceGetAttr,		/* tp_getattr */
	0,					/* tp_setattr */
	0,					/* tp_compare */
	(reprfunc)rpcSourceRepr,		/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	0,					/* tp_as_mapping */
	0,					/* tp_hash */
	0,					/* tp_call */
	0,					/* tp_str */
	0,					/* tp_getattro */
	0,					/* tp_setattro */
	0,					/* tp_as_buffer */
	0,					/* tp_xxx4 */
	0,					/* tp_doc */
};
