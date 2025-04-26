TARGET=doc.$(LIB_EXTENSION)
SRCS=$(wildcard ./deps/yyjson/src/*.c) $(wildcard src/*.c)
OBJS=$(SRCS:.c=.o)
GCDA=$(OBJS:.o=.gcda)
INSTALL?=install

ifdef YYJSON_COVERAGE
COVFLAGS=--coverage
endif

.PHONY: all install clean

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $(WARNINGS) $(COVFLAGS) $(CPPFLAGS) -o $@ -c $<

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS) $(PLATFORM_LDFLAGS) $(COVFLAGS)

install: $(TARGET)
	$(INSTALL) -d $(INST_LIBDIR)
	$(INSTALL) $(TARGET) $(INST_LIBDIR)
	$(INSTALL) yyjson.lua $(INST_LUADIR)
	rm -f $(OBJS) $(GCDA) *.so

clean:
	rm -f $(OBJS) $(GCDA) *.so
