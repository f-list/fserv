CXXFLAGS= -g -O2 -DNDEBUG
LDFLAGS= -g
OUTDIR= release/
ifdef DEBUG
	CXXFLAGS= -g -O0 -DDEBUG
	LDFLAGS= -g
	OUTDIR = debug/
endif
TARGETDIR= ../$(OUTDIR)
INSTALLDIR= ../bin/

CXXFLAGS+=	-Wall -Werror -fno-strict-aliasing -I/usr/include/luajit-2.0 -I/usr/local/include -I../lib/lua/src -I../lib/evhttp
LDFLAGS+=	-L/usr/local/lib -L../lib/lua/src -L../lib/glog_install/lib -L../lib/evhttp -levhttpclient -lpthread -lrt -lev -lm -lluajit-5.1 -lglog -ljansson -lcurl -lhiredis -licuuc -licudata -ltcmalloc -luuid

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
	@${CXX} $(CXXFLAGS) -c $< -o $(TARGETDIR)$@

%.lua:
	@echo "copy ../script/$@ to $(TARGETDIR)script/$@"
	@cp ../script/$@ $(TARGETDIR)script/$@

all: facceptor fserv

fserv: outdir_folders $(PRECOMP_GCH) $(FSERV_OBJECTS)
	@echo "ld $(CXX) $(TARGETDIR)$@"
	@$(CXX) $(FSERV_OBJECTS) $(LDFLAGS) -o $(TARGETDIR)$@

facceptor: outdir_folders $(FACCEPTOR_OBJECTS)
	@echo "ld $(CXX) $(TARGETDIR)$@"
	@$(CXX) $(FACCEPTOR_OBJECTS) $(FACCEPTOR_LDFLAGS) -o $(TARGETDIR)$@


outdir_folders:
	@echo "Creating $(TARGETDIR) ..."
	@mkdir -p $(TARGETDIR)
	@echo "Creating $(TARGETDIR)script ..."
	@mkdir -p $(TARGETDIR)script
	@echo "Creating $(TARGETDIR)logs ..."
	@mkdir -p $(TARGETDIR)logs

install_folders:
	@echo "Creating $(INSTALLDIR) ..."
	@mkdir -p $(INSTALLDIR)
	@echo "Creating $(INSTALLDIR)script ..."
	@mkdir -p $(INSTALLDIR)script
	@echo "Creating $(INSTALLDIR)logs ..."
	@mkdir -p $(INSTALLDIR)logs

install: fserv facceptor install_folders deploy
	@echo "Installing fserv from $(TARGETDIR) to $(INSTALLDIR) ..."
	@cp $(TARGETDIR)fserv $(INSTALLDIR)
	@echo "Installing facceptor from $(TARGETDIR) to $(INSTALLDIR) ..."
	@cp $(TARGETDIR)facceptor $(INSTALLDIR)facceptor
	@echo "Installing '$(FSERV_SCRIPTS)' from $(TARGETDIR)script to $(INSTALLDIR)script ..."
	@cp -r $(TARGETDIR)script/ $(INSTALLDIR)

deploy: $(FSERV_SCRIPTS)

clean:
	@echo "Cleaning fserv, facceptor ..."
	rm -f *~ $(TARGETDIR)*.o $(TARGETDIR)*.gch $(TARGETDIR)fserv $(TARGETDIR)facceptor
	rm -fr $(TARGETDIR)script $(TARGETDIR)logs
