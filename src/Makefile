CXXFLAGS= -g -O3 -DNDEBUG
OUTDIR = release/
ifdef DEBUG
	CXXFLAGS= -O0 -DDEBUG
	OUTDIR = debug/
endif

CXXFLAGS+=	-g -O2 -Wall -Werror -fno-strict-aliasing -I/usr/local/include -I../lib/lua/src -I../lib/evhttp
LDFLAGS+=	-g -L/usr/local/lib -L../lib/lua/src -L../lib/glog_install/lib -L../lib/evhttp -levhttpclient -lpthread -lrt -lev -lm -lluajit -lglog -ljansson -lcurl -lhiredis -licuuc -licudata -ltcmalloc


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
	@cp ../script/$@ ../$(OUTDIR)/script/$@

all: facceptor fserv

fserv: outdir_folders $(PRECOMP_GCH) $(FSERV_O)
	@echo "LD $(OUTDIR)$@"
	@$(CXX) $(FSERV_O) $(LDFLAGS) -o ../$(OUTDIR)$@

facceptor: outdir_folders $(FACCEPTOR_O)
	@echo "LD $(OUTDIR)$@"
	@$(CXX) $(FACCEPTOR_O) $(FACCEPTOR_LDFLAGS) -o ../$(OUTDIR)$@


outdir_folders:
	@echo "MKDIR $(OUTDIR)"
	@mkdir -p ../$(OUTDIR)
	@echo "MKDIR $(OUTDIR)script"
	@mkdir -p ../$(OUTDIR)script
	@echo "MKDIR $(OUTDIR)logs"
	@mkdir -p ../$(OUTDIR)logs

install_folders:
	@echo "MKDIR bin"
	@mkdir -p ../bin
	@echo "MKDIR bin/script"
	@mkdir -p ../bin/script
	@echo "MKDIR bin/logs"
	@mkdir -p ../bin/logs

install: fserv facceptor install_folders $(FSERV_SCRIPTS)
	@echo "INSTALL fserv"
	@cp $(OUTDIR)fserv ../bin/fserv
	@echo "INSTALL facceptor"
	@cp $(OUTDIR)facceptor ../bin/facceptor
	@cp ../$(OUTDIR)/script/$@ ../bin/script/$@

clean:
	@echo "CLEAN *~ *.o *.gch fserv facceptor"
	@rm -f *~ *.o *.gch $(OUTDIR)fserv $(OUTDIR)facceptor
