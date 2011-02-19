CFLAGS = -Os
ARFLAGS = -rcs

targetlib = parser.a
parserset = conv.o dump.o mix.o rmi.o smf.o vos.o
infolstset = smf_control.lst smf_instrument.lst smf_meta.lst vos_genre.lst

all : $(targetlib)

conv.o : conv.c conv.h mix.h smf.h vos.h
	$(CC) $(CFLAGS) -c conv.c

dump.o : dump.c dump.h dump.inl conv.h mix.h rmi.h smf.h vos.h
	$(CC) $(CFLAGS) -c dump.c

dump.inl : $(infolstset) lst2inl.py
	./lst2inl.py

mix.o : mix.c mix.h smf.h
	$(CC) $(CFLAGS) -c mix.c

rmi.o : smf.h rmi.c rmi.h
	$(CC) $(CFLAGS) -c rmi.c

smf.o : smf.c smf.h
	$(CC) $(CFLAGS) -c smf.c

vos.o : smf.h vos.c vos.h
	$(CC) $(CFLAGS) -c vos.c

$(targetlib) : $(parserset)
	$(AR) $(ARFLAGS) $(targetlib) $(parserset)

.PHONY : all clean

clean :
	$(RM) dump.inl $(parserset) $(targetlib)
