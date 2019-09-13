# stack push
DIR_STACK := $(call PUSH,${DIR_STACK},${SUBPROJ})

SUB_DIR		:= ${DIR_STACK:/%=%}
SUB_NAME	:= $(subst /,_,${SUB_DIR})
SUB_BUILDIR	:= ${BUILDIR}/${SUB_DIR}
SUB_CCFLAGS	:= 
SUB_CPPATH	:= 
SUB_LDFLAGS	:= 
SUB_LIBPATH	:= 
SUB_LIBS	:= 

#$(warning ${SUB_DIR})
#$(warning ${SUB_NAME})
#$(warning ${SUB_BUILDIR})

# create obj dir
$(call MAKE_DIR,${SUB_BUILDIR})
