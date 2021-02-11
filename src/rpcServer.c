/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 */


#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "xmlrpc.h"
#include "rpcInternal.h"


#ifdef MSWINDOWS
	#include <winsock2.h>
#else
	#define CLOSE_ON_EXEC	FD_CLOEXEC
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <unistd.h>
#endif


#define	READ_SIZE	4096


static	bool		serveAccept(
				rpcDisp		*dp,
				rpcSource	*sp,
				int		actions,
				PyObject	*params
			);
static	bool		serverReadHeader(
				rpcDisp		*dp,
				rpcSource	*sp,
				int		actions,
				PyObject	*params
			);
static	bool		readRequest(
				rpcDisp		*dp,
				rpcSource	*sp,
				int		actions,
				PyObject	*params
			);
static	bool		writeResponse(
				rpcDisp		*dp,
				rpcSource	*sp,
				int		actions,
				PyObject	*params
			);
static	bool		doResponse(
				rpcServer	*servp,
				rpcSource	*srcp,
				PyObject	*result,
				bool		keepAlive
			);
static	PyObject	*dispatch(
				rpcServer	*servp,
				rpcSource	*srcp,
				PyObject	*request,
				bool		*keepAlive
			);
static	bool		grabError(
				int		*faultCode,
				char		**faultString,
				PyObject	*exc,
				PyObject	*v,
				PyObject	*tb
			);
static	PyObject	*pyRpcServerActiveFds(PyObject *self, PyObject *args);
static	PyObject	*pyRpcServerAddMethods(PyObject *self, PyObject *args);
static	PyObject	*pyRpcServerAddSource(PyObject *self, PyObject *args);
static	PyObject	*pyRpcServerBindAndListen(PyObject *self, PyObject *args);
static	PyObject	*pyRpcServerClose(PyObject *self, PyObject *args);
static	PyObject	*pyRpcServerDelSource(PyObject *self, PyObject *args);
static	PyObject	*pyRpcServerExit(PyObject *self, PyObject *args);
static	PyObject	*pyRpcServerGetAttr(rpcServer *sp, char *name);
static	PyObject	*pyRpcServerQueueResponse(PyObject *self, PyObject *args);
static	PyObject	*pyRpcServerQueueFault(PyObject *self, PyObject *args);
static	PyObject	*pyRpcServerSetAuth(PyObject *self, PyObject *args);
static	PyObject	*pyRpcServerSetFdAndListen(PyObject *self, PyObject *args);
static	PyObject	*pyRpcServerSetOnErr(PyObject *self, PyObject *args);
static	PyObject	*pyRpcServerWork(PyObject *self, PyObject *args);
static	bool		nbRead(int fd, PyObject **buffpp, bool *eof);
static	bool		authenticate(rpcServer *servp, PyObject *addInfo);


rpcServer *
rpcServerNew(void)
{
	rpcServer	*sp;

	sp = PyObject_NEW(rpcServer, &rpcServerType);
	if (sp == NULL)
		return NULL;
	sp->disp = rpcDispNew();
	if (sp->disp == NULL)
		return NULL;
	sp->src = rpcSourceNew(-1);
	if (sp->src == NULL)
		return NULL;
	sp->src->doClose = true;
	sp->comtab = PyDict_New();
	if (sp->comtab == NULL)
		return NULL;
	sp->authFunc = NULL;
	return  sp;
}


void
rpcServerClose(rpcServer *sp)
{
	if (sp->src->fd >= 0)
		close(sp->src->fd);
	sp->src->fd = -1;
	rpcDispClear(sp->disp);
}


void
rpcServerDealloc(rpcServer *sp)
{
	Py_DECREF(sp->src);
	rpcDispDealloc(sp->disp);
}


bool
rpcServerAddCMethod(
	rpcServer 	*servp,
	char		*method,
	PyObject	*(*cfunc)(
				rpcServer *,
				rpcSource *,
				char *,
				char *,
				PyObject *,
				PyObject *
			)
)
{
	PyObject	*pyfunc;

	pyfunc = PyString_FromStringAndSize((char*)&cfunc, sizeof(cfunc));
	if (pyfunc == NULL)
		return false;
	if (PyDict_SetItemString(servp->comtab, method, pyfunc))
		return false;
	return true;
}


bool
rpcServerAddPyMethods(rpcServer *sp, PyObject *toAdd)
{
	PyObject	*items,
			*elem,
			*method,
			*func;
	int		i;
	
	unless (PyDict_Check(toAdd)) {
		PyErr_SetString(rpcError, "addMethods requires dictionary");
		return false;
	}
	items = PyDict_Items(toAdd);
	if (items == NULL)
		return false;
	for (i = 0; i < PyList_GET_SIZE(items); ++i) {
		elem = PyList_GET_ITEM(items, i);
		assert(PyTuple_Check(elem));
		assert(PyTuple_GET_SIZE(elem) == 2);
		method = PyTuple_GET_ITEM(elem, 0);
		func = PyTuple_GET_ITEM(elem, 1);
		unless (PyString_Check(method)) {
			PyErr_SetString(rpcError, "method names must be strings");
			return false;
		}
		unless (PyCallable_Check(func)) {
			PyErr_SetString(rpcError, "method must be callable");
			return false;
		}
		if (PyDict_SetItem(sp->comtab, method, func))
			return false;
	}
	return true;
}


bool 
rpcServerBindAndListen(rpcServer *servp, int port, int queue)
{
	int			fd,
				sflag;
	struct sockaddr_in	saddr;
#ifdef MSWINDOWS
	ulong		flag	= 1;
#endif /* MSWINDOWS */

#ifdef MSWINDOWS
	fd = socket(PF_INET, SOCK_STREAM, 0);
	if ((fd == INVALID_SOCKET)
	or  (ioctlsocket((SOCKET)fd, FIONBIO, &flag) == SOCKET_ERROR)) {
		PyErr_SetFromErrno(rpcError);
		return false;
	}
#else
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if ((fd < 0)
	or  (fcntl(fd, F_SETFL, O_NONBLOCK) != 0)
	or  (fcntl(fd, F_SETFD, CLOSE_ON_EXEC) != 0)) {
		PyErr_SetFromErrno(rpcError);
		return false;
	}
#endif /* MSWINDOWS */
	servp->src->fd = fd;

	sflag = 1;
	fd = servp->src->fd;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
		(void *)&sflag, sizeof(sflag)) != 0) {
		rpcServerClose(servp);
		PyErr_SetFromErrno(rpcError);
		return false;
	}
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons((ushort) port);
	if ((bind(fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
	or  (listen(fd, queue) < 0)) {
		PyErr_SetFromErrno(rpcError);
		rpcServerClose(servp);
		return false;
	}
	rpcLogSrc(3, servp->src, "server listening on port %d", port);
	servp->src->actImp = ACT_INPUT;
	servp->src->func = serveAccept;
	servp->src->params = (PyObject *)servp;
	unless (rpcDispAddSource(servp->disp, servp->src)) {
		rpcServerClose(servp);
		return false;
	}
	return true;
}


bool 
rpcServerSetFdAndListen(rpcServer *servp, int fd, int queue)
{
#ifdef MSWINDOWS
	ulong		flag	= 1;

	unless ((ioctlsocket((SOCKET)fd, FIONBIO, &flag) == 0)
	and     (listen(fd, queue) >= 0)) {
			PyErr_SetFromErrno(rpcError);
			return false;
		}
#else
	unless ((fcntl(fd, F_SETFL, O_NONBLOCK) == 0)
	and     (fcntl(fd, F_SETFD, CLOSE_ON_EXEC) == 0)
	and     (listen(fd, queue) >= 0)) {
			PyErr_SetFromErrno(rpcError);
			return false;
		}
#endif /* MSWINDOWS */
	rpcLogSrc(3, servp->src, "server listening on fd %d", fd);
/*	Py_INCREF(servp);	why was this here? */
	servp->src->fd = fd;
	servp->src->actImp = ACT_INPUT;
	servp->src->func = serveAccept;
	servp->src->params = (PyObject *)servp;
	unless (rpcDispAddSource(servp->disp, servp->src))
		return false;
	return true;
}


void
rpcServerSetAuth(rpcServer *sp, PyObject *authFunc)
{
	if (sp->authFunc) {
		Py_DECREF(sp->authFunc);
	}
	sp->authFunc = authFunc;
	if (authFunc)
		Py_INCREF(authFunc);
}


static bool
serveAccept(rpcDisp *dp, rpcSource *sp, int actions, PyObject *servp)
{
	socklen_t		len,
				res;
	uint			in;
	rpcSource		*client;
	struct sockaddr_in	addr;
#ifdef MSWINDOWS
	ulong		flag	= 1;
#endif /* MSWINDOWS */

	len = sizeof(addr);
	res = accept(sp->fd, (struct sockaddr *)&addr, &len);
	if (res >= 0) {
#ifdef MSWINDOWS
		unless (ioctlsocket((SOCKET)res, FIONBIO, &flag) == 0) {
			PyErr_SetFromErrno(rpcError);
			return false;
		}
#else
		unless ((fcntl(res, F_SETFL, O_NONBLOCK) == 0)
		and     (fcntl(res, F_SETFD, CLOSE_ON_EXEC) == 0)) {
			PyErr_SetFromErrno(rpcError);
			return false;
		}
#endif /* MSWINDOWS */
		client = rpcSourceNew(res);
		if (client == NULL)
			return false;
		client->doClose = true;
		client->desc = alloc(strlen("255.255.255.255:123456") + 1);
		if (client->desc == NULL)
			return false;
		in = ntohl(addr.sin_addr.s_addr);
		sprintf(client->desc, "%u.%u.%u.%u:%u",
			0xFF & (in>>24), 0xFF & (in>>16),
			0xFF & (in>>8), 0xFF & in, ntohs(addr.sin_port));
		rpcLogSrc(3, sp, "server got connection from %s", client->desc);
		client->actImp = ACT_INPUT;
		client->func = serverReadHeader;
		client->params = Py_BuildValue("(s,O)", "", servp);
		if (client->params == NULL)
			return false;
		rpcSourceSetOnErr(client, sp->onErrType, sp->onErr);
		unless (rpcDispAddSource(dp, client))
			return false;
		Py_DECREF(client);
	} else unless (isBlocked(get_errno())) {
		PyErr_SetFromErrno(rpcError);
		return false;
	} else
		fprintf(rpcLogger, "blocked on accept\n");
	sp->actImp = ACT_INPUT;
	sp->func = serveAccept;
	sp->params = servp;
	Py_INCREF(sp->params);
	unless (rpcDispAddSource(dp, sp))
		return false;
	return true;
}



static bool
serverReadHeader(rpcDisp *dp, rpcSource *sp, int actions, PyObject *params)
{
	PyObject	*buff,
			*args,
			*servp;
	bool		eof,
			res;
	long		blen;
	char		*hp,		/* start of header */
			*bp,		/* start of body */
			*cp,		/* current position */
			*ep,		/* end of string read */
			*lp;		/* start of content-length value */

	unless (PyArg_ParseTuple(params, "SO:serverReadHeader", &buff, &servp))
		return false;
	eof = false;
	unless (nbRead(sp->fd, &buff, &eof))
		return false;
	bp = NULL;
	lp = NULL;
	hp = PyString_AS_STRING(buff);
	ep = hp + PyString_GET_SIZE(buff);
	rpcLogSrc(7, sp, "server read %d bytes of header",
			PyString_GET_SIZE(buff));
	for (cp = hp; (bp == NULL) and (cp < ep); ++cp) {
		if ((ep - cp > 16)
		and (strncasecmp(cp, "Content-length: ", 16) == 0))
			lp = cp + 16;
		if ((ep - cp > 4)
		and (strncmp(cp, "\r\n\r\n", 4) == 0))
			bp = cp + 4;
		if ((ep - cp > 2)
		and (strncmp(cp, "\n\n", 2) == 0))
			bp = cp + 2;
	}
	if (bp == NULL) {
		if (eof) {
			if (PyString_GET_SIZE(buff) == 0) {
				close(sp->fd);
				sp->fd = -1;
				Py_DECREF(buff);
				rpcLogSrc(3, sp, "received EOF");
				return true;
			}
			Py_DECREF(buff);
			PyErr_SetString(rpcError, "got EOS while reading");
			return false;
		}
		sp->actImp = ACT_INPUT;
		sp->func = serverReadHeader;
		sp->params = Py_BuildValue("(O,O)", buff, servp);
		Py_DECREF(buff);
		if ((sp->params == NULL)
		or  (not rpcDispAddSource(dp, sp)))
			return false;
		return true;
	}
	if (lp == NULL) {
		Py_DECREF(buff);
		PyErr_SetString(rpcError,
			"no Content-length parameter found in header");
		return false;
	}
	unless (decodeActLong(&lp, ep, &blen)) {
		Py_DECREF(buff);
		PyErr_SetString(rpcError, "invalid Content-length");
		return false;
	}
	rpcLogSrc(7, sp, "server finished reading header");
	rpcLogSrc(9, sp, "server content length should be %d", blen);
	args = Py_BuildValue("(s#,s#,l,O)", hp, bp-hp, bp, ep-bp, blen, servp);
	if (args == NULL)
		return false;
	res = readRequest(dp, sp, actions, args);
	Py_DECREF(args);
	Py_DECREF(buff);

	return res;
}



static bool
readRequest(rpcDisp *dp, rpcSource *srcp, int actions, PyObject *params)
{
	PyObject	*head,
			*body,
			*servp,
			*result;
	bool		eof,
			res,
			keepAlive;
	long		blen,
			slen;

	unless (PyArg_ParseTuple(params, "SSlO:readRequest",
					&head, &body, &blen, &servp))
		return false;
	unless (nbRead(srcp->fd, &body, &eof))
		return false;
	slen = PyString_GET_SIZE(body);
	rpcLogSrc(9, srcp, "server read %d of %d body bytes", slen, blen);
	if (slen > blen) {
		Py_DECREF(body);
		PyErr_SetString(rpcError, "readRequest read too many bytes");
		return false;
	} else if (blen == slen) {
		rpcLogSrc(9, srcp, "server finished reading body");
		Py_INCREF(head);	/* hack so concat doesn't fail */
		PyString_ConcatAndDel(&head, body);
		if (head == NULL)
			return false;
		result = dispatch((rpcServer *)servp, srcp, head, &keepAlive);
		res = doResponse((rpcServer *)servp, srcp, result, keepAlive);
		Py_DECREF(head);
		return res;
	} else {
		if (eof) {
			Py_DECREF(body);
			PyErr_SetString(rpcError, "got EOS while reading body");
			return false;
		}
		srcp->actImp = ACT_INPUT;
		srcp->func = readRequest;
		srcp->params = Py_BuildValue("(S,S,l,O)",
				head, body, blen, servp);
		Py_DECREF(body);
		if (srcp->params == NULL)
			return false;
		unless (rpcDispAddSource(dp, srcp))
			return false;
		return true;
	}
}


static bool
doResponse(rpcServer *servp, rpcSource *srcp, PyObject *result, bool keepAlive)
{
	PyObject	*addInfo,
			*response,
			*strRes,
			*params,
			*exc,
			*v,
			*tb;
	int		faultCode;
	char		*faultString;
	bool		res;

	addInfo = PyDict_New();
	if (addInfo == NULL)
		return false;
	response = NULL;
	if (result == NULL) {
		PyErr_Fetch(&exc, &v, &tb);
		PyErr_NormalizeException(&exc, &v, &tb);
		if (exc == NULL) {
			return (false);
		} else if (PyErr_GivenExceptionMatches(v, rpcPostpone)) {
			PyObject	*o;
			if (srcp->params == NULL) {
				srcp->params = Py_BuildValue("(i)", keepAlive);
				if (srcp->params == NULL)
					return false;
			} else {
				o = Py_BuildValue("(i,O)", keepAlive, srcp->params);
				if (o == NULL)
					return false;
				Py_DECREF(srcp->params);
				srcp->params = o;
			}
			rpcLogSrc(7, srcp, "received postpone request");
			PyErr_Restore(exc, v, tb);
			PyErr_Clear();
			Py_DECREF(addInfo);
			return (true);
		} else if (exc and grabError(&faultCode, &faultString, exc, v, tb)) {
			response = buildFault(faultCode, faultString, addInfo);
			free(faultString);
		} else {
			response = buildFault(-1, "Unknown error", addInfo);
		}
		PyErr_Restore(exc, v, tb);
		assert(PyErr_Occurred());
		PyErr_Print();
		PyErr_Clear();
	} else {
		response = buildResponse(result, addInfo);
		Py_DECREF(result);
	}
	/* one more attempt at failure recovery */
	if (response == NULL)
		response = buildFault(-1, "Unknown error", addInfo);
	Py_DECREF(addInfo);
	if (response == NULL)
		return false;
	if (rpcLogLevel >= 8) {
		strRes = PyObject_Repr(response);
		if (strRes == NULL)
			return false;
		rpcLogSrc(8, srcp, "server responding with %s",
			PyString_AS_STRING(strRes));
		Py_DECREF(strRes);
	}
	params = Py_BuildValue("(O,i,O)", response, (int)keepAlive, servp);
	Py_DECREF(response);
	if (params == NULL)
		return false;
	res = writeResponse(servp->disp, srcp, ACT_OUTPUT, params);
	Py_DECREF(params);

	return res;
}

static PyObject *
dispatch(rpcServer *servp, rpcSource *srcp, PyObject *request, bool *keepAlive)
{
	PyObject	*args,
			*method,
			*params,
			*addInfo,
			*tuple,	
			*pyfunc,
			*(*cfunc)(
				rpcServer *,
				rpcSource *,
				char *,
				char *,
				PyObject *
			),
			*pyuri,
			*result,
			*strReq,
			*strRes;
	char		buff[256],
			*uri;

	if (rpcLogLevel >= 8) {
		strReq = PyObject_Repr(request);
		if (strReq == NULL)
			return NULL;
		rpcLogSrc(8, srcp, "server got request %s",
			PyString_AS_STRING(strReq));
		Py_DECREF(strReq);
	}
	tuple = parseRequest(request);
	if (tuple == NULL)
		return NULL;
	assert(PyTuple_Check(tuple));
	assert(PyTuple_GET_SIZE(tuple) == 3);
	method = PyTuple_GET_ITEM(tuple, 0);
	params = PyTuple_GET_ITEM(tuple, 1);
	addInfo = PyTuple_GET_ITEM(tuple, 2);
	assert(PyDict_Check(addInfo));
	unless (authenticate(servp, addInfo)) {
		Py_DECREF(tuple);
		return NULL;
	}
	*keepAlive = doKeepAliveFromDict(addInfo);
	pyuri = PyDict_GetItemString(addInfo, "URI");
	assert(pyuri != NULL);
	assert(PyString_Check(pyuri));
	uri = PyString_AS_STRING(pyuri);

	if (rpcLogLevel >= 5) {
		strReq = PyObject_Repr(params);
		if (strReq == NULL)
			return NULL;
		rpcLogSrc(5, srcp, "server got request ('%s', %s)",
			PyString_AS_STRING(method), PyString_AS_STRING(strReq));
		Py_DECREF(strReq);
	} else if (rpcLogLevel >= 3)
		rpcLogSrc(3, srcp, "server got request '%s'",
			PyString_AS_STRING(method));

	assert(PyString_Check(method));
	unless (PyMapping_HasKey(servp->comtab, method)) {
		snprintf(buff, 255, "unknown command: \'%s\'",
			PyString_AS_STRING(method));
		Py_DECREF(tuple);
		PyErr_SetString(rpcError, buff);
		return NULL;
	}
	pyfunc = PyDict_GetItem(servp->comtab, method);
	if (PyCallable_Check(pyfunc)) {
		args = Py_BuildValue("(O,O,s,O,O)",
				 servp, srcp, uri, method, params);
		Py_DECREF(tuple);
		if (args == NULL)
			return NULL;
		result = PyObject_CallObject(pyfunc, args);
		Py_DECREF(args);
	} else if (PyString_Check(pyfunc)) {
		assert(PyString_GET_SIZE(pyfunc) == sizeof(cfunc));
		memcpy(&cfunc, PyString_AS_STRING(pyfunc), sizeof(cfunc));
		result = cfunc(
				servp,
				srcp,
				uri,
				PyString_AS_STRING(method),
				params
			);
		Py_DECREF(tuple);
	} else {
		setPyErr("illegal type for server callback");
		return NULL;
	}
	if (result == NULL)
		return NULL;
	if (rpcLogLevel >= 5) {
		strRes = PyObject_Str(result);
		if (strRes == NULL)
			return NULL;
		rpcLogSrc(5, srcp, "server responding %s", 
			PyString_AS_STRING(strRes));
		Py_DECREF(strRes);
	}

	return result;
}


static bool
grabError(
	int		*faultCode,
	char		**faultString,
	PyObject	*exc,
	PyObject	*v,
	PyObject	*tb
)
{
	PyObject	*err1,
			*err2,
			*delim;

	if (PyErr_GivenExceptionMatches(v, rpcFault))
		return rpcFault_Extract(v, faultCode, faultString);
	err1 = PyObject_Str(exc);
	err2 = PyObject_Str(v);
	delim = PyString_FromString(": ");
	if (err1 == NULL || err2 == NULL || delim == NULL)
		return (false);
	PyString_Concat(&err1, delim);
	if (err1 == NULL)
		return (false);
	PyString_Concat(&err1, err2);
	if (err1 == NULL)
		return (false);
	*faultString = alloc(PyString_GET_SIZE(err1) + 1);
	if (*faultString == NULL)
		return (false);
	strcpy(*faultString, PyString_AS_STRING(err1));
	*faultCode = -1;
	Py_DECREF(delim);
	Py_DECREF(err1);
	Py_DECREF(err2);
	return (true);
}


static bool
authenticate(rpcServer *servp, PyObject *addInfo)
{
	PyObject	*auth,
			*encPair,
			*decPair,
			*name,
			*pass,
			*pyuri,
			*res;
	char		*bp,
			*cp,
			*ep,
			error[256];

	if (servp->authFunc == NULL)
		return true;
	pyuri = PyDict_GetItemString(addInfo, "URI");
	assert(pyuri != NULL);
	assert(PyString_Check(pyuri));
	auth = PyDict_GetItemString(addInfo, "Authorization");
	if (auth == NULL) {
		name = Py_None;
		pass = Py_None;
		Py_INCREF(Py_None);
		Py_INCREF(Py_None);
	} else {
		assert(PyString_Check(auth));
		if (strncasecmp("Basic ", PyString_AS_STRING(auth), 6)) {
			setPyErr("unsupported authentication method");
			return false;
		}
		encPair = PyString_FromString(PyString_AS_STRING(auth) + 6);
		if (encPair == NULL)
			return false;
		decPair = rpcBase64Decode(encPair);
		Py_DECREF(encPair);
		if (decPair == NULL)
			return false;
		bp = PyString_AS_STRING(decPair);
		ep = bp + PyString_GET_SIZE(decPair);
		cp = strchr((const char *)bp, ':');
		if (cp == NULL) {
			setPyErr("illegal authentication string");
			fprintf(rpcLogger, "illegal authentication is '%s'\n", bp);
			return false;
		}
		name = PyString_FromStringAndSize(bp, cp-bp);
		pass = PyString_FromStringAndSize(cp+1, ep-(cp+1));
		if (name == NULL || pass == NULL)
			return false;
		Py_DECREF(decPair);
	}
	res = PyObject_CallFunction(servp->authFunc, "(O,O,O)",
	                              pyuri, name, pass);
	Py_DECREF(name);
	Py_DECREF(pass);
	if (res == NULL)
		return false;
	unless ((PyTuple_Check(res))
	and     (PyTuple_GET_SIZE(res) == 2)
	and     (PyInt_Check(PyTuple_GET_ITEM(res, 0)))
	and     (PyString_Check(PyTuple_GET_ITEM(res, 1)))) {
		fprintf(rpcLogger, "authentication function returned ");
		PyObject_Print(res, rpcLogger, 0);
		Py_DECREF(res);
		fprintf(rpcLogger, "; defaulting to (0, 'unknown')\n");
		setPyErr("authentication failed for domain 'unknown'");
		return false;
	}
	if (PyInt_AsLong(PyTuple_GET_ITEM(res, 0))) {
		Py_DECREF(res);
		return true;
	}
	memset(error, 0, sizeof(error));
	snprintf(error, sizeof(error)-1, "authentication failed for domain '%s'",
	         PyString_AS_STRING(PyTuple_GET_ITEM(res, 1)));
	setPyErr(error);
	Py_DECREF(res);

	return false;
}


static bool
writeResponse(rpcDisp *dp, rpcSource *srcp, int actions, PyObject *params)
{
	rpcServer	*servp;
	PyObject	*toWrite;
	int		keepAlive,
			nb,
			slen;

	unless (PyArg_ParseTuple(params, "SiO:writeResponse",
					&toWrite, &keepAlive, &servp))
		return false;
	slen = PyString_GET_SIZE(toWrite);
	nb = write(srcp->fd, PyString_AS_STRING(toWrite), slen);
	rpcLogSrc(9, srcp, "server wrote %d of %d bytes", nb, slen);
	if (nb < 0 and isBlocked(get_errno()))
		nb = 0;
	if (nb < 0) {
		PyErr_SetFromErrno(rpcError);
		return false;
	} else if (nb == slen) {
		rpcLogSrc(9, srcp, "server finished writing response");
		srcp->actImp = ACT_INPUT;
		srcp->func = serverReadHeader;
		srcp->params = Py_BuildValue("(s,O)", "", servp);
		if (srcp->params == NULL)
			return false;
		if (keepAlive) {
			unless (rpcDispAddSource(dp, srcp))
				return false;
		} else {
			close(srcp->fd);
			srcp->fd = -1;
		}
		return true;
	} else {
		assert(slen > nb);
		toWrite = PyString_FromStringAndSize(
			PyString_AS_STRING(toWrite) + nb, slen - nb);
		if (toWrite == NULL)
			return false;
		srcp->actImp = ACT_OUTPUT;
		srcp->func = writeResponse;
		srcp->params = Py_BuildValue("(O,i,O)",
					toWrite, keepAlive, servp);
		Py_DECREF(toWrite);
		if (srcp->params == NULL)
			return false;
		unless (rpcDispAddSource(dp, srcp))
			return false;
		return true;
	}
}


/*
 * Register some commands with an rpc server
 */
static PyObject *
pyRpcServerAddMethods(PyObject *self, PyObject *args)
{
	rpcServer	*sp;
	PyObject	*toAdd;

	sp = (rpcServer *)self;
	unless ((PyArg_ParseTuple(args, "O", &toAdd)
	and     (rpcServerAddPyMethods(sp, toAdd))))
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}


/*
 * Bind an rpc server to a port, and start it listening
 */
static PyObject *
pyRpcServerBindAndListen(PyObject *self, PyObject *args)
{
	rpcServer	*sp;
	int		port,
			queue;

	sp = (rpcServer *)self;
	unless ((PyArg_ParseTuple(args, "ii", &port, &queue)
	and     (rpcServerBindAndListen(sp, port, queue))))
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}


/*
 * Bind an rpc server to a port, and start it listening
 */
static PyObject *
pyRpcServerSetFdAndListen(PyObject *self, PyObject *args)
{
	rpcServer	*sp;
	int		fd,
			queue;

	sp = (rpcServer *)self;
	unless ((PyArg_ParseTuple(args, "ii", &fd, &queue)
	and     (rpcServerSetFdAndListen(sp, fd, queue))))
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}


/*
 * Bind an rpc server to a port, and start it listening
 */
static PyObject *
pyRpcServerWork(PyObject *self, PyObject *args)
{
	bool		timedOut;
	double		timeout;
	rpcServer	*sp;

	sp = (rpcServer *)self;
	unless ((PyArg_ParseTuple(args, "d", &timeout))
	and     (rpcDispWork(sp->disp, timeout, &timedOut)))
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}


/*
 * Bind an rpc server to a port, and start it listening
 */
static PyObject *
pyRpcServerActiveFds(PyObject *self, PyObject *args)
{
	rpcServer	*sp;

	sp = (rpcServer *)self;
	unless (PyArg_ParseTuple(args, ""))
		return NULL;

	return rpcDispActiveFds(sp->disp);
}


/*
 * Bind an rpc server to a port, and start it listening
 */
static PyObject *
pyRpcServerClose(PyObject *self, PyObject *args)
{
	rpcServer	*sp;

	sp = (rpcServer *)self;
	unless (PyArg_ParseTuple(args, ""))
		return NULL;

	rpcServerClose(sp);

	Py_INCREF(Py_None);
	return Py_None;
}


/*
 * Set a handler for errors on the client
 */
static PyObject *
pyRpcServerSetOnErr(PyObject *self, PyObject *args)
{
	PyObject	*func;
	rpcServer	*servp;

	servp = (rpcServer *)self;
	unless (PyArg_ParseTuple(args, "O", &func))
		return NULL;
	unless (PyCallable_Check(func)) {
		PyErr_SetString(rpcError, "error handler must be callable");
		return NULL;
	}
	if (PyObject_Compare(func, Py_None))
		rpcSourceSetOnErr(servp->src, ONERR_TYPE_PY, func);
	else
		rpcSourceSetOnErr(servp->src, ONERR_TYPE_DEF, NULL);

	Py_INCREF(Py_None);
	return Py_None;
}



/*
 * returns number of bytes read, 
 */
static bool
nbRead(int fd, PyObject **buffpp, bool *eof)
{
	PyObject	*buffp;
	long		bytesAv,
			olen,
			slen;
	char		*cp;
	int		res;
	
	*eof = false;
	buffp = *buffpp;
	assert(PyString_Check(buffp));
	olen = PyString_GET_SIZE(buffp);
	slen = olen;
	bytesAv = slen + READ_SIZE;
	cp = alloc(bytesAv);
	if (cp == NULL)
		return false;
	memcpy(cp, PyString_AS_STRING(buffp), slen);
	while (true) {
		if (slen + READ_SIZE > bytesAv) {
			bytesAv = max(bytesAv * 2, slen + READ_SIZE);
			cp = ralloc(cp, bytesAv);
			if (cp == NULL)
				return false;
		}
		res = read(fd, cp + slen, READ_SIZE);
		if (res > 0)
			slen += res;
		else if (res == 0) {
			*eof = true;
			break;
		} else if (res < 0) {
			 if (isBlocked(get_errno()))
				break;
			else {		/* bad error */
				PyErr_SetFromErrno(rpcError);
				return false;
			}
		}
	}
	buffp = PyString_FromStringAndSize(cp, slen);
	if (buffp == NULL)
		return false;
	*buffpp = buffp;
	free(cp);

	return true;
}


/*
 * Tell an rpc server to exit the "work routine" asap
 */
static PyObject *
pyRpcServerExit(PyObject *self, PyObject *args)
{
	rpcServer	*servp;

	servp = (rpcServer *)self;
	servp->disp->etime = 0.0;
	Py_INCREF(Py_None);

	return Py_None;
}

/*
 * Tell an rpc server to exit the "work routine" asap
 */
static PyObject *
pyRpcServerSetAuth(PyObject *self, PyObject *args)
{
	rpcServer	*servp;
	PyObject	*authFunc;

	servp = (rpcServer *)self;
	unless (PyArg_ParseTuple(args, "O", &authFunc))
		return NULL;
	rpcServerSetAuth(servp, authFunc);
	Py_INCREF(Py_None);
	return Py_None;
}


/*
 * Tell an rpc server to exit the "work routine" asap
 */
static PyObject *
pyRpcServerAddSource(PyObject *self, PyObject *args)
{
	rpcServer	*servp;
	rpcSource	*srcp;

	servp = (rpcServer *)self;
	unless (PyArg_ParseTuple(args, "O!", &rpcSourceType, &srcp))
		return NULL;
	if (srcp->func == NULL)
		return setPyErr("callback function was NULL");
	if (srcp->actImp == 0)
		return setPyErr("no callback actions to observe");
	if (srcp->params == NULL)
		return setPyErr("callback params was NULL");
	unless (PyTuple_Check(srcp->params))
		return setPyErr("callback params was not a tuple");
	unless (PyTuple_GET_SIZE(srcp->params) == 2)
		return setPyErr("callback params was not a 2 length tuple");
	unless (PyCallable_Check(PyTuple_GET_ITEM(srcp->params, 0)))
		return setPyErr("callback params 1 was not callable");
	unless (rpcDispAddSource(servp->disp, srcp))
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}


/*
 * Tell an rpc server to exit the "work routine" asap
 */
static PyObject *
pyRpcServerDelSource(PyObject *self, PyObject *args)
{
	rpcServer       *servp;
	rpcSource       *srcp;

	servp = (rpcServer *)self;
	unless (PyArg_ParseTuple(args, "O!", &rpcSourceType, &srcp))
		return NULL;
	unless (rpcDispDelSource(servp->disp, srcp))
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject *
pyRpcServerQueueResponse(PyObject *self, PyObject *args)
{
	rpcServer	*servp;
	rpcSource	*srcp;
	PyObject	*result,
			*o;
	bool		keepAlive;

	servp = (rpcServer *)self;
	unless (PyArg_ParseTuple(args, "O!O", &rpcSourceType, &srcp, &result))
		return (NULL);
	assert(result != NULL);
	if (!PyTuple_Check(srcp->params))
		return setPyErr("srcp->params was not a tuple");
	if (PyTuple_GET_SIZE(srcp->params) == 0)
		return setPyErr("srcp->params did not have length 1 or 2");
	if (! PyInt_Check(PyTuple_GET_ITEM(srcp->params, 0)))
		return setPyErr("srcp->params[0] is not an int");

	if (PyTuple_GET_SIZE(srcp->params) == 1) {
		keepAlive = PyInt_AsLong(PyTuple_GET_ITEM(srcp->params, 0));
		Py_DECREF(srcp->params);
		srcp->params = NULL;
	} else if (PyTuple_GET_SIZE(srcp->params) != 2) {
		keepAlive = PyInt_AsLong(PyTuple_GET_ITEM(srcp->params, 0));
		o = PyTuple_GET_ITEM(srcp->params, 1);
		Py_INCREF(o);
		Py_DECREF(srcp->params);
		srcp->params = o;
	} else
		return setPyErr("srcp->params did not have length 1 or 2");

	Py_INCREF(result);
	if (doResponse(servp, srcp, result, keepAlive)) {
		Py_INCREF(Py_None);
		return (Py_None);
	} else
		return (NULL);
}


static PyObject *
pyRpcServerQueueFault(PyObject *self, PyObject *args)
{
	rpcServer	*servp;
	rpcSource	*srcp;
	PyObject	*faultCode,
			*faultString;

	servp = (rpcServer *)self;
	unless (PyArg_ParseTuple(args, "O!OS", &rpcSourceType, &srcp,
	                         &faultCode, &faultString))
		return (NULL);
	unless (PyInt_Check(faultCode)) {
		PyErr_SetString(rpcError, "errorCode must be an integer");
		return NULL;
	}
	rpcFaultRaise(faultCode, faultString);
	Py_INCREF(Py_None);
	return (Py_None);
}


/*
 * member functions for server object
 */
static PyMethodDef pyRpcServerMethods[] = {
	{ "activeFds",      (PyCFunction)pyRpcServerActiveFds,      1, 0 },
	{ "addMethods",	    (PyCFunction)pyRpcServerAddMethods,     1, 0 },
	{ "bindAndListen",  (PyCFunction)pyRpcServerBindAndListen,  1, 0 },
	{ "close",          (PyCFunction)pyRpcServerClose,          1, 0 },
	{ "setFdAndListen", (PyCFunction)pyRpcServerSetFdAndListen, 1, 0 },
	{ "work",           (PyCFunction)pyRpcServerWork,           1, 0 },
	{ "exit",           (PyCFunction)pyRpcServerExit,           1, 0 },
	{ "addSource",	    (PyCFunction)pyRpcServerAddSource,      1, 0 },
	{ "delSource",	    (PyCFunction)pyRpcServerDelSource,      1, 0 },
	{ "setAuth",        (PyCFunction)pyRpcServerSetAuth,        1, 0 },
	{ "setOnErr",       (PyCFunction)pyRpcServerSetOnErr,       1, 0 },
	{ "queueFault",     (PyCFunction)pyRpcServerQueueFault,     1, 0 },
	{ "queueResponse",  (PyCFunction)pyRpcServerQueueResponse,  1, 0 },
	{ NULL,		NULL},
};


/*
 * return an attribute for a server object
 */
static PyObject *
pyRpcServerGetAttr(rpcServer *sp, char *name)
{
	return Py_FindMethod(pyRpcServerMethods, (PyObject *)sp, name);
}


/*
 * map characterstics of an edb object
 */
PyTypeObject rpcServerType = {
	PyObject_HEAD_INIT(0)
#if PY_MAJOR_VERSION < 3
	0,
#endif
	.tp_name = "rpcServer",
	.tp_basicsize = sizeof(rpcServer),
	.tp_itemsize = 0,
	.tp_dealloc = (destructor)rpcServerDealloc,
	.tp_print = NULL,
	.tp_getattr = (getattrfunc)pyRpcServerGetAttr,
	.tp_setattr = NULL,
#if PY_MAJOR_VERSION < 3
	.tp_compare = NULL,
#endif
	.tp_repr = NULL,
	.tp_as_number = NULL,
	.tp_as_sequence = NULL,
	.tp_as_mapping = NULL,
	.tp_hash = NULL,
	.tp_call = NULL,
	.tp_str = NULL,
	.tp_getattro = NULL,
	.tp_setattro = NULL,
	.tp_as_buffer = NULL,
	.tp_flags = 0,
	.tp_doc = NULL
};
