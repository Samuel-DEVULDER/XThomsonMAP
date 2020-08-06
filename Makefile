#
# Makefile for XThomsonMAP
#
# (c) Samuel DEVULDER, 2020.
#

PRJ=XThomsonMAP

RM=rm
CC=gcc
ZIP=zip
GIT=git

CFLAGS=-O2 -fomit-frame-pointer -Wall
LFLAGS= -s -Wl,--kill-at -shared 

all: $(PRJ).usr

zip: $(PRJ).zip

clean:
	-$(RM) *.usr *.zip

%.zip: %.usr LICENSE README.md #testcases/*.MAP
	$(ZIP) -9vor $@ $^

%.usr: %.c Makefile
	$(CC) $(CFLAGS) $(LFLAGS) $< -o $@
	
update:
	git pull

git: all
	git add .
	git commit -m "$m"
	git push -u origin master 