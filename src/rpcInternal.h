/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 *
 * Miscellaneous utilities and definitions for internal module use
 */

#ifndef _RPCINTERNAL_H_
#define _RPCINTERNAL_H_


#include "Python.h"
#ifdef MSWINDOWS
	#include <winsock2.h>
	#define EINPROGRESS	WSAEINPROGRESS
	#define EWOULDBLOCK	WSAEWOULDBLOCK
	#define ETIMEDOUT	WSAETIMEDOUT

	#define close		closesocket
	#define snprintf	_snprintf
	#define strcasecmp	_stricmp
	#define strncasecmp	_strnicmp
	#define write(a, b, c)	send(a, b, c, 0)
	#define read(a, b, c)	recv(a, b, c, 0)
#else
	#define max(a,b) 	(((a)>(b))?(a):(b))
	#define min(a,b) 	(((a)<(b))?(a):(b))
#endif /* MSWINDOWS */


/*
 * In order to improve readibility
 */
#define	and		&&
#define	or		||
#define	not		!
#define	unless(a)	if (!(a))
#define	true		(0==0)
#define	false		(0!=0)
#define	bool		int
#define	uint		unsigned int
#define	uchar		unsigned char
#define	ulong		unsigned long
#define	ushort		unsigned short
#define	EOS		'\0'
#define	isBlocked(a)	(a == EINPROGRESS || a == EAGAIN || a == EWOULDBLOCK)


void		*setPyErr(char *error);
void		*alloc(uint nBytes);
void		*ralloc(void *vp, uint nBytes);
void		rpcLogMsg(int level, char *formp, ...);
void		rpcLogSrc(int level, rpcSource *srcp, char *formp, ...);
int		get_errno(void);
void		set_errno(int num);
bool		decodeActLong(char **cp, char *ep, long *l);
bool		decodeActLongHex(char **cp, char *ep, long *l);
bool		decodeActDouble(char **cp, char *ep, double *d);


#endif	/* _RPCINTERNAL_H_ */
