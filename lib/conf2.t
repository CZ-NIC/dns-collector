# test config file
Top {
  nr1=16
  nrs1		2 3 5 \
	    7 11 13 \
	    \
	    17M
  nrs2	3 3k 3 3 3
  str1	"hello, world\n"
  str2	'Hagenuk,
      the best' "\
      " qu'est-ce que c'est?
  u1	0xbadcafebadbeefc0
  #d1	-1.14e-25
  d1 7%
  firsttime
  secondtime 56
  ^top.master:set	alice HB8+
  slaves:clear
}

unknown.ignored :-)

top.slaves	cairns gpua 7 7 -10% +10%
top.slaves	daintree rafc 4 5 -171%

topp.a=15
top.nr1=   15
