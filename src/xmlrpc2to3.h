/*
 * A python implementation of the xmlrpc spec from www.xmlrpc.com
 *
 * Copyright (C) 2021, Sippy Software, Inc.
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

#ifndef _XMLRPC2TO3_H_
#define _XMLRPC2TO3_H_

#define PyString_FromString PyUnicode_FromString
#define PyString_FromStringAndSize PyUnicode_FromStringAndSize
#define PyString_AsString PyBytes_AS_STRING
#define PyString_AS_STRING PyBytes_AS_STRING
#define PyInt_FromLong PyLong_FromLong
#define PyInt_Check PyLong_Check
#define PyInt_CheckExact PyLong_CheckExact
#define PyInt_AsLong PyLong_AsLong
#define PyInt_AS_LONG PyLong_AsLong
#define PyString_GET_SIZE PyBytes_Size
#define PyString_Concat PyBytes_Concat
#define PyString_ConcatAndDel PyBytes_ConcatAndDel
#define _PyString_Resize _PyBytes_Resize
#define PyString_Check PyUnicode_Check
#define PyObject_Compare(inst, obj) ((inst) == (obj))

static inline PyObject *
Py_FindMethod(PyMethodDef *ml, PyObject *self, const char *name) {
	for (; ml->ml_name != NULL; ml++) {
		if (name[0] == ml->ml_name[0] &&
		    strcmp(name+1, ml->ml_name+1) == 0)
			return PyCFunction_New(ml, self);
	}
	return NULL;
}


#endif /* _XMLRPC2TO3_H_ */
