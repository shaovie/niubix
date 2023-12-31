###########################################################################
#                           _              
#   ~Q~                    (_)             
#                 ___ _   ___ __ __      __
#                / __| | | | / __\ \ /\ / /
#               | (__  |_| | \__ \\ V  V / 
#                \___|\__,_|_|___/ \_/\_/  
#
# Copyright (C) 2023 - 8888, Cui Shaowei, <shaovie@gmail.com>, It's free.
###########################################################################

                               BIN_DIR = ./bin
                                TARGET = $(BIN_DIR)/niubix
                              C_CFLAGS = -Wall -W -Wpointer-arith -pipe -fPIC
                                MACROS = -D_REENTRANT -D__USE_POSIX
                                    CC = gcc
                            CPP_CFLAGS = -Wall -Wextra -Wuninitialized -Wpointer-arith -pipe -fPIC -std=c++11
                                  MAKE = make
                                LINKER = g++
                          INCLUDE_DIRS = -I ./thirdparty
                                  LIBS = -lpthread -lssl -lcrypto
                            OPTIM_FLAG = -O2
                                   CPP = g++
                                LFLAGS = -W -fPIC
                              LIB_DIRS =
                                 VPATH = . src
                            OBJECT_DIR = ./.obj/
                              CPPFILES = \
                                         acceptor.cpp \
                                         admin.cpp \
                                         app.cpp \
                                         async_taskq.cpp \
                                         backend.cpp \
                                         conf.cpp \
                                         connector.cpp \
                                         evpoll.cpp \
                                         global.cpp \
                                         health_check.cpp \
                                         http.cpp \
                                         http_frontend.cpp \
                                         http_frontend_handle_req.cpp \
                                         http_parser.cpp \
                                         io_handle.cpp \
                                         leader.cpp \
                                         log.cpp \
                                         ssl.cpp \
                                         task_in_worker.cpp \
                                         timer_qheap.cpp \
                                         worker.cpp \
                                         worker_timing_event.cpp \
                                         niubix.cpp

                                CFILES = 

# To use 'make debug=0' build release edition.
ifdef debug
	ifeq ("$(origin debug)", "command line")
		ifeq ($(debug), 0)
	    MACROS += -DNDEBUG
		else
		  MACROS += -g -DDO_DEBUG
	  endif
	endif
else
  MACROS += -g -DDO_DEBUG
endif

# To use 'make quiet=1' all the build command will be hidden.
ifdef quiet
	ifeq ("$(origin quiet)", "command line")
		ifeq ($(quiet), 1)
	    Q = @
	  endif
	endif
endif

OBJECTS := $(addprefix $(OBJECT_DIR), $(notdir $(CPPFILES:%.cpp=%.o)))
OBJECTS += $(addprefix $(OBJECT_DIR), $(notdir $(CFILES:%.c=%.o)))

CALL_CFLAGS := $(C_CFLAGS) $(INCLUDE_DIRS) $(MACROS) $(OPTIM_FLAG)
CPPALL_CFLAGS := $(CPP_CFLAGS) $(INCLUDE_DIRS) $(MACROS) $(OPTIM_FLAG)
LFLAGS += $(LIB_DIRS) $(OPTIM_FLAG)

all: checkdir $(TARGET)

$(TARGET): $(OBJECTS)
	$(Q)$(LINKER) $(strip $(LFLAGS)) $(MACROS) -o $@ $(OBJECTS) $(LIBS)

$(OBJECT_DIR)%.o:%.cpp
	$(Q)$(CPP) $(strip $(CPPALL_CFLAGS)) -c $< -o $@

$(OBJECT_DIR)%.o:%.c
	$(Q)$(CC) $(strip $(CALL_CFLAGS)) -c $< -o $@

checkdir:
	@if ! [ -d "$(BIN_DIR)" ]; then \
		mkdir $(BIN_DIR) ; \
		fi
	@if ! [ -d "$(OBJECT_DIR)" ]; then \
		mkdir $(OBJECT_DIR); \
		fi
clean:
	$(Q)rm -f $(OBJECTS)
cleanall: clean
	$(Q)rm -f $(TARGET)
tag:
	$(Q)ctags -R src/*

.PHONY: all clean cleanall checkdir tag
