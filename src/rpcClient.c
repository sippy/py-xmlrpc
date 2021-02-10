/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 */


#include <assert.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include "xmlrpc.h"
#include "rpcInternal.h"


#ifdef MSWINDOWS
	#include <winsock2.h>
#else
	#include <netdb.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <unistd.h>
#endif



#define	READ_SIZE		4096

#define	STATE_CONNECT		0
#define	STATE_CONNECTING	1
#define	STATE_WRITE		2
#define	STATE_READ_HEADER	3
#define	STATE_READ_BODY		4
#define	STATE_READ_CHUNK	5

#define	RETURN_ERR		0
#define	RETURN_AGAIN		1
#define	RETURN_DONE		2
#define	RETURN_READ_MORE	3		/* only for chunked encoding */


static	rpcClient	*rpcClientNewFromDisp(
				char		*host,
				int		port,
				char		*url,
				rpcDisp		*disp
			);
static	bool		connecting(rpcClient *cp);
static	bool		writeRequest(rpcClient *cp, PyObject **toWritep);
static	bool		readResponse(
				rpcClient	*cp,
				PyObject	**bodyp,
				long		blen
			);
static	bool		readHeader(
				rpcClient 	*cp,
 				PyObject	**headp,
				PyObject	**bodyp,
				long		*blen,
				bool		*chunked
			);
static	bool		nbRead(int fd, PyObject **buffpp, bool *eof);
static	bool		executed(rpcClient *cp, PyObject *resp, PyObject *arg);
static	PyObject	*pyRpcClientExecute(PyObject *self, PyObject *args);
static	PyObject	*pyRpcClientGetAttr(rpcClient *cp, char *name);
static	PyObject	*pyRpcClientWork(PyObject *self, PyObject *args);
static	bool		clientConnect(rpcClient *cp);
static	bool		cleanAndRetFalse(PyObject *listp);
static	bool		pyClientCallback(
				rpcClient	*cp,
				PyObject	*resp,
				PyObject	*extArgs
			);
static	bool		execDispatch(
				rpcDisp		*dp,
				rpcSource	*sp,
				int		actions,
				PyObject	*args
			);
static	bool		addAuthentication(
				PyObject	*addInfo,
				char		*name,
				char		*pass
			);
static	bool		readChunks(
				rpcClient	*client,
				PyObject	**bodyp,
				PyObject	**chunkp
			);
static	int		processChunk(
				rpcClient	*client,
				PyObject	**bodyp,
				PyObject	**chunkp
			);


rpcClient *
rpcClientNew(char *host, int port, char *url)
{
	rpcDisp		*dp;
	rpcClient	*cp;

	dp = rpcDispNew();
	if (dp == NULL)
		return NULL;
	cp = rpcClientNewFromDisp(host, port, url, dp);
	Py_DECREF(dp);

	return  cp;
}


rpcClient *
rpcClientNewFromServer(char *host, int port, char *url, rpcServer *servp)
{
	return rpcClientNewFromDisp(host, port, url, servp->disp);
}


static rpcClient *
rpcClientNewFromDisp(char *host, int port, char *url, rpcDisp *disp)
{
	rpcClient	*cp;
	rpcSource	*sp;
	int		slen;

	cp = PyObject_NEW(rpcClient, &rpcClientType);
	if (cp == NULL)
		return NULL;
	cp->host = alloc(strlen(host) + 1);
	if (cp->host == NULL)
		return NULL;
	strcpy(cp->host, host);
	cp->url = alloc(strlen(url) + 1);
	if (cp->url == NULL)
		return NULL;
	strcpy(cp->url, url);
	cp->port = port;
	cp->disp = disp;
	cp->execing = false;
	Py_INCREF(disp);
	sp = rpcSourceNew(-1);
	if (sp == NULL)
		return NULL;
	sp->doClose = true;
	slen = strlen(host) + strlen(":123456") + 1;  /* max desc length */
	sp->desc = alloc(slen);
	if (sp->desc == NULL)
		return NULL;
	if (port == 80)
		snprintf(sp->desc, slen, "%s", host);
	else
		snprintf(sp->desc, slen, "%s:%i", host, port);
	sp->desc[slen - 1] = EOS;
	cp->src = sp;

	return  cp;
}


void
rpcClientClose(rpcClient *cp)
{
	if (cp->src->fd >= 0)
		close(cp->src->fd);
	cp->src->fd = -1;	/* make sure we don't use the fd again */
}


void
rpcClientDealloc(rpcClient *cp)
{
	if (cp->host)
		free(cp->host);
	if (cp->url)
		free(cp->url);
	rpcClientClose(cp);
	cp->host = NULL;
	cp->url = NULL;
	Py_DECREF(cp->src);
	Py_DECREF(cp->disp);
	PyObject_DEL(cp);
}


static bool
execDispatch(rpcDisp *dp, rpcSource *sp, int actions, PyObject *params)
{
	bool		(*cfunc) (rpcClient *, PyObject *, PyObject *),
			res;
	PyObject	*head,
			*body,
			*chunk,
			*args,
			*nargs,
			*strReq,
			*pyfunc,	/* python string that holds func ptr */
			*cleanup,	/* list of items to be DECREF'd */
			*funcArgs;	/* args to the callback function */
	rpcClient	*cp;
	int		state,
			nacts,
			nstate,
			r;
	long		blen;
	bool		chunked;

	nargs = NULL;				/* to appease the compiler */
	r = -1;
	cleanup = PyList_New(0);
	if (cleanup == NULL)
		return false;
	unless (PyArg_ParseTuple(params, "OiSOO:execDispatch",
				&cp, &state, &pyfunc, &funcArgs, &args))
		return false;
	assert(cp->ob_type == &rpcClientType);
	assert(cp->execing == true);

	switch (state) {
	case STATE_CONNECT:		/* args is request, but not touched */
		if (cp->src->fd < 0 and not clientConnect(cp)) {
			cp->execing = false;
			return cleanAndRetFalse(cleanup);
		}
		nstate = STATE_CONNECTING;
		nacts = ACT_OUTPUT;
		nargs = args;
		break;
	case STATE_CONNECTING:
		r = connecting(cp);
		if (r == RETURN_ERR) {
			cp->execing = false;
			return cleanAndRetFalse(cleanup);
		} else if (r == RETURN_AGAIN) {
			nstate = STATE_CONNECTING;
			nacts = ACT_OUTPUT;
			nargs = args;
			break;
		}
		assert(r == RETURN_DONE);
		/* windows sucks. you can't trust SOL_ERROR being set to 0 *
		 * meaning it is connected but select on write is ok       */
		nstate = STATE_WRITE;
		nacts = ACT_OUTPUT;
		nargs = args;
		break;
	case STATE_WRITE:		/* args is toWriteStr */
		r = writeRequest(cp, &args);
		if (r == RETURN_ERR) {
			cp->execing = false;
			return cleanAndRetFalse(cleanup);
		} else if (r == RETURN_AGAIN) {
			nstate = STATE_WRITE;
			nacts = ACT_OUTPUT;
			nargs = args;
			if (PyList_Append(cleanup, args)) {
				cp->execing = false;
				return false;
			}
			break;
		}
		assert (r == RETURN_DONE);
		args = PyString_FromString("");
		if ((args == NULL)
		or (PyList_Append(cleanup, args))) {
				cp->execing = false;
			return false;
		}
	case STATE_READ_HEADER:		/* args is buff */
		head = args;
		body = NULL;
		r = readHeader(cp, &head, &body, &blen, &chunked);
		if (r == RETURN_ERR) {
			cp->execing = false;
			return cleanAndRetFalse(cleanup);
		} else if (r == RETURN_AGAIN) {
			nstate = STATE_READ_HEADER;
			nacts = ACT_INPUT;
			nargs = head;
			if (PyList_Append(cleanup, head)) {
				cp->execing = false;
				return false;
			}
			break;
		}
		assert (r == RETURN_DONE);
		/* If we are reading a chunked response, the body becomes *
		 * the first chunk to be parsed and the new body is empty */
		if (chunked)
			args = Py_BuildValue("(S,s,i,S,i)",
			                     head, "", blen, body, 1);
		else
			args = Py_BuildValue("(S,S,i,s,i)",
			                     head, body, blen, "", 0);
		if ((args == NULL)
		or  (PyList_Append(cleanup, args))) {
			cp->execing = false;
			return cleanAndRetFalse(cleanup);
		}
		Py_DECREF(head);
		Py_DECREF(body);
	case STATE_READ_BODY:	/* args is (head, body, blen, chunk, chunked) */
		unless (PyArg_ParseTuple(args, "SSlSi:execDispatchReadBody",
		                         &head, &body, &blen,
		                         &chunk, &chunked)) {
			cp->execing = false;
			return cleanAndRetFalse(cleanup);
		}
		if (chunked)
			r = readChunks(cp, &body, &chunk);
		else
			r = readResponse(cp, &body, blen);
		if (r == RETURN_ERR) {
			cp->execing = false;
			return cleanAndRetFalse(cleanup);
		} else if (r == RETURN_AGAIN) {
			nstate = STATE_READ_BODY;
			nacts = ACT_INPUT;
			nargs = Py_BuildValue("(O,O,i,O,i)", head, body, blen, chunk, chunked);
			Py_DECREF(body);
			if (chunked) {
				Py_DECREF(chunk);
			}
			if (nargs == NULL) {
				cp->execing = false;
				return cleanAndRetFalse(cleanup);
			}
			if (PyList_Append(cleanup, nargs)) {
				cp->execing = false;
				return false;
			}
			break;
		}
		if (chunked) {
			Py_DECREF(chunk);	/* we no longer need this */
		}
 		cp->execing = false;
		assert (r == RETURN_DONE);
		Py_INCREF(head);	/* hack so concat doesn't fail */
		PyString_Concat(&head, body);
		Py_DECREF(body);
		if (head == NULL)
			return false;
		if (rpcLogLevel >= 9) {
			strReq = PyObject_Repr(head);
			if (strReq == NULL)
				return false;
			rpcLogSrc(9, cp->src, "server response is %s",
					PyString_AS_STRING(strReq));
			Py_DECREF(strReq);
		}
		memcpy(&cfunc, PyString_AS_STRING(pyfunc), sizeof(cfunc));
		res = cfunc(cp, head, funcArgs);
		(void)cleanAndRetFalse(cleanup);
		unless (doKeepAlive(head, TYPE_RESP))
			rpcClientClose(cp);
		Py_DECREF(head);
		return res;
	default:
		PyErr_SetString(rpcError, "unknown state to execDispatch");
		return cleanAndRetFalse(cleanup);
	}
	sp->actImp = nacts;
	sp->func = execDispatch;
	sp->params = Py_BuildValue("(O,i,O,O,O)",
			cp, nstate, pyfunc, funcArgs, nargs);
	(void)cleanAndRetFalse(cleanup);
	if (sp->params == NULL)
		return false;
	unless (rpcDispAddSource(dp, sp))
		return false;

	return true;
}


/*
 * Doubly DECREF a list and return false
 */
bool cleanAndRetFalse(PyObject *listp)
{
	int		i;
	PyObject	*item;

	assert(PyList_Check(listp));
	for (i = 0; i < PyList_GET_SIZE(listp); ++i) {
		item = PyList_GET_ITEM(listp, i);
		Py_DECREF(item);
	}
	Py_DECREF(listp);

	return false;
}


bool
clientConnect(rpcClient *cp)
{
	int			fd;
	rpcSource		*sp;
	struct sockaddr_in	addr;
	struct hostent		*hp;
#ifdef MSWINDOWS
	ulong		flag	= 1;
#endif /* MSWINDOWS */

#ifdef MSWINDOWS
	fd = socket(PF_INET, SOCK_STREAM, 0);
	if ((fd == INVALID_SOCKET)
	or  (ioctlsocket((SOCKET)fd, FIONBIO, &flag) != 0)) {
		(rpcClient *)PyErr_SetFromErrno(rpcError);
		return false;
	}
#else
	fd = socket(AF_INET, SOCK_STREAM, 0);
	unless ((fd >= 0) 
	and     (fcntl(fd, F_SETFL, O_NONBLOCK) == 0)) {
		PyErr_SetFromErrno(rpcError);
		return false;
	}
#endif /* MSWINDOWS */
	cp->src->fd = fd;

	sp = cp->src;
	hp = gethostbyname(cp->host);
	if (hp == NULL) {
		PyErr_SetFromErrno(rpcError);
		return false;
	}
	addr.sin_family = hp->h_addrtype;
	memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);
	addr.sin_port = htons((ushort) cp->port);
	unless ((connect(sp->fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
	or      (isBlocked(get_errno()))) {
		PyErr_SetFromErrno(rpcError);
		return false;
	}

	return true;
}


static bool
connecting(rpcClient *cp)
{
	int		fd, val;
	socklen_t	len;

	fd = cp->src->fd;
	len = sizeof(val);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&val, &len) < 0) {
		PyErr_SetFromErrno(rpcError);
		return false;
	}
	if (val == 0) {
		rpcLogSrc(1, cp->src, "client connection succeeded");
		return RETURN_DONE;
	} else if (isBlocked(val))
		return RETURN_AGAIN;

	/* val != 0 and not blocked: an error occured */
	set_errno(val);
	PyErr_SetFromErrno(rpcError);

	return RETURN_ERR;
}


bool
rpcClientNbExecute(
	rpcClient	*cp,
	char		*method,
	PyObject	*params, 
	bool		(*func) (rpcClient *, PyObject *, PyObject *),
	PyObject	*funcArgs,
	char		*name,
	char		*pass
)
{
	PyObject	*pyHost,
			*req,
			*addInfo,
			*strReq;
	rpcSource	*sp;

	
	if (cp->execing) {
		PyErr_SetString(rpcError, "client already executing");
		return false;
	}
	sp = cp->src;
	if (rpcLogLevel >= 5) {
		strReq = PyObject_Str(params);
		if (strReq == NULL)
			return false;
		rpcLogSrc(5, sp, "client queueing command ('%s', %s)",
			method, PyString_AS_STRING(strReq));
		Py_DECREF(strReq);
	} else if (rpcLogLevel >= 3)
		rpcLogSrc(3, sp, "client queueing command '%s'", method);

	addInfo = PyDict_New();
	if (addInfo == NULL)
		return false;
	unless (addAuthentication(addInfo, name, pass))
		return false;
	
	pyHost = PyString_FromString(cp->src->desc);
	if ((pyHost == NULL)
	or  (PyDict_SetItemString(addInfo, "Host", pyHost)))
		return false;
	req = buildRequest(cp->url, method, params, addInfo);
	Py_DECREF(pyHost);
	Py_DECREF(addInfo);
	if (req == NULL)
		return false;
	if (rpcLogLevel >= 9) {
		strReq = PyObject_Repr(req);
		if (strReq == NULL)
			return false;
		rpcLogSrc(9, sp, "client request is %s",
				PyString_AS_STRING(strReq));
		Py_DECREF(strReq);
	}
	if (sp->fd < 0)
		sp->params = Py_BuildValue("(O,i,s#,O,O)", cp, STATE_CONNECT,
					&func, sizeof(func), funcArgs, req);
	else
		sp->params = Py_BuildValue("(O,i,s#,O,O)", cp, STATE_WRITE,
					&func, sizeof(func), funcArgs, req);
	Py_DECREF(req);
	if (sp->params == NULL)
		return false;
	sp->actImp = ACT_IMMEDIATE;
	sp->func = execDispatch;
	unless (rpcDispAddSource(cp->disp, sp))
		return false;
	cp->execing = true;

	return true;
}


static bool
addAuthentication(PyObject *addInfo, char *name, char *pass)
{
	char		*decPair,
			*encPair;
	PyObject	*pyEncPair,
			*pyDecPair;
	PyObject	*pyBasic;

	if (name == NULL and pass == NULL)
		return true;
	decPair = NULL;		/* to appease the compiler */
	if (name != NULL and pass != NULL) {
		decPair = alloc(strlen(name) + 1 + strlen(pass) + 1);
		if (decPair == NULL)
			return false;
		sprintf(decPair, "%s:%s", name, pass);
	} else if (name != NULL and pass == NULL) {
		decPair = alloc(strlen(name) + 1 + 1);
		if (decPair == NULL)
			return false;
		sprintf(decPair, "%s:", name);
	} else if (name == NULL and pass != NULL) {
		decPair = alloc(1 + strlen(pass) + 1);
		if (decPair == NULL)
			return false;
		sprintf(decPair, ":%s", pass);
	}
	pyDecPair = PyString_FromString(decPair);
	if (pyDecPair == NULL)
		return false;
	free(decPair);
	encPair = rpcBase64Encode(pyDecPair);
	if (encPair == NULL)
		return false;
	Py_DECREF(pyDecPair);
	pyBasic = PyString_FromString("Basic ");
	if (pyBasic == NULL)
		return false;
	pyEncPair = PyString_FromString(encPair);
	free(encPair);
	if (pyEncPair == NULL)
		return false;
	PyString_ConcatAndDel(&pyBasic, pyEncPair);
	if (PyDict_SetItemString(addInfo, "Authorization", pyBasic))
		return false;
	Py_DECREF(pyBasic);

	return true;
}


PyObject *
rpcClientExecute(
	rpcClient	*cp,
	char		*method,
	PyObject	*params,
	double		timeout,
	char		*name,
	char		*pass
)
{
	bool		timedOut;
	rpcDisp		*tmp;
	PyObject	*result,
			*response,
			*tuple;

	tmp = cp->disp;
	cp->disp = rpcDispNew();
	if (cp->disp == NULL) {
		cp->disp = tmp;
		return NULL;
	}
	unless ((rpcClientNbExecute(cp, method, params, executed,
					Py_None, name, pass))
	and     (rpcDispWork(cp->disp, timeout, &timedOut))) {
		Py_DECREF(cp->disp);
		cp->disp = tmp;
		cp->execing = false;
		return NULL;
	}
	Py_DECREF((PyObject *)cp->disp);
	cp->disp = tmp;
	if (timedOut) {
		cp->execing = false;
		set_errno(ETIMEDOUT);
		PyErr_SetFromErrno(rpcError);
		return NULL;
	}
	response = cp->src->params;
	cp->src->params = NULL;
	tuple = parseResponse(response);
	Py_DECREF(response);
	if (tuple == NULL)
		return NULL;
	assert(PyTuple_Check(tuple));
	assert(PyTuple_GET_SIZE(tuple) == 2);
	result = PyTuple_GET_ITEM(tuple, 0);
	Py_INCREF(result);
	Py_DECREF(tuple);

	return result;
}


static bool
executed(rpcClient *cp, PyObject *resp, PyObject *args)
{
	cp->src->params = resp;
	Py_INCREF(resp);

	return true;
}


/*
 * write some of a request
 */
static int
writeRequest(rpcClient *cp, PyObject **toWritep)
{
	PyObject	*toWrite;
	int		nb,
			slen;

	toWrite = *toWritep;
	slen = PyString_GET_SIZE(toWrite);
	nb = write(cp->src->fd, PyString_AS_STRING(toWrite), slen);
	rpcLogSrc(7, cp->src, "client wrote %d of %d bytes", nb, slen);
	if (nb < 0 and isBlocked(get_errno()))
		nb = 0;
	if (nb < 0) {
		PyErr_SetFromErrno(rpcError);
		return RETURN_ERR;
	} else if (nb == slen) {
		rpcLogSrc(7, cp->src, "client finished writing request");
		return RETURN_DONE;
	} else {
		assert(slen > nb);
		toWrite = PyString_FromStringAndSize(
			PyString_AS_STRING(toWrite) + nb, slen - nb);
		if (toWrite == NULL)
			return RETURN_ERR;
		*toWritep = toWrite;
		return RETURN_AGAIN;
	}
}


/*
 * Parse a header string to extract relevant information from it.
 *
 * The unparsed header comes in through **headp.  The header is
 * parsed, and separated into the body (*body), while several imporant
 * data points are extracted from it.  blen is the length of the body
 * if Content-length is specified, otherwise it is -1.  Chunked is
 * whether or not the encoding is chunked.
 *
 * Note: if this function returns RETURN_AGAIN, the caller takes ownership
 * of the headp pointer ONLY and must DECREF it.  If RETURN_DONE is
 * returned, the caller takes ownership of BOTH the header AND body and
 * must DECREF both of them.  If RETURN_ERR is returned, no ownership is
 * transferred.
 */
static bool
readHeader(
	rpcClient	*client,
	PyObject	**headp,
	PyObject	**bodyp,
	long		*blen,
	bool		*chunked
)
{
	PyObject	*buff;
	bool		eof;
	char		*hp,		/* start of header */
			*bp,		/* start of body */
			*cp,		/* current position */
			*ep,		/* end of string read */
			*lp,		/* start of content-length value */
			*te;		/* start of transfer-encoding */

	*chunked = false;
	buff = *headp;
	unless (nbRead(client->src->fd, &buff, &eof))
		return RETURN_ERR;
	bp = NULL;
	lp = NULL;
	te = NULL;
	hp = PyString_AS_STRING(buff);
	ep = hp + PyString_GET_SIZE(buff);
	rpcLogSrc(9, client->src, "client read %d bytes of header and body",
			(ep - hp));
	for (cp = hp; (bp == NULL) and (cp < ep); ++cp) {
		if ((ep - cp > 16)
		and (strncasecmp(cp, "Content-length: ", 16) == 0))
			lp = cp + 16;
		if ((ep - cp > 19)
		and (strncasecmp(cp, "Transfer-Encoding: ", 19) == 0))
			te = cp + 19;
		if ((ep - cp > 4)
		and (strncmp(cp, "\r\n\r\n", 4) == 0))
			bp = cp + 4;
		if ((ep - cp > 2)
		and (strncmp(cp, "\n\n", 2) == 0))
			bp = cp + 2;
	}
	if (bp == NULL) {
		if (eof) {
			Py_DECREF(buff);
			PyErr_SetString(rpcError, "got EOS while reading");
			return false;
		}
		*headp = buff;
		return RETURN_AGAIN;
	}
	if ((te != NULL)
	and (strncasecmp(te, "chunked\r\n", 9) == 0)) {
		*chunked = true;
		*blen = -1;
	} else if (lp == NULL) {
		fprintf(rpcLogger, "No Content-length parameter found\n");
		fprintf(rpcLogger, "reading to EOF...\n");
		*blen = -1;
	} else unless (decodeActLong(&lp, ep, blen)) {
		Py_DECREF(buff);
		PyErr_SetString(rpcError, "invalid Content-length");
		return RETURN_ERR;
	}
	rpcLogSrc(9, client->src, "client finished reading header");
	rpcLogSrc(9, client->src,
	         "client bodylen should be %ld %s chunked mode",
	         *blen, *chunked ? "in" : "not in");
	*headp = PyString_FromStringAndSize(hp, bp-hp);
	*bodyp = PyString_FromStringAndSize(bp, ep-bp);
	if (*headp == NULL || *bodyp == NULL)
		return RETURN_ERR;
	Py_DECREF(buff);

	return (RETURN_DONE);
}


/*
 * IMPORTANT NOTE:
 * If this method returns anything but RETURN_ERR, it is the responsibility
 * of the caller takes ownership of the new body value and must DECREF it
 * appropriately.
 */
static bool
readResponse(rpcClient *cp, PyObject **bodyp, long blen)
{
	PyObject	*body;
	bool		eof;
	long		slen;

	body = *bodyp;
	unless (nbRead(cp->src->fd, &body, &eof))
		return false;
	slen = PyString_GET_SIZE(body);
	rpcLogSrc(9, cp->src, "client read %ld of %d bytes of lbody",
	          slen, blen);
	if (blen < 0) {		/* we need to read to EOF */
		*bodyp = body;
		if (eof)
			return RETURN_DONE;
		else
			return RETURN_AGAIN;
	}
	if (slen >= blen) {
		*bodyp = body;
		return RETURN_DONE;
	} else if (eof) {
		Py_DECREF(body);
		PyErr_SetString(rpcError, "unexpected EOF while reading");
		return RETURN_ERR;
	} else {
		*bodyp = body;
		return RETURN_AGAIN;
	}
}


/*
 *
 * IMPORTANT NOTE:
 * If this method returns anything but RETURN_ERR, it is the responsibility
 * of the caller takes ownership of the new body and chunk values and must
 * DECREF them appropriately.
 *
 * We go through a bit of a song and dance to make sure that the reference
 * counting works out just right.  Note that XDECREF is used because we want
 * to check if the value is NULL first.
 */
static bool
readChunks(rpcClient *client, PyObject **bodyp, PyObject **chunkp)
{
	PyObject	*obody,
			*ochunk;
	bool		eof;
	int		r;

	unless (nbRead(client->src->fd, chunkp, &eof))
		return RETURN_ERR;
	obody = NULL;
	ochunk = *chunkp;
	while (true) {
		r = processChunk(client, bodyp, chunkp);
		Py_XDECREF(obody);
		Py_XDECREF(ochunk);
		if (r != RETURN_AGAIN)
			break;
		obody = *bodyp;
		ochunk = *chunkp;
	}
	if (r == RETURN_READ_MORE) {
		if (eof) {
			Py_XDECREF(obody);
			Py_XDECREF(ochunk);
			PyErr_SetString(rpcError, "unexpected EOF while reading");
			return RETURN_ERR;
		}
		return RETURN_AGAIN;
	}
	return r;
}


/*
 * Read a single "chunked" response.
 *
 * A chunk consists of a hexidecimal length followed by that much data
 * followed by a CR LF i.e.:
 *
 * "18
 * abcdefghijklmnopqrstuvwxyz
 * "
 * (where the LF's are really CR-LF's)
 *
 * The last chunk is marked by a length of 0, and this can be followed by
 * a "trailer" which ends with a single emtpy CR-LF line
 *
 * This method returns the usual callback values, but may also return
 * the value RETURN_READ_MORE which means we should return to the select()
 * loop and read more information.
 *
 * IMPORTANT NOTE:
 * If this method returns anything but RETURN_ERR, it is the responsibility
 * of the caller takes ownership of the new body and chunk values and must
 * DECREF them appropriately.
 */
static int
processChunk(rpcClient *client, PyObject **bodyp, PyObject **chunkp)
{
	long		clen;		/* chunk length */
	char		*sp,		/* pointer to the start of the chunk */
			*bp,		/* pointer to the body of the chunk */
			*ep;		/* pointer to the end of the chunk */
	PyObject	*nbody,		/* new body */
			*nchunk,	/* new chunk */
			*append;	/* string to append */

	bp = PyString_AS_STRING(*chunkp);
	sp = bp;
	ep = bp + PyString_GET_SIZE(*chunkp);
	rpcLogSrc(9, client->src, "client processing chunk %s", sp);
	while (true) {
		if (bp+1 >= ep) {
			Py_INCREF(*bodyp);
			Py_INCREF(*chunkp);
			return RETURN_READ_MORE;
		}
		if (strncmp(bp, "\r\n", 2) == 0) {
			bp += 2;
			break;
		}
		bp++;
	}
	unless (decodeActLongHex(&sp, bp, &clen)) {
		PyErr_SetString(rpcError, "invalid size in chunk");
		return RETURN_ERR;
	}
	rpcLogSrc(7, client->src, "chunk length is %ld", clen);
	if ((ep-bp) < (clen+strlen("\r\n"))) {
		Py_INCREF(*bodyp);
		Py_INCREF(*chunkp);
		return RETURN_READ_MORE;
	}
	if (clen == 0) {
		rpcLogSrc(7, client->src, "client reading footer", clen);
		while (sp < ep) {
			if (ep-sp >= 4 and strncmp(sp, "\r\n\r\n", 4) == 0) {
				Py_INCREF(*bodyp);
				Py_INCREF(*chunkp);
				return RETURN_DONE;
			}
			sp++;
		}
		Py_INCREF(*bodyp);
		Py_INCREF(*chunkp);
		return RETURN_READ_MORE;
	}
	unless (strncmp(bp+clen, "\r\n", 2) == 0) {
		PyErr_SetString(rpcError, "chunk did not end in CR LF");
		return RETURN_ERR;
	}
	rpcLogSrc(7, client->src, "client finished reading chunk", clen);
	append = PyString_FromStringAndSize(bp, clen);
	if (append == NULL)
		return RETURN_ERR;
	nbody = *bodyp;
	Py_INCREF(nbody);
	PyString_Concat(&nbody, append);
	Py_DECREF(append);
	sp = bp + clen + strlen("\r\n");	/* new chunk start */
	nchunk = PyString_FromStringAndSize(sp, ep-sp);
	if (nchunk == NULL)
		return RETURN_ERR;
	*bodyp = nbody;
	*chunkp = nchunk;
	return RETURN_AGAIN;
}


/*
 * returns number of bytes read, 
 *
 * NOTE: that buffpp ownership is given to the caller, and it is it's
 * responsiblity to DECREF it.
 */
static bool
nbRead(int fd, PyObject **buffpp, bool *eof)
{
	PyObject	*buffp;
	ulong		bytesAv,
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
				free(cp);
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
 * Module procedure: execute a command on a rpc Server
 */
static PyObject *
pyRpcClientExecute(PyObject *self, PyObject *args)
{
	char		*method;
	PyObject	*params,
			*res;
	double		timeout;
	PyObject	*pyName,
			*pyPass;
	char		*name,
			*pass;

	unless (PyArg_ParseTuple(args, "sOdOO", &method, &params,
				&timeout, &pyName, &pyPass))
		return NULL;
	unless (PySequence_Check(params)) {
		PyErr_SetString(rpcError, "execute params must be a sequence");
		return NULL;
	}
	if (PyObject_Compare(pyName, Py_None) == 0)
		name = NULL;
	else if (PyString_Check(pyName))
		name = PyString_AS_STRING(pyName);
	else
		return setPyErr("name must be a string or None");
	if (PyObject_Compare(pyPass, Py_None) == 0)
		pass = NULL;
	else if (PyString_Check(pyPass))
		pass = PyString_AS_STRING(pyPass);
	else
		return setPyErr("pass must be a string or None");
	res = rpcClientExecute((rpcClient *)self, method, params, timeout, name, pass);

	return res;
}


/*
 * queue up a function for later execution
 */
static PyObject *
pyRpcNbClientExecute(PyObject *self, PyObject *args)
{
	rpcClient	*cp;
	char		*method;
	PyObject	*params,
			*extArgs,
			*pyfunc;
	PyObject	*pyName,
			*pyPass;
	char		*name,
			*pass;
	bool		res;

	cp = (rpcClient *)self;
	unless (PyArg_ParseTuple(args, "sOOOOO", &method, &params,
				&pyfunc, &extArgs, &pyName, &pyPass))
		return NULL;
	unless (PySequence_Check(params)) {
		PyErr_SetString(rpcError, "execute params must be a sequence");
		return NULL;
	}
	if (PyObject_Compare(pyName, Py_None) == 0)
		name = NULL;
	else if (PyString_Check(pyName))
		name = PyString_AS_STRING(pyName);
	else
		return setPyErr("name must be a string or None");
	if (PyObject_Compare(pyPass, Py_None) == 0)
		pass = NULL;
	else if (PyString_Check(pyPass))
		pass = PyString_AS_STRING(pyPass);
	else
		return setPyErr("pass must be a string or None");
	extArgs = Py_BuildValue("(O,O)", pyfunc, extArgs);
	if (params == NULL)
		return NULL;
	res = rpcClientNbExecute(cp, method, params, pyClientCallback,
				extArgs, name, pass);
	Py_DECREF(extArgs);
	unless (res)
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}
	

static	bool
pyClientCallback(rpcClient *cp, PyObject *resp, PyObject *args)
{
	PyObject	*params,
			*pyfunc,
			*extArgs,
			*res;

	unless (PyArg_ParseTuple(args, "OO:pyClientCallback",
					&pyfunc, &extArgs)) {
		return false;
	}

	assert(PyCallable_Check(pyfunc));
	params = Py_BuildValue("(O,O,O)", cp, resp, extArgs);
	if (params == NULL)
		return false;
	res = PyObject_CallObject(pyfunc, params);
	Py_DECREF(params);
	unless (res)
		return false;

	return true;
}


/*
 * Work on a socket for a while
 */
static PyObject *
pyRpcClientWork(PyObject *self, PyObject *args)
{
	bool		timedOut;
	double		timeout;
	rpcClient	*cp;

	cp = (rpcClient *)self;
	unless ((PyArg_ParseTuple(args, "d", &timeout))
	and     (rpcDispWork(cp->disp, timeout, &timedOut)))
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}


/*
 * Set a handler for errors on the client
 */
static PyObject *
pyRpcClientSetOnErr(PyObject *self, PyObject *args)
{
	PyObject	*func;
	rpcClient	*cp;

	cp = (rpcClient *)self;
	unless (PyArg_ParseTuple(args, "O", &func))
		return NULL;
	unless (PyCallable_Check(func)) {
		PyErr_SetString(rpcError, "error handler must be callable");
		return NULL;
	}
	if (PyObject_Compare(func, Py_None))
		rpcSourceSetOnErr(cp->src, ONERR_TYPE_PY, func);
	else
		rpcSourceSetOnErr(cp->src, ONERR_TYPE_DEF, NULL);

	Py_INCREF(Py_None);
	return Py_None;
}


/*
 * Set a handler for errors on the client
 */
static PyObject *
pyRpcClientActiveFds(PyObject *self, PyObject *args)
{
	rpcClient	*cp;

	cp = (rpcClient *)self;
	unless (PyArg_ParseTuple(args, ""))
		return NULL;

	return rpcDispActiveFds(cp->disp);
}


/*
 * Close the client fd
 */
static PyObject *
pyRpcClientClose(PyObject *self, PyObject *args)
{
	rpcClient	*cp;

	cp = (rpcClient *)self;
	unless (PyArg_ParseTuple(args, ""))
		return NULL;
	rpcClientClose(cp);

	Py_INCREF(Py_None);
	return Py_None;
}


/*
 * member functions for client object
 */
static PyMethodDef pyRpcClientMethods[] = {
	{ "activeFds",	(PyCFunction)pyRpcClientActiveFds,	1,	0 },
	{ "close",	(PyCFunction)pyRpcClientClose,		1,	0 },
	{ "execute",	(PyCFunction)pyRpcClientExecute,	1,	0 },
	{ "nbexecute",	(PyCFunction)pyRpcNbClientExecute,	1,	0 },
	{ "setOnErr",	(PyCFunction)pyRpcClientSetOnErr,	1,	0 },
	{ "work",	(PyCFunction)pyRpcClientWork,		1,	0 },
	{ NULL,		NULL},
};


/*
 * return an attribute for a client object
 */
static PyObject *
pyRpcClientGetAttr(rpcClient *cp, char *name)
{
	return Py_FindMethod(pyRpcClientMethods, (PyObject *)cp, name);
}


/*
 * map characterstics of a client object
 */
PyTypeObject rpcClientType = {
	PyObject_HEAD_INIT(0)
	0,
	"rpcClient",
	sizeof(rpcClient),
	0,
	(destructor)rpcClientDealloc,		/* tp_dealloc */
	0,					/* tp_print */
	(getattrfunc)pyRpcClientGetAttr,	/* tp_getattr */
	0,					/* tp_setattr */
	0,					/* tp_compare */
	0,					/* tp_repr */
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
