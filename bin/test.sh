java -agentpath:./libjct.so=filterList=com.test,traceFile=calltrace.log -cp test-src com.test.HelloWorld
ruby bin/tree.rb

for f in thread-*.xml
do
    java -jar java/dist/calltrace2seq.jar -i $f  -o output/ -f $f
done
