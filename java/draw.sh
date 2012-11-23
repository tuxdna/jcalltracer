#!/bin/sh

# Sample invocation:
# sh draw.sh INPUTXMLFILE-/tmp/calltrace.xml  OUTPUTFILENAME-output

LIBS=`echo third-party-libs/*.jar | tr ' ' :`
CLASSPATH=$LIBS:src/

java -cp $CLASSPATH Calltrace2Seq $@

