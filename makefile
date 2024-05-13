.PHONY: all clean dep install

CXX = g++
CC = gcc
CXXFLAGS = -std=gnu++11 -w -g
CFLAGS = 
LDFLAGS = 
LIBS = -lpthread -lrt -ldl

SRCDIR = src
OBJDIR = obj
BINDIR = bin
STRMSVR_CODEC_DIR   = ./$(SRCDIR)/codec
STRMSVR_COMMON_DIR  = ./$(SRCDIR)/common
STRMSVR_SERVICE_DIR = ./$(SRCDIR)/service

SRCS := $(wildcard $(SRCDIR)/**/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/**/*.cpp))
SRCS += $(wildcard $(SRCDIR)/*.cpp)
OBJS += $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.cpp))
DEPS := $(OBJS:.o=.d)

TARGET = $(BINDIR)/streamSvr

INC += \
	-I$(STRMSVR_CODEC_DIR) \
	-I$(STRMSVR_COMMON_DIR) \
	-I$(STRMSVR_SERVICE_DIR)

##################################################
#
##################################################
all: $(TARGET)
$(TARGET): $(OBJS) | $(BINDIR)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INC) -MMD -MP -c $^ -o $@
# -MMD:
# Generate dependency of files but not including standard lib header. (-MM)
# And write to .d file(filename according to -o).Keep compiling. (-MD)
# -MP:
# Add a phony target for each dependency other than the main file, causing each to depend on nothing.
# These dummy rules work around errors ‘make’ gives if you remove header files without updating the ‘Makefile’ to match.

-include $(DEPS)

$(OBJDIR):
	mkdir -p $(OBJDIR)
	mkdir -p $(patsubst $(SRCDIR)/%, $(OBJDIR)/%, $(dir $(wildcard $(SRCDIR)/**/)))

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(OBJDIR) $(BINDIR)

dep:

install:
