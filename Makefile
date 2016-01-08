CC=gcc
CXX=g++


COMPONENT_NAME = pingu

CPPUTEST_HOME = cpputest
CPPUTEST_CPPFLAGS += -I$(CPPUTEST_HOME)/include -Wno-error=unused-parameter $(CUSTOM_FLAGS)
CPPUTEST_CXXFLAGS = -include $(CPPUTEST_HOME)/include/CppUTest/MemoryLeakDetectorNewMacros.h
CPPUTEST_CFLAGS = -include $(CPPUTEST_HOME)/include/CppUTest/MemoryLeakDetectorMallocMacros.h -std=c99

LD_LIBRARIES = -L$(CPPUTEST_HOME)/lib -lCppUTest -lCppUTestExt -lm
SRC_DIRS = src

TEST_SRC_DIRS = tests

INCLUDE_DIRS = src $(CPPUTEST_HOME)/include

include $(CPPUTEST_HOME)/build/MakefileWorker.mk
