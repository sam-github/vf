#!/usr/local/bin/perl -wnp
#
# QNX4 (<EM>http://www.qnx.com</EM>). O

s#(\S*)\s+\(<EM>(.*)</EM>\)#<A HREF="$2">$1</A>#g;

