JDK_HOME = /usr/lib/jvm/java-6-openjdk-amd64

SRC_DIR = src

INCLUDES = -I$(SRC_DIR) -I$(JDK_HOME)/include -I$(JDK_HOME)/include/linux

all: shared
	cd java; make

# Generate a shared library
shared: $(SRC_DIR)/jcalltracer.cpp
	g++  $(INCLUDES) -fPIC -c -g -Wall $(SRC_DIR)/keystore.c
	g++ -fpermissive $(INCLUDES) -fPIC -c -DMAX_THREADS=1000 -DJVMTI_TYPE=1 -g -Wall $(SRC_DIR)/jcalltracer.cpp
	g++ -shared -o libcalltracer5.so jcalltracer.o keystore.o -ldb -lstdc++

tags:
	rm -f TAGS
	find . -type f -print | etags -
	find $(JDK_HOME)/include -type f -print | etags - --append

clean:
	rm -f *.so *.o

docs:
	maruku README.md
