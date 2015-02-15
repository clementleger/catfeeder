#############################################################################
#
# Makefile for librf24 examples on Raspberry Pi
#
# License: GPL (General Public License)
# Author:  gnulnulf <arco@appeltaart.mine.nu>
# Date:    2013/02/07 (version 1.0)
#
# Description:
# ------------
# use make all and make install to install the examples
# You can change the install directory by editing the prefix line
#
prefix := /usr/local

# The recommended compiler flags for the Raspberry Pi
CCFLAGS=-Ofast -mfpu=vfp -mfloat-abi=hard -march=armv6zk -mtune=arm1176jzf-s
CCFLAGS += -I ../arduino/catfeeder/
#CCFLAGS=

# define all programs
PROGRAMS = catfeeder

SOURCES = catfeeder.cpp cmdline.c

all: ${PROGRAMS}

${PROGRAMS}: ${SOURCES}
	g++ ${CCFLAGS} -I. -Wall -lrf24-bcm ${SOURCES} -o $@

cmdline.c: catfeeder.ggo
	gengetopt < $<

clean:
	rm -rf $(PROGRAMS) cmdline.c cmdline.h
