# header
include ${CURDIR}/make/Header.mk

# C/C++ rules
$(call OBJ_CPP_RULE,${SUB_BUILDIR},${SUB_DIR})

SOURCES		:= feaspump.cpp transformers.cpp ranking.cpp
TARGET		:= libfp.${STATICLIBEXT}

DEPS            := libprop.${STATICLIBEXT} libcpxutils.${STATICLIBEXT} libutils.${STATICLIBEXT}

# build shared library fp
#SHAREDLIBNAME	:=
#ifeq (${PLATFORM},darwin)
#	SHAREDLIBNAME	:= -install_name @rpath/${TARGET}
#endif
#$(call DECL_SHARED_LIBRARY,${TARGET},${SOURCES},${SHAREDLIBNAME},${DEPS})

$(call DECL_STATIC_LIBRARY,${TARGET},${SOURCES},${SHAREDLIBNAME},${DEPS})

DEPS            := ${TARGET} libprop.${STATICLIBEXT} libcpxutils.${STATICLIBEXT} libutils.${STATICLIBEXT}

ifeq (${PLATFORM}, darwin)
SUB_LIBS        := -Wl,-all_load -lfp -lprop -lcpxutils -lutils
else
SUB_LIBS        := -Wl,--whole-archive -lfp -lprop -Wl,--no-whole-archive -lcpxutils -lutils
endif

# build prober
$(call DECL_BINARY_EXEC,fp2,main.cpp,${DEPS})

# footer
include ${CURDIR}/make/Footer.mk
