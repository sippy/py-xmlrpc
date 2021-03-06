# Fast XMLRPC Python Library

This kit contains an implementation of the xmlrpc protocol written in C and
wrapped up in python.  The spec for the XMLRPC protocol can be found at
"http://www.xmlrpc.com".


## AUTHOR

Shilad Sen, Sourcelight Technologies Inc. <shilad.sen@sourcelight.com>

Sourcelight Technologies, Inc
906 University Place, Suite B-211
Evanston, IL 60201


Thanks to Chris Jensen for the windows compatability and to Pat Szuta for
the DateTime and Base64 objects.


## INSTALLATION

See the INSTALL file which should be included with this distribution.


## INFO

This is version 0.9.0 of the turbo_xmlrpc library, based off version
0.8.8.4b of the py-xmlrpc module from SourceForge.  There have been some
big changes since earlier versions.  Please see the CHANGELOG for version
information.

xmlrpc.py is pretty well documented.  If you want more documentation, at
this point you'll have to go to the source.  Some time I should document
everything better, especially the nonblocking aspects of the server and
client.


## FEATURES

* Efficiency:  This library seems to be 20x-100x faster than other xmlrpc
libraries I've tried.  I have benchmarked this library at about 1000 RPC
calls per second between two different celeron 466 machines.  Both speed
and memory use seem to be exponentially better (related to the size of
objects being sent across) than the existing Python libraries.

* Non-Blocking IO:  This becomes important when you have critical
applications (especially servers) that CANNOT be held up because a client
is trickling a request in or is reading a response at 1 byte per second.

* Event based:  This goes hand in hand with non-blocking IO.  It lets you
use an event based model for your client/server application, where the
main body is basically the select loop on the file descriptors you are
interested in.  The work is done by registering callbacks for different
events you received on file descriptors.  This can be very nice.  There is
some work to be done here, but all the hooks are in place.  See TO DO for
a list of the features that have yet to be incorporated.

* Portable: Source should be compatible on both windows and POSIX.

* Compatiblity: pyxmlrpclib.py is a drop in replacement for xmlrpclib.
To use it, simply change pyxmlrpclib instead of xmlrpclib.



## CONTENTS

```
README			- this file
CHANGELOG		- features / fixes
_xmlrpcmodule.so	- a python module written in C for xmlrpc
turbo_xmlrpc.py		- a python wrapper for the low-level module
example.py		- example python code
```

if you have a source distribution, there will be other files.


## LICENSE

A full copy of the LGPL license is included in the file COPYING.

This is an Pyton implementation of the xmlrpc spec from www.xmlrpc.com

Copyright (c) 2001 by Shilad Sen, Sourcelight Technologies, Inc

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
