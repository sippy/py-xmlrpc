/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 */


#include <assert.h>
#include <math.h>
#include "xmlrpc.h"
#include "rpcInternal.h"


#ifdef MSWINDOWS
	#include <sys/types.h>
	#include <sys/timeb.h>
	#include <winsock2.h>
	#define	USE_FTIME
#else
	#include <unistd.h>
	#include <sys/time.h>
#endif /* MSWINDOWS */


#define	INIT_SOURCES	64


static	bool		dispNextEv(rpcDisp *dp, double timeOut);
static	int		dispHandleError(rpcSource *srcp);
static	double		get_time(void);


rpcDisp *
rpcDispNew(void)
{
	rpcDisp	*dp;

	dp = PyObject_NEW(rpcDisp, &rpcDispType);
	if (dp == NULL)
		return NULL;
	dp->maxid = 1;
	dp->scard = 0;
	dp->salloc = INIT_SOURCES;
	dp->etime = -1.0;
	dp->srcs = alloc(dp->salloc * sizeof(*dp->srcs));
	if (dp->srcs == NULL)
		return NULL;
	memset(dp->srcs, 0, dp->salloc * sizeof(*dp->srcs));

	return dp;
}


void
rpcDispClear(rpcDisp *dp)
{
	uint	i;

	for (i = 0; i < dp->scard; ++i)
		Py_DECREF(dp->srcs[i]);
	dp->scard = 0;
}


void
rpcDispDealloc(rpcDisp *dp)
{
	if (dp->srcs) {
		rpcDispClear(dp);
		free(dp->srcs);
	}
	PyMem_DEL(dp);
}


bool
rpcDispAddSource(rpcDisp *dp, rpcSource *sp)
{
	if (dp->scard + 1 > dp->salloc) {
		dp->salloc *= 2;
		dp->srcs = ralloc(dp->srcs, dp->salloc * sizeof(*dp->srcs));
		if (dp->srcs == NULL)
			return false;
		memset(dp->srcs + dp->scard, 0,
			(dp->salloc - dp->scard) * sizeof(*dp->srcs));
	}
	Py_INCREF(sp);
	sp->id = dp->maxid;
	dp->srcs[dp->scard] = sp;
	dp->scard++;
	dp->maxid++;

	return true;
}


/*
 * the only way this can fail is if we can't find the source
 */
bool
rpcDispDelSource(rpcDisp *dp, rpcSource *sp)
{
	bool		found;
	uint		i;

	found = false;
	for (i = 0; i < dp->scard; ++i)
		if (found)
			dp->srcs[i - 1] = dp->srcs[i];
		else if (dp->srcs[i]->id == sp->id)
			found = true;
	if (not found)
		return false;
	Py_DECREF(sp);
	dp->scard--;
	dp->srcs[dp->scard] = NULL;

	return true;
}


/*
 * return a list of important filenos
 */
PyObject *
rpcDispActiveFds(rpcDisp *dp)
{
	PyObject	*inp,
			*out,
			*exc,
			*fd,
			*res;
	rpcSource	*src;
	uint		i;

	inp = PyList_New(0);
	out = PyList_New(0);
	exc = PyList_New(0);
	if (inp == NULL || out == NULL || exc == NULL)
		return NULL;
	for (i = 0; i < dp->scard; ++i) {
		src = dp->srcs[i];
		fd = PyInt_FromLong((long)src->fd);
		if (fd == NULL)
			return NULL;
		if ((src->actImp & ACT_INPUT)
		and (PyList_Append(inp, fd)))
			return NULL;
		if ((src->actImp & ACT_OUTPUT)
		and (PyList_Append(out, fd)))
			return NULL;
		if ((src->actImp & ACT_EXCEPT)
		and (PyList_Append(exc, fd)))
			return NULL;
		Py_DECREF(fd);
	}
	res = Py_BuildValue("(O,O,O)", inp, out, exc);
	Py_DECREF(inp);
	Py_DECREF(out);
	Py_DECREF(exc);

	return res;
}


/*
 * dispatch events on the active file descriptors for some length of time
 */
bool
rpcDispWork(rpcDisp *dp, double timeout, bool *timedOut)
{
	rpcSource	**srcs,
			*sp,
			tp;
	double		ct;
	bool		found;
	uint		i,
			j,
			scard;
	int		res;
	
	*timedOut = false;
	ct = 0.0;			/* to appease the compiler */
	if (timeout >= 0.0) {
		ct = get_time();
		if (ct < 0) {
			PyErr_SetFromErrno(rpcError);
			return false;
		}
		dp->etime = ct + timeout;
	} else
		dp->etime = -1.0;
	while (dp->scard != 0) {
		unless (dispNextEv(dp, (dp->etime - ct)))
			return false;
		scard = dp->scard;
		srcs = alloc(scard * sizeof(*srcs));
		memcpy(srcs, dp->srcs, scard * sizeof(*srcs));
		for (i = 0; i < scard; ++i) {
			sp = srcs[i];
			unless (sp->actOcc)
				continue;
			found = false;
			for (j = 0; dp->scard; ++j)
				if (dp->srcs[j]->id == sp->id) {
					found = true;
					break;
				}
			/* if nothing changed out from under us... */
			/* fix: otherwise log an error?? */
			unless (found and (sp->actImp & sp->actOcc))
				continue;

			Py_INCREF(sp);
			rpcDispDelSource(dp, sp);
			/* copy the source so we can reset the original */
			/* and call the copied version */
			memcpy(&tp, sp, sizeof(*sp));
			sp->id = -1;
			sp->actImp = 0;
			sp->actOcc = 0;
			sp->params = NULL;
			sp->func = NULL;
			unless ((tp.func)(dp, sp, tp.actOcc, tp.params)) {
				Py_DECREF(tp.params);
				res = dispHandleError(sp);
				unless (res & ONERR_KEEP_WORK) {
					Py_DECREF(sp);
					return false;
				}
			} else
				Py_DECREF(tp.params);
			Py_DECREF(sp);
		}
		free(srcs);
		if (dp->etime >= 0.0) {
			ct = get_time();
			if (ct < 0) {
				PyErr_SetFromErrno(rpcError);
				return false;
			}
			if (dp->etime < ct) {
				*timedOut = true;
				break;
			}
		}
	}

	return true;
}


static int
dispHandleError(rpcSource *srcp)
{
	int		(*cfunc)(rpcSource *),
			res;
	PyObject	*pyfunc,
			*pyres,
			*tuple,
			*exc,
			*v,
			*tb,
			*nexc,
			*nv,
			*ntb;

	nexc = NULL;
	nv = NULL;
	ntb = NULL;

	PyErr_Fetch(&exc, &v, &tb);
	PyErr_NormalizeException(&exc, &v, &tb);
	PyErr_Clear();

	res = ONERR_KEEP_DEF;
	if (srcp->onErr == NULL)
		/* do nothing */;
	else if (srcp->onErrType == ONERR_TYPE_C) {
		cfunc = srcp->onErr;
		res = cfunc(srcp);
	} else {
		nexc = exc;
		nv = v;
		ntb = tb;
		Py_XINCREF(nexc);
		Py_XINCREF(nv);
		Py_XINCREF(ntb);
		if (nexc == NULL) {
			Py_INCREF(Py_None);
			nexc = Py_None;
		}
		if (nv == NULL) {
			Py_INCREF(Py_None);
			nv = Py_None;
		}
		if (ntb == NULL) {
			Py_INCREF(Py_None);
			ntb = Py_None;
		}
		pyfunc = srcp->onErr;
		assert(PyCallable_Check(pyfunc));
		tuple = Py_BuildValue("(O,(O,O,O))", srcp, nexc, nv, ntb);
		if (tuple == NULL) {
			fprintf(rpcLogger, "BAD ERROR HANDLER ERROR!!\n");
			PyErr_Print();
		} else {
			pyres = PyObject_CallObject(pyfunc, tuple);
			if (pyres == NULL) {
				fprintf(rpcLogger, "ERROR HANDLER FAILED!!\n");
				PyErr_Print();
			} else if (PyInt_Check(pyres)) {
				res = (int)PyInt_AS_LONG(pyres);
			} else {
				fprintf(rpcLogger, "Error handler returned:");
				PyObject_Print(pyres, rpcLogger, 0);
				fprintf(rpcLogger, "\n");
				fprintf(rpcLogger, "Defaulting to %d\n",
						ONERR_KEEP_DEF);
			}
			Py_DECREF(tuple);
			Py_XDECREF(pyres);
		}
		Py_DECREF(nexc);
		Py_DECREF(nv);
		Py_DECREF(ntb);
	}
	if (res & ONERR_KEEP_DEF) {
		if (srcp->doClose and srcp->fd >= 0) {
			close(srcp->fd);
			srcp->fd = -1;
		}
		if (srcp->desc == NULL)
			fprintf(rpcLogger, "Error from source <fd %d>:\n",
				srcp->fd);
		else
			fprintf(rpcLogger, "Error from source <%s, fd %d>:\n",
				srcp->desc, srcp->fd);
		PyErr_Restore(exc, v, tb);
	} else if (! res & ONERR_KEEP_WORK) {
		PyErr_Restore(exc, v, tb);
	} else {
		Py_XDECREF(exc);
		Py_XDECREF(v);
		Py_XDECREF(tb);
	}

	return res;
}


/*
 * find the next events
 */
static bool
dispNextEv(rpcDisp *dp, double timeout)
{
	rpcSource	*src;
	struct timeval	tv;
	fd_set		inFd,
			outFd,
			excFd;
	int		i,
			nEvents,
			maxFd;
	bool		hasImm;

	maxFd = -1;
	hasImm = false;
	FD_ZERO(&inFd);
	FD_ZERO(&outFd);
	FD_ZERO(&excFd);

	for (i = 0; i < (int)dp->scard; ++i) {
		src = dp->srcs[i];
		src->actOcc = 0;
		unless (src->actImp)
			continue;
		if (src->actImp & ACT_IMMEDIATE) {
			src->actOcc |= ACT_IMMEDIATE;
			hasImm = true;
		} else if (src->fd < 0) {
			fprintf(rpcLogger, "BAD FD!!: %d\n", src->fd);
			continue;
		} else {
			if (src->actImp & ACT_INPUT)
				FD_SET((uint)src->fd, &inFd);
			if (src->actImp & ACT_OUTPUT)
				FD_SET((uint)src->fd, &outFd);
			if (src->actImp & ACT_EXCEPT)
				FD_SET((uint)src->fd, &excFd);
			if (src->fd > maxFd)
				maxFd = src->fd;
		}
	}
	if (hasImm)
		timeout = 0.0;
	if (maxFd != -1) {
		Py_BEGIN_ALLOW_THREADS
		if (timeout < 0.0)
			nEvents = select(maxFd+1, &inFd, &outFd, &excFd, NULL);
		else {
			tv.tv_sec = (int)floor(timeout);
			tv.tv_usec = ((int)floor(1000000.0 *
					(timeout-floor(timeout)))) % 1000000;
			nEvents = select(maxFd+1, &inFd, &outFd, &excFd, &tv);
		}
		Py_END_ALLOW_THREADS
		if (nEvents < 0) {
			PyErr_SetFromErrno(rpcError);
			return false;
		}
	}
	for (i = 0; i < (int)dp->scard; ++i) {
		src = dp->srcs[i];
		if (src->actImp & ACT_IMMEDIATE)
			continue;
		if (FD_ISSET(src->fd, &inFd))
			src->actOcc |= ACT_INPUT;
		if (FD_ISSET(src->fd, &outFd))
			src->actOcc |= ACT_OUTPUT;
		if (FD_ISSET(src->fd, &excFd))
			src->actOcc |= ACT_EXCEPT;
	}

	return true;
}

/*
 * os independent funtion to get current time
 */
static double
get_time(void)
{
#ifdef USE_FTIME
	struct _timeb	tbuff;
	
	_ftime(&tbuff);
	return ((double) tbuff.time + ((double)tbuff.millitm / 1000.0) + \
		((double) tbuff.timezone * 60));
#else
	struct timeval	tv;
	struct timezone	tz;
	
	if (gettimeofday(&tv, &tz) < 0) {
		PyErr_SetFromErrno(rpcError);
		return -1;
	}
	return (tv.tv_sec + tv.tv_usec / 1000000.0);
#endif /* USE_FTIME */
}


/*
 * map characterstics of a client object
 */
PyTypeObject rpcDispType = {
	PyObject_HEAD_INIT(0)
	0,
	"rpcDisp",
	sizeof(rpcDisp),
	0,
	(destructor)rpcDispDealloc,		/* tp_dealloc */
	0,					/* tp_print */
	0,					/* tp_getattr */
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
