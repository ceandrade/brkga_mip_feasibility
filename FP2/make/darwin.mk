#CC	            := clang++ -Qunused-arguments -fcolor-diagnostics -std=c++11 #-stdlib=libc++
CC	            := g++ -std=c++11 #-stdlib=libc++
SHAREDLIBEXT	:= dylib
SHAREDLDFLAGS	:= -dynamiclib
#LDFLAGS	        := -stdlib=libc++
