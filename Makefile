all: atdate

release: 
	zip grupo02.zip atdate.c README Makefile

CFLAGS += -g
