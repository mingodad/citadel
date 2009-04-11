#! /bin/sed -nf

H
$ {
  x
  s/\n//g
  p
}
$ { 
  s/.*icalproperty_kind {\(.*\)} icalproperty_kind.*/\1/
 }
