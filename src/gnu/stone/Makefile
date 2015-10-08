# stone		simple repeater
# Copyright(c)1995-1999 by Hiroaki Sengoku <sengoku@gcd.org>
#
# -DUSE_POP	use POP -> APOP conversion
# -DUSE_SSL	use OpenSSL
# -DCPP		preprocessor for reading config. file
# -DH_ERRNO	h_errno is not defined in header files
# -DIGN_SIGTERM	ignore SIGTERM signal
# -DINET_ADDR	use custom inet_addr(3)
# -DNO_ALRM	without SIGALRM signal
# -DNO_BCOPY	without bcopy(3)
# -DNO_SNPRINTF	without snprintf(3)
# -DNO_SYSLOG	without syslog(2)
# -DNO_THREAD	without thread
# -DOS2		OS/2 with EMX
# -DWINDOWS	Windows95/98/NT

SSL_FLAGS=	-DUSE_SSL -I/usr/local/ssl/include
SSL_LIBS=	-L/usr/local/ssl/lib -lssl -lcrypto

POP_FLAGS=	-DUSE_POP
POP_LIBS=	md5c.o

all:
	@echo "run make with one of the following arguments"
	@echo "linux     ; for Linux"
	@echo "bsd       ; for FreeBSD or BSD/OS"
	@echo "sun       ; for SunOS 4.x with gcc"
	@echo "solaris   ; for Solaris with gcc"
	@echo "hp        ; for HP-UX with gcc"
	@echo "irix      ; for IRIX"
	@echo "win       ; for Windows 95/NT with VC++"
	@echo "mingw     ; for Windows 95/NT with MinGW"
	@echo "emx       ; for OS/2 with EMX"
	@echo "using POP -> APOP conv., add '-pop' (example: linux-pop)"
	@echo "using above conv. and OpenSSL, add '-ssl' (example: linux-ssl)"

clean:
	rm -f stone $(POP_LIBS) stone.exe stone.obj md5c.obj

md5c.c:
	@echo "*** md5c.c is contained in RFC1321"

stone: stone.c
	$(CC) $(CFLAGS) $(FLAGS) -o $@ $? $(LIBS)

pop_stone: $(POP_LIBS)
	$(MAKE) FLAGS="$(POP_FLAGS)" LIBS="$(POP_LIBS)" $(TARGET)

ssl_stone: $(POP_LIBS)
	$(MAKE) FLAGS="$(POP_FLAGS) $(SSL_FLAGS)" LIBS="$(POP_LIBS) $(SSL_LIBS)" $(TARGET)

stone.exe: stone.c
	$(CC) $(FLAGS) $? $(LIBS)

pop_stone.exe: md5c.obj
	$(MAKE) FLAGS=-DUSE_POP LIBS="md5c.obj" $(TARGET)

ssl_stone.exe: md5c.obj
	$(MAKE) FLAGS="-DUSE_POP -DUSE_SSL" LIBS="md5c.obj ssleay32.lib libeay32.lib" $(TARGET)
#	$(MAKE) FLAGS=-DUSE_SSL LIBS="ssl32.lib crypt32.lib" $(TARGET)

linux:
	$(MAKE) FLAGS="-DINET_ADDR -DCPP='\"/usr/bin/cpp -traditional\"' $(FLAGS)" stone

linux-pop:
	$(MAKE) TARGET=linux pop_stone

linux-ssl:
	$(MAKE) TARGET=linux ssl_stone

bsd:
	$(MAKE) FLAGS="-DCPP='\"/usr/bin/cpp -traditional\"' $(FLAGS)" stone

bsd-pop:
	$(MAKE) TARGET=bsd pop_stone

bsd-ssl:
	$(MAKE) TARGET=bsd ssl_stone

sun:
	$(MAKE) CC=gcc FLAGS="-DINET_ADDR -DNO_SNPRINTF -DIGN_SIGTERM -DCPP='\"/usr/lib/cpp\"' $(FLAGS)" stone

sun-pop:
	$(MAKE) TARGET=sun pop_stone

sun-ssl:
	$(MAKE) TARGET=sun ssl_stone

solaris:
	$(MAKE) CC=gcc FLAGS="-DNO_SNPRINTF $(FLAGS)" LIBS="-lnsl -lsocket $(LIBS)" stone

solaris-pop:
	$(MAKE) TARGET=solaris pop_stone

solaris-ssl:
	$(MAKE) TARGET=solaris ssl_stone

hp:
	$(MAKE) CC=gcc FLAGS="-DNO_SNPRINTF -DH_ERRNO -DCPP='\"/lib/cpp\"' $(FLAGS)" stone

hp-pop:
	$(MAKE) TARGET=hp pop_stone

hp-ssl:
	$(MAKE) TARGET=hp ssl_stone

irix:
	$(MAKE) FLAGS="-DNO_SNPRINTF $(FLAGS)" stone

irix-pop:
	$(MAKE) TARGET=irix pop_stone

irix-ssl:
	$(MAKE) TARGET=irix ssl_stone

win:
	$(MAKE) FLAGS="-DWINDOWS $(FLAGS)" LIBS="/MT wsock32.lib $(LIBS) /link /NODEFAULTLIB:LIBC" stone.exe

win-pop:
	$(MAKE) TARGET=win pop_stone.exe

win-ssl:
	$(MAKE) TARGET=win ssl_stone.exe

mingw.exe: stone.c
	$(CC) $(FLAGS) -o stone.exe $? $(LIBS)

mingw:
	$(MAKE) FLAGS="-DWINDOWS $(FLAGS)" LIBS="-lwsock32 $(LIBS)" mingw.exe

mingw-pop:
	$(MAKE) TARGET=mingw pop_stone

mingw-ssl: md5c.o
	$(MAKE) FLAGS="$(POP_FLAGS) $(SSL_FLAGS)" LIBS="$(POP_LIBS) -lssl32 -leay32" mingw

emx:
	$(MAKE) CC=gcc FLAGS="-DOS2 -Zmts -Zsysv-signals $(FLAGS)" LIBS="$(LIBS) -lsocket" stone.exe

emx-pop:
	$(MAKE) TARGET=emx pop_stone

emx-ssl:
	$(MAKE) TARGET=emx ssl_stone
