CXXFLAGS=	-g -O2 -Wall -Werror -fno-strict-aliasing -I/usr/local/include -I../lib/lua/src
LDFLAGS=	-g -L/usr/local/lib -L../lib/lua/src -L../lib/glog_install/lib -levhttpclient -lpthread -lrt -lev -lm -lluajit -lglog -ljansson -lcurl -lhiredis -licuuc -licudata -ltcmalloc
FSERV_O=	channel.o connection.o fserv.o login_evhttp.o lua_channel.o lua_chat.o lua_connection.o lua_constants.o messagebuffer.o native_command.o redis.o server.o server_state.o startup_config.o unicode_tools.o websocket.o base64.o md5.o modp_b64.o sha1.o
PRECOMP_GCH=	precompiled_headers.hpp.gch
FACCEPTOR_O=	facceptor.o
FACCEPTOR_LDFLAGS=	-lev
FSERV_SCRIPTS=	startup_config.lua main.lua

.cpp.o:
	@echo "CC $<"
	@$(CXX) -c $(CXXFLAGS) $< -o $@

%.hpp.gch: %.hpp
	@echo "PRECOMPILE $@"
	@${CXX} -g -I../lib/lua/src -Wall -Werror -fno-strict-aliasing -c $< -o $@

%.lua:
	@echo "INSTALL script/$@"
	@cp ../script/$@ ../bin/script/$@

all: facceptor fserv

fserv: $(PRECOMP_GCH) $(FSERV_O)
	@echo "LD $@"
	@$(CXX) $(FSERV_O) $(LDFLAGS) -o $@

facceptor: $(FACCEPTOR_O)
	@echo "LD $@"
	@$(CXX) $(FACCEPTOR_O) $(FACCEPTOR_LDFLAGS) -o $@

install_folders:
	@echo "MKDIR bin"
	@mkdir -p ../bin
	@echo "MKDIR bin/script"
	@mkdir -p ../bin/script
	@mkdir -p ../bin/logs

install: fserv facceptor install_folders $(FSERV_SCRIPTS)
	@echo "INSTALL fserv"
	@cp fserv ../bin/fserv
	@echo "INSTALL facceptor"
	@cp facceptor ../bin/facceptor

clean:
	@echo "CLEAN *~ *.o *.gch fserv facceptor"
	@rm -f *~ *.o *.gch fserv facceptor
