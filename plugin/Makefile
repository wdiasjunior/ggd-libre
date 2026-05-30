CC = gcc

# Static Cairo/pixman sysroots
LIN_SYSROOT = third_party/mingw-cairo/linux-sysroot
MINGW_SYSROOT = third_party/mingw-cairo/sysroot

# Shared flags
COMMON_CFLAGS = -Wall -Wextra -O2 -Iclap-sdk/include -Ithird_party/cjson -Isrc

# Core sources (platform-independent)
CORE_SOURCES = src/plugin_entry.c \
               src/plugin.c \
               src/audio_engine.c \
               src/sample_bank.c \
               src/midi_map.c \
               src/params.c \
               src/state.c \
               src/wav_reader.c \
               src/gui_common.c \
               src/gui_widgets.c \
               third_party/cjson/cJSON.c

# Linux build (static Cairo, dynamic X11)
CFLAGS = $(COMMON_CFLAGS) -fPIC \
         -I$(LIN_SYSROOT)/include \
         -I$(LIN_SYSROOT)/include/cairo \
         -I$(LIN_SYSROOT)/include/pixman-1 \
         $(shell pkg-config --cflags x11 2>/dev/null)
LDFLAGS = -shared -lm -lpthread \
          -L$(LIN_SYSROOT)/lib -lcairo -lpixman-1 \
          $(shell pkg-config --libs x11 freetype2 2>/dev/null)
LIN_SOURCES = $(CORE_SOURCES) src/gui_x11.c
LIN_OBJECTS = $(LIN_SOURCES:.c=.o)
TARGET = ggd-libre-linux.clap

# Windows cross-compilation (fully static)
MINGW_CC = x86_64-w64-mingw32-gcc
MINGW_CFLAGS = $(COMMON_CFLAGS) \
               -I$(MINGW_SYSROOT)/include \
               -I$(MINGW_SYSROOT)/include/cairo \
               -I$(MINGW_SYSROOT)/include/pixman-1 \
               -DCAIRO_WIN32_STATIC_BUILD
MINGW_LDFLAGS = -shared -static-libgcc -static -lpthread -lm \
                -L$(MINGW_SYSROOT)/lib -lcairo -lpixman-1 \
                -lgdi32 -luser32 -lmsimg32
WIN_SOURCES = $(CORE_SOURCES) src/gui_win32.c
WIN_TARGET = ggd-libre-windows.clap

.PHONY: all clean install windows

all: $(TARGET)

$(TARGET): $(LIN_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

install: $(TARGET)
	mkdir -p ~/.clap
	cp $(TARGET) ~/.clap/

windows:
	$(MINGW_CC) $(MINGW_CFLAGS) -o $(WIN_TARGET) $(WIN_SOURCES) $(MINGW_LDFLAGS)

clean:
	rm -f $(LIN_OBJECTS) $(TARGET) $(WIN_TARGET)
