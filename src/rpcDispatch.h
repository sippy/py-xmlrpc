/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 *
 * An object which monitors file descriptors for events and performs
 * callbacks when interesting events happen
 */


#ifndef _RPCDISPATCH_H_
#define _RPCDISPATCH_H_


#include "rpcInclude.h"


PyTypeObject	rpcDispType;
struct _disp;				/* to appease the compiler gods */


/*
 * A dispatcher for file-descriptor based events
 */
typedef struct _disp {
	PyObject_HEAD			/* python standard */
	uint		maxid,		/* the next source id to use */
			scard,		/* number of sources */
			salloc;		/* amount of sources allocated */
	double		etime;		/* when work should stop */
	rpcSource	**srcs;		/* array of pointers to sources */
} rpcDisp;


rpcDisp		*rpcDispNew(void);
void		rpcDispClear(rpcDisp *dp);
void		rpcDispDealloc(rpcDisp *dp);
PyObject	*rpcDispActiveFds(rpcDisp *dp);
bool		rpcDispAddSource(rpcDisp *dp, rpcSource *sp);
bool		rpcDispDelSource(rpcDisp *dp, rpcSource *sp);
bool		rpcDispWork(rpcDisp *dp, double timeout, bool *timedOut);


#endif /* _RPCDISPATCH_H_ */
