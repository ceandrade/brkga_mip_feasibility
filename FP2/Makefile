##
# disable implicit rules and suffixes and enable silent run
##

#MAKEFLAGS	+= --no-builtin-rules
.SILENT:
.SUFFIXES:

##
# default commands and flags
##

CC	            := g++ -std=c++11
CCFLAGS	        := -Wall -fPIC
CPPPATH         :=
LIBPATH         :=
LDFLAGS	        :=
LIBS            := 
STATICLIBEXT	:= a
SHAREDLIBEXT	:= so
SHAREDLDFLAGS	:= -shared
AR	            := ar rc
RANLIB	        := ranlib
RM	            := rm -rf
ECHO	        := /bin/echo

##
# System info
##

PLATFORM	:= $(shell uname -s | tr [:upper:] [:lower:])
HOST		:= $(shell hostname | tr [:upper:] [:lower:])
#DATE		:= $(shell date +%Y_%m_%d_%H%M)

ifneq (${MODE},opt)
		MODE = dbg
endif

##
# common include/lib directories
##

BUILDIR		:= obj
BOOST_INCDIR	:= /opt/local/include
#LIBXML2_INCDIR	:= /usr/include/libxml2
#LIBXML2_LIBDIR	:= /usr/lib
LIBXML2_INCDIR	:= /opt/local/include/libxml2
LIBXML2_LIBDIR	:= /opt/local/lib

##
# Local Makefiles
##
-include make/${PLATFORM}.mk
-include make/${HOST}.mk

ifeq (${MODE}, dbg)
	CCFLAGS	+= -ggdb3 -fexceptions
else
	#CCFLAGS	+= -O3 -DBOOST_DISABLE_ASSERTS -DNDEBUG -ftree-vectorize -funroll-loops -mtune=native -msse2
	MACHINE = native
	CCFLAGS += -std=c++11
	CCFLAGS += -O3 -march=native -mtune=native \
		-fomit-frame-pointer -ftracer \
		-funroll-loops -fpeel-loops -fprefetch-loop-arrays \
		-DBOOST_DISABLE_ASSERTS -DNDEBUG
endif

# Disable AVX extensions since some CPUs are not capable to use them.
CCFLAGS += -mno-avx

$(eval ALL_INCDIRS := ${BOOST_INCDIR} ${LIBXML2_INCDIR} ${CURDIR})

CPPPATH		+= $(addprefix -I,$(strip ${ALL_INCDIRS}))
LIBPATH		+= -L${BUILDIR}

ifeq (${PLATFORM},darwin)
	RPATHFLAGS	:= -Wl,-rpath,$(abspath ${BUILDIR})
else
	#RPATHFLAGS	:= -z origin -Wl,--rpath=$$$$ORIGIN/.
	RPATHFLAGS	:= -Wl,--rpath=$(abspath ${BUILDIR})
endif

##
# Global data for non-recursive make
##
DIR_STACK 	:=
TGT_ALL 	:=
SYMBLINKS_ALL 	:=

##
# Function for declaring rules/libraries/executables/dependencies
# in a way compatible with a non-recursive call
##

# OBJ_CPP_RULE - Add a rule to build object files from C++ sources
define OBJ_CPP_RULE
$(eval \
${1}/%.o: ${2}/%.cpp | ${1}
	${ECHO} Compiling $$< 
	${ECHO} ${CC} $${CCFLAGS} ${SUB_CCFLAGS} $${CPPPATH} ${SUB_CPPPATH} -MMD -MP -MF $${@:.o=.d} -c $$< -o $$@
	${CC} $${CCFLAGS} ${SUB_CCFLAGS} $${CPPPATH} ${SUB_CPPPATH} -MMD -MP -MF $${@:.o=.d} -c $$< -o $$@ \
	|| (${ECHO} "Error in command: ${CC} $${CCFLAGS} ${SUB_CCFLAGS} $${CPPPATH} ${SUB_CPPPATH} -MMD -MP -MF $${@:.o=.d} -c $$< -o $$@" && false)
	${ECHO}	
)
endef

# DECL_STATIC_LIBRARY - Build a static library from object files
# Include corresponding dependency information and add to target list
# ${1} target
# ${2} objects
define DECL_STATIC_LIBRARY
$(eval \
${BUILDIR}/${1}: ${2:%.cpp=${SUB_BUILDIR}/%.o}
	${ECHO} "Building library $(notdir $$@)"
	${ECHO} ${AR} $$@ $$^
	${AR} $$@ $$^ \
	|| (${ECHO} "Error in command: ${AR} $$@ $$^" && false)
	${ECHO} ${RANLIB} $$@
	${RANLIB} $$@
	${ECHO}

ifneq (${MAKECMDGOALS},clean)
        $(eval -include ${2:%.cpp=${SUB_BUILDIR}/%.d})
endif

TGT_ALL += ${BUILDIR}/${1}
)
endef

# DECL_SHARED_LIBRARY - Build a shared library from object files
# Include corresponding dependency information and add to target list
# Add dependencies relations
# ${1} target
# ${2} objects
# ${3} shared library name
# ${4} dependencies
define DECL_SHARED_LIBRARY
$(eval \
${BUILDIR}/${1}: ${2:%.cpp=${SUB_BUILDIR}/%.o}
	${ECHO} "Building shared library $(notdir $$@)"
	${ECHO} ${CC} $$^ -o $$@ ${SHAREDLDFLAGS} ${3} $${LDFLAGS} ${SUB_LDFLAGS} $${LIBPATH} ${SUB_LIBPATH} ${SUB_LIBS} $${LIBS}
	${CC} $$^ -o $$@ ${SHAREDLDFLAGS} ${3} $${LDFLAGS} ${SUB_LDFLAGS} $${LIBPATH} ${SUB_LIBPATH} ${SUB_LIBS} $${LIBS} \
	|| (${ECHO} "Error in command: ${CC} $$^ -o $$@ ${SHAREDLDFLAGS} ${SHAREDLIBNAME} $${LDFLAGS} ${SUB_LDFLAGS} $${LIBPATH} ${SUB_LIBPATH} ${SUB_LIBS} $${LIBS}" && false)
	${ECHO}

ifneq (${MAKECMDGOALS},clean)
        $(eval -include ${2:%.cpp=${SUB_BUILDIR}/%.d})
endif

TGT_ALL	+= ${BUILDIR}/${1}

$(foreach SUB_DEP,${4},$(call ADD_DEP,${1},${SUB_DEP}))
)
endef

# DECL_BINARY_EXEC - Build a binary executable from object files
# Include corresponding dependency information and add to target list
# Add dependencies relations
# ${1} target
# ${2} objects
# ${3} dependencies
define DECL_BINARY_EXEC
$(eval \
${BUILDIR}/${1}: ${2:%.cpp=${SUB_BUILDIR}/%.o}
	${ECHO} "Building binary exec $(notdir $$@)"
	${ECHO} ${CC} $$< -o $$@ $${LDFLAGS} ${SUB_LDFLAGS} ${RPATHFLAGS} $${LIBPATH} ${SUB_LIBPATH} ${SUB_LIBS} $${LIBS}
	${CC} $$< -o $$@ $${LDFLAGS} ${SUB_LDFLAGS} ${RPATHFLAGS} $${LIBPATH} ${SUB_LIBPATH} ${SUB_LIBS} $${LIBS} \
	|| (${ECHO} "Error in command: ${CC} $$< -o $$@ $${LDFLAGS} ${SUB_LDFLAGS} ${RPATHFLAGS} $${LIBPATH} ${SUB_LIBPATH} ${SUB_LIBS} $${LIBS}" && false)
	${ECHO} "Creating link ${1} to in source dir ${SUB_DIR}"
	ln -sf $(abspath ${BUILDIR}/${1}) ${SUB_DIR}/${1}.${MODE}
	${ECHO}

ifneq (${MAKECMDGOALS},clean)
        $(eval -include ${2:%.cpp=${SUB_BUILDIR}/%.d})
endif

TGT_ALL 	+= ${BUILDIR}/${1}
SYMBLINKS_ALL	+= ${SUB_DIR}/${1}.${MODE}

$(foreach SUB_DEP,${DEPS},$(call ADD_DEP,${1},${SUB_DEP}))
)
endef

# ADD_DEP - Add a dependency relation
# ${1} target
# ${2} dependency
define ADD_DEP
$(eval ${BUILDIR}/${1}: ${BUILDIR}/${2})
endef

# MAKE_DIR - Create directory ${1}
define MAKE_DIR
$(eval \
${1}:
	${ECHO} "Creating directory $$@"
	mkdir -p $$@ \
)
endef

##
# Non-recursive include
##

# stack management
include make/stack.mk

# INCLUDE_SUBMAKEFILE - Function that includes a submakefile
define INCLUDE_SUBMAKEFILE
	SUBPROJ := ${1}
	include	${1}/Main.mk
endef

SUBPROJECTS := utils cpxutils propagator feaspump
$(foreach SUBP,${SUBPROJECTS},$(eval $(call INCLUDE_SUBMAKEFILE,${SUBP})))

##
# phony targets: all & clean
# all is the default goal
##

.PHONY: all
all: ${TGT_ALL}

.DEFAULT_GOAL := all

.PHONY: clean doc ctags

clean:
	${ECHO} -n "Cleaning -> "
	${RM} ${BUILDIR}
	${RM} ${SYMBLINKS_ALL}
	${RM} html
	${ECHO} done

doc:
	${ECHO} "Generating documentation with doxygen"
	doxygen Doxyfile

ctags:
	${ECHO} "Updating ctags"
	ctags --c-kinds=+p --fields=+S --tag-relative=yes --exclude=include --file-scope=no -R
