# Makefile for the _xmlrpc module
#
# $Id$

# locations
SRC		= ./src
LIB		= .

# python installation
PYTHON		= 1.5
PYTHONDIR	= /usr/lib/python$(PYTHON)
PYTHONLIB	= $(PYTHONDIR)/site-packages
PYTHONINC	= /usr/include/python$(PYTHON)

# compiler things
CC		= gcc
DEBUG		= -g
OPTIMIZE	= -O6 -fomit-frame-pointer
CCSHARED	= -fPIC
INCLUDES	= -I. -I$(PYTHONINC)
GCCWARN		= -Wall -Wstrict-prototypes
CFLAGS		= $(DEBUG) $(GCCWARN) $(OPTIMIZE) $(INCLUDES) $(CCSHARED)
LDSHARED	= -shared
LDLIBS		=

# installation
INSTALL_SO	= install -m 755
INSTALL_PY	= install -m 644

# which files we compile
SOURCES		= xmlrpcmodule xmlrpc rpcBase64 rpcBoolean rpcClient rpcDate \
		  rpcDispatch rpcInternal rpcServer rpcSource rpcUtils rpcFault \
		  rpcPostpone
OBJECTS		= $(addprefix $(SRC)/,$(addsuffix .o,$(SOURCES)))

MODULE		= $(LIB)/_xmlrpcmodule.so

all:		$(MODULE)

$(MODULE): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDSHARED) -o $@ $(OBJECTS) $(LDLIBS)

install: $(MODULE) xmlrpc.py
	$(INSTALL_SO) $(MODULE) $(PYTHONLIB)
	$(INSTALL_PY) xmlrpc.py $(PYTHONLIB)

# compile rule: object with .c source and header
%.o : %.c %.h Makefile
	$(CC) $(CFLAGS) -c $< -o $@
# compile rule: object with .c source and no header
%.o : %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SRC)/*.o
	rm -f $(LIB)/*.so
	rm -f $(LIB)/*.pyc
	rm -f $(LIB)/*.pyo
