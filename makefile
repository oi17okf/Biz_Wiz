CC_DESKTOP = g++

# Common compiler flags
CFLAGS = -Wall -std=c++11 -Wno-reorder -Wno-sign-compare

# Flags for desktop
LFLAGS_DESKTOP = -lwinmm -g

# Files
SRC = algorithm.cpp tinyxml2.cpp
INCLUDE = tinyxml2.h
OUT_DESKTOP = desktop

# Targets
all: desktop

desktop: $(SRC) $(INCLUDE) 
	$(CC_DESKTOP) $(SRC) -o $(OUT_DESKTOP) $(CFLAGS) $(LFLAGS_DESKTOP)

clean:
	rm -f $(OUT_DESKTOP) *.o 
