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
	std::string setterName;
	std::string type;
};

struct TemplateAction {
	enum Action {
		APPENDSTRING,
		FOREACH_LOOP,
		END_LOOP,
		EXPRESSION,
		CONDITIONAL,
		CONDITIONAL_ELSE,
		CONDITIONAL_ELSEIF,
		END_CONDITIONAL,
		BEGIN_BLOCK,
		END_BLOCK,
		BLOCK_PARENT
	};
	Action action;
	std::string arg1;
	std::string arg2;
	int expr_size_hint;
	size_t source_line;
	size_t source_col;
};

struct TemplateBlock {
	std::string name;
	std::vector<TemplateAction> actions;
};

struct CodeSnippets {
	bool strlocaltime;

	bool any() const {
		return strlocaltime;
	}
};

struct CompileSession {
	std::string classname;
	std::string classname_extends;
	std::string init_code;
	std::string deinit_code;
	std::set<std::string> cpp_includes;
	std::set<std::string> cpp_impl_includes;
	std::string class_namespace;
	std::vector<TemplateVariable> variables;
	std::vector<TemplateVariable> params;
	std::vector<TemplateAction> actions;
	std::vector<TemplateBlock> blocks;
	CodeSnippets snippets;
	bool remove_expression_only_lines;
	bool print_source_line_comments;
};

std::pair<std::string, std::string> TemplateCompiler::compile(const std::string & input, const std::string & classname) const
{
	CompileSession session{ classname };
	session.remove_expression_only_lines = _remove_expression_only_lines;
	session.print_source_line_comments = _print_source_line_comment;

	ParseTemplate(input, session);

	for(auto& a : session.actions)
		if(a.action == TemplateAction::APPENDSTRING)
			a.arg1 = SanitizePlainText(a.arg1);
	for(auto& b : session.blocks)
		for (auto& a : b.actions)
			if (a.action == TemplateAction::APPENDSTRING)
				a.arg1 = SanitizePlainText(a.arg1);

	if (session.snippets.strlocaltime) {
		session.cpp_impl_includes.insert("<chrono>");
		session.cpp_impl_includes.insert("<sstream>");
		session.cpp_impl_includes.insert("<iomanip>");
	}
	session.cpp_includes.insert("<string>");
	session.cpp_includes.insert("<map>");

	auto header = BuildTemplateHeader(session);
	auto impl = BuildTemplateBody(session);

	return{ header, impl };
}

void TemplateCompiler::ParseTemplate(const std::string & input, CompileSession & session)
{
	auto& actions = session.actions;
	auto& variables = session.variables;
	auto& cpp_includes = session.cpp_includes;
	auto& cpp_impl_includes = session.cpp_impl_includes;
	std::string sline;
	std::istringstream stream(input);
	size_t cnt_line = 0;
	size_t in_comment_section = 0;
	size_t in_init_section = 0;
	size_t in_deinit_section = 0;
	while (std::getline(stream, sline)) {
		size_t offset = 0;
		while (offset < sline.size()) {
			if (in_comment_section == 0 && in_init_section == 0 && in_deinit_section == 0) {
				auto pos = std::min(sline.find("{%", offset), std::min(sline.find("{{", offset), sline.find("{#", offset)));
				if (pos == std::string::npos) {
					actions.push_back({ TemplateAction::APPENDSTRING, sline.substr(offset) + "\n", "", -1, cnt_line, offset });
					break;
				}
				if (sline.substr(pos, 2) == "{#") {
					in_comment_section++;
					offset = pos + 2;
					continue;
				}

				bool is_cmd = sline.substr(pos, 2) == "{%";
				auto endpos = sline.find(is_cmd ? "%}" : "}}", pos);
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
						variables.push_back({ parts[1], parts[2], join(" ", parts, 3) });
					}
					else if (parts[0] == "param") {
						session.params.push_back({ parts[1], "", join(" ", parts, 2) });
					}
					else if (parts[0] == "extends") {
						session.classname_extends = parts[1];
					}
					else if (parts[0] == "namespace") {
						session.class_namespace = parts[1];
					}
					else if (parts[0] == "#include") {
						cpp_includes.insert(parts[1]);
					}
					else if (parts[0] == "#include_impl") {
						cpp_impl_includes.insert(parts[1]);
					}
					else if (parts[0] == "for") {
						actions.push_back({ TemplateAction::FOREACH_LOOP, parts[1], parts[3], -1, cnt_line, offset });
					}
					else if (parts[0] == "endfor") {
						actions.push_back({ TemplateAction::END_LOOP, "", "", -1, cnt_line, offset });
					}
					else if (parts[0] == "if") {
						actions.push_back({ TemplateAction::CONDITIONAL, join(" ", parts, 1), "", -1, cnt_line, offset });
					}
					else if (parts[0] == "elif") {
						actions.push_back({ TemplateAction::CONDITIONAL_ELSEIF, join(" ", parts, 1), "", -1, cnt_line, offset });
					}
					else if (parts[0] == "else") {
						actions.push_back({ TemplateAction::CONDITIONAL_ELSE, "", "", -1, cnt_line, offset });
					}
					else if (parts[0] == "endif") {
						actions.push_back({ TemplateAction::END_CONDITIONAL, "", "", -1, cnt_line, offset });
					}
					else if (parts[0] == "block") {
						actions.push_back({ TemplateAction::BEGIN_BLOCK, parts[1], "", -1, cnt_line, offset });
					}
					else if (parts[0] == "endblock") {
						actions.push_back({ TemplateAction::END_BLOCK, "", "", -1, cnt_line, offset });
					}
					else if (parts[0] == "parent()") {
						actions.push_back({ TemplateAction::BLOCK_PARENT, "", "", -1, cnt_line, offset });
					}
					else if (parts[0] == "init") {
						in_init_section++;
					}
					else if (parts[0] == "deinit") {
						in_deinit_section++;
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
			else {
				if(in_comment_section != 0) {
					auto pos = sline.find("#}", offset);
					if (pos != std::string::npos) {
						offset = pos + 2;
						in_comment_section--;
						continue;
					}
					offset = pos;
				}
				if(in_init_section != 0 || in_deinit_section != 0) {
					auto pos = sline.find("{%", offset);
					bool is_end = false;
					if (pos != std::string::npos) {
						auto moffset = pos + 2;
						auto endpos = sline.find("%}", moffset);
						if(endpos != std::string::npos)
						{
							auto parts = split(sline.substr(moffset, endpos - moffset), " ");
							if(parts.size() == 1 && parts[0] == (in_init_section != 0 ? "endinit" : "enddeinit")) {
								is_end = true;
							}
							offset = endpos + 2;
						} else {
							offset = moffset;
						}
					}
					if(!is_end) {
						if(in_init_section != 0)
							session.init_code += sline.substr(offset) + "\n";
						else session.deinit_code += sline.substr(offset) + "\n";
						offset = sline.size();
					} else {
						if(in_init_section != 0)
							in_init_section--;
						else in_deinit_section--;
					}
				}
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

	std::vector<size_t> blockstack;
	for (size_t i = 0; i < actions.size();) {
		auto& a = actions[i];
		if (a.action == TemplateAction::BEGIN_BLOCK) {
			blockstack.push_back(session.blocks.size());
			session.blocks.push_back({ a.arg1 });
			i++;
		}
		else if (a.action == TemplateAction::BLOCK_PARENT) {
			if (blockstack.empty()) throw std::runtime_error("Invalid parent call");
			a.arg1 = session.blocks[*blockstack.rbegin()].name;
			session.blocks[*blockstack.rbegin()].actions.push_back(a);
			actions.erase(actions.begin() + i);
		}
		else if (a.action == TemplateAction::END_BLOCK) {
			if (blockstack.empty()) throw std::runtime_error("Invalid parent call");
			blockstack.pop_back();
			actions.erase(actions.begin() + i);
		}
		else if(!blockstack.empty()) {
			session.blocks[*blockstack.rbegin()].actions.push_back(a);
			actions.erase(actions.begin() + i);
		}
		else {
			i++;
		}
	}

	if(!session.classname_extends.empty()) {
		for(auto& a : actions)
			if(a.action != TemplateAction::BEGIN_BLOCK)
				throw std::runtime_error("Derived templates should not have actions outside of blocks");
	}

	CheckActions(actions);
	for(auto& e : session.blocks) {
		CheckActions(e.actions);
	}
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
		expr.arg1 = "__TIME__";
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

std::string TemplateCompiler::BuildTemplateHeader(const CompileSession & session)
{
	std::stringstream header;
	header << "#pragma once" << std::endl;
	for (auto& incl : session.cpp_includes) {
		header << "#include " << incl << std::endl;
	}
	header << "#if __cpp_lib_any" << std::endl;
	header << "#include <any>" << std::endl;
	header << "namespace tmpl {" << std::endl;
	header << "using std::any;" << std::endl;
	header << "using std::any_cast;" << std::endl;
	header << "}" << std::endl;
	header << "#else" << std::endl;
	header << "#include \"any.h\"" << std::endl;
	header << "namespace tmpl {" << std::endl;
	header << "using linb::any;" << std::endl;
	header << "using linb::any_cast;" << std::endl;
	header << "}" << std::endl;
	header << "#endif" << std::endl;
	for(auto& ns : split(session.class_namespace, "::"))
	{
		header << "namespace " << ns << " {" << std::endl;
	}
	header << "class " << session.classname;
	if(session.classname_extends.empty()) header << std::endl;
	else header << " : public " << session.classname_extends << std::endl;
	header << "{" << std::endl;
	header << TAB << "public:" << std::endl;
	header << TAB << TAB << session.classname << "();" << std::endl;
	header << TAB << TAB << "virtual ~" << session.classname << "();" << std::endl;
	if(session.classname_extends.empty()) {
		header << TAB << TAB << "std::string render(std::map<std::string, tmpl::any>& params) const;" << std::endl; // Main render method
		header << TAB << TAB << "void render(std::string& str, std::map<std::string, tmpl::any>& params) const;" << std::endl; // Render append
	}
	for (auto& var : session.variables) {
		header << TAB << TAB << "void set" << var.setterName << "(" << var.type << " " << var.name << ") { this->" << var.name << " = " << var.name << "; }" << std::endl;
		header << TAB << TAB << var.type << " get" << var.setterName << "() const { return this->" << var.name << "; }" << std::endl;
	}
	header << TAB << "protected:" << std::endl;
	for (auto& var : session.variables) {
		header << TAB << TAB << var.type << " " << var.name << "; // " << var.setterName << std::endl;
	}
	for (auto& a : session.actions) {
		if (a.action == TemplateAction::BEGIN_BLOCK)
			header << TAB << TAB << "virtual void renderBlock_" << a.arg1 << "(std::string& str, std::map<std::string, tmpl::any>& params) const;" << std::endl;
	}

	if(session.snippets.any()) {
		header << TAB << "private:" << std::endl;
		if (session.snippets.strlocaltime) {
			header << TAB << TAB << "static std::string strlocaltime(time_t time, const char* fmt);" << std::endl;
		}
	}

	header << "};" << std::endl;
	
	for(auto& ns : split(session.class_namespace, "::"))
	{
		header << "} // namespace " << ns << std::endl;
	}

	return header.str();
}

std::string TemplateCompiler::BuildTemplateBody(const CompileSession & session)
{
	std::string line;
	std::stringstream impl;

	impl << "#include \"" << session.classname << ".h\"" << std::endl;
	for(auto& s : session.cpp_impl_includes) {
		impl << "#include " << s << std::endl;
	}

	for(auto& ns : split(session.class_namespace, "::"))
	{
		impl << "namespace " << ns << " {" << std::endl;
	}

	impl << std::endl;

	impl << session.classname << "::" << session.classname << "()" << std::endl;
	impl << "{" << std::endl;
	std::istringstream iss(session.init_code);
	while(std::getline(iss, line)) impl << TAB << line << std::endl;
	impl << "}" << std::endl;
	impl << std::endl;
	impl << session.classname << "::~" << session.classname << "()" << std::endl;
	impl << "{" << std::endl;
	iss = std::istringstream(session.deinit_code);
	while(std::getline(iss, line)) impl << TAB << line << std::endl;
	impl << "}" << std::endl;
	impl << std::endl;

	if(session.classname_extends.empty()) {
		// Main render method, implemented using append render
		impl << "std::string " << session.classname << "::render(std::map<std::string, tmpl::any>& params) const" << std::endl;
		impl << "{" << std::endl;
		impl << TAB << "std::string res;" << std::endl;
		impl << TAB << "this->render(res, params);" << std::endl;
		impl << TAB << "return res;" << std::endl;
		impl << "}" << std::endl;
		impl << std::endl;
		// Render at the end of an existing string
		impl << "void " << session.classname << "::render(std::string& str, std::map<std::string, tmpl::any>& params) const" << std::endl;
		impl << "{" << std::endl;

		impl << BuildActionRender(session.actions, session, "");

		impl << "}" << std::endl;
	}

	if (session.snippets.strlocaltime) {
		impl << "std::string " << session.classname << R"(::strlocaltime(time_t time, const char* fmt) {
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
})" << std::endl;
	}

	for (auto& e : session.blocks) {
		impl << "void " << session.classname << "::renderBlock_" << e.name << "(std::string& str, std::map<std::string, tmpl::any>& params) const" << std::endl;
		impl << "{" << std::endl;

		impl << BuildActionRender(e.actions, session, e.name);

		impl << "}" << std::endl;
	}

	for(auto& ns : split(session.class_namespace, "::"))
	{
		impl << "} // namespace " << ns << std::endl;
	}

	return impl.str();
}

void TemplateCompiler::CheckActions(const std::vector<TemplateAction>& actions)
{
	// Check for consistency
	size_t opened_blocks = 0;
	size_t closed_blocks = 0;
	for (auto& e : actions) {
		switch (e.action)
		{
		case TemplateAction::END_CONDITIONAL:
		case TemplateAction::END_LOOP: closed_blocks++; break;
		case TemplateAction::CONDITIONAL:
		case TemplateAction::FOREACH_LOOP: opened_blocks++; break;
		default: break;
		}
	}

	if (opened_blocks != closed_blocks)
		throw std::runtime_error("Invalid template: Opened " + std::to_string(opened_blocks) + " blocks, closed " + std::to_string(closed_blocks));
}

std::string TemplateCompiler::BuildActionRender(const std::vector<TemplateAction>& actions, const CompileSession& session, const std::string& block)
{
	std::ostringstream impl;

	for(auto& var : session.params) {
		impl << TAB << var.type << "& " << var.name << " = tmpl::any_cast<" << var.type << "&>(params.at(\"" << var.name << "\"));" << std::endl;
	}

	size_t indent_level = 0;
	for (auto& action : actions) {
		std::string indent;
		for (size_t i = 0; i < indent_level; i++)
			indent += TAB;

		if (session.print_source_line_comments)
			impl << indent << TAB << "// Line " << action.source_line << " Column " << action.source_col << std::endl;

		if (action.action == TemplateAction::APPENDSTRING) {
			impl << indent << TAB << "str.append(\"" << action.arg1 << "\");" << std::endl;
		}
		else if (action.action == TemplateAction::FOREACH_LOOP) {
			impl << indent << TAB << "for(auto &" << action.arg1 << ":" << action.arg2 << ")" << std::endl;
			impl << indent << TAB << "{" << std::endl;
			indent_level++;
		}
		else if (action.action == TemplateAction::END_LOOP || action.action == TemplateAction::END_CONDITIONAL) {
			impl << indent << "}" << std::endl;
			indent_level--;
		}
		else if (action.action == TemplateAction::EXPRESSION) {
			impl << indent << TAB << "str.append(" << action.arg1 << ");" << std::endl;
		}
		else if (action.action == TemplateAction::CONDITIONAL) {
			impl << indent << TAB << "if( " << action.arg1 << ")" << std::endl;
			impl << indent << TAB << "{" << std::endl;
			indent_level++;
		}
		else if (action.action == TemplateAction::CONDITIONAL_ELSEIF) {
			impl << indent << "} else if( " << action.arg1 << ")" << std::endl;
			impl << indent << "{" << std::endl;
		}
		else if (action.action == TemplateAction::CONDITIONAL_ELSE) {
			impl << indent << "} else {" << std::endl;
		}
		else if (action.action == TemplateAction::BEGIN_BLOCK) {
			impl << indent << TAB << "this->renderBlock_" << action.arg1 << "(str, params);" << std::endl;
		}
		else if (action.action == TemplateAction::BLOCK_PARENT) {
			if (!session.classname_extends.empty()) {
				impl << indent << TAB << session.classname_extends << "::renderBlock_" << block << "(str, params);" << std::endl;
			}
			else {
				throw std::runtime_error("There is no parent template");
			}
		}
	}
	return impl.str();
}
