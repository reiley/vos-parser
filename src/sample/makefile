CFLAGS = -Os

parserdir = ../parser
parserlib = parser.a

all : main.exe player.exe midimix.exe miditest.exe vosmix.exe vostest.exe

main.exe : main.o $(parserdir)/$(parserlib)
	$(CC) $(CFLAGS) -o main.exe main.o $(parserdir)/$(parserlib) -lwinmm

main.o : main.c
	$(CC) $(CFLAGS) -c main.c

player.exe : player.o $(parserdir)/$(parserlib)
	$(CC) $(CFLAGS) -o player.exe player.o $(parserdir)/$(parserlib) -lwinmm

player.o : player.c
	$(CC) $(CFLAGS) -c player.c

midimix.exe : midimix.o $(parserdir)/$(parserlib)
	$(CC) $(CFLAGS) -o midimix.exe midimix.o $(parserdir)/$(parserlib)

midimix.o : midimix.c
	$(CC) $(CFLAGS) -c midimix.c

miditest.exe : miditest.o $(parserdir)/$(parserlib)
	$(CC) $(CFLAGS) -o miditest.exe miditest.o $(parserdir)/$(parserlib)

miditest.o : miditest.c
	$(CC) $(CFLAGS) -c miditest.c

vosmix.exe : vosmix.o $(parserdir)/$(parserlib)
	$(CC) $(CFLAGS) -o vosmix.exe vosmix.o $(parserdir)/$(parserlib)

vosmix.o : vosmix.c
	$(CC) $(CFLAGS) -c vosmix.c

vostest.exe : vostest.o $(parserdir)/$(parserlib)
	$(CC) $(CFLAGS) -o vostest.exe vostest.o $(parserdir)/$(parserlib)

vostest.o : vostest.c
	$(CC) $(CFLAGS) -c vostest.c

$(parserdir)/$(parserlib) :
	$(MAKE) -C $(parserdir) $(parserlib)

.PHONY : clean

clean :
	$(RM) *.exe *.o
