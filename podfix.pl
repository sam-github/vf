#!/usr/local/bin/perl -wnp
#
# QNX4 (<EM>http://www.qnx.com</EM>). O

$root = "http://toronto.qenesis.com/Sam/";

s#$root##g;
s#(\S*)\s+\(<EM>(.*)</EM>\)#<A HREF="$2">$1</A>#g;

#s/A<([^\|]*)\|(.*)>/<A href="">E<60>A>/g < vf.html


