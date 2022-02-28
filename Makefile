TARGET=$(PACKAGE).$(LIB_EXTENSION)
SRCS=$(wildcard ./deps/yyjson/src/*.c) $(wildcard $(SRCDIR)/*.c)
OBJS=$(SRCS:.c=.o)
INSTALL?=install

.PHONY: all install clean

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $(WARNINGS) $(COVERAGE) $(CPPFLAGS) -o $@ -c $<

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS) $(PLATFORM_LDFLAGS) $(COVERAGE)

install:
	$(INSTALL) $(TARGET) $(LIBDIR)
	rm -f ./src/*.o
	rm -f ./*.so

clean:
	rm -f ./src/*.o
	rm -f ./*.so
