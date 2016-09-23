CC ?= gcc
PKG_CONFIG ?= pkg-config
DEST ?= $(DESTDIR)`$(PKG_CONFIG) --variable=plugindir pidgin`

TARGET = grouptyping.so
SRC = grouptyping.c

.PHONY:	all install clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -fPIC $(CFLAGS) -shared -o $@ $^ $(LDFLAGS) `$(PKG_CONFIG) pidgin --libs --cflags` -g -ggdb

install: $(TARGET)
	mkdir -p $(DEST)
	install -p $(TARGET) $(DEST)

clean:
	rm -f $(TARGET)
