#
# Copyright (C) 2002 Ken'ichi Fukamachi <fukachan@fml.org>
#          All rights reserved. 
#
# $FML: Makefile,v 1.1 2002/01/07 10:51:54 fukachan Exp $
# 

all: ipfilter.o

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *~
	rm -f *.o
