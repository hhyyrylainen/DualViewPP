#!/bin/sh
G_DEBUG=resident-modules valgrind --tool=helgrind --num-callers=20 --log-file=hgdump ./dualviewpp
