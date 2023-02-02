.PHONY: all clean

include $(dir $(lastword $(MAKEFILE_LIST)))/config.mk

CPPFLAGS += -Isrc/core/ddsc/src -Isrc/core/ddsc/include -Isrc/core/ddsi/include -Isrc/ddsrt/include
CPPFLAGS += $(addprefix -I, $(wildcard src/ddsrt/src/*/include))
CPPFLAGS += -Ibuild/src/core/include -Ibuild/src/ddsrt/include
CPPFLAGS += -DDDSI_INCLUDE_NETWORK_PARTITIONS

SHLIBS = ddsc
EXES   = ddsperf

all: $(SHLIBS:%=$(GEN)/$(LIBPRE)%$(SO)) $(EXES:%=$(GEN)/%$X)
lib: $(SHLIBS:%=$(GEN)/$(LIBPRE)%$(SO))

clean:
	rm -rf $(GEN)/*

LIBCDDS_SRCDIRS := src/core/ddsi/src src/core/ddsc/src src/ddsrt/src $(DDSRT:%=src/ddsrt/src/*/%)
LIBCDDS_SRC := $(filter-out %/getopt.c, $(wildcard $(LIBCDDS_SRCDIRS:%=%/*.c)))
ifeq "$(words $(DDSRT))" "2" # max = 2 ...
  pct=%
  XX=$(filter src/ddsrt/src/%/$(word 1, $(DDSRT))/, $(dir $(LIBCDDS_SRC)))
  YY=$(patsubst %/$(word 1, $(DDSRT))/, %/$(word 2, $(DDSRT))/, $(XX))
  LIBCDDS_SRC := $(filter-out $(YY:%=%$(pct)), $(LIBCDDS_SRC))
endif

$(GEN)/$(LIBPRE)ddsc$(SO): CPPFLAGS += -Dddsc_EXPORTS
$(GEN)/$(LIBPRE)ddsc$(SO): CPPFLAGS += -fPIC

ifneq "$(COMPILE_MANY_ATONCE)" "true"
$(GEN)/$(LIBPRE)ddsc$(SO): $(LIBCDDS_SRC:%.c=$(GEN)/%$O)
	$(make_shlib)
else # /Fo bit is MSVC specific
$(GEN)/$(LIBPRE)ddsc$(SO): $(LIBCDDS_SRC)
	xs="" ;\
	  for x in $(foreach x, $^, $(call getabspath, $x)) ; do \
	    [ $$x -nt $(GEN)/`basename $$x .c`$O ] && xs="$$xs $$x" ; \
          done ; \
	  echo "compile: $$xs" ; \
	  [ -z "$$xs" ] || $(CC) $(CPPFLAGS) -MP8 -Fo.\\$(GEN)\\ -c $$xs
	$(LD) $(LDFLAGS) $(SHLIB_OFLAG)$@ $(LIBCDDS_SC:%=$(GEN)/%$O) $(LDLIBS)
endif

DDSPERF_SRCDIRS := src/tools/ddsperf build/src/tools/ddsperf
DDSPERF_SRC := $(wildcard $(DDSPERF_SRCDIRS:%=%/*.c))

$(GEN)/ddsperf$X: LDLIBS += -L. -lddsc
$(GEN)/ddsperf$X: CPPFLAGS += -Ibuild/src/tools/ddsperf
$(GEN)/ddsperf$X: $(DDSPERF_SRC:%.c=%.o) | $(GEN)/$(LIBPRE)ddsc$(SO)
	$(make_exe)

$(GEN)/%.STAMP: ; @[ -d $(dir $@) ] || { mkdir -p $(dir $@) ; touch $@ ; }

$(GEN)/%$O: %.c $(GEN)/%.STAMP
	$(CC) $(CPPFLAGS) $(OBJ_OFLAG)$@ -c $<

$(GEN)/%.d: %.c
	$(make_dep)
