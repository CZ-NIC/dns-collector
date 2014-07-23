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
1403685533 1403685533
1403678333 1403678333
xt_timestamp.parse error: 'Invalid value of timestamp: '1403685533X'.'.
xt_timestamp.parse error: 'Invalid value of timestamp: '2014X-06-25 08:38:53'.'.
xt_timestamp.parse error: 'Invalid value of timestamp: '2X014-06-25 08:38:53'.'.
xt_timestamp.parse error: 'Invalid value of timestamp: '2014-06-25 08:38:53X'.'.
xt_timestamp.parse error: 'Invalid value of timestamp: 'X1403685533'.'.
EOF
