CC_DESKTOP = g++
CC_WEB     = emcc

# Paths for raylib - @CHANGE TO YOU LOCAL PATHS WHEN COMPILING!@
RAYLIB_DESKTOP_LIB_PATH = C:\Users\Oscar\Desktop\raylib\src
RAYLIB_WEB_LIB_PATH     = C:\Users\Oscar\Desktop\raylib\src\web
RAYLIB_INCLUDE_PATH     = C:\Users\Oscar\Desktop\raylib\src

# Common compiler flags
CFLAGS = -Wall -std=c++11 -I$(RAYLIB_INCLUDE_PATH) -lraylib -Wno-reorder

# Flags for desktop
LFLAGS_DESKTOP = -L$(RAYLIB_DESKTOP_LIB_PATH) -lopengl32 -lgdi32 -lwinmm

# Flags for web
LFLAGS_WEB = -L$(RAYLIB_WEB_LIB_PATH) -s USE_GLFW=3 -s FULL_ES2=1 -s ASYNCIFY -s ALLOW_MEMORY_GROWTH=1 -DPLATFORM_WEB 

# Files
SRC = main.cpp tinyxml2.cpp
INCLUDE = tinyxml2.h
OUT_DESKTOP = desktop
OUT_WEB = index.html
BASE_HTML = base.html
UNUSED = --shell-file $(BASE_HTML)
IO = --preload-file example2.csv --preload-file RequestForPayment.xes_


# Targets
all: desktop

desktop: $(SRC) $(INCLUDE) 
	$(CC_DESKTOP) $(SRC) -o $(OUT_DESKTOP) $(CFLAGS) $(LFLAGS_DESKTOP)

web: $(SRC) $(INCLUDE) $(BASE_HTML) 
	$(CC_WEB) $(SRC) -o $(OUT_WEB) $(CFLAGS) $(LFLAGS_WEB) $(IO)
clean:
	rm -f $(OUT_DESKTOP) $(OUT_WEB) *.o index.js index.wasm
