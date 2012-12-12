JDK_HOME = /usr/lib/jvm/java-6-openjdk

SRC_DIR = src

INCLUDES = -I$(SRC_DIR) -I$(JDK_HOME)/include -I$(JDK_HOME)/include/linux

all: shared java test
	mv src/libjct.so .

test:
	cd test-src; make

java:
	cd java; make

# Generate a shared library
shared: 
	cd src; make

tags:
	rm -f TAGS
	find src -type f -print | etags -
	find $(JDK_HOME)/include -type f -print | etags - --append

clean:
	cd src; make clean
	rm -f *.so *.o
docs:
	maruku README.md
