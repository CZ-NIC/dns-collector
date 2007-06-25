# Tests for fastbufs

Run:	../obj/lib/fb-file-t
Out:	112
	<hello><hello><hello><hello><hello><hello><hello><hello><hello><hello><hello><hello><hello><hello><hello><hello>
	112 116

Run:	../obj/lib/fb-grow-t
Out:	<10><10><0>1234512345<10><9>5<10>
	<10><10><0>1234512345<10><9>5<10>
	<10><10><0>1234512345<10><9>5<10>
	<10><10><0>1234512345<10><9>5<10>
	<10><10><0>1234512345<10><9>5<10>

Run:	../obj/lib/fb-pool-t
