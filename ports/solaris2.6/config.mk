ifeq "$(CONFIG)" ""
  override CONFIG := $(shell $(dir $(lastword $(MAKEFILE_LIST)))/guess-config)
  ifeq "$(CONFIG)" ""
    $(error "Failed to guess config")
  endif
endif

OS := $(shell echo $(CONFIG) | sed -e 's/^[^.]*\.//' -e 's/^\([^-_]*\)[-_].*/\1/')
PROC := $(shell echo $(CONFIG) | sed -e 's/^\([^.]*\)\..*/\1/')

ifeq "$(OS)" "darwin"
  DDSRT = $(OS) posix
  RULES = darwin
  CC = clang
  LD = $(CC)
  OPT = -fsanitize=address #-O3 -DNDEBUG
  PROF =
  CPPFLAGS += -Wall -g $(OPT) $(PROF)
  CFLAGS += $(CPPFLAGS) #-fno-inline
  LDFLAGS += -g $(OPT) $(PROF)
  X =
  O = .o
  A = .a
  SO = .dylib
  LIBPRE = lib
endif
ifeq "$(OS)" "solaris2.6"
  DDSRT = $(OS) posix
  RULES = unix
  CC = gcc -std=gnu99 -mcpu=v8
  LD = $(CC)
  OPT = -O2 -DNDEBUG
  PROF =
  CPPFLAGS += -Wall -g $(OPT) $(PROF) -D_REENTRANT -D__EXTENSIONS__ -D__SunOS_5_6 -I$(PWD)/ports/solaris2.6/include
  CFLAGS += $(CPPFLAGS)
  LDFLAGS += -g $(OPT) $(PROF)
  LDLIBS += -lposix4 -lsocket -lnsl -lc
  X =
  O = .o
  A = .a
  SO = .so
  LIBPRE = lib
endif
ifeq "$(OS)" "linux"
  DDSRT = posix
  RULES = unix
  CC = gcc-6.2 -std=gnu99 -fpic -mcx16
  OPT = #-fsanitize=address
  # CC = gcc-6.2 -std=gnu99 -fpic -mcx16
  # OPT = -O3 -DNDEBUG -flto
  LD = $(CC)
  PROF =
  CPPFLAGS += -Wall -g $(OPT) $(PROF)
  CFLAGS += $(CPPFLAGS) #-fno-inline
  LDFLAGS += -g $(OPT) $(PROF)
  X =
  O = .o
  A = .a
  SO = .so
  LIBPRE = lib
endif
ifeq "$(OS)" "win32"
  DDSRT = windows
  RULES = windows
  CC = cl
  LD = link
  # OPT = -O2 -DNDEBUG
  OPT = -MDd
  PROF =
  CPPFLAGS = -Zi -W3 $(OPT) $(PROF) -TC # -bigobj
  CFLAGS += $(CPPFLAGS)
  LDFLAGS += -nologo -incremental:no -subsystem:console -debug
  X = .exe
  O = .obj
  A = .lib
  SO = .dll
  LIBPRE =
#  VS_VERSION=12.0
#  ifeq "$(VS_VERSION)" "12.0" # This works for VS2013 + Windows 10
#    VS_HOME=/cygdrive/c/Program Files (x86)/Microsoft Visual Studio 12.0
#    WINDOWSSDKDIR=/cygdrive/c/Program Files (x86)/Windows Kits/8.1
#  else # This works for VS2010 + Windows 7
#    VS_HOME=/cygdrive/C/Program Files (x86)/Microsoft Visual Studio 10.0
#    WINDOWSSDKDIR=/cygdrive/C/Program Files (x86)/Microsoft SDKs/Windows/v7.0A
#  endif
endif
ifeq "$(OS)" "wine"
  export WINEDEBUG=-all
  DDSRT = windows
  RULES = wine
  GEN = gen.wine
  CC = wine cl
  LD = wine link
  CPPFLAGS = -nologo -W3 -TC -analyze -D_WINNT=0x0604 -Drestrict=
  CFLAGS += $(CPPFLAGS)
  LDFLAGS += -nologo -incremental:no -subsystem:console -debug
  X = .exe
  O = .obj
  A = .lib
  SO = .dll
  LIBPRE =
endif

# use $(DDSRT) as a proxy for $(CONFIG) not matching anything
ifeq "$(DDSRT)" ""
  $(error "$(CONFIG): unsupported config")
endif

# We're assuming use of cygwin, which means Windows path names can be
# obtained using "cygpath". With "-m" we get slashes (rather than
# backslashes), which all of MS' tools accept and which are far less
# troublesome in make.
ifeq "$(CC)" "cl"
  N_PWD := $(shell cygpath -m '$(PWD)')
  #N_VS_HOME := $(shell cygpath -m '$(VS_HOME)')
  #N_WINDOWSSDKDIR := $(shell cygpath -m '$(WINDOWSSDKDIR)')
else # not Windows
  N_PWD := $(PWD)
endif

# More machine- and platform-specific matters.
ifeq "$(CC)" "cl" # Windows
  ifeq "$(PROC)" "x86_64"
    MACHINE = -machine:X64
  endif
  LDFLAGS += $(MACHINE)
  OBJ_OFLAG = -Fo
  EXE_OFLAG = -out:
  SHLIB_OFLAG = -out:
  CPPFLAGS += -D_CRT_SECURE_NO_WARNINGS
#  ifeq "$(VS_VERSION)" "12.0" # This works for VS2013 + Windows 10
#    CPPFLAGS += '-I$(N_VS_HOME)/VC/include' '-I$(N_WINDOWSSDKDIR)/Include/um' '-I$(N_WINDOWSSDKDIR)/Include/shared'
#    ifeq "$(PROC)" "x86_64"
#      LDFLAGS += '-libpath:$(N_VS_HOME)/VC/lib/amd64' '-libpath:$(N_WINDOWSSDKDIR)/lib/winv6.3/um/x64'
#    else
#      LDFLAGS += '-libpath:$(N_VS_HOME)/VC/lib' '-libpath:$(N_WINDOWSSDKDIR)/lib/winv6.3/um/x86'
#    endif
#  else # This works for VS2010 + Windows 7
#    CPPFLAGS += '-I$(N_VS_HOME)/VC/include' '-I$(N_WINDOWSSDKDIR)/Include'
#    ifeq "$(PROC)" "x86_64"
#      LDFLAGS += '-libpath:$(N_VS_HOME)/VC/lib/amd64' '-libpath:$(N_WINDOWSSDKDIR)/lib/x64'
#    else
#      LDFLAGS += '-libpath:$(N_VS_HOME)/VC/lib' '-libpath:$(N_WINDOWSSDKDIR)/lib'
#    endif
#  endif
else
  ifeq "$(CC)" "wine cl"
    OBJ_OFLAG =-Fo
    EXE_OFLAG = -out:
    SHLIB_OFLAG = -out:
    CPPFLAGS += -D_CRT_SECURE_NO_WARNINGS
  else # not Windows (-like)
    OBJ_OFLAG = -o
    EXE_OFLAG = -o
    SHLIB_OFLAG = -o
    ifeq "$(PROC)" "x86"
      CFLAGS += -m32
      LDFLAGS += -m32
    endif
    ifeq "$(PROC)" "x86_64"
      CFLAGS += -m64
      LDFLAGS += -m64
    endif
  endif
endif

ifeq "$(CC)" "cl"
  LDFLAGS += -libpath:$(N_PWD)/gen
  LIBDEP_SYS = kernel32 ws2_32
else
  ifeq "$(CC)" "wine cl"
  else
    LDFLAGS += -L$(N_PWD)/gen
    LIBDEP_SYS = kernel32 ws2_32
  endif
endif

getabspath=$(abspath $1)
ifeq "$(RULES)" "darwin"
  ifneq "$(findstring clang, $(CC))" ""
    define make_exe
	$(LD) $(LDFLAGS) $(patsubst -L%, -rpath %, $(filter -L%, $(LDFLAGS))) $(EXE_OFLAG)$@ $^ $(LDLIBS)
    endef
    define make_shlib
	$(LD) $(LDFLAGS) $(patsubst -L%, -rpath %, $(filter -L%, $(LDFLAGS))) -dynamiclib -install_name @rpath/$(notdir $@) $(SHLIB_OFLAG)$@ $^ $(LDLIBS)
    endef
  else # assume gcc
    comma=,
    define make_exe
	$(LD) $(LDFLAGS) $(patsubst -L%, -Wl$(comma)-rpath$(comma)%, $(filter -L%, $(LDFLAGS))) $(EXE_OFLAG)$@ $^ $(LDLIBS)
    endef
    define make_shlib
	$(LD) $(LDFLAGS) $(patsubst -L%, -Wl$(comma)-rpath$(comma)%, $(filter -L%, $(LDFLAGS))) -dynamiclib -Wl,-install_name,@rpath/$(notdir $@) $(SHLIB_OFLAG)$@ $^ $(LDLIBS)
    endef
  endif
  define make_archive
	ar -ru $@ $?
  endef
  define make_dep
	$(CC) -M $(CPPFLAGS) $< | sed 's|[a-zA-Z0-9_-]*\.o|gen/&|' > $@ || { rm $@ ; exit 1 ; }
  endef
else
  ifeq "$(RULES)" "unix"
    LDLIBS += -lpthread
    comma=,
    define make_exe
	$(LD) $(LDFLAGS) $(patsubst -L%,-Wl$(comma)-rpath$(comma)%, $(filter -L%, $(LDFLAGS))) $(EXE_OFLAG)$@ $^ $(LDLIBS)
    endef
    define make_shlib
	$(LD) $(LDFLAGS) -Wl$(comma)--no-allow-shlib-undefined $(patsubst -L%,-Wl$(comma)-rpath$(comma)%, $(filter -L%, $(LDFLAGS))) -shared $(SHLIB_OFLAG)$@ $^ $(LDLIBS)
    endef
    define make_archive
	ar -ru $@ $?
    endef
    define make_dep
	$(CC) -M $(CPPFLAGS) $< | sed 's|[a-zA-Z0-9_-]*\.o|gen/&|' > $@ || { rm $@ ; exit 1 ; }
    endef
  else
    ifeq "$(RULES)" "windows"
      define make_exe
	$(LD) $(LDFLAGS) $(EXE_OFLAG)$@ $^ $(LDLIBS)
      endef
      define make_shlib
	$(LD) $(LDFLAGS) $(SHLIB_OFLAG)$@ $^ $(LDLIBS)
      endef
      define make_archive
	lib $(MACHINE) /out:$@ $^
      endef
      define make_dep
	$(CC) -E $(CPPFLAGS) $(CPPFLAGS) $< | grep "^#line.*\\\\vdds\\\\" | cut -d '"' -f 2 | sort -u | sed -e 's@\([A-Za-z]\)\:@ /cygdrive/\1@' -e 's@\\\\@/@g' -e '$$!s@$$@ \\@' -e '1s@^@$*$O: @' >$@
      endef
    else
      ifeq "$(RULES)" "wine"
        COMPILE_MANY_ATONCE=true
        getabspath=$1
        lc = $(subst A,a,$(subst B,b,$(subst C,c,$(subst D,d,$(subst E,e,$(subst F,f,$(subst G,g,$(subst H,h,$(subst I,i,$(subst J,j,$(subst K,k,$(subst L,l,$(subst M,m,$(subst N,n,$(subst O,o,$(subst P,p,$(subst Q,q,$(subst R,r,$(subst S,s,$(subst T,t,$(subst U,u,$(subst V,v,$(subst W,w,$(subst X,x,$(subst Y,y,$(subst Z,z,$1))))))))))))))))))))))))))
        FAKEPWD = $(call lc,z:$(subst /,\\\\,$(PWD)))
        define make_exe
	  $(LD) $(LDFLAGS) $(EXE_OFLAG)$@ $^ $(LDLIBS)
        endef
        define make_shlib
	  $(LD) $(LDFLAGS) $(SHLIB_OFLAG)$@ $^ $(LDLIBS)
        endef
        define make_archive
	  lib $(MACHINE) /out:$@ $^
        endef
        define make_dep
          $(CC) -E $(CPPFLAGS) $(CPPFLAGS) $< | grep "^#line.*\\\\vdds\\\\" | cut -d '"' -f 2 | sort -u | sed -e 's@$(FAKEPWD)\(\\\\\)*@ @' -e 's@\\\\@/@g' -e '$$!s@$$@ \\@' -e '1s@^@$*$O: @' >$@
        endef
      else
        $(error "$(OS) not covered by build macros for")
      endif
    endif
  endif
endif

ifeq "$(GEN)" ""
  GEN = gen
endif

%$O:
%$X:
%$(SO):
%.d:
