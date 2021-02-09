/*
 * Copyright (C) 2001, Shilad Sen, Sourcelight Technologies, Inc.
 * See xmlrpc.h or the README for more copyright information.
 */


#include <assert.h>
#include "xmlrpc.h"
#include "rpcInternal.h"

static  void            rpcBase64Dealloc(rpcBase64 *bp);
static  PyObject        *rpcBase64Repr(rpcBase64 *bp);
static  PyObject        *rpcBase64Str(rpcBase64 *bp);
static	PyObject	*rpcBase64GetAttr(rpcBase64 *bp, char *name);
static	int		rpcBase64SetAttr(
				rpcBase64 *bp,
				char *name,
				PyObject *pyval
			);



/****************************************************************
 *                                                              *
 *   The following code was ruthlessly stolen from binascii.c   *
 *   Jack Jansen is the listed author of the module             *
 *                                                              *
 ****************************************************************/


static	PyObject	*binascii_b2a_base64(PyObject *self, PyObject *args);
static	PyObject	*binascii_a2b_base64(PyObject *self, PyObject *args);


static PyObject *Error;

static char table_a2b_base64[] = {
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	-1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
	52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1, 0,-1,-1, /* Note PAD->0 */
	-1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
	15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
	-1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
	41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};

#define BASE64_PAD '='
#define BASE64_MAXBIN 57	/* Max binary chunk size (76 char line) */

static unsigned char table_b2a_base64[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


static PyObject *
binascii_a2b_base64(self, args)
	PyObject *self;
        PyObject *args;
{
	unsigned char *ascii_data, *bin_data;
	int leftbits = 0;
	unsigned char this_ch;
	unsigned int leftchar = 0;
	int npad = 0;
	PyObject *rv;
	int ascii_len, bin_len;
	
	if ( !PyArg_ParseTuple(args, "t#", &ascii_data, &ascii_len) )
		return NULL;

	bin_len = ((ascii_len+3)/4)*3; /* Upper bound, corrected later */

	/* Allocate the buffer */
	if ( (rv=PyString_FromStringAndSize(NULL, bin_len)) == NULL )
		return NULL;
	bin_data = (unsigned char *)PyString_AsString(rv);
	bin_len = 0;
	for( ; ascii_len > 0 ; ascii_len--, ascii_data++ ) {
		/* Skip some punctuation */
		this_ch = (*ascii_data & 0x7f);
		if ( this_ch == '\r' || this_ch == '\n' || this_ch == ' ' )
			continue;
		
		if ( this_ch == BASE64_PAD )
			npad++;
		this_ch = table_a2b_base64[(*ascii_data) & 0x7f];
		if ( this_ch == (unsigned char) -1 ) continue;
		/*
		** Shift it in on the low end, and see if there's
		** a byte ready for output.
		*/
		leftchar = (leftchar << 6) | (this_ch);
		leftbits += 6;
		if ( leftbits >= 8 ) {
			leftbits -= 8;
			*bin_data++ = (leftchar >> leftbits) & 0xff;
			leftchar &= ((1 << leftbits) - 1);
			bin_len++;
		}
	}
	/* Check that no bits are left */
	if ( leftbits ) {
		PyErr_SetString(Error, "Incorrect padding");
		Py_DECREF(rv);
		return NULL;
	}
	/* and remove any padding */
	bin_len -= npad;
	/* and set string size correctly */
	_PyString_Resize(&rv, bin_len);
	return rv;
}

	
static PyObject *
binascii_b2a_base64(self, args)
	PyObject *self;
        PyObject *args;
{
	unsigned char *ascii_data, *bin_data;
	int leftbits = 0;
	unsigned char this_ch;
	unsigned int leftchar = 0;
	PyObject *rv;
	int bin_len;
	
	if ( !PyArg_ParseTuple(args, "s#", &bin_data, &bin_len) )
		return NULL;
	
	/* We're lazy and allocate to much (fixed up later) */
	if ( (rv=PyString_FromStringAndSize(NULL, bin_len*2)) == NULL )
		return NULL;
	ascii_data = (unsigned char *)PyString_AsString(rv);

	for( ; bin_len > 0 ; bin_len--, bin_data++ ) {
		/* Shift the data into our buffer */
		leftchar = (leftchar << 8) | *bin_data;
		leftbits += 8;

		/* See if there are 6-bit groups ready */
		while ( leftbits >= 6 ) {
			this_ch = (leftchar >> (leftbits-6)) & 0x3f;
			leftbits -= 6;
			*ascii_data++ = table_b2a_base64[this_ch];
		}
	}
	if ( leftbits == 2 ) {
		*ascii_data++ = table_b2a_base64[(leftchar&3) << 4];
		*ascii_data++ = BASE64_PAD;
		*ascii_data++ = BASE64_PAD;
	} else if ( leftbits == 4 ) {
		*ascii_data++ = table_b2a_base64[(leftchar&0xf) << 2];
		*ascii_data++ = BASE64_PAD;
	} 
	*ascii_data++ = '\n';	/* Append a courtesy newline */
	
	_PyString_Resize(&rv, (ascii_data -
			       (unsigned char *)PyString_AsString(rv)));
	return rv;
}

/****************************************************************
 *                                                              *
 *                    END OF STOLEN CODE                        *
 *                                                              *
 ****************************************************************/


/*
 * create a new edb base64 object
*/

PyObject *
rpcBase64New(PyObject *po)
{
	rpcBase64	*bp;

	bp = PyObject_NEW(rpcBase64, &rpcBase64Type);
	if (bp == NULL)
		return (NULL);
	Py_INCREF(po);
	bp->value = po;	
	return (PyObject *)bp;
}


char *
rpcBase64Encode(PyObject *str)
{
	PyObject	*arg,
			*pystr;
	char		*encstr;
	int		slen;

	arg = Py_BuildValue("(S)", str);
	unless(arg)
		return (NULL);
	pystr = binascii_b2a_base64(NULL, arg);
	Py_DECREF(arg);
	if (pystr == NULL)
		return (NULL);
	assert(PyString_Check(pystr));
	slen = PyString_GET_SIZE(pystr);
	encstr = alloc(sizeof(*str) * slen + 1);
	encstr[slen] = EOS;
	if (encstr == NULL)
		return NULL;
	memcpy(encstr, PyString_AS_STRING(pystr), slen);
	Py_DECREF(pystr);
	encstr[slen-1] = '\0';		/* remove newline (??FIX??) */

	return encstr;
}


PyObject *
rpcBase64Decode(PyObject *str)
{
	PyObject	*args,
			*ret;

	args = Py_BuildValue("(O)", str);
	if (args == NULL)
		return NULL;
	ret = binascii_a2b_base64(NULL, args);
	Py_DECREF(args);

	return ret;
}


/*
 * free resources associated with a base64 object
*/

static void
rpcBase64Dealloc(rpcBase64 *bp)
{
	if (bp->value) {
		Py_DECREF(bp->value);
	}
	PyObject_DEL(bp);
}


/*
 * represent a base64 xml object
*/
static PyObject *
rpcBase64Str(rpcBase64 *bp)
{
	Py_INCREF(bp->value);
	return bp->value;
}


/*
 * convert a base64 xml to a string object
*/
static PyObject *
rpcBase64Repr(rpcBase64 *bp)
{
	char		*buff;
	PyObject	*repr,
			*res;
	
	repr = PyObject_Repr(bp->value);
	if (repr == NULL)
		return (NULL);
	assert(PyString_Check(repr));
	buff = alloc(PyString_GET_SIZE(repr) + strlen("base64()") + 1);
	Py_DECREF(repr);
	sprintf(buff, "base64(%s)", PyString_AS_STRING(repr));
	res = PyString_FromString(buff);
	free(buff);
	return (res);
}


static PyMethodDef rpcBase64Methods[] = {
	{ NULL, NULL },
};


static	PyObject
*rpcBase64GetAttr(rpcBase64 *bp, char *name)
{
	assert(bp != NULL);
	assert(name != NULL);
	if (strcmp("data", name) == 0) {
		Py_INCREF(bp->value);
		return bp->value;
	}
	return Py_FindMethod(rpcBase64Methods, (PyObject *)bp, name);
}


static	int
rpcBase64SetAttr(rpcBase64 *bp, char *name, PyObject *value)
{
	assert(bp != NULL);
	assert(name != NULL);
	if (strcmp("data", name) == 0) {
		unless (PyString_Check(value)) {
			PyErr_SetString(PyExc_TypeError, "data must be string");
			return -1;
		}
		if (bp->value) {
			Py_DECREF(bp->value);
		}
		Py_INCREF(bp->value);
		bp->value = value;
		return 0;
	} else {
		PyErr_SetString(PyExc_AttributeError, "unknown attribute");
		return -1;
	}
	return 0;
}


/*
 * map characteristics of the base64 edb object 
*/
PyTypeObject rpcBase64Type = {
	PyObject_HEAD_INIT(0)
	0,
	"rpcBase64",
	sizeof(rpcBase64),
	0,
	(destructor)rpcBase64Dealloc,
	0,
	(getattrfunc)rpcBase64GetAttr,
	(setattrfunc)rpcBase64SetAttr,
	0,
	(reprfunc)rpcBase64Repr,
	0,
	0,
	0,
	0,
	0,
	(reprfunc)rpcBase64Str,
	0,
	0,
	0,
	0,
	0,
};
