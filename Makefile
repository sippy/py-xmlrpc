CC		= gcc
SRC		= ./src
LIB		= .
DEBUG		= -g
OPTIMIZE	= -O2
PYTHON		= 1.5
PYTHONDIR	= /usr/lib/python$(PYTHON)
PYTHONLIB	= $(PYTHONDIR)/site-packages
PYTHONINC	= /usr/include/python$(PYTHON)
CCSHARED	= -fPIC
INCLUDES	= -I. -I$(PYTHONINC)
GCCWARN		= -Wall -Wstrict-prototypes
CFLAGS		= $(DEBUG) $(GCCWARN) $(OPTIMIZE) $(INCLUDES) $(CCSHARED)
LDSHARED	= -shared
LDLIBS		=
INSTALL_LIB	= install -m 755
INSTALL_HEAD	= install -m 644

all:		xmlrpcmodule.so

xmlrpcmodule.so:	$(SRC)/xmlrpcmodule.o $(SRC)/xmlrpc.o \
		$(SRC)/rpcBase64.o $(SRC)/rpcBoolean.o \
		$(SRC)/rpcClient.o $(SRC)/rpcDate.o $(SRC)/rpcDispatch.o \
		$(SRC)/rpcInternal.o $(SRC)/rpcServer.o $(SRC)/rpcSource.o \
		$(SRC)/rpcUtils.o $(SRC)/rpcFault.o
		$(CC) $(CFLAGS) $(LDSHARED) -o $(LIB)/_xmlrpcmodule.so \
		$(SRC)/xmlrpcmodule.o $(SRC)/xmlrpc.o \
		$(SRC)/rpcBase64.o $(SRC)/rpcBoolean.o $(SRC)/rpcClient.o \
		$(SRC)/rpcDate.o $(SRC)/rpcDispatch.o $(SRC)/rpcInternal.o \
		$(SRC)/rpcServer.o $(SRC)/rpcSource.o $(SRC)/rpcUtils.o \
		$(SRC)/rpcFault.o \
		$(LDLIBS) 

xmlrpcmodule.o:	$(SRC)/xmlrpc.o
		$(CC) $(CFLAGS) -o $(SRC)/xmlrpcmodule.o -c $(SRC)/xmlrpcmodule.c

xmlrpc.o:	$(SRC)/rpcBase64.o $(SRC)/rpcBoolean.o \
		$(SRC)/rpcClient.o $(SRC)/rpcDate.o $(SRC)/rpcDispatch.o \
		$(SRC)/rpcInternal.o $(SRC)/rpcServer.o $(SRC)/rpcSource.o \
		$(SRC)/rpcUtils.o $(SRC)/rpcFault.o
		$(CC) $(CFLAGS) -o $(SRC)/xmlrpc.o $(SRC)/xmlrpc.c

install:
		cp $(LIB)/_xmlrpcmodule.so $(LIB)/xmlrpc.py $(PYTHONLIB)
		chown root.root $(PYTHONLIB)/_xmlrpcmodule.so
		chown root.root $(PYTHONLIB)/xmlrpc.py
		chmod 644 $(PYTHONLIB)/_xmlrpcmodule.so
		chmod 644 $(PYTHONLIB)/xmlrpc.py


rpcBase64.o:	$(SRC)/rpcBase64.h
rpcBoolean.o:	$(SRC)/rpcBoolean.h
rpcClient.o:	$(SRC)/rpcClient.h
rpcFault.o:	$(SRC)/rpcFault.h
rpcDate.o:	$(SRC)/rpcDate.h
rpcDispatch.o:	$(SRC)/rpcDispatch.h
rpcInternal.o:	$(SRC)/rpcInternal.h
rpcServer.o:	$(SRC)/rpcServer.h
rpcSource.o:	$(SRC)/rpcSource.h
rpcUtils.o:	$(SRC)/rpcUtils.h

clean:
		rm -f $(SRC)/*.o
		rm -f $(LIB)/*.so
		rm -f $(LIB)/*.pyc
		rm -f $(LIB)/*.pyo
