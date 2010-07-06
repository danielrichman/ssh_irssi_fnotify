#!/usr/bin/make -f
# -*- makefile -*-

#    Copyright (C) 2008  Daniel Richman
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    For a full copy of the GNU General Public License, 
#    see <http://www.gnu.org/licenses/>.

CC = gcc
GDK_PIXBUF_CSOURCE = gdk-pixbuf-csource
OPT = -O2 -march=native
PEDANT = -Wall

PKGCFGS  = gtk+-2.0 gthread-2.0 glib-2.0 libnotify
P_CFLAGS = $(shell pkg-config --cflags $(PKGCFGS))
P_LIBS   = $(shell pkg-config --libs   $(PKGCFGS))

CFLAGS = $(OPT) $(PEDANT) $(P_CFLAGS)
LINKLIBS = $(P_LIBS)

cfiles      := $(wildcard *.c)
pngfiles    := $(wildcard *.png)
pngheaders  := $(patsubst %.png,%.h,$(pngfiles))
objects     := $(patsubst %.c,%.o,$(cfiles))
headers     := $(wildcard *.h)

ssh_irssi_fnotify : $(objects)
	$(CC) $(CFLAGS) $(LINKLIBS) -o $@ $(objects)

main.c : $(pngheaders)

%.h : %.png
	$(GDK_PIXBUF_CSOURCE) --raw --build-list $(patsubst %.png,%,$<) $< > $@

%.o : %.c $(headers)
	$(CC) -c $(CFLAGS) $< -o $@

clean :
	rm -f $(objects) $(pngheaders) ssh_irssi_fnotify

.PHONY : clean all
.DEFAULT_GOAL := ssh_irssi_fnotify
