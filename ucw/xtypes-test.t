Run: ../obj/ucw/xtypes-test
Out <<EOF
4 4
4KB 4KB
4MB 4MB
4GB 4GB
4TB 4TB
xt_size.parse error: 'Invalid format of size: '1X'.'.
xt_size.parse error: 'Invalid value of size: 'KB'.'.
xt_size.parse error: 'Invalid value of size: ''.'.
0 false
1 true
false false
true true
EOF
