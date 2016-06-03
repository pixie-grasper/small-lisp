CC = clang
CXX = clang++
LINK = clang++
MKDIR = mkdir
LS = ls --color=no
CP = cp
CXXWARNFLAGS = -Weverything -Wno-c++98-compat -Wno-reserved-id-macro -Wno-padded -Wno-format-nonliteral -Wno-c++98-compat-pedantic -Wno-weak-vtables -Wno-documentation-unknown-command -Wno-documentation -Wno-missing-prototypes

BUILDDIR = build
SRCS = $(wildcard src/*.cc)
OBJS = $(SRCS:src/%.cc=$(BUILDDIR)/%.o)
DEPS = $(SRCS:src/%.cc=$(BUILDDIR)/%.d)
HEADERS = $(SRCS:.cc=.h)
PROJECT = small-lisp

# if exists libc++, use it.
LIBCPP = $(shell if $(CXX) dummy.cc -o dummy.out -lc++ -std=c++1y > /dev/null 2>&1; then echo '-lc++'; else echo '-lstdc++'; fi)

default: SYNTAX_CHECK $(PROJECT)

$(BUILDDIR):
	$(MKDIR) $(BUILDDIR)

.PHONY: SYNTAX_CHECK
SYNTAX_CHECK:
	./cpplint.py --filter=-build/c++11 $(SRCS) $(HEADERS)

$(PROJECT): $(OBJS)
	$(LINK) $< -o $@ $(LIBCPP) -lm

$(BUILDDIR)/%.o: src/%.cc $(BUILDDIR) Makefile
	$(CXX) -c $< -o $@ -std=c++1y -MMD -MP $(CXXWARNFLAGS)

.PHONY: clean
clean:
	rm -rf build dummy.out

-include $(DEPS)
