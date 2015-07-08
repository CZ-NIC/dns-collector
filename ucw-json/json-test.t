# Tests for the JSON library
# (c) 2015 Martin Mares <mj@ucw.cz>

### Literals ###

Name:	Null
Run:	../obj/ucw-json/json-test -rw
In:	null
Out:	null

Name:	True
In:	true
Out:	true

Name:	False
In:	false
Out:	false

Name:	Invalid literal 1
In:	lomikel
Exit:	1
Err:	ERROR: Invalid literal name at line 1:8

Name:	Invalid literal 2
In:	aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
Exit:	1
Err:	ERROR: Invalid literal name at line 1:101

### Numbers ###

Name:	Plain number
In:	42
Out:	42

Name:	Negative number
In:	-42
Out:	-42

Name:	Zero number
In:	0
Out:	0

# The largest number guaranteed to be precise by RFC 7159
Name:	Large number
In:	9007199254740991
Out:	9007199254740991

Name:	Fractional number 1
In:	12345.54321
Out:	12345.54321

Name:	Fractional number 2
In:	0.333333333
Out:	0.333333333

Name:	Number in scientific notation 1
In:	3.14159e20
Out:	3.14159e+20

Name:	Number in scientific notation 2
In:	3.14159e+20
Out:	3.14159e+20

Name:	Number in scientific notation 3
In:	3.14159e-20
Out:	3.14159e-20

Name:	No leading zero allowed
In:	01234
Exit:	1
Err:	ERROR: Malformed number: leading zero at line 1:2

Name:	Bare sign is not a number
In:	-
Exit:	1
Err:	ERROR: Malformed number: just minus at line 1:2

Name:	No leading decimal point allowed
In:	.1234
Exit:	1
Err:	ERROR: Numbers must start with a digit at line 1:1

Name:	Digits after decimal point required
In:	1234.
Exit:	1
Err:	ERROR: Malformed number: no digits after decimal point at line 1:6

Name:	Exponent part must be non-empty 1
In:	1e
Exit:	1
Err:	ERROR: Malformed number: empty exponent at line 1:3

Name:	Exponent part must be non-empty 2
In:	1e+
Exit:	1
Err:	ERROR: Malformed number: empty exponent at line 1:4

Name:	Number out of range
In:	1e3000000
Exit:	1:
Err:	ERROR: Number out of range at line 1:10

### Strings ###

Name:	Plain string
In:	"foo"
Out:	"foo"

Name:	Empty string
In:	""
Out:	""

Name:	UTF-8 string
In:	"šelmička"
Out:	"šelmička"

Name:	Unterminated string
In:	"infinity
Exit:	1
Err:	ERROR: Unterminated string at line 1:10

Name:	Multi-line string
In:	"infi
	nity"
Exit:	1
Err:	ERROR: Unterminated string at line 1:6

# Some characters are written as \uXXXX on output, which is correct
Name:	Escaped characters
In:	"\"\\\/\b\f\n\r\t"
Out:	"\"\\/\u0008\u000c\n\r\t"

Name:	Improper escaped characters
In:	"\a"
Exit:	1
Err:	ERROR: Invalid backslash sequence in string at line 1:3

Name:	Unicode escapes
In:	"\u0041\u010d\u010D\u0001"
Out:	"Ačč\u0001"

Name:	Improper Unicode escapes 1
In:	"\u"
Exit:	1
Err:	ERROR: Invalid Unicode escape sequence at line 1:4

Name:	Improper Unicode escapes 2
In:	"\u
Exit:	1
Err:	ERROR: Invalid Unicode escape sequence at line 1:4

Name:	Improper Unicode escapes 3
In:	"\uABCZ"
Exit:	1
Err:	ERROR: Invalid Unicode escape sequence at line 1:7

### Unicode magic ###

# TAB is forbidden
Name:	Control characters 1
Run:	../obj/ucw-json/json-test -RW
In:	"<09>"
Exit:	1
Err:	ERROR: Invalid control character in string at line 1:2

# Surprisingly, DEL is not
Name:	Control characters 2
In:	"<7f>"
Out:	"<7f>"<0a>

# U+31234
Name:	UTF-8 outside BMP
In:	"<f0><b1><88><b4>"
Out:	"<f0><b1><88><b4>"<0a>

Name:	Private use characters in BMP
In:	"<ef><80><80>"
Exit:	1
Err:	ERROR: Invalid private-use character in string at line 1:2

Name:	UTF-8 outside BMP
In:	"<f0><b1><88><b4>"
Out:	"<f0><b1><88><b4>"<0a>

Name:	Private use characters outside BMP
In:	"<f3><b0><80><80>"
Exit:	1
Err:	ERROR: Invalid private-use character in string at line 1:2

# U+FF0000
Name:	UTF-8 outside UCS
In:	"<f8><bf><b0><80><80>"
Exit:	1
Err:	ERROR: Invalid non-Unicode character in string at line 1:2

# U+D800
Name:	UTF-8 surrogate 1
In:	"<ed><a0><80>"
Exit:	1
Err:	ERROR: Invalid surrogate character in string at line 1:2

# U+DC00
Name:	UTF-8 surrogate 2
In:	"<ed><b0><80>"
Exit:	1
Err:	ERROR: Invalid surrogate character in string at line 1:2

# U+FEFF
Name:	UTF-8 BOM
In:	<ef><bb><bf>
Exit:	1
Err:	ERROR: Misplaced byte-order mark, complain in Redmond at line 1:1

Name:	Escaped NUL
In:	"\u0000"
Exit:	1
Err:	ERROR: Zero bytes in strings are not supported at line 1:7

Name:	Escaped surrogate
In:	"\udaff\udcba"
Out:	"<f3><8f><b2><ba>"<0a>

Name:	Escaped surrogate malformation 1
In:	"\udaff"
Exit:	1
Err:	ERROR: Escaped high surrogate codepoint must be followed by a low surrogate codepoint at line 1:8

Name:	Escaped surrogate malformation 2
In:	"\udcff"
Exit:	1
Err:	ERROR: Invalid escaped surrogate codepoint at line 1:7

Name:	Escaped low private-use character
In:	"\uedac"
Exit:	1
Err:	ERROR: Invalid escaped private-use character at line 1:7

Name:	Escaped high private-use character
In:	"\udbff\udc00"
Exit:	1
Err:	ERROR: Invalid escaped private-use character at line 1:13

# Switch back to normal mode
Name:	Invalid ASCII character
Run:	../obj/ucw-json/json-test -rw
In:	@
Exit:	1
Err:	ERROR: Invalid character at line 1:1

### Arrays ###

Name:	Empty array
In:	[]
Out:	[]

Name:	One-element array
In:	[1]
Out:	[ 1 ]

Name:	Two-element array
In:	[1,2]
Out:	[ 1, 2 ]

Name:	Nested arrays
In:	[[1,2],["a","b"]]
Out:	[ [ 1, 2 ], [ "a", "b" ] ]

Name:	Multi-line array
In:	[
	"a",    null,false
	,false
	]
Out:	[ "a", null, false, false ]

Name:	Unterminated array 1
In:	[1,2
Exit:	1
Err:	ERROR: Comma or right bracket expected at line 2:0

Name:	Unterminated array 2
In:	[1,2,
Exit:	1
Err:	ERROR: Unterminated array at line 2:0

Name:	Extra comma not allowed
In:	[1,2,]
Exit:	1
Err:	ERROR: Misplaced end of array at line 1:6

Name:	Solitary comma not allowed
In:	,
Exit:	1
Err:	ERROR: Misplaced comma at line 1:1

Name:	Deeply nested array
In:	[[[[[[[[[[]]]]]]]]]]
Out:	[ [ [ [ [ [ [ [ [ [] ] ] ] ] ] ] ] ] ]

Name:	Deeply unclosed array
In:	[[[[[[[[[[]
Exit:	1
Err:	ERROR: Comma or right bracket expected at line 2:0

Name:	Missing comma
In:	[1 2]
Exit:	1
Err:	ERROR: Comma or right bracket expected at line 1:5

### Objects ###

Name:	Empty object
In:	{}
Out:	{}

Name:	One-entry object
In:	{"a":"b"}
Out:	{ "a": "b" }

Name:	Two-entry object
In:	{"a":1,"b":2}
Out:	{ "a": 1, "b": 2 }

Name:	Nested objects
In:	{
		"a": [1,2],
		"b": { "x": true, "y": false }
	}
Out:	{ "a": [ 1, 2 ], "b": { "x": true, "y": false } }

Name:	Unterminated object 1
In:	{
Exit:	1
Err:	ERROR: Unterminated object at line 2:0

Name:	Unterminated object 2
In:	{ "a"
Exit:	1
Err:	ERROR: Colon expected at line 2:0

Name:	Unterminated object 3
In:	{ "a":
Exit:	1
Err:	ERROR: Unterminated object at line 2:0

Name:	Unterminated object 4
In:	{ "a":1,
Exit:	1
Err:	ERROR: Unterminated object at line 2:0

Name:	Extra comma not allowed in objects
In:	{ "a":1, }
Exit:	1
Err:	ERROR: Misplaced end of object at line 1:10

Name:	Non-string key
In:	{1:2}
Exit:	1
Err:	ERROR: Object key must be a string at line 1:3

Name:	Repeated key
In:	{"a":1, "a":2}
Exit:	1
Err:	ERROR: Key already set at line 1:14

Name:	Missing object comma
In:	{"a":1 "b":2}
Exit:	1
Err:	ERROR: Comma expected at line 1:10

### Top-level problems ###

Name:	Empty input
Exit:	1
Err:	ERROR: Empty input at line 1:0

Name:	Multiple values
In:	1 2
Exit:	1
Err:	ERROR: Only one top-level value allowed at line 1:4
