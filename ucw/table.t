Run: ../obj/ucw/table-t
Out <<EOF
col3_bool
     true
    false
            col0_str	col2_uint	col1_int	col3_bool
         sdsdf,aaaaa	XXX-22222	   10000	     true
          test,bbbbb	      100	    -100	    false
         sdsdf,aaaaa	XXX-22222	   10000	     true
          test,bbbbb	      100	    -100	    false
col3_bool
     true
    false
col3_bool	            col0_str
     true	         sdsdf,aaaaa
    false	          test,bbbbb
            col0_str	col3_bool	col2_uint
         sdsdf,aaaaa	     true	XXX-22222
          test,bbbbb	    false	      100
            col0_str	col3_bool	col2_uint	            col0_str	col3_bool	col2_uint	            col0_str	col3_bool	col2_uint
         sdsdf,aaaaa	     true	XXX-22222	         sdsdf,aaaaa	     true	XXX-22222	         sdsdf,aaaaa	     true	XXX-22222
          test,bbbbb	    false	      100	          test,bbbbb	    false	      100	          test,bbbbb	    false	      100
            col0_str	col1_int	col2_uint	col3_bool	col4_double
         sdsdf,aaaaa	   10000	XXX-22222	     true	        AAA
          test,bbbbb	    -100	      100	    false	       1.50
col0_int	 col1_any
     -10	    10000
     -10	     1.40
10,20,30	1.40,1.50,1.60
EOF
