Run: ../obj/ucw/table-test -T cols:col3_bool
Out <<EOF
col3_bool
     true
    false
EOF


Run: ../obj/ucw/table-test -T cols:col3_bool,col0_str
Out <<EOF
col3_bool	            col0_str
     true	         sdsdf,aaaaa
    false	          test,bbbbb
EOF


Run: ../obj/ucw/table-test -T cols:col3_bool,col0_str
Out <<EOF
col3_bool	            col0_str
     true	         sdsdf,aaaaa
    false	          test,bbbbb
EOF


Run: ../obj/ucw/table-test -T cols:col3_bool,col0_str,col4_double
Out <<EOF
col3_bool	            col0_str	col4_double
     true	         sdsdf,aaaaa	       1.50
    false	          test,bbbbb	       1.50
EOF


Run: ../obj/ucw/table-test -T cols:col3_bool,col0_str,col4_double -T 'col-delim:;' -T fmt:machine
Out <<EOF
col3_bool;col0_str;col4_double
true;sdsdf,aaaaa;1.50
false;test,bbbbb;1.50
EOF


Run: ../obj/ucw/table-test -T cols:col3_bool,col0_str,col4_double -T 'col-delim:;' -T fmt:machine -T header:1
Out <<EOF
col3_bool;col0_str;col4_double
true;sdsdf,aaaaa;1.50
false;test,bbbbb;1.50
EOF


Run: ../obj/ucw/table-test -T cols:col3_bool,col0_str,col4_double -T 'col-delim:;' -T fmt:machine -T header:0
Out <<EOF
true;sdsdf,aaaaa;1.50
false;test,bbbbb;1.50
EOF

Run: ../obj/ucw/table-test -T cols:col3_bool,col0_str,col4_double -T 'col-delim:;' -T fmt:machine -T noheader
Out <<EOF
true;sdsdf,aaaaa;1.50
false;test,bbbbb;1.50
EOF

Run: ../obj/ucw/table-test -T cols:col3_bool,col0_str,col4_double -T 'col-delim:AHOJ' -T fmt:machine -T noheader
Out <<EOF
trueAHOJsdsdf,aaaaaAHOJ1.50
falseAHOJtest,bbbbbAHOJ1.50
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
