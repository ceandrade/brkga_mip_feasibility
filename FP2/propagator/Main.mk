# header
include ${CURDIR}/make/Header.mk

# C/C++rules
$(call OBJ_CPP_RULE,${SUB_BUILDIR},${SUB_DIR})

SOURCES		:= domain.cpp propagator.cpp prop_engine.cpp linear_propagator.cpp varbound_propagator.cpp logic_propagator.cpp
TARGET		:= libprop.${STATICLIBEXT}

DEPS            := libutils.${STATICLIBEXT}

# build shared library prop
#SHAREDLIBNAME	:=
#ifeq (${PLATFORM},darwin)
#	SHAREDLIBNAME	:= -install_name @rpath/${TARGET}
#endif
#$(call DECL_SHARED_LIBRARY,${TARGET},${SOURCES},${SHAREDLIBNAME},libutils.${STATICLIBEXT})

$(call DECL_STATIC_LIBRARY,${TARGET},${SOURCES},${SHAREDLIBNAME},libutils.${STATICLIBEXT})

DEPS            := ${TARGET} libcpxutils.${STATICLIBEXT} libutils.${STATICLIBEXT}

ifeq (${PLATFORM}, darwin)
SUB_LIBS        := -Wl,-all_load -lprop -lcpxutils -lutils
else
SUB_LIBS        := -Wl,--whole-archive -lprop -Wl,--no-whole-archive -lcpxutils -lutils
endif

# footer
include ${CURDIR}/make/Footer.mk
