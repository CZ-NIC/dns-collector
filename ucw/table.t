Run: ../obj/ucw/table-t
Out <<EOF
col3_bool
     true
    false
            col0_str	col2_uint	col1_int	col3_bool
               sdsdf	XXX-22222	   10000	     true
                test	      100	    -100	    false
               sdsdf	XXX-22222	   10000	     true
                test	      100	    -100	    false
col3_bool
     true
    false
col3_bool	            col0_str
     true	               sdsdf
    false	                test
            col0_str	col3_bool	col2_uint
               sdsdf	     true	XXX-22222
                test	    false	      100
            col0_str	col3_bool	col2_uint	            col0_str	col3_bool	col2_uint	            col0_str	col3_bool	col2_uint
               sdsdf	     true	XXX-22222	               sdsdf	     true	XXX-22222	               sdsdf	     true	XXX-22222
                test	    false	      100	                test	    false	      100	                test	    false	      100
            col0_str	col1_int	col2_uint	col3_bool	col4_double
               sdsdf	   10000	XXX-22222	     true	        AAA
                test	    -100	      100	    false	       1.50
col0_int	 col1_any
     -10	    10000
     -10	     1.40
      10	     1.40
EOF
