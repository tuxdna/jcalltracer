#JDK_HOME = /usr/lib/jvm/java-6-openjdk-amd64

SRC_DIR = src

INCLUDES = -I$(SRC_DIR) -I$(JDK_HOME)/include -I$(JDK_HOME)/include/linux

all: calltracer5.so
	cd java; make

# Generate a shared library
# Make sure that #define JVMPI_TYPE and #define JVMTI_TYPE and #define TEST_MYTRACE are commented in ctrace.c file.
calltracer5.so: $(SRC_DIR)/ctrace.c
	g++ -fpermissive $(INCLUDES) -fPIC -c -DMAX_THREADS=1000 -DJVMTI_TYPE=1 -g -Wall $(SRC_DIR)/ctrace.c
	g++ -shared -Wl -o libcalltracer5.so ctrace.o

tags:
	find . -type f -print | etags -
	find $(JDK_HOME)/include -type f -print | etags - --append

clean:
	rm -f *.so *.o calltracer5 TAGS

docs:
	maruku README.md
