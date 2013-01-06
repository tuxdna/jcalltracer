
JDK_HOME := $(JDK_HOME)
SRC_DIR = .
INCLUDES = -I$(SRC_DIR) -I$(JDK_HOME)/include -I$(JDK_HOME)/include/linux

all: shared

# Generate a shared library
shared: $(SRC_DIR)/jcalltracer.cpp
	g++ -ggdb $(INCLUDES) -fPIC -c -DMAX_THREADS=1000 -DJVMTI_TYPE=1 -Wall $(SRC_DIR)/jcalltracer.cpp
	g++ -ggdb -shared -o libjct.so jcalltracer.o -ldb -lstdc++

clean:
	rm -f *.so *.o *~
