include $(APPDIR)/Make.defs

ifeq ($(CONFIG_LVX_USE_DEMO_CONTEST2026_145_DEMO),y)
PROGNAME  = demo
PRIORITY  = 100
STACKSIZE = 16384
MODULE    = $(CONFIG_LVX_USE_DEMO_CONTEST2026_145_DEMO)

CSRCS = $(wildcard hal/*.c)
CSRCS += $(wildcard test/*.c)
CSRCS += $(wildcard ui/*.c)
MAINSRC = main.c

CFLAGS += -I$(CURDIR) -I$(CURDIR)/hal -I$(CURDIR)/test -I$(CURDIR)/ui
endif

include $(APPDIR)/Application.mk
