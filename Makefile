all clean install:
	@cd src && ${MAKE} $@
	@cd utils && ${MAKE} $@

.PHONY: all clean install