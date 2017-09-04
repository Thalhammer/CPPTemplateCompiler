#include "TemplateCompiler.h"
#include "StringHelper.h"
#include <sstream>
#include <vector>
#include <set>
#include <chrono>
#include <iomanip>
#include <map>

#ifdef __linux__
#define mylocaltime(x,y) localtime_r(x,y)
#else
#define mylocaltime(x,y) localtime_s(y,x)
#endif

const std::string TAB = "\t";

struct TemplateVariable {
	std::string name;
	std::string type;
	std::string setterName;
};

struct TemplateAction {
	enum Action {
		APPENDSTRING,
		FOREACH_LOOP,
		END_BLOCK,
		EXPRESSION
	};
	Action action;
	std::string arg1;
	std::string arg2;
	int expr_size_hint;
	size_t source_line;
	size_t source_col;
};

struct CodeSnippets {
	bool strlocaltime;
};

struct CompileSession {
	std::string classname;
	std::set<std::string> cpp_includes;
	std::vector<TemplateVariable> variables;
	std::vector<TemplateAction> actions;
	CodeSnippets snippets;
	bool remove_expression_only_lines;
};

std::pair<std::string, std::string> TemplateCompiler::compile(const std::string & input, const std::string & classname)
{
	CompileSession session{ classname };
	session.remove_expression_only_lines = _remove_expression_only_lines;

	ParseTemplate(input, session);
	
	if (session.snippets.strlocaltime) {
		session.cpp_includes.insert("<chrono>");
		session.cpp_includes.insert("<sstream>");
		session.cpp_includes.insert("<iomanip>");
	}

	std::stringstream header;
	for (auto& incl : session.cpp_includes) {
		header << "#include " << incl << std::endl;
	}
	header << "class " << classname << std::endl;
	header << "{" << std::endl;
	header << TAB << "public:" << std::endl;
	header << TAB << TAB << "std::string render() const;" << std::endl; // Main render method
	header << TAB << TAB << "void render(std::string& str) const;" << std::endl; // Render append
	for (auto& var : session.variables) {
		header << TAB << TAB << "void set" << var.setterName << "(" << var.type << " " << var.name << ") { this->" << var.name << " = " << var.name << "; }" << std::endl;
		header << TAB << TAB << var.type << " get" << var.setterName << "() const { return this->" << var.name << "; }" << std::endl;
	}
	header << TAB << "private:" << std::endl;
	for (auto& var : session.variables) {
		header << TAB << TAB << var.type << " " << var.name << "; // " << var.setterName << std::endl;
	}
	if (session.snippets.strlocaltime) {
		header << TAB << TAB << "static std::string strlocaltime(time_t time, const char* fmt);" << std::endl;
	}
	header << "};";

	std::stringstream impl;

	impl << "#include \"" << classname << ".h\"" << std::endl;

	// Main render method, implemented using append render
	impl << "std::string " << classname << "::render() const" << std::endl;
	impl << "{" << std::endl;
	impl << TAB << "std::string res;" << std::endl;
	impl << TAB << "this->render(res);" << std::endl;
	impl << TAB << "return res;" << std::endl;
	impl << "}" << std::endl;
	impl << std::endl;
	// Render at the end of an existing string
	impl << "void " << classname << "::render(std::string& str) const" << std::endl;
	impl << "{" << std::endl;

	size_t total_string_size = 0;
	for (auto& action : session.actions)
	{
		if (action.action == TemplateAction::APPENDSTRING)
		{
			action.arg1 = SanitizePlainText(action.arg1);
			total_string_size += action.arg1.size();
		}
		else if (action.action == TemplateAction::EXPRESSION && action.expr_size_hint != -1)
		{
			total_string_size += action.expr_size_hint;
		}
	}
	if (total_string_size != 0) {
		impl << TAB << "if(str.capacity() < str.size() + " << total_string_size << ") {" << std::endl;
		impl << TAB << TAB << "str.reserve(str.size() + " << total_string_size << ");" << std::endl;
		impl << TAB << "}" << std::endl;
	}

	size_t indent_level = 0;

	for (auto& action : session.actions) {
		std::string indent;
		for (size_t i = 0; i < indent_level; i++)
			indent += TAB;
		if (action.action == TemplateAction::APPENDSTRING) {
			impl << indent << TAB << "// Line " << action.source_line << " Column " << action.source_col << std::endl;
			impl << indent << TAB << "str.append(\"" << action.arg1 << "\");" << std::endl;
		}
		else if (action.action == TemplateAction::FOREACH_LOOP) {
			impl << indent << TAB << "// Line " << action.source_line << " Column " << action.source_col << std::endl;
			impl << indent << TAB << "for(auto &" << action.arg1 << ":" << action.arg2 << ")" << std::endl;
			impl << indent << TAB << "{" << std::endl;
			indent_level++;
		}
		else if (action.action == TemplateAction::END_BLOCK) {
			impl << indent << "// Line " << action.source_line << " Column " << action.source_col << std::endl;
			impl << indent << "}" << std::endl;
			indent_level--;
		}
		else if (action.action == TemplateAction::EXPRESSION) {
			impl << indent << TAB << "// Line " << action.source_line << " Column " << action.source_col << std::endl;
			impl << indent << TAB << "str.append(" << action.arg1 << ");" << std::endl;
		}
	}

	impl << "}" << std::endl;

	if (session.snippets.strlocaltime) {
		impl << "std::string " << classname << "::strlocaltime(time_t time, const char* fmt) {" << std::endl;
		impl << TAB << "struct tm t;" << std::endl;
		impl << TAB << "std::string s;" << std::endl;
		impl << "#ifdef _WIN32" << std::endl;
		impl << TAB << "localtime_s(&t, &time);" << std::endl;
		impl << "#else" << std::endl;
		impl << TAB << "localtime_r(&time, &t);" << std::endl;
		impl << "#endif" << std::endl;
		impl << TAB << "s.resize(80);" << std::endl;
		impl << TAB << "s.resize(std::strftime((char*)s.data(), s.size(), fmt, &t));" << std::endl;
		impl << TAB << "return s;" << std::endl;
		impl << "}" << std::endl;
	}

	return{ header.str(), impl.str() };
}

void TemplateCompiler::ParseTemplate(const std::string & input, CompileSession & session)
{
	auto& actions = session.actions;
	auto& variables = session.variables;
	auto& cpp_includes = session.cpp_includes;
	std::string sline;
	std::istringstream stream(input);
	size_t cnt_line = 0;
	while (std::getline(stream, sline)) {
		size_t offset = 0;
		while(offset < sline.size()) {
			auto pos = std::min(sline.find("{%", offset), sline.find("{{", offset));
			if (pos == std::string::npos) {
				actions.push_back({ TemplateAction::APPENDSTRING, sline.substr(offset) + "\n", "", -1, cnt_line, offset });
				break;
			}
			bool is_cmd = sline.substr(pos, 2) == "{%";
			auto endpos = sline.find(is_cmd?"%}":"}}", pos);
			if (endpos == std::string::npos)
				throw std::runtime_error("Missing end of start tag at pos " + std::to_string(pos));
			if (pos != offset) {
				if (trim_copy(sline.substr(endpos + 2)).empty()) {
					if (!(session.remove_expression_only_lines && is_cmd && trim_copy(sline.substr(endpos + 2)).empty())) {
						std::string plain = sline.substr(offset, pos - offset) + "\n";
						actions.push_back({ TemplateAction::APPENDSTRING, plain, "", -1, cnt_line, offset });
					}
				}
				else {
					std::string plain = sline.substr(offset, pos - offset);
					actions.push_back({ TemplateAction::APPENDSTRING, plain, "", -1, cnt_line, offset });
				}
			}
			if (is_cmd) {
				std::string command = sline.substr(pos + 2, endpos - pos - 2);
				auto parts = split(command, " ");
				if (parts[0] == "variable") {
					// Variable definition
					variables.push_back({ parts[1], parts[2], parts[3] });
				}
				else if (parts[0] == "#include") {
					cpp_includes.insert(parts[1]);
				}
				else if (parts[0] == "for") {
					actions.push_back({ TemplateAction::FOREACH_LOOP, parts[1], parts[3], -1, cnt_line, offset });
				}
				else if (parts[0] == "endfor") {
					actions.push_back({ TemplateAction::END_BLOCK, "", "", -1, cnt_line, offset });
				}
				offset = endpos + 2;
			}
			else {
				// Variable output
				auto expr = TemplateAction{ TemplateAction::EXPRESSION, sline.substr(pos + 2, endpos - pos - 2), "", -1, cnt_line, offset };
				// Try to eval at compile time
				ReplaceMacros(expr, session);
				actions.push_back(expr);
				offset = endpos + 2;
			}
		}
		cnt_line++;
	}
	// Merge APPENDSTRING
	for (size_t i = 1; i < actions.size();)
	{
		if (actions[i].action == TemplateAction::APPENDSTRING && actions[i - 1].action == TemplateAction::APPENDSTRING) {
			actions[i - 1].arg1 += actions[i].arg1;
			actions.erase(actions.begin() + i);
		}
		else i++;
	}

	// Check for consistency
	size_t opened_blocks = 0;
	size_t closed_blocks = 0;
	for (auto& e : actions) {
		switch (e.action)
		{
		case TemplateAction::END_BLOCK: closed_blocks++; break;
		case TemplateAction::FOREACH_LOOP: opened_blocks++; break;
		default: break;
		}
	}
	if (opened_blocks != closed_blocks)
		throw std::runtime_error("Invalid template: Opened " + std::to_string(opened_blocks) + " blocks, closed " + std::to_string(closed_blocks));
}

std::string TemplateCompiler::SanitizePlainText(const std::string& str)
{
	std::string res;
	for (auto& c : str)
	{
		switch (c) {
		case '\'': res += "\\'"; break;
		case '"': res += "\\\""; break;
		case '\\': res += "\\\\"; break;
		case '\n': res += "\\n"; break;
		case '\r': res += "\\r"; break;
		default: res += c;
		}
	}
	return res;
}

void TemplateCompiler::ReplaceMacros(TemplateAction& expr, CompileSession& session)
{
	if (expr.action != TemplateAction::EXPRESSION)
		return;

	auto trimmed = trim_copy(expr.arg1);
	if (trimmed == "__compile_time__") {
		std::stringstream ss;
		auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		struct tm t;
		mylocaltime(&time, &t);
		ss << std::put_time(&t, "%X");
		expr.action = TemplateAction::APPENDSTRING;
		expr.arg1 = ss.str();
	}
	else if (trimmed == "__compile_date__") {
		std::stringstream ss;
		auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		struct tm t;
		mylocaltime(&time, &t);
		ss << std::put_time(&t, "%b %d %Y");
		expr.action = TemplateAction::APPENDSTRING;
		expr.arg1 = ss.str();
	}
	else if (trimmed == "__compile_datetime__") {
		std::stringstream ss;
		auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		struct tm t;
		mylocaltime(&time, &t);
		ss << std::put_time(&t, "%b %d %Y %X");
		expr.action = TemplateAction::APPENDSTRING;
		expr.arg1 = ss.str();
	}
	else if (trimmed == "__date__") {
		expr.arg1 = "__DATE__";
		expr.expr_size_hint = 11;
	}
	else if (trimmed == "__time__") {
		expr.arg1 = "__TIME__" ;
		expr.expr_size_hint = 8;
	}
	else if (trimmed == "__datetime__") {
		expr.arg1 = "std::string(__DATE__) + \" \" + __TIME__";
		expr.expr_size_hint = 20;
	}
	else if (trimmed == "__current_time__") {
		expr.arg1 = "strlocaltime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()), \"%X\")";
		expr.expr_size_hint = 8;
		session.snippets.strlocaltime = true;
	}
	else if (trimmed == "__current_date__") {
		expr.arg1 = "strlocaltime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()), \"%b %d %Y\")";
		expr.expr_size_hint = 11;
		session.snippets.strlocaltime = true;
	}
	else if (trimmed == "__current_datetime__") {
		expr.arg1 = "strlocaltime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()), \"%b %d %Y %X\")";
		expr.expr_size_hint = 20;
		session.snippets.strlocaltime = true;
	}
	else if (trimmed == "__classname__") {
		expr.arg1 = session.classname;
		expr.action = TemplateAction::APPENDSTRING;
	}
}
