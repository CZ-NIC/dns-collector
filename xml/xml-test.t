# Tests for the XML parser
# (c) 2008 Pavel Charvat <pchar@ucw.cz>

Run:	../obj/xml/xml-test
In:	<?xml version="1.0"?>
	<html></html>
Out:	PULL: start
	PULL: eof

Run:	../obj/xml/xml-test -s
In:	<?xml version="1.0" encoding="ISO-8859-1"?>
	<html><a a1="val1" a2="val2">text1&amp;amp;&lt;</a>text2</html>
Out:	PULL: start
	SAX:  document_start
	SAX:  xml_decl version=1.0 standalone=0 fb_encoding=ISO-8859-1
	SAX:  stag <html>
	SAX:  stag <a> a1='val1' a2='val2'
	SAX:  chars text='text1&amp;<'
	SAX:  etag </a>
	SAX:  chars text='text2'
	SAX:  etag </html>
	SAX:  document_end
	PULL: eof

Run:	../obj/xml/xml-test -sptd
In:	<?xml version="1.0"?>
	<!DOCTYPE root [
	<!ELEMENT root (#PCDATA|a)*>
	<!ENTITY % pe1 "<!ENTITY e1 'text'>">
	%pe1;
	<!ENTITY e2 '&lt;&e1;&gt;'>
	<!ELEMENT a (#PCDATA)*>
	]>
	<root>&e1;<a>&e2;</a></root>
Out:	PULL: start
	SAX:  document_start
	SAX:  xml_decl version=1.0 standalone=0 fb_encoding=UTF-8
	SAX:  doctype_decl type=root public='' system='' extsub=0 intsub=1
	SAX:  dtd_start
	SAX:  dtd_end
	SAX:  stag <root>
	PULL: stag <root>
	SAX:  chars text='text'
	PULL: chars text='text'
	SAX:  stag <a>
	PULL: stag <a>
	SAX:  chars text='<text>'
	PULL: chars text='<text>'
	PULL: etag </a>
	SAX:  etag </a>
	PULL: etag </root>
	SAX:  etag </root>
	SAX:  document_end
	PULL: eof
	DOM:  element <root>
	DOM:      chars text='text'
	DOM:      element <a>
	DOM:          chars text='<text>'
