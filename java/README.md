How to build ?

     $ make

How to run?

     $ java -cp `echo third-party-libs/*.jar | tr ' ' :`:src/ Calltrace2Seq

Example:

    $ java -cp `echo third-party-libs/*.jar | tr ' ' :`:src/ Calltrace2Seq INPUTXMLFILE-/tmp/calltrace.xml  OUTPUTFILENAME-output
    Generated the SVG file: /tmp/./output.svg
    Generated the TRC file: /tmp/./output.trc
    Generated the JPG file: /tmp/./output.jpg

