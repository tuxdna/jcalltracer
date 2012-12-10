JDK_HOME = /usr/lib/jvm/java-openjdk

SRC_DIR = src

INCLUDES = -I$(SRC_DIR) -I$(JDK_HOME)/include -I$(JDK_HOME)/include/linux

all: shared java

test:
	cd test-src; make

java:
	cd java; make

keystore.o:
	g++  $(INCLUDES) -fPIC -c -g -Wall $(SRC_DIR)/keystore.c

# Generate a shared library
shared: keystore.o $(SRC_DIR)/jcalltracer.cpp
	g++ -fpermissive $(INCLUDES) -fPIC -c -DMAX_THREADS=1000 -DJVMTI_TYPE=1 -g -Wall $(SRC_DIR)/jcalltracer.cpp
	g++ -shared -o libjct.so jcalltracer.o keystore.o -ldb -lstdc++

tags:
	rm -f TAGS
	find src -type f -print | etags -
	find $(JDK_HOME)/include -type f -print | etags - --append

clean:
	rm -f *.so *.o

docs:
	maruku README.md
