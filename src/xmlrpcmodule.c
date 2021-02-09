/*
 * A python implementation of the xmlrpc spec from www.xmlrpc.com
 *
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * 
 * The author can be reached at:
 *
 * shilad.sen@sourcelight.com
 *
 * Shilad Sen
 * Sourcelight Technologies, Inc.
 * 906 University Place, Suite B-211
 * Evanston, IL 60201
 * 
 */




#include <assert.h>
#include "xmlrpc.h"


#define	and		&&
#define	or		||
#define	not		!
#define	unless(a)	if (!(a))
#define	TRUE		(0==0)
#define	FALSE		(0!=0)
#define	bool		int
#define	uint		unsigned int
#define	uchar		unsigned char
#define	ulong		unsigned long
#define	ushort		unsigned short


static PyObject		*logFileObj = NULL;

static PyObject		*pySetLogLevel(PyObject *self, PyObject *args);
static PyObject		*pySetLogger(PyObject *self, PyObject *args);
static PyObject		*getDateFormat(PyObject *self, PyObject *args);
static PyObject		*setDateFormat(PyObject *self, PyObject *args);
static PyObject		*makeXmlrpcBool(PyObject *self, PyObject *args);
static PyObject		*makeXmlrpcBase64(PyObject *self, PyObject *args);
static PyObject		*makeXmlrpcDate(PyObject *self, PyObject *args);
static PyObject		*makeXmlrpcClient(PyObject *self, PyObject *args);
static PyObject		*makeXmlrpcClientFromServe(
				PyObject *self,
				PyObject *args
			);
static PyObject		*makeXmlrpcServer(PyObject *self, PyObject *args);
static PyObject		*makeXmlrpcSource(PyObject *self, PyObject *args);
static PyObject		*rpcEncode(PyObject *self, PyObject *args);
static PyObject		*rpcDecode(PyObject *self, PyObject *args);
static PyObject		*rpcBuildCall(PyObject *self, PyObject *args);
static PyObject		*rpcBuildRequest(PyObject *self, PyObject *args);
static PyObject		*rpcBuildResponse(PyObject *self, PyObject *args);
static PyObject		*rpcBuildFault(PyObject *self, PyObject *args);
static PyObject		*rpcParseResponse(PyObject *self, PyObject *args);
static PyObject		*rpcParseCall(PyObject *self, PyObject *args);
static PyObject		*rpcParseRequest(PyObject *self, PyObject *args);
static void		*setPyErr(char *error);
static int		insint(PyObject *d, char *name, int value);
static int		insstr(PyObject *d, char *name, char *value);


/*
 * Module function map
 */
static PyMethodDef rpcModuleMethods[] = {
        /* client-server */
	{ "client",		(PyCFunction)makeXmlrpcClient,		1, },
	{ "clientFromServer",	(PyCFunction)makeXmlrpcClientFromServe,	1, },
	{ "server",		(PyCFunction)makeXmlrpcServer,		1, },
	/* encoders */
	{ "boolean",		(PyCFunction)makeXmlrpcBool,		1, },
	{ "base64",		(PyCFunction)makeXmlrpcBase64,		1, },
	{ "dateTime",		(PyCFunction)makeXmlrpcDate,		1, },
	{ "source",		(PyCFunction)makeXmlrpcSource,		1, },
	{ "encode",		(PyCFunction)rpcEncode,			1, },
	{ "buildCall",		(PyCFunction)rpcBuildCall,		1, },
	{ "buildRequest",	(PyCFunction)rpcBuildRequest,		1, },
	{ "buildResponse",	(PyCFunction)rpcBuildResponse,		1, },
	{ "buildFault",		(PyCFunction)rpcBuildFault,		1, },
	/* parsers for data */
	{ "decode",		(PyCFunction)rpcDecode,			1, },	
	{ "parseCall",		(PyCFunction)rpcParseCall,		1, },
	{ "parseRequest",	(PyCFunction)rpcParseRequest,		1, },
	{ "parseResponse",	(PyCFunction)rpcParseResponse,		1, },
	/* misc functions  */
	{ "setLogLevel",	(PyCFunction)pySetLogLevel,		1, },
	{ "setLogger",	        (PyCFunction)pySetLogger,		1, },
	{ "getDateFormat",	(PyCFunction)getDateFormat,		1, },
	{ "setDateFormat",	(PyCFunction)setDateFormat,		1, },
	{  NULL,		 NULL,					0, },
};


/*
 * module procedure: set the log level
 */
static PyObject *
pySetLogLevel(PyObject *self, PyObject *args)
{
	int	level;

	unless (PyArg_ParseTuple(args, "i", &level))
		return NULL;

	setLogLevel(level);

	Py_INCREF(Py_None);
	return Py_None;
}

/*
 * module procedure: set the logger file
 */
static PyObject *
pySetLogger(PyObject *self, PyObject *args)
{
	PyObject *object = NULL;
	FILE    *file = NULL;

	unless (PyArg_ParseTuple(args, "O!", &PyFile_Type, &object))
		return NULL;

	assert(object != NULL);
	assert(PyFile_Check(object));
	if (logFileObj != NULL) {
		Py_DECREF(logFileObj);
	}
	logFileObj = object;
	Py_INCREF(logFileObj);
	file = PyFile_AsFile(object);
	setLogger(file);

	Py_INCREF(Py_None);
	return Py_None;
}


/*
 * module procedure: create a base64 object
*/

static PyObject *
setDateFormat(PyObject *self, PyObject *args)
{
	int		i;

	unless (PyArg_ParseTuple(args, "i", &i))
		return (NULL);
	if ((i != XMLRPC_DATE_FORMAT_US)
	and (i != XMLRPC_DATE_FORMAT_EUROPE))
		return setPyErr("date format must be DATE_FORMAT_US or DATE_FORMAT_EUROPE");
	rpcDateFormat = i;
	Py_INCREF(Py_None);
	return (Py_None);
}


/*
 * module procedure: create a base64 object
*/
static PyObject *
getDateFormat(PyObject *self, PyObject *args)
{
	unless (PyArg_ParseTuple(args, ""))
		return (NULL);
	return (PyInt_FromLong((long)rpcDateFormat));
}


/*
 * module procedure: create a boolean object
 */
static PyObject *
makeXmlrpcBool(PyObject *self, PyObject *args)
{
	int	value;

	unless (PyArg_ParseTuple(args, "i", &value))
		return NULL;

        if (value) {
            Py_INCREF(Py_True);
            return Py_True;
        }
        Py_INCREF(Py_False);
        return Py_False;
}



/*
 * module procedure: create a base64 object
*/

static PyObject *
makeXmlrpcBase64(PyObject *self, PyObject *args)
{
	PyObject	*str;

	unless (PyArg_ParseTuple(args, "S", &str))
		return (NULL);
	return (rpcBase64New(str));
}


/*
 * module procedure: create a date object
 */
static PyObject *
makeXmlrpcDate(PyObject *self, PyObject *args)
{
	PyObject	*tmp;
	int		i;

	unless (PyTuple_Check(args)) {
		PyErr_SetString(rpcError, "dateTime expects a 6-int tuple");
		return (NULL);
	}
	unless (PyTuple_Size(args) == 6) {
		PyErr_SetString(rpcError, "dateTime expects a 6-int tuple");
		return (NULL);
	}
	for (i = 0; i < 6; ++i) {
		tmp = PyTuple_GET_ITEM(args, i);
		unless (PyInt_Check(tmp)) {
			PyErr_SetString(rpcError, "tuple must be ints");
			return (NULL);
		}
	}

	return (rpcDateNew(args));
}


/*
 * module procedure: create a boolean object
 */
static PyObject *
makeXmlrpcClient(PyObject *self, PyObject *args)
{
	char		*host,
			*url;
	int		port;

	unless (PyArg_ParseTuple(args, "sis", &host, &port, &url))
		return NULL;

	return (PyObject *)rpcClientNew(host, port, url);
}


/*
 * module procedure: create a boolean object
 */
static PyObject *
makeXmlrpcClientFromServe(PyObject *self, PyObject *args)
{
	rpcServer	*servp;
	char		*host,
			*url;
	int		port;

	unless (PyArg_ParseTuple(args, "sisO!",
		&host, &port, &url, &rpcServerType, &servp))
		return NULL;

	return (PyObject *)rpcClientNewFromServer(host, port, url, servp);
}


/*
 * module procedure: create a server object
 */
static PyObject *
makeXmlrpcServer(PyObject *self, PyObject *args)
{
	unless (PyArg_ParseTuple(args, ""))
		return NULL;

	return (PyObject *)rpcServerNew();
}


/*
 * module procedure: create a source object
 */
static PyObject *
makeXmlrpcSource(PyObject *self, PyObject *args)
{
	int	fd;

	unless (PyArg_ParseTuple(args, "i", &fd))
		return NULL;

	return (PyObject *)rpcSourceNew(fd);
}


/*
 * module procedure: encode an object in xml
 */
static PyObject *
rpcEncode(PyObject *self, PyObject *args)
{
	PyObject	*value;

	unless (PyArg_ParseTuple(args, "O", &value))
		return NULL;

	return xmlEncode(value);
}


/*
 * module procedure: encode an object in xml
 */
static PyObject *
rpcDecode(PyObject *self, PyObject *args)
{
	PyObject	*sp;

	unless (PyArg_ParseTuple(args, "S", &sp))
		return NULL;

	return xmlDecode(sp);
}


/*
 * module procedure: encode a request and return the xml string
 */
static PyObject *
rpcBuildCall(PyObject *self, PyObject *args)
{
    char 	*method;
    PyObject	*params;

    unless (PyArg_ParseTuple(args, "sO", &method, &params))
	return NULL;	
    unless (PySequence_Check(params))
	return setPyErr("build request params must be a sequence");
    return buildCall(method, params);
}

/*
 * module procedure: encode an object in xml
 */
static PyObject *
rpcBuildRequest(PyObject *self, PyObject *args)
{
	char		*method,
			*url;
	PyObject	*params,
			*addInfo;

	unless (PyArg_ParseTuple(args, "ssOO",
				&url, &method, &params, &addInfo))
		return NULL;
	unless (PyDict_Check(addInfo))
		return setPyErr("additional info must be a dictonary");
	unless (PySequence_Check(params))
		return setPyErr("build request params must be a sequence");
	return buildRequest(url, method, params, addInfo);
}


/*
 * module procedure: encode an object in xml
 */
static PyObject *
rpcBuildResponse(PyObject *self, PyObject *args)
{
	PyObject	*result,
			*addInfo;

	unless (PyArg_ParseTuple(args, "OO", &result, &addInfo))
		return NULL;
	unless (PyDict_Check(addInfo))
		return setPyErr("additional info must be a dictonary");

	return buildResponse(result, addInfo);
}


/*
 * module procedure: encode an object in xml
 */
static PyObject *
rpcBuildFault(PyObject *self, PyObject *args)
{
	PyObject	*addInfo;
	int		errCode;
	char		*errStr;

	unless (PyArg_ParseTuple(args, "isO", &errCode, &errStr, &addInfo))
		return NULL;
	unless (PyDict_Check(addInfo))
		return setPyErr("additional info must be a dictonary");

	return buildFault(errCode, errStr, addInfo);
}

/*
 * module procedure: encode an object in xml
 */
static PyObject *
rpcParseCall(PyObject *self, PyObject *args)
{
	PyObject	*request;

	unless (PyArg_ParseTuple(args, "O", &request))
	    return NULL;
	unless (PyString_Check(request))
	    return setPyErr("request must be a string");

	return parseCall(request);
}

/*
 * module procedure: encode an object in xml
 */
static PyObject *
rpcParseResponse(PyObject *self, PyObject *args)
{
	PyObject	*response;

	unless (PyArg_ParseTuple(args, "O", &response))
		return NULL;
	unless (PyString_Check(response))
		return setPyErr("response must be a string");

	return parseResponse(response);
}


/*
 * module procedure: encode an object in xml
 */
static PyObject *
rpcParseRequest(PyObject *self, PyObject *args)
{
	PyObject	*request;

	unless (PyArg_ParseTuple(args, "O", &request))
		return NULL;
	unless (PyString_Check(request))
		return setPyErr("request must be a string");

	return parseRequest(request);
}

/*
 * module initialization done at load time
 */
void
init_xmlrpc(void) {
	PyObject *m, *d;

	xmlrpcInit();
#ifdef MS_WINDOWS
	unless (rpcNTinit())
		return;
#endif
	m = Py_InitModule("_xmlrpc", rpcModuleMethods);
	d = PyModule_GetDict(m);
	PyDict_SetItemString(d, "error", rpcError);
	PyDict_SetItemString(d, "fault", rpcFault);
	PyDict_SetItemString(d, "postpone", rpcPostpone);

	unless ((insint(d, "ACT_INPUT",           ACT_INPUT))
	and     (insint(d, "ACT_OUTPUT",          ACT_OUTPUT))
	and     (insint(d, "ACT_EXCEPT",          ACT_EXCEPT))
	and     (insint(d, "ONERR_TYPE_C",        ONERR_TYPE_C))
	and     (insint(d, "ONERR_TYPE_PY",       ONERR_TYPE_PY))
	and     (insint(d, "ONERR_TYPE_DEF",      ONERR_TYPE_DEF))
	and     (insint(d, "ONERR_KEEP_DEF",      ONERR_KEEP_DEF))
	and     (insint(d, "ONERR_KEEP_SRC",      ONERR_KEEP_SRC))
	and     (insint(d, "ONERR_KEEP_WORK",     ONERR_KEEP_WORK))
	and     (insint(d, "DATE_FORMAT_US",      XMLRPC_DATE_FORMAT_US))
	and     (insint(d, "DATE_FORMAT_EUROPE",  XMLRPC_DATE_FORMAT_EUROPE))
	and     (insstr(d, "VERSION",             XMLRPC_VER))
	and     (insstr(d, "LIBRARY",             XMLRPC_LIB_STR))) {
		fprintf(rpcLogger, "weird shit happened in module loading\n");
		return;
	}
}


/*
 * Convenience routine to export an integer value.
 */
static int
insint(PyObject *d, char *name, int value)
{
	PyObject *v = PyInt_FromLong((long) value);
	if (v == NULL)
		return FALSE;
	if (PyDict_SetItemString(d, name, v) == 0) {
		PyErr_Clear();
		Py_DECREF(v);
		return TRUE;
	}
	Py_DECREF(v);
	return FALSE;
}


/*
 * Convenience routine to export a string value.
 */
static int
insstr(PyObject *d, char *name, char *value)
{
	PyObject *v = PyString_FromString(value);
	if (v == NULL)
		return FALSE;
	if (PyDict_SetItemString(d, name, v) == 0) {
		Py_DECREF(v);
		PyErr_Clear();
		return TRUE;
	}
	Py_DECREF(v);
	return FALSE;
}


/*
 * Set the python error and return NULL
 */
static void *
setPyErr(char *error)
{
	PyErr_SetString(rpcError, error);

	return NULL;
}
