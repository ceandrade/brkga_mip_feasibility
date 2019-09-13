# header
include ${CURDIR}/make/Header.mk
include ${CURDIR}/make/pcfisch4b.mk

CPPPATH		+= -I${CPLEX_INCDIR}

# C/C++ rules
$(call OBJ_CPP_RULE,${SUB_BUILDIR},${SUB_DIR})

ifeq (${PLATFORM}, darwin)
	LDFLAGS	+= -Wl,-no_compact_unwind
endif
LIBPATH         += -L${CPLEX_LIBDIR}
LIBS            := -lcplex ${LIBS}
ifeq (${PLATFORM}, darwin)
	LIBS	+= -framework CoreFoundation -framework IOKit
else
	LIBS	+= -lpthread
endif

SUB_LIBS	:= -lcpxutils -lutils

SOURCES		:= cpxutils.cpp cpxmacro.cpp model.cpp cpxbb.cpp gomory.cpp cpxapp.cpp
TARGET		:= libcpxutils.${STATICLIBEXT}

LIBCPXUTILS	:= ${TARGET}

# build library
$(call DECL_STATIC_LIBRARY,${TARGET},${SOURCES})

DEPS		:= libcpxutils.${STATICLIBEXT} libutils.${STATICLIBEXT}

# footer
include ${CURDIR}/make/Footer.mk
