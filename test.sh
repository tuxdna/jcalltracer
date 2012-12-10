rm -f keystore.db
java -agentpath:./libjct.so=filterList=com.test,traceFile=calltrace.log -cp test-src com.test.HelloWorld
# cp call.trace trace.xml
# java -cp `echo java/third-party-libs/*.jar | tr ' ' :`:java/src/ Calltrace2Seq INPUTXMLFILE-./trace.xml  OUTPUTFILENAME-output
