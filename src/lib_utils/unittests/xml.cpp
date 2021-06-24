
#include "tests/tests.hpp"
#include "lib_utils/xml.hpp"

unittest("XML serialization: escaped characters") {
	Tag tag { "T" };
	tag.content = "&quot;\" &amp;& &lt;< &gt;> &apos;' &ampo";
	std::string expected = "<T>&quot;&quot; &amp;&amp; &lt;&lt; &gt;&gt; &apos;&apos; &amp;ampo</T>\n";
	ASSERT_EQUALS(expected, serializeXml(tag));
}
