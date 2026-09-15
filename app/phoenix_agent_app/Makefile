include $(APPDIR)/Make.defs

ifeq ($(CONFIG_LVX_USE_DEMO_CONTEST2026_145_PHOENIX_AGENT_APP),y)
PROGNAME  = phoenix_agent_app
PRIORITY  = 100
STACKSIZE = 32768
MODULE    = $(CONFIG_LVX_USE_DEMO_CONTEST2026_145_PHOENIX_AGENT_APP)

CSRCS = $(filter-out main.c, $(wildcard *.c))
CSRCS += $(wildcard utils/*.c)
CSRCS += $(wildcard core/*.c)
CSRCS += $(wildcard hal/*.c)
CSRCS += $(wildcard hal/drivers/*.c)
CSRCS += $(wildcard perception/*.c)
CSRCS += $(wildcard tools/*.c)
CSRCS += $(wildcard harness/*.c)
CSRCS += $(wildcard voice/*.c)
CSRCS += $(wildcard ui/*.c)
CSRCS += $(wildcard cartridges/*.c)
CSRCS += $(wildcard test/test_autodrive.c)
MAINSRC = main.c

CFLAGS += -I$(CURDIR) -I$(CURDIR)/utils -I$(CURDIR)/core -I$(CURDIR)/hal -I$(CURDIR)/perception -I$(CURDIR)/tools -I$(CURDIR)/harness -I$(CURDIR)/voice -I$(CURDIR)/ui -I$(CURDIR)/cartridges
endif

include $(APPDIR)/Application.mk
