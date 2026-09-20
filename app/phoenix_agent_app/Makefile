include $(APPDIR)/Make.defs

ifeq ($(CONFIG_LVX_USE_DEMO_CONTEST2026_145_PHOENIX_AGENT_APP),y)
PROGNAME  = phoenix_agent_app
PRIORITY  = $(CONFIG_PHOENIX_AGENT_PRIORITY)
ifeq ($(PRIORITY),)
PRIORITY  = 100
endif

STACKSIZE = $(CONFIG_PHOENIX_AGENT_STACKSIZE)
ifeq ($(STACKSIZE),)
STACKSIZE = 32768
endif

MODULE    = $(CONFIG_LVX_USE_DEMO_CONTEST2026_145_PHOENIX_AGENT_APP)

CSRCS = $(filter-out main.c, $(wildcard *.c))
CSRCS += $(wildcard utils/*.c)
CSRCS += $(wildcard core/*.c)
CSRCS += $(wildcard core/web_api/*.c)
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

CFLAGS += -I$(CURDIR) -I$(CURDIR)/utils -I$(CURDIR)/core -I$(CURDIR)/core/web_api -I$(CURDIR)/hal -I$(CURDIR)/perception -I$(CURDIR)/tools -I$(CURDIR)/harness -I$(CURDIR)/voice -I$(CURDIR)/ui -I$(CURDIR)/cartridges
ifneq ($(CONFIG_LIB_CURL),)
CFLAGS += -I$(APPDIR)/external/curl/curl/include
endif
endif

include $(APPDIR)/Application.mk
