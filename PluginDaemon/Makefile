#---------------------------------------------------------------------------------
.SUFFIXES:

#-----------------------------------------------------------------------------------
#Some project information
#-----------------------------------------------------------------------------------
NAME 		:= 	magic-mirror
TARGET		:=	$(NAME)
BUILD		:=	build

#set source and include directories
SOURCES		:= source source/api source/plugins
INCLUDES	:= includes includes/api

#DS2 Specific stuff=================================================================
BASEDIR		:= $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

LIBS 		:=
EXTLIBS 	:= -lm -luuid -lrt -L/usr/local/lib -lwebsockets


CFLAGS += -std=gnu99 -O3 -Wall -march=armv7-a -mfloat-abi=hard -mfpu=vfpv3-d16
#compiler tools
CC 	= gcc
CXX = gcc
LD 	= $(CC)


LIBINCLUDES +=


#Back to normal now==================================================================

CFLAGS		+= $(LIBINCLUDES) $(INCLUDE)
CXXFLAGS	:= $(CFLAGS)
LDFLAGS		= $(EXTLIBS) $(LIBS) $(FRAMEWORKS) -g

#-----------------------------------------------------------------------------------
#Building rules - Super awesome recursive makefile calling!
#-----------------------------------------------------------------------------------

#if in project root directory
ifneq ($(BUILD),$(notdir $(CURDIR)))

#collect source files and set up paths
export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
COBJFILES 	:=  $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.m)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES 		:= 	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))

#generate o files from c files
export OFILES	:=	$(COBJFILES:.m=.o) $(CFILES:.c=.o) $(CPPFILES:.cpp=.o) $(SFILES:.S=.o)

#include directories
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					-I$(CURDIR)/$(BUILD)

#if this is a C project, use C compiler
#otherwise use C++ compiler
ifeq ($(strip $(CPPFILES)),)
	export LD:=$(CC)
else
	export LD:=g++
endif

.PHONY: all $(BUILD) clean release test

all: $(BUILD)

#create build directory, change into it, then call make on this makefile again
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile


clean:
	@echo clean ...
	@rm -rf $(BUILD) $(OUTPUT).bin index.html

#-----------------------------------------------------------------------------------
#Build Targets when not in root directory
#-----------------------------------------------------------------------------------
else

#collection dependencies
DEPENDS	:=	$(OFILES:.o=.d)

#build targets
$(OUTPUT).bin: $(OFILES)

%.o: %.c
	@echo $(notdir $<)
	$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -c $< -o $@

%.o: %.m
	@echo $(notdir $<)
	$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -c $< -o $@

%.o: %.cpp
	@echo $(notdir $<)
	$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) -c $< -o $@

%.o: %.S
	@echo $(notdir $<)
	$(CC) $(CFLAGS) $(DEPSDIR)/$*.d -D_ASSEMBLER_ -D__ASSEMBLY__ -c $< -o $@

%.bin:
	@echo linking $(notdir $@)
	@$(LD) $(LDFLAGS) $(OFILES) -o $@
endif


#-----------------------------------------------------------------------------------
#Clean and package project directory in tar file
#-----------------------------------------------------------------------------------
release: clean
	tar -zcvf "goldd5_$(shell basename $(CURDIR)).tar.gz" *
