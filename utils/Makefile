CXXFLAGS= -g -O3 -DNDEBUG
OUTDIR = release/
ifdef DEBUG
	CXXFLAGS= -g -O0 -DDEBUG
	OUTDIR = debug/
endif
TARGETDIR = ../$(OUTDIR)

CXXFLAGS+=	-Wall -Werror
LDFLAGS+=	-lpthread
FACCEPTOR_STRESS_O=	facceptor_stress.o
FACCEPTOR_STRESS_OBJECTS= $(FACCEPTOR_STRESS_O:%.o=$(TARGETDIR)%.o)

$(TARGETDIR)%.o: %.cpp
	@echo "$(CXX) $(TARGETDIR)$<"
	@$(CXX) -c $(CXXFLAGS) $< -o $(TARGETDIR)$@

all: facceptor_stress

facceptor_stress: outdir_folders $(FACCEPTOR_STRESS_OBJECTS)
	@echo "ld $(CXX) $(TARGETDIR)$@"
	@$(CXX) $(FACCEPTOR_STRESS_OBJECTS) $(LDFLAGS) -o $(TARGETDIR)$@

outdir_folders:
	@echo "Creating $(TARGETDIR) ..."
	@mkdir -p $(TARGETDIR)

clean:
	@echo "CLEAN"
	rm -f $(TARGETDIR)*~ $(TARGETDIR)*.o $(TARGETDIR)facceptor_stress

install:

deploy:
