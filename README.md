
JCallTracer is a tool to generate Sequence Diagrams for big and multi-threaded Java applications.
Large Java application themselves use a lot of RAM, leaving little space for a JVMTI agent to maintain
its own data-structures to trace the calls.

This tool works ( experimental at the moment ), by offloading much of the internal data-structures to disk using squential writes. Sequential writes are fast!

Parts of this project are based on the code from [Java Call Tracer](http://sourceforge.net/projects/javacalltracer/)

How to build?

 * Prerequisites:

    - JDK v5.0 +
    - GNU C++ compiler ( g++ )
    - GNU Make tool
    - Ruby 1.9
    - Ant - Java build tool

 * Set the JDK_HOME environment variable:

    $ export JDK_HOME=/usr/lib/jvm/java-6-openjdk-amd46
 
 * Now invoke Make

    $ make


 * This is your agent: libjct.so

    $ java -agentpath:./libjct.so com.test.HelloWorld


 * Checking out the tool

    Simply invoke the test script

    $ sh bin/test.sh

    OR

    $ javac test-src/com/test/HelloWorld.java
    $ java -agentpath:./libjct.so=filterList=com.test,traceFile=calltrace.log -cp test-src com.test.HelloWorld

    Loading agent: libjct
    
    Hello World!
    
    12
    saleem


    Now view SVG files in Firefox

    $ firefox output/
