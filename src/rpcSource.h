/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 *
 * An object which represents a file-descriptor source to be monitored
 * for some "important" events.
 */

#ifndef _RPCSOURCE_H_
#define _RPCSOURCE_H_


#include "rpcInclude.h"


#define	ACT_INPUT	(1 << 0)
#define	ACT_OUTPUT	(1 << 1)
#define	ACT_EXCEPT	(1 << 2)
#define	ACT_IMMEDIATE	(1 << 3)

#define	ONERR_TYPE_C	(1 << 0)
#define	ONERR_TYPE_PY	(1 << 1)
#define	ONERR_TYPE_DEF	(1 << 2)

#define	ONERR_KEEP_DEF	(1 << 0)
#define	ONERR_KEEP_SRC	(1 << 1)	/* unimplemented */
#define	ONERR_KEEP_WORK	(1 << 2)


PyTypeObject	rpcSourceType;


struct _source;			/* to appease the compiler gods */
struct _disp;			/* to appease the compiler gods */


/*
 * a source object
 */
typedef struct _source {
	PyObject_HEAD			/* python standard */
	int		fd,		/* the file descriptor for events */
			id,		/* id associated with the source */
			actImp,		/* actions that are important */
			actOcc;		/* actions that occured in last event */
	char		*desc;		/* description of the source */
	bool		(*func)(	/* callback handler */
				struct _disp	*dp,
				struct _source	*sp,
				int		actions,
				PyObject	*params
			);
	PyObject	*params;	/* parameters for callback */
	char		onErrType;	/* is the handler in c or python? */
	void		*onErr;		/* error handler */
	bool		doClose;	/* should we close the fd when done? */
} rpcSource;



rpcSource	*rpcSourceNew(int fd);
void		rpcSourceDealloc(rpcSource *sp);
void		rpcSourceSetParams(rpcSource *sp, PyObject *params);
void		rpcSourceSetOnErr(rpcSource *sp, int funcType, void *func);


#endif /* _RPCSOURCE_H_ */
