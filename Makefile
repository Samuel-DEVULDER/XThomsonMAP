#
# Makefile for XThomsonMAP
#
# (c) Samuel DEVULDER, 2020.
#

RM=rm
CC=gcc
ZIP=zip
GIT=git

all: XThomsonMAP.zip

clean:
	-$(RM) *.usr *.zip

%.zip: %.usr LICENSE README.md
	$(ZIP) -9vor $@ $^

%.usr: %.c
	$(CC) -s -O2 -fomit-frame-pointer -shared -Wl,--kill-at -Wall $< -o $@
	
update:
	git pull

git:
	git add .
	git commit -m "$m"
	git push -u origin master 