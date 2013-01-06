JDK_HOME := $(JDK_HOME)

SRC_DIR = src

INCLUDES = -I$(SRC_DIR) -I$(JDK_HOME)/include -I$(JDK_HOME)/include/linux

all: shared java test
	mv src/libjct.so .
	cd java; ant jar

test:
	cd test-src; make

# Generate a shared library
shared: 
	cd src; make

tags:
	rm -f TAGS
	find src -type f -print | etags -
	find $(JDK_HOME)/include -type f -print | etags - --append

clean:
	cd src; make clean
	cd java; ant clean
	rm -f *.so *.o
docs:
	maruku README.md
