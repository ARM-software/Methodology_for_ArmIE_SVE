CXX      = armclang++
CXXFLAGS = -O3 -mcpu=native -pthread
CPPFLAGS = -Iinclude/
LDFLAGS  =
LIBS     =

# When enabling this option, make sure LDFLAGS and LIBS
# point to a correct Boost and zlib installation
ENABLE_GZ_SUPPORT = no

ifeq ($(ENABLE_GZ_SUPPORT),yes)
    CXXFLAGS += -DENABLE_GZIP
    CPPFLAGS += -I/apps/boost/include
    LDFLAGS += -L/apps/boost/lib -L/apps/zlib
    LIBS += -lz -lboost_iostreams
endif

##################################################
# DO NOT TOUCH ANYTHING BELOW THIS LINE          #
##################################################

INCS = include/Options.hpp \
	   include/Utils.hpp

OBJS = src/analyze.o \
	   src/Options.o

TARGET = bin/analyze

analyze: bin/analyze

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

%.o: %.cpp $(INCS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $< $(LDFLAGS) $(LIBS)


clean:
	rm -rf $(OBJS) $(TARGET)
