.PHONY: all clean

CC = g++
CFLAGS = -w -Wall -ggdb -std=c++17 -Ilib/WMT/lib/ -Ilib/glad/include/ -Ilib/imgui/ -Ilib/ -Isrc/ -I /usr/local/include
LDFLAGS = -lSDL2 -lSDL2_image -lSDL2_ttf -lGL -lGLU -lglfw -lpng -ldl -lGLEW

SOURCES  = $(wildcard src/*.c src/*.cpp lib/kaitai/*.cpp)
#SOURCES += lib/WMT/lib/zip.cpp lib/WMT/lib/wmt.cpp
OBJECTS  = $(patsubst %.cpp,%.o,$(patsubst %.c,%.o,$(SOURCES)))
DEPS     = $(patsubst %.cpp,%.d,$(patsubst %.c,%.d,$(SOURCES)))

all: main

%.d : %.c
	$(CC) -MM $(CFLAGS) $< >$@
%.d : %.cpp
	$(CC) -MM $(CFLAGS) $< >$@

main: $(OBJECTS)
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

%.o : %.c
	$(CC) $< -c -o $@ $(CFLAGS)
%.o : %.cpp
	$(CC) $< -c -o $@ $(CFLAGS)

clean:
	$(RM) main $(OBJECTS) $(DEPS)

include $(DEPS)
