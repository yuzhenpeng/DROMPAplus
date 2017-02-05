CC = g++
CFLAGS += -std=c++11 -O2 -Wall -W
LDFLAGS =
LIBS += -lboost_program_options -lboost_system -lboost_filesystem -lboost_iostreams
LIBS_DP += -lz -lgsl -lgslcblas -lboost_thread

SRCDIR = ./src
OBJDIR = ./obj
BINDIR = ./bin
SSPDIR = ./src/SSP
SSPSRCDIR = $(SSPDIR)/src
SSPCMNDIR = $(SSPDIR)/common
SSPOBJDIR = $(SSPDIR)/obj
SSPCMNOBJDIR = $(SSPDIR)/cobj
ALGLIBDIR = $(SSPDIR)/common/alglib

PROGRAMS = parse2wig+ drompa+
TARGET = $(addprefix $(BINDIR)/,$(PROGRAMS))
#$(warning $(TARGET))

ifdef DEBUG
CFLAGS += -DDEBUG
endif
ifdef PRINTFRAGMENT
CFLAGS += -DPRINTFRAGMENT
endif
ifdef PRINTREAD
CFLAGS += -DPRINTREAD
endif

OBJS_UTIL = $(SSPCMNOBJDIR)/util.o $(SSPCMNOBJDIR)/BoostOptions.o
OBJS_PW = $(OBJDIR)/pw_main.o $(SSPOBJDIR)/Mapfile.o $(SSPOBJDIR)/ParseMapfile.o $(OBJDIR)/pw_makefile.o $(SSPOBJDIR)/ReadBpStatus.o $(SSPOBJDIR)/LibraryComplexity.o $(OBJDIR)/WigStats.o $(OBJDIR)/GenomeCoverage.o $(OBJDIR)/GCnormalization.o $(SSPOBJDIR)/ShiftProfile.o $(SSPCMNOBJDIR)/statistics.o $(ALGLIBDIR)/libalglib.a
OBJS_DD = $(OBJDIR)/dd_main.o $(OBJDIR)/dd_readfile.o $(SSPCMNOBJDIR)/ReadAnnotation.o

.PHONY: all clean

all: $(TARGET)

$(BINDIR)/parse2wig+: $(OBJS_PW) $(OBJS_UTIL)
	@if [ ! -e `dirname $@` ]; then mkdir -p `dirname $@`; fi
	$(CC) -o $@ $^ $(LIBS) $(LIBS_DP)
$(BINDIR)/drompa+: $(OBJS_DD) $(OBJS_UTIL)
	$(CC) -o $@ $^ $(LIBS) $(LIBS_DP)

$(ALGLIBDIR)/libalglib.a:
	make -C $(ALGLIBDIR)
$(SSPOBJDIR)/%.o: $(SSPSRCDIR)/%.cpp
	make -C $(SSPDIR) $(OBJDIR)/$(@F)
$(CMNOBJDIR)/%.o: $(CMNDIR)/%.cpp
	make -C $(SSPDIR) $(OBJDIR)/$(@F)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@if [ ! -e `dirname $@` ]; then mkdir -p `dirname $@`; fi
	$(CC) -o $@ -c $< $(CFLAGS) $(WFLAGS)

clean:
	rm -rf bin obj

HEADS_UTIL = $(SSPSRCDIR)/MThread.hpp $(SSPCMNDIR)/BoostOptions.hpp $(SSPCMNDIR)/inline.hpp $(SSPCMNDIR)/seq.hpp $(SSPCMNDIR)/util.hpp

$(OBJDIR)/dd_main.o: $(SRCDIR)/dd_opt.hpp
$(OBJDIR)/pw_main.o: $(SRCDIR)/pw_makefile.hpp $(SRCDIR)/GCnormalization.hpp $(SSPSRCDIR)/ReadBpStatus.hpp $(SRCDIR)/GenomeCoverage.hpp
$(OBJDIR)/pw_makefile.o: $(SRCDIR)/pw_makefile.hpp $(SSPSRCDIR)/ReadBpStatus.hpp
$(OBJDIR)/GCnormalization.o: $(SRCDIR)/GCnormalization.hpp $(SSPSRCDIR)/ReadBpStatus.hpp
$(OBJDIR)/ReadBpStatus.o: $(SSPSRCDIR)/ReadBpStatus.hpp
$(OBJS_UTIL): Makefile $(HEADS_UTIL)
$(OBJS_PW): Makefile $(SRCDIR)/pw_gv.hpp $(SRCDIR)/WigStats.hpp $(SSPSRCDIR)/ParseMapfile.hpp $(SSPCMNDIR)/statistics.hpp $(SSPSRCDIR)/LibraryComplexity.hpp $(HEADS_UTIL) $(SSPSRCDIR)/ShiftProfile.hpp
$(OBJS_DD): Makefile $(SRCDIR)/dd_gv.hpp $(SRCDIR)/dd_readfile.hpp $(HEADS_UTIL)
