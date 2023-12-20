LLVM_CONFIG ?= llvm-config-10

LLVMVER  = $(shell $(LLVM_CONFIG) --version 2>/dev/null )
LLVM_UNSUPPORTED = $(shell $(LLVM_CONFIG) --version 2>/dev/null | egrep -q '^3\.[0-7]|^1[2-9]' && echo 1 || echo 0 )
LLVM_NEW_API = $(shell $(LLVM_CONFIG) --version 2>/dev/null | egrep -q '^1[0-9]' && echo 1 || echo 0 )
LLVM_MAJOR = $(shell $(LLVM_CONFIG) --version 2>/dev/null | sed 's/\..*//')
LLVM_BINDIR = $(shell $(LLVM_CONFIG) --bindir 2>/dev/null)
LLVM_STDCXX = gnu++11
LLVM_APPLE = $(shell clang -v 2>&1 | grep -iq apple && echo 1 || echo 0)
LLVM_LTO   = 0

ifeq "$(LLVMVER)" ""
  $(warning [!] llvm_mode needs llvm-config, which was not found)
endif

ifeq "$(LLVM_UNSUPPORTED)" "1"
  $(warning llvm_mode only supports llvm versions 3.8.0 up to 11)
endif

ifeq "$(LLVM_APPLE)" "1"
  $(warning llvm_mode will not compile with Xcode clang...)
endif

# We were using llvm-config --bindir to get the location of clang, but
# this seems to be busted on some distros, so using the one in $PATH is
# probably better.

CC         = $(LLVM_BINDIR)/clang
CXX        = $(LLVM_BINDIR)/clang++

ifeq "$(shell test -e $(CC) || echo 1 )" "1"
  # llvm-config --bindir may not providing a valid path, so ...
  ifeq "$(shell test -e '$(BIN_DIR)/clang' && echo 1)" "1"
    # we found one in the local install directory, lets use these
    CC         = $(BIN_DIR)/clang
    CXX        = $(BIN_DIR)/clang++
  else
    # hope for the best
    $(warning we have trouble finding clang/clang++ - llvm-config is not helping us)
    CC         = clang
    CXX        = clang++
  endif
endif

# sanity check.
# Are versions of clang --version and llvm-config --version equal?
CLANGVER = $(shell $(CC) --version | sed -E -ne '/^.*version\ ([0-9]\.[0-9]\.[0-9]).*/s//\1/p')

ifneq "$(CLANGVER)" "$(LLVMVER)"
  CC = $(shell $(LLVM_CONFIG) --bindir)/clang
  CXX = $(shell $(LLVM_CONFIG) --bindir)/clang++
endif

# After we set CC/CXX we can start makefile magic tests

#  CFLAGS          ?= -O0 -funroll-loops -DDEBUG
CFLAGS          ?= -O0 -funroll-loops 
override CFLAGS = -Wall -g -Wno-pointer-sign -Wno-unused-function

CXXFLAGS          ?= -O0 -funroll-loops 
# CXXFLAGS          ?= -O0 -funroll-loops -DDEBUG
override CXXFLAGS += -Wall -g -Wno-variadic-macros


CLANG_CFL    = -std=c++17 `$(LLVM_CONFIG) --cxxflags` -fno-rtti -fpic $(CXXFLAGS)
CLANG_LFL    = -std=c++17 `$(LLVM_CONFIG) --ldflags` $(LDFLAGS)

# If prerequisites are not given, warn, do not build anything, and exit with code 0
ifeq "$(LLVMVER)" ""
  NO_BUILD = 1
endif

ifneq "$(LLVM_UNSUPPORTED)$(LLVM_APPLE)" "00"
  NO_BUILD = 1
endif

ifeq "$(NO_BUILD)" "1"
  TARGETS = no_build
else
  TARGETS = callgraph.o callgraph_pass.o callgraph_pass.so 
endif

all: $(TARGETS)

no_build:
	@printf "%b\\n" "\\033[0;31mPrerequisites are not met, skipping build\\033[0m"

callgraph.o: ./src/callgraph.cpp
	-$(CXX) $(CLANG_CFL) -c -fPIC ./src/callgraph.cpp

callgraph_pass.o: callgraph_pass.cpp
	-$(CXX) $(CLANG_CFL) -c -fPIC callgraph_pass.cpp

callgraph_pass.so: callgraph_pass.o callgraph.o 
	-$(CXX) $(CLANG_CFL) -fno-rtti -fPIC -std=$(LLVM_STDCXX) -shared callgraph_pass.o callgraph.o -o $@ $(CLANG_LFL)

.NOTPARALLEL: clean

clean:
	rm -f ./*.o ./*.so
 