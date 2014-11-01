CXXFLAGS= -g -O2 -DNDEBUG
LDFLAGS= -g
OUTDIR = release/
ifdef DEBUG
	CXXFLAGS= -O0 -DDEBUG
	LDFLAGS= -g
	OUTDIR = debug/
endif
TARGETDIR = ../$(OUTDIR)

CXXFLAGS+=	-Wall -Werror -fno-strict-aliasing -I/usr/local/include -I../lib/lua/src -I../lib/evhttp
LDFLAGS+=	-L/usr/local/lib -L../lib/lua/src -L../lib/glog_install/lib -L../lib/evhttp -levhttpclient -lpthread -lrt -lev -lm -lluajit -lglog -ljansson -lcurl -lhiredis -licuuc -licudata -ltcmalloc

FSERV_O=	channel.o connection.o fserv.o login_evhttp.o lua_channel.o lua_chat.o lua_connection.o lua_constants.o messagebuffer.o native_command.o redis.o server.o server_state.o startup_config.o unicode_tools.o websocket.o base64.o md5.o modp_b64.o sha1.o
PRECOMP_GCH=	$(TARGETDIR)precompiled_headers.hpp.gch
FACCEPTOR_O=	facceptor.o
FACCEPTOR_LDFLAGS=	-lev
FSERV_SCRIPTS=	startup_config.lua main.lua
FSERV_OBJECTS= $(FSERV_O:%.o=$(TARGETDIR)%.o)
FACCEPTOR_OBJECTS= $(FACCEPTOR_O:%.o=$(TARGETDIR)%.o)

$(TARGETDIR)%.o: %.cpp
	@echo "$(CXX) $<"
	@$(CXX) -c $(CXXFLAGS) $< -o $(TARGETDIR)$@

$(TARGETDIR)%.hpp.gch: %.hpp
	@echo "$(CXX)-precompile $@"
	@${CXX} -g -I../lib/lua/src -Wall -Werror -fno-strict-aliasing -c $< -o $(TARGETDIR)$@

%.lua:
	@echo "INSTALL script/$@"
	@cp ../script/$@ $(TARGETDIR)/script/$@

all: facceptor fserv

fserv: outdir_folders $(PRECOMP_GCH) $(FSERV_OBJECTS) $(FSERV_SCRIPTS)
	@echo "ld $(CXX) $(TARGETDIR)$@"
	@$(CXX) $(FSERV_OBJECTS) $(LDFLAGS) -o $(TARGETDIR)$@

facceptor: outdir_folders $(FACCEPTOR_OBJECTS)
	@echo "ld $(CXX) $(TARGETDIR)$@"
	@$(CXX) $(FACCEPTOR_OBJECTS) $(FACCEPTOR_LDFLAGS) -o $(TARGETDIR)$@


outdir_folders:
	@echo "MKDIR $(TARGETDIR)"
	@mkdir -p $(TARGETDIR)
	@echo "MKDIR $(TARGETDIR)script"
	@mkdir -p $(TARGETDIR)script
	@echo "MKDIR $(TARGETDIR)logs"
	@mkdir -p $(TARGETDIR)logs

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
	@cp $(TARGETDIR)facceptor ../bin/facceptor
	@cp $(TARGETDIR)/script/$@ ../bin/script/$@

clean:
	@echo "CLEAN"
	rm -f *~ $(TARGETDIR)*.o $(TARGETDIR)*.gch $(TARGETDIR)fserv $(TARGETDIR)facceptor
	rm -fr $(TARGETDIR)script $(TARGETDIR)logs
