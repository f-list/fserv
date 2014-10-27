![F-List](http://static.f-list.net/images/logo.png)

F-List chat server 
==================

[![Build Status](https://travis-ci.org/f-list/fserv.png?branch=master)](https://travis-ci.org/f-list/fserv)

**Language:** C++ with Lua logic glue.

Compiling notes
---------------

The code has been tested to compile and run on Ubuntu x86, Ubuntu x86\_64 and 
Gentoo x86_64 given the proper version of LuaJIT is used. Modifications to 
libjansson to remove the forced formatting of floats with .0 after whole 
numbers was done to properly support "integers" and still have automatic 
conversion from Lua, which only supports doubles.

It also compiles on Ubuntu 12.04 LTS x86_64, given that libjansson and
google-glog are manually built. 

Required third party libraries
------------------------------

* [LuaJIT](http://www.luajit.org/)
* [libev](http://software.schmorp.de/pkg/libev.html)
* [evhttpclient](https://github.com/jspotter/evhttpclient)
* [google-glog](https://code.google.com/p/google-glog/)
* [google-perftools (tcmalloc)](https://code.google.com/p/gperftools/) - *Optional with minor source and makefile modifications*
* [libjansson](http://www.digip.org/jansson/)
* [hiredis](https://github.com/antirez/hiredis)
* libicu - see local package manager
* libuuid - see local package manager (uuid/uuid-dev)
* curl - see local package manager
* [boost](http://www.boost.org/) - *Optional if you replace the intrusive 
  pointers with something of your own.*

Modifications to src/Makefile to reflect your install folders will likely be 
required. A more robust build system is welcomed, but was not required at the
time the project was written. (CMake?)

I ([@zwagoth](https://github.com/zwagoth)) learned something here: Always make 
comments at time of writing. To make up for this I will **document the basic 
structure of the program to assist in understanding.**

Apologies for the mess of a codebase. This was also one of my first C++
projects of any scale and complexity.

### script/main.lua

Contains almost all core logic for message handling of protocol messages, 
RTB messages, and various basic event callbacks such as connection and 
disconnection.

The file contains tables of anonymous functions that are named after the
events they handle. This file should not store any state and is considered a 
logic storage area only. The basic design of this file is to allow for glue 
logic without forcing Lua to do any heavy lifting and minimizing the amount of
data that is transferred in and out of Lua at the cost of more function calls
into C++.

There are four tables injected into the global file namespace with short names for ease of typing:

* `u`: connection/character(user) functions
* `s`: server/global state functions
* `c`: channel state functions
* `const`: error ids and constant values
	
Protocol callback functions accept two parameters, the connection that the 
command is associated with, and a table copy of the json arguments provided.

Connections are opaque numbers and should NOT be modified in any way. Changing
the value of a connection is unsafe. You have been warned.

### script/startup\_config.lua

A lua file of configuration variables that are used during startup and 
operation of the chat daemon.
		
### src/facceptor.cpp

A minimalistic flash policy server. If you need to support Flash, this is what
you run. Customize the inlined policy to your needs.

### src/fserv.cpp

Program entry point.
Deals with initialization of background threads and curl.

### src/server.cpp

Does way too many things. Code flow starts at `Server::run()`.

Connection flow is as follows:

    listenCallback
    handshakeCallback
    connectionWriteCallback
    connectionReadCallback
    connectionwriteCallback

`listenCallback` sets up per connection event handlers and passes flow into...

`handshakeCallback`, which handles reads for websocket handshakes.

`connectionWriteCallback` handles when a connection is ready to be written to 
and is enabled when items are in the queue to be written to the connection. 
Handles buffering.

`connectionReadCallback` handles all read events when the handshake phase is 
over. All protocol parsing happens here and commands are dispatched and run 
from inside this function.

`pingCallback` handles sending ping events to clients. If the protocol is 
changed, this should be one of the first things to go.

`connectionTimerCallback` is fired periodically and checks if the connection 
is dead and cleans it up.

`runLuaEvent` is newt magic. Also where the magic happens for each command. 
This is where each command shifts into Lua code from C++ code. Handles all Lua 
states and the callback for making sure Lua does not get caught in an infinite 
loop. Handles printing error from inside Lua.

### src/server\_state.cpp

This file stores all of the state data related to channels, connections, bans 
and moderation.

Connections are split between identified and unidentified as character names
are not known until the login server validates that they exist and to keep 
them out of the pool of characters that can be looked up by name.

Handles saving and restoring of state to disk.

### src/login\_curl.cpp

Runs as a background thread that processes login requests and replies and 
passes them back to the main thread.

Serialized login system, uses global login queue. This could be improved quite 
a bit.

Conversion to curl\_multi would be nice, but involves some fun interaction 
with libev or lots of blind polling. Queues must be locked before they are 
accessed, due to threading.

### src/channel.cpp

Basic channel class, maintains state data about a channel for its lifetime. 
All low level actions related to channels happen in this file through hooks in Lua. 
Usually wrapped in an intrusive pointer to manage instance lifetime.

This is where serialization and deserialization of channels from json happens.

### src/connection.cpp

Handles all connection networking and debug Lua states.

Maintains kink lists, status, status message and gender.

Maintains ignore and friend list.

Handles per connection throttles.

This is where buffering of output data happens.

Generally passed around using instrusive pointers to manage instance lifetime.

Maintains an internal list of which channels have been joined. This must be kept in sync with the actual channel user list.

### src/native\_commands.cpp

This file is reserved for the few functions that required raw speed over being customizable.
Handles login (`IDN`) command.
Handles debug (`ZZZ`) command.
Handles search (`FKS`) command.

### src/lua\_chat.cpp

All Lua wrapper commands that fall under the `s` category in Lua files.

### src/lua\_connection.cpp

All Lua wrapper commands that fall under the `u` category in Lua files.

### src/lua\_channel.cpp

All Lua wrapper commands that fall under the `c` category in Lua files.

### src/lua\_constants.cpp

All Lua wrapper values that fall under the `const` category in Lua files.

Error messages and definitions.

### src/lua\_*.cpp files:

Use the defined macros for error and type checking! This is the only way to 
prevent crashing if the incorrect type is passed as lightweight data into a 
function unexpectedly.

Make sure that your Lua stack balances. I've tried hard to make sure this is 
true, but mistakes are easy to make.

Avoid returning tables to Lua code, they are expensive to build and often have
a short timeframe of use. Use best judgement to balance cost of function calls
and cost of building tables.

Avoid passing strings into Lua needlessly. It can be costly due to memory 
copies.

Abuse that lua accepts multiple return values as a native feature. 
See above two notes.

### src/redis.cpp

Handles the push only redis thread. Is a small wrapper around redis commands 
and their return values.
	
Uses an input queue to receive commands. 
Commands are run in a periodic fashion and have no guarantee of reliability.
Can be disabled, and ignores input when disabled.
	
Debugging notes
---------------

gdb works well for debugging this application. Disabling tcmalloc and the JIT
section of LuaJIT should greatly assist in debugging crashes (tcmalloc can hide
some minor heap corruption).

The memory profiling tools in tcmalloc are quite nice. Refer to the tcmalloc 
documentation for more information on how to use it.


