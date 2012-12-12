
JDK_HOME = /usr/lib/jvm/java-6-openjdk
SRC_DIR = .
INCLUDES = -I$(SRC_DIR) -I$(JDK_HOME)/include -I$(JDK_HOME)/include/linux

all: shared

keystore.o:
	g++  $(INCLUDES) -fPIC -c -g -Wall $(SRC_DIR)/keystore.c

# Generate a shared library
shared: keystore.o $(SRC_DIR)/jcalltracer.cpp
	g++ -ggdb $(INCLUDES) -fPIC -c -DMAX_THREADS=1000 -DJVMTI_TYPE=1 -Wall $(SRC_DIR)/jcalltracer.cpp
	g++ -ggdb -shared -o libjct.so jcalltracer.o keystore.o -ldb -lstdc++

clean:
	rm -f *.so *.o *~
