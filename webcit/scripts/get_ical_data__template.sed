#! /bin/sed -nf

H
$ {
  x
  s/\n//g
  p
}
$ { 
  s/.*typedef *enum *__ICALTYPE__ *{\(.*\)} *__ICALTYPE__ *;.*/\1/
 }
