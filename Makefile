CC=		gcc
CFLAGS=		-g -Wall -Werror -std=gnu99 -Iinclude
LD=		gcc
LDFLAGS=	-Llib
LIBS=		-lspidey
AR=		ar
ARFLAGS=	rcs
TARGETS=	bin/spidey

all:		$(TARGETS)

clean:
	@echo Cleaning...
	@rm -f $(TARGETS) lib/*.a src/*.o *.log *.input

.PHONY:		all test clean

# TODO: Add rules for bin/spidey, lib/libspidey.a, and any intermediate objects

# Intermediate object files

src/forking.o:	src/forking.c
	$(CC) $(CFLAGS) -c -o $@ $^

src/handler.o:	src/handler.c
	$(CC) $(CFLAGS) -c -o $@ $^

src/request.o:	src/request.c
	$(CC) $(CFLAGS) -c -o $@ $^

src/single.o:	src/single.c
	$(CC) $(CFLAGS) -c -o $@ $^

src/socket.o:	src/socket.c
	$(CC) $(CFLAGS) -c -o $@ $^

src/utils.o:	src/utils.c
	$(CC) $(CFLAGS) -c -o $@ $^

src/spidey.o:	src/spidey.c
	$(CC) $(CFLAGS) -c -o $@ $^

# Linking intermediate objects into libspidey.a
lib/libspidey.a:	src/forking.o src/handler.o src/request.o src/single.o src/socket.o src/utils.o
	$(AR) $(ARFLAGS) $@ $^

# Linking libspidey.a and spidey.o
bin/spidey:	src/spidey.o lib/libspidey.a
	$(LD) $(LDFLAGS) -o $@ $< $(LIBS)
