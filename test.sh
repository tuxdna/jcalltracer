rm -f keystore.db
# java -agentpath:./libcalltracer5.so=traceFile-./call.trace,filterFile-./filters.txt,outputType-xml,usage-uncontrolled -cp test-src com.test.HelloWorld
java -agentpath:./libcalltracer5.so=traceFile-./call.trace,filterList-com.test,outputType-xml,usage-uncontrolled -cp test-src com.test.HelloWorld
# cp call.trace trace.xml
# java -cp `echo java/third-party-libs/*.jar | tr ' ' :`:java/src/ Calltrace2Seq INPUTXMLFILE-./trace.xml  OUTPUTFILENAME-output
