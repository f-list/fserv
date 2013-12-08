CXXFLAGS=	-O2 -Wall -Werror
LDFLAGS=	-lpthread
FACCEPTOR__STRESS_O=	facceptor_stress.o

.cpp.o:
	@echo "CC $<"
	@$(CXX) -c $(CXXFLAGS) $<

all: facceptor_stress

facceptor_stress: $(FACCEPTOR__STRESS_O)
	@echo "LD $@"
	@$(CXX) $(FACCEPTOR__STRESS_O) $(LDFLAGS) -o $@

clean:
	@echo "CLEAN *~ *.o facceptor_stress"
	@rm -f *~ *.o facceptor_stress

install:
