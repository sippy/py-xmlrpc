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
 * This is the main include file for the xmlrpc library.  See the
 * individual header files contain more information about individual
 * objects and procedures.
 *
 */


#ifndef _XMLRPC_H_
#define _XMLRPC_H_


#define	XMLRPC_VER		"0.8.8.3"
#define	XMLRPC_VER_STR		"py-xmlrpc-" XMLRPC_VER
#define XMLRPC_LIB_STR		"Sourcelight Technologies " XMLRPC_VER_STR

#define	XMLRPC_DATE_FORMAT_US		1
#define	XMLRPC_DATE_FORMAT_EUROPE	2

#include "rpcBase64.h"
#include "rpcBoolean.h"
#include "rpcClient.h"
#include "rpcDate.h"
#include "rpcDispatch.h"
#include "rpcFault.h"
#include "rpcInclude.h"
#include "rpcPostpone.h"
#include "rpcServer.h"
#include "rpcSource.h"
#include "rpcUtils.h"


PyObject	*rpcError;
PyObject	*rpcFaultStr;
int		rpcLogLevel;
void		xmlrpcInit(void);
void		setLogLevel(int level);
void		setLogger(FILE *logger);
int		rpcDateFormat;
FILE            *rpcLogger;

#ifdef MSWINDOWS
	void		rpcNTcleanup(void);
	int		rpcNTinit(void);
#endif /* MSWINDOWS */



#endif	/* _XMLRPC_H_ */
