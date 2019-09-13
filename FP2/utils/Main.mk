# header
include ${CURDIR}/make/Header.mk

# C/C++ rules
$(call OBJ_CPP_RULE,${SUB_BUILDIR},${SUB_DIR})

LIBS		:= -lxml2 -ldl -llzma -lz ${LIBS}

SOURCES		:= ini_spirit.cpp args_parser.cpp maths.cpp numbers.cpp \
		base64.cpp myxml.cpp path.cpp logger.cpp cutpool.cpp \
		it_display.cpp chrono.cpp app.cpp xmlconfig.cpp

TARGET	:= libutils.${STATICLIBEXT}

LIBUTILS := ${TARGET}

# build target
$(call DECL_STATIC_LIBRARY,${TARGET},${SOURCES})

# footer
include ${CURDIR}/make/Footer.mk
