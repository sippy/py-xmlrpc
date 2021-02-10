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


#include "Python.h"
#include <stdarg.h>
#include <time.h>
#include "xmlrpc.h"
#include "rpcInternal.h"


int		rpcLogLevel = 3;
int		rpcDateFormat = XMLRPC_DATE_FORMAT_US;
PyObject	*rpcError = NULL;
PyObject	*rpcFaultStr = NULL;
FILE		*rpcLogger = NULL;


void
xmlrpcInit(void)
{
	unless (Py_IsInitialized())
		Py_Initialize();

	rpcLogLevel = 3;
	rpcLogger = stderr;
	rpcDateFormat = XMLRPC_DATE_FORMAT_US;
	Py_TYPE(&rpcDateType) = &PyType_Type;
	Py_TYPE(&rpcBase64Type) = &PyType_Type;
	Py_TYPE(&rpcClientType) = &PyType_Type;
	Py_TYPE(&rpcServerType) = &PyType_Type;
	Py_TYPE(&rpcSourceType) = &PyType_Type;
	rpcError = PyString_FromString("xmlrpc.error");
	if (rpcError == NULL) {
		fprintf(rpcLogger, "rpcError is NULL in xmlrpcInit\n");
		exit(1);
	}
	rpcFault = rpcFaultClass();
	if (rpcFault == NULL) {
		fprintf(rpcLogger, "rpcFaultStr is NULL in xmlrpcInit\n");
		exit(1);
	}
	rpcPostpone = rpcPostponeClass();
	if (rpcPostpone == NULL) {
		fprintf(rpcLogger, "rpcPostponeStr is NULL in xmlrpcInit\n");
		exit(1);
	}

}


#ifdef MSWINDOWS
/*
 * Additional initialization and cleanup for NT/Windows
 */
void
rpcNTcleanup(void)
{
	WSACleanup();
}


int
rpcNTinit(void)
{
	WSADATA WSAData;
	int ret;
	char buf[100];
	ret = WSAStartup(0x0101, &WSAData);
	switch (ret) {
	case 0: /* no error */
		atexit(rpcNTcleanup);
		return 1;
	case WSASYSNOTREADY:
		PyErr_SetString(PyExc_ImportError,
				"WSAStartup failed: network not ready");
		break;
	case WSAVERNOTSUPPORTED:
	case WSAEINVAL:
		PyErr_SetString(PyExc_ImportError,
			"WSAStartup failed: requested version not supported");
		break;
	default:
		sprintf(buf, "WSAStartup failed: error code %d", ret);
		PyErr_SetString(PyExc_ImportError, buf);
		break;
	}
	return 0;
}


#endif /* MSWINDOWS */
