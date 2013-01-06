
This project is based on the code from [Java Call Tracer](http://sourceforge.net/projects/javacalltracer/)

How to build?

 * Prerequisites:

    - JDK v5.0 +
    - GNU C++ compiler ( g++ )
    - GNU Make tool

 * Set the JDK_HOME environment variable:

    $ export JDK_HOME=/usr/lib/jvm/java-6-openjdk-amd46
 
 * Now invoke Make

    $ make


 * This is your agent: libjct.so

    $ java -agentpath:./libjct.so com.test.HelloWorld


 * Testing the agent

    $ javac test-src/com/test/HelloWorld.java
    $ java -agentpath:./libjct.so=filterList=com.test,traceFile=calltrace.log -cp test-src com.test.HelloWorld
    Loading agent: libjct
    
    Hello World!
    
    12
    saleem
