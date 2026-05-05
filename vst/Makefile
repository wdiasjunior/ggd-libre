CC = gcc
CFLAGS = -Wall -Wextra -O2 -fPIC \
         -Iclap-sdk/include \
         -Ithird_party/cjson \
         -Isrc \
         $(shell pkg-config --cflags cairo x11 2>/dev/null)
LDFLAGS = -shared -lm \
          $(shell pkg-config --libs cairo x11 2>/dev/null)

SOURCES = src/plugin_entry.c \
          src/plugin.c \
          src/audio_engine.c \
          src/sample_bank.c \
          src/midi_map.c \
          src/params.c \
          src/state.c \
          src/wav_reader.c \
          src/gui.c \
          src/gui_widgets.c \
          third_party/cjson/cJSON.c

OBJECTS = $(SOURCES:.c=.o)
TARGET = ggd-libre.clap

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

install: $(TARGET)
	mkdir -p ~/.clap
	cp $(TARGET) ~/.clap/

clean:
	rm -f $(OBJECTS) $(TARGET)
