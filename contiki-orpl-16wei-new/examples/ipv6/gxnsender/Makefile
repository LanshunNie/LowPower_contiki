CONTIKI = ../../..
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
CONTIKI_PROJECT = nodes-sender
PROJECT_SOURCEFILES += util.c

CONTIKI_CONF_WITH_ORPL_BITMAP = 1

ifdef PERIOD
CFLAGS=-DPERIOD=$(PERIOD)
endif

APPS += udp-fragment

APPS += netsynch task-schedule node-function wake

all: $(CONTIKI_PROJECT)
MODULES += core/net/ipv6/multicast
CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include
