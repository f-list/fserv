all clean install deploy:
	@cd src && ${MAKE} $@
	@cd utils && ${MAKE} $@

.PHONY: all clean install deploy
