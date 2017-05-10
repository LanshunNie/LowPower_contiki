CONTIKI_PROJECT = hello-world
all: $(CONTIKI_PROJECT)
CONTIKI_CONF_WITH_ORPL_BITMAP = 1
	
APPS += udp-fragment
APPS += netsynch task-schedule node-function
CONTIKI = ../..
include $(CONTIKI)/Makefile.include
