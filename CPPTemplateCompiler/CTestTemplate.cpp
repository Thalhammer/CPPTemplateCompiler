#include "CTestTemplate.h"
std::string CTestTemplate::render() const
{
	std::string res;
	this->render(res);
	return res;
}

void CTestTemplate::render(std::string& str) const
{
	if(str.capacity() < str.size() + 644) {
		str.reserve(str.size() + 644);
	}
	// Line 5 Column 0
	str.append("<!DOCTYPE html>\n<html>\n    <head>\n        <title>My Webpage</title>\n    </head>\n    <body>\n        <ul id=\"navigation\">\n");
	// Line 12 Column 0
	for(auto &item:navigation)
	{
		// Line 13 Column 0
		str.append("            <li><a href=\"");
		// Line 13 Column 0
		str.append( item.href );
		// Line 13 Column 40
		str.append("\">");
		// Line 13 Column 40
		str.append( item.caption );
		// Line 13 Column 60
		str.append("</a></li>\n");
	// Line 14 Column 0
	}
	// Line 15 Column 0
	str.append("        </ul>\n        <h1>My Webpage</h1>\n        \n");
	// Line 18 Column 0
	str.append( a_variable );
	// Line 19 Column 0
	str.append("        Classname: CTestTemplate<br>\n        Compile Time: 15:14:46<br>\n        Compile Date: Aug 31 2017<br>\n        Compile Datetime: Aug 31 2017 15:14:46<br>\n        Date: ");
	// Line 23 Column 0
	str.append(__DATE__);
	// Line 23 Column 28
	str.append("<br>\n        Time: ");
	// Line 24 Column 0
	str.append(__TIME__);
	// Line 24 Column 28
	str.append("<br>\n        Datetime: ");
	// Line 25 Column 0
	str.append(std::string(__DATE__) + " " + __TIME__);
	// Line 25 Column 36
	str.append("<br>\n        Current Time: ");
	// Line 26 Column 0
	str.append(strlocaltime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()), "%X"));
	// Line 26 Column 44
	str.append("<br>\n        Current Date: ");
	// Line 27 Column 0
	str.append(strlocaltime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()), "%b %d %Y"));
	// Line 27 Column 44
	str.append("<br>\n        Current Datetime: ");
	// Line 28 Column 0
	str.append(strlocaltime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()), "%b %d %Y %X"));
	// Line 28 Column 52
	str.append("<br>\n		\n");
	// Line 29 Column 0
	str.append( std::to_string(index) );
	// Line 30 Column 0
	str.append("    </body>\n</html>\n");
}
std::string CTestTemplate::strlocaltime(time_t time, const char* fmt) {
	struct tm t;
	std::string s;
#ifdef _WIN32
	localtime_s(&t, &time);
#else
	localtime_r(&time, &t);
#endif
	s.resize(80);
	s.resize(std::strftime((char*)s.data(), s.size(), fmt, &t));
	return s;
}
