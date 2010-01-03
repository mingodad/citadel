#!/bin/bash


#./stringbuf_IO_test -p 6666 -i 0.0.0.0 -s 2 -l 50


./stringbuf_IO_test -p 6666 -i 0.0.0.0 -s2 -b55    &
cat posttest_blob.txt |nc 127.0.0.1 6666

./stringbuf_IO_test -p 6666 -i 0.0.0.0 -s2 -b482    &
cat posttest_headers.txt |nc 127.0.0.1 6666

./stringbuf_IO_test -p 6666 -i 0.0.0.0 -s2 -b537    &
cat posttest.txt |nc 127.0.0.1 6666


./stringbuf_IO_test -p 6666 -i 0.0.0.0 -s2 -n1    &
cat posttest_blob.txt |nc 127.0.0.1 6666

./stringbuf_IO_test -p 6666 -i 0.0.0.0 -s2 -n13    &
cat posttest_headers.txt |nc 127.0.0.1 6666

./stringbuf_IO_test -p 6666 -i 0.0.0.0 -s2 -n14    &
cat posttest.txt |nc 127.0.0.1 6666

posttest.txt
