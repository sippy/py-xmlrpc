/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 */


#include <assert.h>
#include <string.h>
#include "xmlrpc.h"
#include "rpcInternal.h"


#define	BUFF_START	256
#define	EOL		"\r\n"
#define	COM_BEG		"<!-- "
#define	COM_END		" -->"
#define	TAG_LEN(t)	(sizeof(t)-1)


typedef struct {
	char	*beg;		/* beginning of the string */
	ulong	len,		/* length of the string */
		all;		/* length of allocated memory */
} strBuff;

static	char		*chompStr(char **cp, char *ep, ulong *lines);
static	bool		findTag(
				char	*tag,
				char	**cpp,
				char	*ep,
				ulong	*lines,
				bool	chomp
			);
static	bool		findXmlVersion(char **cpp, char *ep, ulong *lines);
static	PyObject	*syntaxErr(ulong line);
static	PyObject	*eosErr(void);
static	bool		buildInt(char *cp, int slen, int *ip);
static	PyObject	*unescapeString(char *bp, char *ep);
static PyObject		*escapeString(PyObject *oldStr);

static	strBuff		*newBuff(void);
static	strBuff		*growBuff(strBuff *sp, ulong moreBytes);
static	void		freeBuff(strBuff *sp);
static	strBuff		*buffConcat(strBuff *sp, char *cp);
static	strBuff		*buffAppend(strBuff *sp, char *cp, ulong len);
static	strBuff		*buffRepeat(strBuff *sp, char c, uint reps);

static	strBuff		*encodeValue(strBuff *sp, PyObject *value, uint tabs);
static	strBuff		*encodeBool(strBuff *sp, PyObject *value);
static	strBuff		*encodeInt(strBuff *sp, PyObject *value);
static	strBuff		*encodeDouble(strBuff *sp, PyObject *value);
static	strBuff		*encodeString(strBuff *sp, PyObject *value);
static	strBuff		*encodeBase64(strBuff *sp, PyObject *value);
static	strBuff		*encodeDate(strBuff *sp, PyObject *value);
static	strBuff		*encodeArray(strBuff *sp, PyObject *value, uint tabs);
static	strBuff		*encodeStruct(strBuff *sp, PyObject *value, uint tabs);

static	PyObject	*decodeValue(char **cp, char *ep, ulong *lines);
static	PyObject	*decodeInt(char **cp, char *ep, ulong *lines);
static	PyObject	*decodeI4(char **cp, char *ep, ulong *lines);
static	PyObject	*decodeDate(char **cp, char *eq, ulong *lines);
static	PyObject	*decodeDouble(char **cp, char *ep, ulong *lines);
static	PyObject	*decodeString(char **cp, char *ep, ulong *lines);
static	PyObject	*decodeTaglessString(char **cp, char *ep, ulong *lines);
static	PyObject	*decodeBool(char **cp, char *ep, ulong *lines);
static	PyObject	*decodeBase64(char **cp, char *ep, ulong *lines);
static	PyObject	*decodeArray(char **cp, char *ep, ulong *lines);
static	PyObject	*decodeStruct(char **cp, char *ep, ulong *lines);

static	strBuff		*buildHeader(
				int		reqType,
				char		*url,
				PyObject	*addInfo,
				long		len
			);
static	PyObject	*parseFault(char *cp, char *ep, long lines);
static	PyObject	*parseHeader(
				char	**cpp,
				char	*ep,
				ulong	*lines,
				int	reqType
			);
static	bool		parseHeaderLine(
				PyObject	*addInfo,
				char		**cp,
				char		 *ep,
				ulong		 *lines
			);
bool	parseParams	(char **cpp, char *ep, ulong *lines, PyObject *params);

static strBuff *
newBuff(void)
{
	strBuff		*sp;

	sp = alloc(sizeof(*sp));
	if (sp == NULL)
		return NULL;
	sp->len = 0;
	sp->all = BUFF_START;
	sp->beg = alloc(sizeof(*sp->beg) * sp->all);
	if (sp->beg == NULL)
		return NULL;
	memset(sp->beg, 0, sp->all);

	return sp;
}


static strBuff *
growBuff(strBuff *sp, ulong moreBytes)
{
	ulong	nBytes;

	if (sp->all > (sp->len + moreBytes + 1))
		return sp;
	else if (sp->all * 2 > (sp->len + moreBytes + 1))
		nBytes = sp->all * 2;
	else
		nBytes = sp->all + moreBytes + 1;
	sp->all = nBytes;
	sp->beg = ralloc(sp->beg, sp->all);
	if (sp->beg == NULL)
		return NULL;
	memset(sp->beg + sp->len, 0, sp->all - sp->len);

	return sp;
}


static strBuff *
buffConcat(strBuff *sp, char *cp)
{
	ulong	l;

	l = strlen(cp);
	sp = growBuff(sp, l);
	if (sp == NULL)
		return NULL;
	strcpy(sp->beg + sp->len, cp);
	sp->len += l;

	return sp;
}


static strBuff *
buffAppend(strBuff *sp, char *cp, ulong len)
{
	sp = growBuff(sp, len);
	if (sp == NULL)
		return NULL;
	memcpy(sp->beg + sp->len, cp, len);
	sp->len += len;

	return sp;
}

/* handy define to avoid useless calls to buffConcat when working with constant strings */
#define buffConstant(sp, x)	buffAppend(sp, x, sizeof(x)-1)

static strBuff *
buffRepeat(strBuff *sp, char c, uint reps)
{
	sp = growBuff(sp, reps);
	if (sp == NULL)
		return NULL;
	memset(sp->beg + sp->len, c, reps);
	sp->len += reps;

	return sp;
}


static void
freeBuff(strBuff *sp)
{
	if (sp->beg != NULL)
		free(sp->beg);
	free(sp);
}


PyObject *
xmlEncode(PyObject *value)
{
	strBuff		*sp;
	PyObject	*res;

	sp = newBuff();
	if (sp == NULL)
		return NULL;
	sp = encodeValue(sp, value, 0);
	if (sp == NULL)
		return NULL;
	res = PyString_FromStringAndSize(sp->beg, sp->len);
	freeBuff(sp);

	return res;
}


static strBuff *
encodeValue(strBuff *sp, PyObject *value, uint tabs)
{
	if (buffConstant(sp, "<value>") == NULL)
		return NULL;
	if (PyInt_Check(value) or PyLong_Check(value))
		sp = encodeInt(sp, value);
	else if (PyFloat_Check(value))
		sp = encodeDouble(sp, value);
	else if (value->ob_type == &rpcBoolType)
		sp = encodeBool(sp, value);
	else if (value->ob_type == &rpcDateType)
		sp = encodeDate(sp, value);
	else if (value->ob_type == &rpcBase64Type)
		sp = encodeBase64(sp, value);
	else if (PyString_Check(value))
		sp = encodeString(sp, value);
	else if (PyList_Check(value) or PyTuple_Check(value))
		sp = encodeArray(sp, value, tabs);
	else if (PyDict_Check(value))
		sp = encodeStruct(sp, value, tabs);
	else {
		PyObject	*str1,
				*str2;
		freeBuff(sp);
		str1 = PyString_FromString("invalid object to encode: ");
		str2 = PyObject_Repr(value);
		if (str1 == NULL or str2 == NULL)
			return NULL;
		PyString_Concat(&str1, str2);
		PyErr_SetString(rpcError, PyString_AS_STRING(str1));
		Py_DECREF(str1);
		Py_DECREF(str2);
		return NULL;
	}
	if (sp == NULL)
		return NULL;
	sp = buffConstant(sp, "</value>");

	return sp;
}


/*
 * encode the int 4 (for example) as: "<int>4</int>"
 * this could definitely be optimized, but I doubt it really hurts us
 */
static strBuff *
encodeInt(strBuff *sp, PyObject *value)
{
	PyObject	*strval;

	strval = PyObject_Str(value);

	if ((strval == NULL)
	or  (buffConstant(sp, "<int>") == NULL)
	or  (buffConcat(sp, PyString_AS_STRING(strval)) == NULL)
	or  (buffConstant(sp, "</int>") == NULL))
		return NULL;
	Py_DECREF(strval);

	return sp;
}


/*
 * encode the double 4.0 (for example) as: "<double>4.0</double>"
 * i should fix the precision, but I'm not sure how to.
 */
static strBuff *
encodeDouble(strBuff *sp, PyObject *value)
{
	char		buff[256];
	double		d;

	d = PyFloat_AS_DOUBLE(value);
	snprintf(buff, 255, "%f", d);
	if ((buffConstant(sp, "<double>") == NULL)
	or  (buffConcat(sp, buff) == NULL)
	or  (buffConstant(sp, "</double>") == NULL))
		return NULL;

	return sp;
}

/*
 * encode the boolean true (for example) as: "<boolean>1</boolean>"
 */
static strBuff *
encodeBool(strBuff *sp, PyObject *value)
{
	if (((rpcBool *)value)->value)
		return buffConstant(sp, "<boolean>1</boolean>");
	else
		return buffConstant(sp, "<boolean>0</boolean>");
}


/*
 * encode "Hello, World!" (for example) as: "<string>Hello, World!</string>"
 */
static strBuff *
encodeString(strBuff *sp, PyObject *value)
{
	PyObject	*escStr;
	uint		len;
	char		*cp;

	escStr = escapeString(value);
	if (escStr == NULL)
		return NULL;
	cp = PyString_AS_STRING(escStr);
	len = PyObject_Length(escStr);

	if ((buffConstant(sp, "<string>") == NULL)
	or  (buffAppend(sp, cp, len) == NULL)
	or  (buffConstant(sp, "</string>") == NULL))
		return NULL;
	Py_DECREF(escStr);

	return sp;
}


/*
 * encode the Base64 type
*/

static strBuff *
encodeBase64(strBuff *sp, PyObject *value)
{
	char		*str;
	
	str = rpcBase64Encode(((rpcBase64 *)value)->value);
	if (str == NULL)
		return NULL;
        if ((buffConstant(sp, "<base64>") == NULL)
        or  (buffAppend(sp, str, strlen(str)) == NULL)
        or  (buffConstant(sp, "</base64>") == NULL)) {
                return NULL;
	}
	free(str);

	return sp;
}



/*
 * encode the Date type
 */

static strBuff *
encodeDate(strBuff *sp, PyObject *value)
{
	int	year,
		month,
		day,
		hour,
		min,
		sec;
	char	add[6];

	PyArg_ParseTuple(((rpcDate *)value)->value, "iiiiii", 
			&year, &month, &day, &hour, &min, &sec);
	if (buffConstant(sp, "<dateTime.iso8601>") == NULL)
		return (NULL);
	if (year < 1000)
		snprintf(add, 5, "0%d", year);
	else
		snprintf(add, 5, "%d", year);
	if (buffConcat(sp, add) == NULL)
		return (NULL);
	if (month < 10)
		snprintf(add, 5, "0%d", month);
	else
		snprintf(add, 5, "%d", month);
	if (buffConcat(sp, add) == NULL)
		return (NULL);
	if (day < 10)
		snprintf(add, 5, "0%d", day);
	else
		snprintf(add, 5, "%d", day);
	if (buffConcat(sp, add) == NULL)
		return (NULL);
	if (buffConstant(sp, "T") == NULL)
		return (NULL);
	if (hour < 10)
		snprintf(add, 5, "0%d:", hour);
	else
		snprintf(add, 5, "%d:", hour);
	if (buffConcat(sp, add) == NULL)
		return (NULL);
	if (min < 10)
		snprintf(add, 5, "0%d:", min);
	else
		snprintf(add, 5, "%d:", min);
	if (buffConcat(sp, add) == NULL)
		return (NULL);
	if (sec < 10)
		snprintf(add, 5, "0%d", sec);
	else
		snprintf(add, 5, "%d", sec);
	if (buffConcat(sp, add) == NULL
	or buffConstant(sp, "</dateTime.iso8601>") == NULL)
		return (NULL);
	return (sp);
}


/*
 * encode an array in xml
 * this could REALLY be optimized
 */
static strBuff *
encodeArray(strBuff *sp, PyObject *value, uint tabs)
{
	PyObject	*elem;
	int		i;

	if ((buffConstant(sp, EOL) == NULL)
	or  (buffRepeat(sp, '\t', tabs + 1) == NULL)
	or  (buffConstant(sp, "<array>") == NULL)
	or  (buffConstant(sp, EOL) == NULL)
	or  (buffRepeat(sp, '\t', tabs + 2) == NULL)
	or  (buffConstant(sp, "<data>") == NULL)
	or  (buffConstant(sp, EOL) == NULL))
		return NULL;
	for (i = 0; i < PyObject_Length(value); ++i) {
		elem = PySequence_GetItem(value, i);
		if ((elem == NULL)
		or  (buffRepeat(sp, '\t', tabs + 3) == NULL))
			return NULL;
		if  (encodeValue(sp, elem, tabs + 3) == NULL)
			return NULL;
		if ((buffConstant(sp, EOL) == NULL))
			return NULL;
		Py_DECREF(elem);
	}
	if ((buffRepeat(sp, '\t', tabs + 2) == NULL)
	or  (buffConstant(sp, "</data>") == NULL)
	or  (buffConstant(sp, EOL) == NULL)
	or  (buffRepeat(sp, '\t', tabs + 1) == NULL)
	or  (buffConstant(sp, "</array>") == NULL)
	or  (buffConstant(sp, EOL) == NULL)
	or  (buffRepeat(sp, '\t', tabs) == NULL))
		return NULL;

	return sp;
}


/*
 * encode an array in xml
 * this could REALLY be optimized
 */
static strBuff *
encodeStruct(strBuff *sp, PyObject *value, uint tabs)
{
	PyObject	*items,
			*tup,
			*name,
			*val;
	int		i;

	items = PyMapping_Items(value);
	if ((items == NULL)
	or  (buffConstant(sp, EOL) == NULL)
	or  (buffRepeat(sp, '\t', tabs + 1) == NULL)
	or  (buffConstant(sp, "<struct>") == NULL)
	or  (buffConstant(sp, EOL) == NULL))
		return NULL;
	for (i = 0; i < PyObject_Length(items); ++i) {
		tup = PySequence_GetItem(items, i);
		name = PySequence_GetItem(tup, 0);
		val = PySequence_GetItem(tup, 1);
		unless (PyString_Check(name)) {
			Py_DECREF(tup);
			Py_DECREF(name);
			Py_DECREF(val);
			return setPyErr("dictionary keys must be strings");
		}
		if ((tup == NULL || name == NULL || val == NULL)
		or  (buffRepeat(sp, '\t', tabs + 2) == NULL)
		or  (buffConstant(sp, "<member>") == NULL)
		or  (buffConstant(sp, EOL) == NULL)
		or  (buffRepeat(sp, '\t', tabs + 3) == NULL)
		or  (buffConstant(sp, "<name>") == NULL)
		or  (buffConcat(sp, PyString_AS_STRING(name)) == NULL)
		or  (buffConstant(sp, "</name>") == NULL)
		or  (buffConstant(sp, EOL) == NULL)
		or  (buffRepeat(sp, '\t', tabs + 3) == NULL)
		or  (encodeValue(sp, val, tabs + 3) == NULL)
		or  (buffConstant(sp, EOL) == NULL)
		or  (buffRepeat(sp, '\t', tabs + 2) == NULL)
		or  (buffConstant(sp, "</member>") == NULL)
		or  (buffConstant(sp, EOL) == NULL))
			return NULL;
		Py_DECREF(tup);
		Py_DECREF(name);
		Py_DECREF(val);
	}
	Py_DECREF(items);
	if ((buffRepeat(sp, '\t', tabs + 1) == NULL)
	or  (buffConstant(sp, "</struct>") == NULL)
	or  (buffConstant(sp, EOL) == NULL)
	or  (buffRepeat(sp, '\t', tabs) == NULL))
		return NULL;

	return sp;
}


PyObject *
xmlDecode(PyObject *sp)
{
	PyObject	*res,
			*tup;
	char		*cp,
			*ep;
	ulong		lines;

	lines = 0;
	cp = PyString_AS_STRING(sp);
	ep = cp + PyObject_Length(sp);
	res = decodeValue(&cp, ep, &lines);
	if (res == NULL)
		return NULL;
	tup = Py_BuildValue("(O, s#)", res, cp, ep - cp);
	Py_DECREF(res);

	return tup;
}


static PyObject *
decodeValue(char **cp, char *ep, ulong *lines)
{
	PyObject	*res;
	char		*tp;

	if (chompStr(cp, ep, lines) >= ep)
		return eosErr();
	tp = *cp + strlen("<value>");		/* HACK!! */
	unless (findTag("<value>", cp, ep, lines, true))
		return NULL;
	if (chompStr(cp, ep, lines) >= ep)
		return eosErr();

	if (strncmp(*cp, "<int>", 5) == 0)
		res = decodeInt(cp, ep, lines);
	else if (strncmp(*cp, "<i4>", 4) == 0)
		res = decodeI4(cp, ep, lines);
	else if (strncmp(*cp, "<boolean>", 9) == 0)
		res = decodeBool(cp, ep, lines);
	else if (strncmp(*cp, "<double>", 8) == 0)
		res = decodeDouble(cp, ep, lines);
	else if (strncmp(*cp, "<string>", 8) == 0)
		res = decodeString(cp, ep, lines);
	else if (strncmp(*cp, "<string/>", 9) == 0)
		res = decodeString(cp, ep, lines);
	else if (strncmp(*cp, "<array>", 7) == 0)
		res = decodeArray(cp, ep, lines);
	else if (strncmp(*cp, "<struct>", 8) == 0)
		res = decodeStruct(cp, ep, lines);
	else if (strncmp(*cp, "<dateTime.iso8601>", 18) == 0)
		res = decodeDate(cp, ep, lines);
	else if (strncmp(*cp, "<base64>", 8) == 0)
		res = decodeBase64(cp, ep, lines);
	else {		/* it must be a string */
		*cp = tp;
		res = decodeTaglessString(cp, ep, lines);
	}
	if (res == NULL)
		return NULL;

	unless (findTag("</value>", cp, ep, lines, false)) {
		Py_DECREF(res);
		return NULL;
	}
	chompStr(cp, ep, lines);

	return res;
}


static PyObject *
decodeInt(char **cp, char *ep, ulong *lines)
{
	long	i;

	*cp += strlen("<int>");
	unless (decodeActLong(cp, ep, &i))
		return NULL;
	if (*cp >= ep)
		return eosErr();
	unless (findTag("</int>", cp, ep, lines, true))
		return NULL;
	return PyInt_FromLong(i);
}


static PyObject *
decodeI4(char **cp, char *ep, ulong *lines)
{
	long	i;

	*cp += strlen("<i4>");
	unless (decodeActLong(cp, ep, &i))
		return NULL;
	if (*cp >= ep)
		return eosErr();
	unless (findTag("</i4>", cp, ep, lines, true))
		return NULL;
	return PyInt_FromLong(i);
}


static PyObject *
decodeBool(char **cp, char *ep, ulong *lines)
{
	PyObject	*res;
	bool		value;

	value = true;		/* to appease the compiler */
	res = NULL;		/* to appease the compiler */
	if (*cp + 20 >= ep)
		return eosErr();
	if (strncmp(*cp, "<boolean>1</boolean>", 20) == 0)
		value = true;
	else if (strncmp(*cp, "<boolean>0</boolean>", 20) == 0)
		value = false;
	else
		syntaxErr(*lines);
	*cp += 20;
	if (chompStr(cp, ep, lines) >= ep)
		return eosErr();

	return rpcBoolNew(value);
}


static PyObject *
decodeDouble(char **cp, char *ep, ulong *lines)
{
	double		d;

	d = 0.0;
	*cp += strlen("<double>");
	unless (decodeActDouble(cp, ep, &d))
		return syntaxErr(*lines);
	unless (findTag("</double>", cp, ep, lines, true))
		return NULL;

	return PyFloat_FromDouble(d);
}


static PyObject *
decodeBase64(char **cp, char *ep, ulong *lines)
{
	PyObject	*encStr,
			*decStr,
			*ret;
	char		*tp;

	*cp += strlen("<base64>");
	tp = *cp;
	while (strncmp(*cp, "</base64>", 9)) {
		if (**cp == '\n')
			(*lines)++;
		if (*cp >= ep)
			return eosErr();
		(*cp)++;
	}
	encStr = PyString_FromStringAndSize(tp, *cp - tp);
	if (encStr == NULL)
		return (NULL);
	unless (findTag("</base64>", cp, ep, lines, true)) {
		Py_DECREF(encStr);
		return (NULL);
	}
	decStr = rpcBase64Decode(encStr);
	Py_DECREF(encStr);
	if (decStr == NULL)
		return NULL;
	ret = rpcBase64New(decStr);
	Py_DECREF(decStr);

	return (ret);
}


/*
 * build an integer from a few digits
 */
static bool
buildInt(char *cp, int slen, int *ip)
{
	char	*ep;
	int	i = 0;

	ep = cp + slen;
	for (; cp < ep; ++cp)
		if ('0' <= *cp && *cp <= '9')
			i = i * 10 + (*cp - '0');
		else {
			PyErr_SetString(rpcError,
				"<dateTime> expects numbers for date values");
        		return (0);
		}
	*ip = i;

        return (1);
}

static PyObject *
decodeDate(char **cp, char *eq, ulong *lines)
{
	rpcDate		*ret;
	PyObject	*arg;
        char            *tp;
	int		year,
			month,
			day,
			hour,
			min,
			sec;

        *cp += strlen("<dateTime.iso8601>");
        tp = *cp;
/*        while (strncmp(*cp, "</dateTime.iso8601>", 19)) {
                if (**cp == '\n')
                        (*lines)++;
                if (*cp >= eq)
                        return eosErr();
                (*cp)++;
        }
        unless (findTag("</dateTime.iso8601>", cp, eq, lines, false))
                return (NULL);
	tp[(*cp-tp)-strlen("</dateTime.iso8601>")] = '\0';
	printf("TP IS %s\n", tp);
	unless (strlen(tp) == 17) {
		PyErr_SetString(rpcError, "Invalid format of <dateTime>");
		return (NULL);
	}*/
	unless ((buildInt(tp,      4, &year))
	and     (buildInt(tp + 4,  2, &month))
	and     (buildInt(tp + 6,  2, &day))
	and     (buildInt(tp + 9,  2, &hour))
	and     (buildInt(tp + 12, 2, &min))
	and     (buildInt(tp + 15, 2, &sec)))
		return NULL;
	arg = Py_BuildValue("(i, i, i, i, i, i)",
		year, month, day, hour, min , sec);
	unless (arg)
		return (NULL);
	ret = (rpcDate *)rpcDateNew(arg);
	Py_DECREF(arg);
	unless (ret)
		return (NULL);
        while (strncmp(*cp, "</dateTime.iso8601>", 19)) {
                if (**cp == '\n')
                        (*lines)++;
                if (*cp >= eq)
                        return eosErr();
                (*cp)++;
        }
        unless (findTag("</dateTime.iso8601>", cp, eq, lines, true))
                return (NULL);

	return (PyObject *)(ret);
}


static PyObject *
decodeString(char **cp, char *ep, ulong *lines)
{
	PyObject	*res;
	char		*tp;

	if ((*cp)[7] == '/')
		*cp += strlen("<string/>");
	else
		*cp += strlen("<string>");
	tp = *cp;
	while (strncmp(*cp, "</string>", 8)) {
		if (**cp == '\n')
			(*lines)++;
		if (*cp >= ep)
			return eosErr();
		(*cp)++;
	}

	res = unescapeString(tp, *cp);
	if (res == NULL)
		return NULL;
	unless (findTag("</string>", cp, ep, lines, true)) {
		Py_DECREF(res);
		return NULL;
	}

	return res;
}


static PyObject *
decodeTaglessString(char **cp, char *ep, ulong *lines)
{
	PyObject	*res;
	char		*tp;

	tp = *cp;
	while (strncmp(*cp, "</value>", 8)) {
		if (**cp == '\n')
			(*lines)++;
		if (*cp >= ep)
			return eosErr();
		(*cp)++;
	}

	res = unescapeString(tp, *cp);
	if (res == NULL)
		return NULL;

	return res;
}


static PyObject *
decodeArray(char **cp, char *ep, ulong *lines)
{
	PyObject	*res,
			*elem;

	unless (findTag("<array>", cp, ep, lines, true))
		return NULL;
	res = PyList_New(0);
	if (res == NULL)
		return NULL;
	if (strncmp("<data>", *cp, 6) == 0) {
		unless (findTag("<data>", cp, ep, lines, true))
			return NULL;
		while (strncmp(*cp, "<value>", 7) == 0) {
			elem = decodeValue(cp, ep, lines);
			if (elem == NULL) {
				Py_DECREF(res);
				return NULL;
			}
			if (PyList_Append(res, elem)) {
				Py_DECREF(res);
				Py_DECREF(elem);
				return NULL;
			}
			Py_DECREF(elem);
		}
		unless (findTag("</data>", cp, ep, lines, true)) {
			Py_DECREF(res);
			return NULL;
		}
	}
	unless (findTag("</array>", cp, ep, lines, true)) {
		Py_DECREF(res);
		return NULL;
	}

	return res;
}


/*
 * fix: doesn't handle null byte keys properly
 */
static PyObject *
decodeStruct(char **cp, char *ep, ulong *lines)
{
	PyObject	*res,
			*val;
	char		*tp,
			*key;
	uint		klen;

	res = PyDict_New();
	if (res == NULL)
		return NULL;
	unless (findTag("<struct>", cp, ep, lines, true)) {
		Py_DECREF(res);
		return NULL;
	}
	while (strncmp(*cp, "<member>", 8) == 0) {
		unless ((findTag("<member>", cp, ep, lines, true))
		and     (findTag("<name>", cp, ep, lines, false))) {
			Py_DECREF(res);
			return NULL;
		}
		tp = *cp;
		while (strncmp(*cp, "</name>", 7)) {	/* find name */
			if (**cp == '\n')
				(*lines)++;
			else if (*cp > ep) {
				Py_DECREF(res);
				eosErr();
			}
			(*cp)++;
		}
		klen = *cp - tp;
		key = alloc(klen + 1);		/* add null byte... */
		if (key == NULL) {
			Py_DECREF(res);
			return NULL;
		}
		strncpy(key, tp, klen);
		key[klen] = EOS;
		unless (findTag("</name>", cp, ep, lines, true)) {
			Py_DECREF(res);
			free(key);
			return NULL;
		}
		val = decodeValue(cp, ep, lines);	/* get value */
		if (val == NULL) {
			Py_DECREF(res);
			free(key);
			return NULL;
		}
		unless ((PyDict_SetItemString(res, key, val) == 0)
		and     (findTag("</member>", cp, ep, lines, true))) {
			Py_DECREF(res);
			free(key);
			Py_DECREF(val);
			return NULL;
		}
		free(key);
		Py_DECREF(val);
	}
	unless (findTag("</struct>", cp, ep, lines, true)) {
		Py_DECREF(res);
		return NULL;
	}

	return res;
}


/*
 * build a request for a remote procedure call
 */
PyObject *
buildRequest(char *url, char *method, PyObject *params, PyObject *addInfo)
{
	PyObject	*res,
			*elem;
	strBuff		*header,
			*body;
	int		i;

	assert(PySequence_Check(params));
	body = newBuff();
	if ((body == NULL)
	or  (buffConstant(body, "<?xml version=\"1.0\"?>") == NULL)
	or  (buffConstant(body, EOL) == NULL)
	or  (buffConstant(body, "<methodCall>") == NULL)
	or  (buffConstant(body, EOL) == NULL)
	or  (buffConstant(body, "\t<methodName>") == NULL)
	or  (buffConcat(body, method) == NULL)
	or  (buffConstant(body, "</methodName>") == NULL)
	or  (buffConstant(body, EOL) == NULL))
		return NULL;
	if ((buffConstant(body, "\t<params>") == NULL)
	or  (buffConstant(body, EOL) == NULL))
		return NULL;
	for (i = 0; i < PyObject_Length(params); ++i) {
		elem = PySequence_GetItem(params, i);
		if (elem == NULL)
			return NULL;
		if ((buffConstant(body, "\t\t<param>") == NULL)
		or  (buffConstant(body, EOL) == NULL)
		or  (buffRepeat(body, '\t', 3) == NULL)
		or  (encodeValue(body, elem, 3) == NULL)
		or  (buffConstant(body, EOL) == NULL)
		or  (buffConstant(body, "\t\t</param>") == NULL)
		or  (buffConstant(body, EOL) == NULL))
			return NULL;
		Py_DECREF(elem);
	}
	if ((buffConstant(body, "\t</params>") == NULL)
	or  (buffConstant(body, EOL) == NULL))
		return NULL;
	if (buffConstant(body, "</methodCall>") == NULL)
		return NULL;

	header = buildHeader(TYPE_REQ, url, addInfo, body->len);
	if ((header == NULL)
	or  (buffAppend(header, body->beg, body->len) == NULL))
		return NULL;
	res = PyString_FromStringAndSize(header->beg, header->len);
	freeBuff(header);
	freeBuff(body);

	return res;
}


/*
 * build a response to a remote procedure call
 */
PyObject *
buildResponse(PyObject *result, PyObject *addInfo)
{
	PyObject	*res;
	strBuff		*header,
			*body;

	body = newBuff();
	if ((body == NULL)
	or  (buffConstant(body, "<?xml version=\"1.0\"?>") == NULL)
	or  (buffConstant(body, EOL) == NULL)
	or  (buffConstant(body, "<methodResponse>") == NULL)
	or  (buffConstant(body, EOL) == NULL)
	or  (buffConstant(body, "\t<params>") == NULL)
	or  (buffConstant(body, EOL) == NULL)
	or  (buffConstant(body, "\t\t<param>") == NULL)
	or  (buffConstant(body, EOL) == NULL)
	or  (buffRepeat(body, '\t', 3) == NULL)
	or  (encodeValue(body, result, 3) == NULL)
	or  (buffConstant(body, EOL) == NULL)
	or  (buffConstant(body, "\t\t</param>") == NULL)
	or  (buffConstant(body, EOL) == NULL)
	or  (buffConstant(body, "\t</params>") == NULL)
	or  (buffConstant(body, EOL) == NULL)
	or  (buffConstant(body, "</methodResponse>") == NULL)
	or  (buffConstant(body, EOL) == NULL))
		return NULL;

	header = buildHeader(TYPE_RESP, NULL, addInfo, body->len);
	if ((header == NULL)
	or  (buffAppend(header, body->beg, body->len) == NULL))
		return NULL;
	res = PyString_FromStringAndSize(header->beg, header->len);
	freeBuff(header);
	freeBuff(body);

	return res;
}


/*
 * build a fault response
 */
PyObject *
buildFault(int errCode, char *errStr, PyObject *addInfo)
{
	PyObject	*error,
			*res;
	strBuff		*header,
			*body;

	error = Py_BuildValue("{s: i, s: s}",
			"faultCode", errCode,
			"faultString", errStr);
	if (error == NULL)
		return NULL;
	body = newBuff();
	if ((body == NULL)
	or  (buffConstant(body, "<?xml version=\"1.0\"?>") == NULL)
	or  (buffConstant(body, EOL) == NULL)
	or  (buffConstant(body, "<methodResponse>") == NULL)
	or  (buffConstant(body, EOL) == NULL)
	or  (buffConstant(body, "\t<fault>") == NULL)
	or  (buffConstant(body, EOL) == NULL)
	or  (buffRepeat(body, '\t', 2) == NULL)
	or  (encodeValue(body, error, 2) == NULL)
	or  (buffConstant(body, EOL) == NULL)
	or  (buffConstant(body, "\t</fault>") == NULL)
	or  (buffConstant(body, EOL) == NULL)
	or  (buffConstant(body, "</methodResponse>") == NULL))
		return NULL;
	Py_DECREF(error);

	header = buildHeader(TYPE_RESP, NULL, addInfo, body->len);
	if ((header == NULL)
	or  (buffAppend(header, body->beg, body->len) == NULL))
		return NULL;
	res = PyString_FromStringAndSize(header->beg, header->len);
	freeBuff(header);
	freeBuff(body);

	return res;
}



static strBuff *
buildHeader(int reqType, char *url, PyObject *addInfo, long bodyLen)
{
	PyObject	*items,
			*tup,
			*key,
			*val;
	strBuff		*header;
	char		buffLen[110];
	int		i;

	assert(PyDict_Check(addInfo));
	header = newBuff();
	if (header == NULL)
		return NULL;
	switch (reqType) {
	case TYPE_REQ:
		if ((buffConstant(header, "POST ") == NULL)
		or  (buffConcat(header, url) == NULL)
		or  (buffConstant(header, " HTTP/1.1") == NULL)
		or  (buffConstant(header, EOL) == NULL)
		or  (buffConstant(header, "User-Agent: ") == NULL)
		or  (buffConcat(header, XMLRPC_LIB_STR) == NULL)
		or  (buffConstant(header, EOL) == NULL))
			return NULL;
		break;
	case TYPE_RESP:
		if ((buffConstant(header, "HTTP/1.1 200 OK") == NULL)
		or  (buffConstant(header, EOL) == NULL)
		or  (buffConstant(header, "Server: ") == NULL)
		or  (buffConcat(header, XMLRPC_LIB_STR) == NULL)
		or  (buffConstant(header, EOL) == NULL))
			return NULL;
		break;
	}
	items = PyDict_Items(addInfo);
	if (items == NULL)
		return NULL;
	for (i = 0; i < PyObject_Length(items); ++i) {
		tup = PySequence_GetItem(items, i);
		assert(PyObject_Length(tup) == 2);
		key = PySequence_GetItem(tup, 0);
		val = PySequence_GetItem(tup, 1);
		if (not PyString_Check(key) || not PyString_Check(val))
			return setPyErr("header info keys and values must be strings");
		if ((buffConcat(header, PyString_AS_STRING(key)) == NULL)
		or  (buffConstant(header, ": ") == NULL)
		or  (buffConcat(header, PyString_AS_STRING(val)) == NULL)
		or  (buffConstant(header, EOL) == NULL))
			return NULL;
		Py_DECREF(tup);
		Py_DECREF(key);
		Py_DECREF(val);
	}
	Py_DECREF(items);
	sprintf(buffLen, "Content-length: %ld%s", bodyLen, EOL);
	if ((buffConstant(header, "Content-Type: text/xml") == NULL)
	or  (buffConstant(header, EOL) == NULL)
	or  (buffConcat(header, buffLen) == NULL)
	or  (buffConstant(header, EOL) == NULL))
		return NULL;

	return header;
}


PyObject *
parseRequest(PyObject *request)
{
	PyObject	*method,
			*addInfo,
			*params,
			*tuple;
	ulong		lines;
	char		*cp,
			*ep,
			*tp;

	lines = 1;
	cp = PyString_AS_STRING(request);
	ep = cp + PyObject_Length(request);
	addInfo = parseHeader(&cp, ep, &lines, TYPE_REQ);
	if (addInfo == NULL)
		return NULL;
	unless ((findXmlVersion(&cp, ep, &lines))
	and     (findTag("<methodCall>", &cp, ep, &lines, true))
	and     (findTag("<methodName>", &cp, ep, &lines, false))) {
		Py_DECREF(addInfo);
		return NULL;
	}
	tp = cp;
	for (; cp < ep; ++cp)
		if (*cp == '\n')
			lines++;
		else if (strncmp("</methodName>", cp, 13) == 0)
			break;
	if (cp >= ep) {
		Py_DECREF(addInfo);
		return eosErr();
	}
	method = PyString_FromStringAndSize(tp, cp - tp);
	if (method == NULL) {
		Py_DECREF(addInfo);
		return NULL;
	}
	unless (findTag("</methodName>", &cp, ep, &lines, true)) {
		Py_DECREF(method);
		Py_DECREF(addInfo);
		return NULL;
	}
	params = PyList_New(0);
	if (params == NULL) {
		Py_DECREF(method);
		Py_DECREF(addInfo);
		return NULL;
	}
	if ((strncmp("<params>", cp, 8) == 0)
	and (not parseParams(&cp, ep, &lines, params))) {
		Py_DECREF(method);
		Py_DECREF(addInfo);
		Py_DECREF(params);
		return NULL;
	}
	unless (findTag("</methodCall>", &cp, ep, &lines, false)) {
		Py_DECREF(method);
		Py_DECREF(addInfo);
		Py_DECREF(params);
		return NULL;
	}
	chompStr(&cp, ep, &lines);
	if (cp != ep) {
		Py_DECREF(method);
		Py_DECREF(addInfo);
		Py_DECREF(params);
		return setPyErr("unused data when parsing request");
	}

	tuple = Py_BuildValue("(O, O, O)", method, params, addInfo);
	Py_DECREF(method);
	Py_DECREF(params);
	Py_DECREF(addInfo);

	return tuple;
}


bool
parseParams(char **cpp, char *ep, ulong *linep, PyObject *params)
{
	ulong		lines;
	char		*cp;
	PyObject	*elem;
	int		r;

	cp = *cpp;
	lines = *linep;

	unless (findTag("<params>", &cp, ep, &lines, true))
		return false;
	while (strncmp(cp, "<param>", 7) == 0) {
		unless (findTag("<param>", &cp, ep, &lines, true))
			return false;
		elem = decodeValue(&cp, ep, &lines);
		if (elem == NULL)
			return false;
		r = PyList_Append(params, elem);
		Py_DECREF(elem);
		unless ((r == 0)
		and     (findTag("</param>", &cp, ep, &lines, true)))
			return false;
	}
	unless (findTag("</params>", &cp, ep, &lines, true))
		return false;
	*cpp = cp;
	*linep = lines;

	return true;
}


bool
doKeepAliveFromDict(PyObject *addInfo)
{
	PyObject	*pyVer,
			*pyCon;
	char		*con;
	double		version;
	bool		keepAlive;

	pyVer = PyDict_GetItemString(addInfo, "HTTP Version");
	if (pyVer == NULL) {
		Py_DECREF(addInfo);
		return false;
	}
	assert(PyFloat_Check(pyVer));
	version = PyFloat_AS_DOUBLE(pyVer);
	pyCon = PyDict_GetItemString(addInfo, "Connection");
	if (pyCon != NULL) {
		assert(PyString_Check(pyCon));
		con = PyString_AS_STRING(pyCon);
	} else
		con = NULL;

	keepAlive = false;
	if ((version == 1.0)
	and (con && strcasecmp(con, "keep-alive") == 0))
		keepAlive = true;
	if ((version == 1.1)
	and (con == NULL || strcasecmp(con, "close")))
		keepAlive = true;
	return (keepAlive);
}


bool
doKeepAlive(PyObject *header, int reqType)
{
	char		*cp;
	ulong		lines;
	PyObject	*addInfo;
	bool		keepAlive;

	cp = PyString_AsString(header);
	lines = 0;
	addInfo = parseHeader(&cp, cp+PyString_GET_SIZE(header),
				&lines, reqType);
	if (addInfo == NULL)
		return false;
	keepAlive = doKeepAliveFromDict(addInfo);
	Py_DECREF(addInfo);

	return keepAlive;
}


/*
 * storing the URI in the header information is ugly, but oh well...
 */
static PyObject *
parseHeader(char **cpp, char *ep, ulong *lines, int reqType)
{
	PyObject	*addInfo,
			*pyUri,
			*pyVersion;
	char 		*cp,
			*tp,
			method[256],
			error[256];
	double		version;

	cp = *cpp;
	pyUri = NULL;
	version = 0.0;

	switch (reqType) {
	case TYPE_REQ:
		tp = cp;
		for (tp = cp; *tp != ' '; ++tp) {
			if (*tp == '\n')
				return setPyErr("illegal Request-Line");
			if (tp + 1 == ep)
				return setPyErr("EOS reached early");
		}
		if (tp - cp > 255)
			return setPyErr("HTTP Method too long");
		strncpy(method, cp, tp-cp);
		method[tp-cp] = EOS;		/* get rid of the space */
		if (strcasecmp(method, "POST") != 0) {
			snprintf(error, 255, "unsupported HTTP Method: '%s'",
				method);
			return setPyErr(error);
		}
		cp = tp + 1;
		for (tp = cp; *tp != ' '; ++tp) {
			if (*tp == '\n')
				return setPyErr("illegal Request-Line");
			if (tp + 1 == ep)
				return setPyErr("EOS reached early");
		}
		pyUri = PyString_FromStringAndSize(cp, tp-cp);
		if (pyUri == NULL)
			return NULL;
		cp = tp + 1;
		if (strncmp(cp, "HTTP/1.0", 8) == 0)
			version = 1.0;
		else if (strncmp(cp, "HTTP/1.1", 8) == 0)
			version = 1.1;
		else {
			Py_XDECREF(pyUri);
			return setPyErr("illegal HTTP Version");
		}
		cp += 11;
		break;
	case TYPE_RESP:
		if (strncmp(cp, "HTTP/1.0 ", 9) == 0)
			version = 1.0;
		else if (strncmp(cp, "HTTP/1.1 ", 9) == 0)
			version = 1.1;
		else
			return setPyErr("illegal HTTP version");
		cp += 9;
		break;
	}
	while (cp <= ep and *cp != '\n')
		cp++;
	cp++;
	(*lines)++;
	if (cp > ep) {
		Py_XDECREF(pyUri);
		return eosErr();
	}
	addInfo = PyDict_New();
	if (addInfo == NULL) {
		Py_XDECREF(pyUri);
		return NULL;
	}
	pyVersion = PyFloat_FromDouble(version);
	if (pyVersion == NULL) {
		Py_XDECREF(addInfo);
		Py_XDECREF(pyUri);
		return NULL;
	}
	if (PyDict_SetItemString(addInfo, "HTTP Version", pyVersion)) {
		Py_XDECREF(addInfo);
		Py_XDECREF(pyUri);
		return NULL;
	}
	Py_DECREF(pyVersion);
	if (pyUri != NULL) {
		if (PyDict_SetItemString(addInfo, "URI", pyUri))
			return NULL;
		Py_DECREF(pyUri);
	}
	while (cp <= ep)
		if (*cp == '\n') {
			cp++;
			(*lines)++;
			break;
		} else if (cp[0] == '\r' && cp[1] == '\n') {
			cp += 2;
			(*lines)++;
			break;
		} else unless (parseHeaderLine(addInfo, &cp, ep, lines))
			return NULL;
	if (chompStr(&cp, ep, lines) > ep)
		return eosErr();
	*cpp = cp;

	return addInfo;
}


static bool
parseHeaderLine(PyObject *addInfo, char **cpp, char *ep, ulong *lines)
{
	PyObject	*name,
			*value;
	char		*cp,
			*sp,
			*tp;

	value = NULL;		/* to appease the compiler */
	cp = *cpp;
	tp = cp;
	while (*cp != ':' and cp <= ep)
		cp++;
	if (cp > ep)
		return (bool)eosErr();
	name = PyString_FromStringAndSize(tp, cp - tp);
	sp = PyString_AS_STRING(name);
	tp = sp + PyString_GET_SIZE(name);
	for (; sp < tp; ++sp) {				/* capitalize */
		if (sp == PyString_AS_STRING(name)) {
			if ('a' <= *sp && *sp <= 'z')
				*sp -= 'a' - 'A';
		} else
			if ('A' <= *sp && *sp <= 'Z')
				*sp += 'a' - 'A';
	}
	cp++;
	while ((cp <= ep)
	and    (*cp == '\t' || *cp == ' '))
		cp++;
	if (cp > ep)
		return (bool)eosErr();
	tp = cp;
	for (;cp <= ep; ++cp)
		if (*cp == '\n') {
			value = PyString_FromStringAndSize(tp, cp - tp);
			cp += 1;
			break;
		} else if (cp[0] == '\r' && cp[1] == '\n') {
			value = PyString_FromStringAndSize(tp, cp - tp);
			cp += 2;
			break;
		}
	if (cp > ep)
		return (bool)eosErr();
	if (value == NULL)
		return false;
	if (PyDict_SetItem(addInfo, name, value))
		return false;
	Py_DECREF(name);
	Py_DECREF(value);
	*cpp = cp;
	return true;
}


PyObject *
parseResponse(PyObject *request)
{
	PyObject	*tuple,
			*addInfo,
			*result;
	ulong		lines;
	char		*cp,
			*ep;

	lines = 1;
	cp = PyString_AS_STRING(request);
	ep = cp + PyObject_Length(request);
	addInfo = parseHeader(&cp, ep, &lines, TYPE_RESP);
	if (addInfo == NULL)
		return NULL;
	unless ((findXmlVersion(&cp, ep, &lines))
	and     (findTag("<methodResponse>", &cp, ep, &lines, true))) {
		Py_DECREF(addInfo);
		return NULL;
	}
	if (strncmp("<fault>", cp, 7) == 0) {
		Py_DECREF(addInfo);
		return parseFault(cp, ep, lines);
	}
	unless ((findTag("<params>", &cp, ep, &lines, true))
	and     (findTag("<param>", &cp, ep, &lines, true))) {
		Py_DECREF(addInfo);
		return NULL;
	}
	result = decodeValue(&cp, ep, &lines);
	if (result == NULL) {
		Py_DECREF(addInfo);
		return NULL;
	}
	unless ((findTag("</param>", &cp, ep, &lines, true))
	and     (findTag("</params>", &cp, ep, &lines, true))
	and     (findTag("</methodResponse>", &cp, ep, &lines, false))) {
		Py_DECREF(addInfo);
		Py_DECREF(result);
		return NULL;
	}
	chompStr(&cp, ep, &lines);
	if (cp != ep) {
		Py_DECREF(addInfo);
		Py_DECREF(result);
		return setPyErr("unused data when parsing response");
	}

	tuple = Py_BuildValue("(O, O)", result, addInfo);
	Py_DECREF(result);
	Py_DECREF(addInfo);

	return tuple;
}


static PyObject *
parseFault(char *cp, char *ep, long lines)
{
	PyObject	*errDict,
			*errStr,
			*errCode;

	unless (findTag("<fault>", &cp, ep, &lines, true))
		return NULL;
	errDict = decodeValue(&cp, ep, &lines);
	if (errDict == NULL)
		return NULL;
	unless ((PyDict_Check(errDict))
	and     (PyMapping_HasKeyString(errDict, "faultCode"))
	and     (PyMapping_HasKeyString(errDict, "faultString"))) {
		Py_DECREF(errDict);
		return setPyErr("illegal fault value");
	}
	errCode = PyDict_GetItemString(errDict, "faultCode");
	errStr = PyDict_GetItemString(errDict, "faultString");
	if (errCode == NULL || errStr == NULL)
		return NULL;
	unless (PyInt_Check(errCode) && PyString_Check(errStr)) {
		Py_DECREF(errDict);
		return setPyErr("illegal fault value");
	}
	rpcFaultRaise(errCode, errStr);
	Py_DECREF(errDict);
	unless ((findTag("</fault>", &cp, ep, &lines, true))
	and     (findTag("</methodResponse>", &cp, ep, &lines, false))) {
		return NULL;
	}
	chompStr(&cp, ep, &lines);
	if (cp != ep) {
		return setPyErr("unused data when parsing response");
	}

	return NULL;
}


static PyObject *
eosErr(void)
{
	return setPyErr("EOS error while decoding xml");
}


static PyObject *
syntaxErr(ulong line)
{
	char	buff[100];

	snprintf(buff, 100, "syntax error in line %ld\n", line);

	return setPyErr(buff);
}


static char *
chompStr(char **cp, char *ep, ulong *lines)
{
	for (; *cp < ep; ++(*cp)) {
		if (**cp == '\t' or **cp == ' ' or **cp == '\r')
			continue;
		else if (**cp == '\n')
			(*lines)++;
		else if ((ep - *cp >= TAG_LEN(COM_BEG))
		and      (strncmp(*cp, COM_BEG, TAG_LEN(COM_BEG)) == 0)) {
			*cp += TAG_LEN(COM_BEG);
			while (true) {
				if (ep - *cp < TAG_LEN(COM_END)) {
					*cp = ep;
					return ep;		/* no matches */
				}
				if (strncmp(*cp, COM_END, TAG_LEN(COM_END)) == 0) {
					*cp += TAG_LEN(COM_END);
					break;
				}
				(*cp)++;
			}
		} else
			return *cp;
	}
	return *cp;
}


bool
findXmlVersion(char **cpp, char *ep, ulong *lines)
{
	double	ver;
	char	*cp;

	cp = *cpp;
	unless (strncmp("<?xml version=", cp, 14) == 0) {
		setPyErr("bad xml version tag");
		return false;
	}
	cp += 14;
	unless  (*cp == '\'' || *cp == '\"') {
		setPyErr("bad xml version tag");
		return false;
	}
	cp++;
	unless (decodeActDouble(&cp, ep, &ver)) {
		setPyErr("bad xml version tag");
		return false;
	}
	while (strncmp(cp, "?>", 2)) {
		if ((cp >= ep)
		or  (*cp == '\n')) {
			setPyErr("bad xml version tag");
			return false;
		}
		++cp;
	}
	cp += 2;
	if (chompStr(&cp, ep, lines) > ep)
		return false;
	*cpp = cp;
	return true;
}


bool
findTag(char *tag, char **cpp, char *ep, ulong *lines, bool chomp)
{
	char		err[256];
	uint		l;

	l = strlen(tag);
	if (strncmp(*cpp, tag, l)) {
		snprintf(err, 255, "couldn't find %s tag in line %ld: %.30s",
			tag, *lines, *cpp);
		setPyErr(err);
		return false;
	}
	(*cpp) += l;
	if (chomp and chompStr(cpp, ep, lines) > ep) {
		(void)eosErr();
		return false;
	}
	return true;
}


static PyObject *
unescapeString(char *bp, char *ep)
{
	PyObject	*res;
	char		*newStr,
			*tp;
	int		remLen;
	ulong		tmp;

	if (ep == bp)
		return PyString_FromString("");
	assert(ep > bp);
	newStr = alloc(sizeof(*bp) * (ep - bp + 1));
	tp = newStr;
	while (bp < ep) {
		if (*bp == '&') {
			remLen = ep - bp;
			if ((remLen >= 4)
			and (strncmp(bp, "&lt;", 4) == 0)) {
				*(tp++) = '<';
				bp += 4;
				continue;
			}
			if ((remLen >= 4)
			and (strncmp(bp, "&gt;", 4) == 0)) {
				*(tp++) = '>';
				bp += 4;
				continue;
			}
			if ((remLen >= 3)
			and (strncmp(bp, "&&;", 3) == 0)) {
				*(tp++) = '&';
				bp += 3;
				continue;
			}
			if ((remLen >= 5)
			and (strncmp(bp, "&amp;", 5) == 0)) {
				*(tp++) = '&';
				bp += 5;
				continue;
			}
			if ((remLen >= 6)
			and (strncmp(bp, "&apos;", 6) == 0)) {
				*(tp++) = '\'';
				bp += 6;
				continue;
			}
			if ((remLen >= 6)
			and (strncmp(bp, "&quot;", 6) == 0)) {
				*(tp++) = '"';
				bp += 6;
				continue;
			}
			if ((remLen > 4)
			and (strncasecmp(bp, "&#x", 3) == 0)) {
				bp += 3;
				if (!decodeActLongHex(&bp, ep, &tmp))
					return setPyErr("Illegal quoted sequence");
				if (*bp++ != ';')
					return setPyErr("Illegal quoted sequence");
				*(tp++) = tmp;
				continue;
			}
			if ((remLen > 3)
			and (strncmp(bp, "&#", 2) == 0)) {
				bp += 3;
				if (!decodeActLong(&bp, ep, &tmp))
					return setPyErr("Illegal quoted sequence");
				if (*bp++ != ';')
					return setPyErr("Illegal quoted sequence");
				*(tp++) = tmp;
				continue;
 			}
			return setPyErr("Illegal quoted sequence");
		} else
			*(tp++) = *(bp++);
	}
	*tp = EOS;
	res = PyString_FromStringAndSize(newStr, tp-newStr);
	free(newStr);

	return res;
}


static PyObject *
escapeString(PyObject *oldStr)
{
	PyObject	*newStr;
	int		newLen, oldLen;
	char		*np,
			*op,
			*ep;

	assert(PyString_Check(oldStr));
	newLen = 0;
	oldLen = PyString_GET_SIZE(oldStr);
	ep = PyString_AS_STRING(oldStr) + oldLen;
	op = PyString_AS_STRING(oldStr);
	for (; op < ep; ++op) {
		switch (*op) {
		case '<' :
			newLen += 4;
			break;
		case '&' :
			newLen += 5;
			break;
		default	 :
			newLen++;
			break;
		}
	}
	/* check if we really need to work on this string */
	if (oldLen <= newLen) {
	    Py_INCREF(oldStr);	/* the calling function expects this to be a new object
				 * and calls Py_DECREF on it */
	    return oldStr;	/* avoid extra work because it is not necessary */
	}
	
	newStr = PyString_FromStringAndSize(NULL, newLen);
	if (newStr == NULL)
		return NULL;
	op = PyString_AS_STRING(oldStr);
	np = PyString_AS_STRING(newStr);
	for (; op < ep; ++op) {
		switch (*op) {
		case '<' :
			strncpy(np, "&lt;", 4);
			np += 4;
			break;
		case '&' :
			strncpy(np, "&amp;", 5);
			np += 5;
			break;
		default	 :
			*(np++) = *op;
			break;
		}
	}
	assert(np == (PyString_AS_STRING(newStr) + newLen));
	*np = EOS;	/* probably unnecessary (python already does it) */

	return newStr;
}
