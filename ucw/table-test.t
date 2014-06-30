Run: ../obj/ucw/table-test -T cols:col3_bool
Out <<EOF
col3_bool
     true
    false
EOF


Run: ../obj/ucw/table-test -T cols:col3_bool,col0_str
Out <<EOF
col3_bool	            col0_str
     true	               sdsdf
    false	                test
EOF


Run: ../obj/ucw/table-test -T cols:col3_bool,col0_str
Out <<EOF
col3_bool	            col0_str
     true	               sdsdf
    false	                test
EOF


Run: ../obj/ucw/table-test -T cols:col3_bool,col0_str,col4_double
Out <<EOF
col3_bool	            col0_str	col4_double
     true	               sdsdf	       1.50
    false	                test	       1.50
EOF


Run: ../obj/ucw/table-test -T cols:col3_bool,col0_str,col4_double -T 'col-delim:;' -T fmt:machine
Out <<EOF
col3_bool;col0_str;col4_double
true;sdsdf;1.50
false;test;1.50
EOF


Run: ../obj/ucw/table-test -T cols:col3_bool,col0_str,col4_double -T 'col-delim:;' -T fmt:machine -T header:1
Out <<EOF
col3_bool;col0_str;col4_double
true;sdsdf;1.50
false;test;1.50
EOF


Run: ../obj/ucw/table-test -T cols:col3_bool,col0_str,col4_double -T 'col-delim:;' -T fmt:machine -T header:0
Out <<EOF
true;sdsdf;1.50
false;test;1.50
EOF

Run: ../obj/ucw/table-test -T cols:col3_bool,col0_str,col4_double -T 'col-delim:;' -T fmt:machine -T noheader
Out <<EOF
true;sdsdf;1.50
false;test;1.50
EOF

Run: ../obj/ucw/table-test -T cols:col3_bool,col0_str,col4_double -T 'col-delim:AHOJ' -T fmt:machine -T noheader
Out <<EOF
trueAHOJsdsdfAHOJ1.50
falseAHOJtestAHOJ1.50
EOF

Run: ../obj/ucw/table-test -T 'cols:*' -T fmt:blockline
Out <<EOF
col0_str: sdsdf
col1_int: 10000
col2_uint: XXX-22222
col3_bool: true
col4_double: 1.50

col0_str: test
col1_int: -100
col2_uint: 100
col3_bool: false
col4_double: 1.50

EOF

Run: ../obj/ucw/table-test -n
Out <<EOF
Tableprinter option parser returned: 'Unknown table column 'test_col0_str', possible column names are: col0_str, col1_int, col2_uint, col3_bool, col4_double.'.
EOF


Run: ../obj/ucw/table-test -d
Out <<EOF
col0_int	 col1_int	 col2_int
       0	        1	        2
      10	       11	       12
EOF


Run: ../obj/ucw/table-test -i
Out <<EOF
Tableprinter option parser returned error: "Tableprinter: invalid option: 'invalid:option'.".
Tableprinter option parser returned error: "Tableprinter: invalid option: 'invalid'.".
setting key: novaluekey; value: (null)
setting key: valuekey; value: value
EOF
