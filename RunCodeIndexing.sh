#!/bin/sh
#cscope -b -q
find . -not -path "./build/*" -type f -iname "*.h" -o -iname "*.cpp"  > cscope.files
cscope -b

