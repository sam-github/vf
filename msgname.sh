#!/bin/sh
sed -e s/_\([_A-Z]*\) *\(.*\)/case \2: return "\1";/
