/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 */


#include <time.h>
#include <assert.h>
#include "Python.h"
#include "xmlrpc.h"
#include "rpcInternal.h"


/*
 * Set the python error and return NULL
 */
void *
setPyErr(char *error)
{
	PyErr_SetString(rpcError, error);

	return NULL;
}


void
rpcLogMsg(int level, char *formp, ...)
{
	char		buff[100];
	struct tm	*tm;
	time_t		t;
	va_list		ap;

	if (level > rpcLogLevel)
		return;
	time(&t);
	tm = localtime(&t);
	if (rpcDateFormat == XMLRPC_DATE_FORMAT_US) {
		if (strftime(buff, 100-1, "%m/%d/%Y %H:%M:%S", tm) <= 0)
			return;
	} else {
		assert(rpcDateFormat == XMLRPC_DATE_FORMAT_EUROPE);
		if (strftime(buff, 100-1, "%Y/%m/%d %H:%M:%S", tm) <= 0)
			return;
	}
	va_start(ap, formp);
	fprintf(stderr, "%s ", buff);
	vfprintf(stderr, formp, ap);
	fprintf(stderr, "\n");
	fflush(stderr);
	va_end(ap);
}


void
rpcLogSrc(int level, rpcSource *srcp, char *formp, ...)
{
	char		buff[100];
	struct tm	*tm;
	time_t		t;
	va_list		ap;

	if (level > rpcLogLevel)
		return;
	time(&t);
	tm = localtime(&t);
	if (rpcDateFormat == XMLRPC_DATE_FORMAT_US) {
		if (strftime(buff, 100-1, "%m/%d/%Y %H:%M:%S", tm) <= 0)
			return;
	} else {
		assert(rpcDateFormat == XMLRPC_DATE_FORMAT_EUROPE);
		if (strftime(buff, 100-1, "%Y/%m/%d %H:%M:%S", tm) <= 0)
			return;
	}
	if (srcp->desc) {
		if (srcp->fd >= 0)
			fprintf(stderr, "%s <source %s fd %d> ",
				buff, srcp->desc, srcp->fd);
		else
			fprintf(stderr, "%s <source %s> ",
				buff, srcp->desc);
	} else
		fprintf(stderr, "%s <source fd %d> ", buff, srcp->fd);
	va_start(ap, formp);
	vfprintf(stderr, formp, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}


void
setLogLevel(int level)
{
	rpcLogLevel = level;
}


/*
 * os independant way to get errno
 */
int
get_errno(void)
{
#ifdef MSWINDOWS
	return WSAGetLastError();
#else
	return errno;
#endif
}


/*
 * os independant way to set errno
 */
void
set_errno(int num)
{
#ifdef MSWINDOWS
	WSASetLastError(num);
#else
	errno = num;
#endif
}



bool
decodeActDouble(char **cp, char *ep, double *d)
{
	char		*tp,
			*dp;
	bool		dot;

	dot = false;
	tp = *cp;
	if (**cp == '-')
		++*cp;
	for (; *cp < ep; ++*cp)
		if (**cp == '.') {
			if (dot)
				return false;
			else
				dot = true;
		} else if (**cp < '0' || **cp > '9')
			break;
	dp = alloc(*cp - tp + 1);
	strncpy(dp, tp, *cp - tp);
	dp[*cp - tp] = EOS;
	*d = atof(dp);
	free(dp);

	return (*cp > tp);
}


bool
decodeActLong(char **cp, char *ep, long *l)
{
	long		t;
	char		*tp;
	int		sign;

	tp = *cp;
	t = 0;
	sign = 1;

	if (**cp == '-') {
		sign = -1;
		++*cp;
	}
	while (*cp < ep && **cp <= '9' && **cp >= '0')
		t = 10 * t + *((*cp)++) - '0';
	*l = t * sign;

	return (*cp > tp);
}


 
bool
decodeActLongHex(char **cp, char *ep, long *l)
{
	long		t;
	char		*tp;
	int		sign;

	tp = *cp;
	t = 0;
	sign = 1;

	if (**cp == '-') {
		sign = -1;
		++*cp;
	}
	while (*cp < ep) {
		if (**cp <= '9' && **cp >= '0')
			t = 16 * t + *((*cp)++) - '0';
		else if (**cp <= 'z' && **cp >= 'a')
			t = 16 * t + 10 + *((*cp)++) - 'a';
		else if (**cp <= 'Z' && **cp >= 'A')
			t = 16 * t + 10 + *((*cp)++) - 'A';
		else
			break;
	}
	*l = t * sign;

	return (*cp > tp);
}


/*
 * Allocate memory and set an appropriate error if it fails
 */
void *
alloc(uint nBytes)
{
	void	*vp;

	if (nBytes == 0)
		return NULL;
	vp = malloc(nBytes);
	if (vp == NULL)
		PyErr_SetString(rpcError, "out of memory");
	return vp;
}


/*
 * Re-Allocate memory and set an appropriate error if it fails
 */
void *
ralloc(void *vp, uint nBytes)
{
	vp = realloc(vp, nBytes);
	if (vp == NULL)
		PyErr_SetString(rpcError, "out of memory");
	return vp;
}
