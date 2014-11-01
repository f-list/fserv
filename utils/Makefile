CXXFLAGS= -g -O3 -DNDEBUG
OUTDIR = release/
ifdef DEBUG
	CXXFLAGS= -O0 -DDEBUG
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

facceptor_stress: $(FACCEPTOR_STRESS_OBJECTS)
	@echo "ld $(CXX) $(TARGETDIR)$@"
	@$(CXX) $(FACCEPTOR_STRESS_OBJECTS) $(LDFLAGS) -o $(TARGETDIR)$@

clean:
	@echo "CLEAN"
	rm -f $(TARGETDIR)*~ $(TARGETDIR)*.o $(TARGETDIR)facceptor_stress

install:
