#include "Generator.h"
#include "StringHelper.h"
#include <sstream>
#include <chrono>
#include <iomanip>

#ifdef __linux__
#define mylocaltime(x,y) localtime_r(x,y)
#else
#define mylocaltime(x,y) localtime_s(y,x)
#endif

namespace cpptemplate {
	std::string Generator::SanitizePlainText(const std::string& str)
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

	NodePtr Generator::ReplaceMacros(NodePtr inode, ASTPtr ast)
	{
		if (inode->get_type() != NodeType::Expression)
			return inode;

		auto node = std::dynamic_pointer_cast<ExpressionNode>(inode);

		auto trimmed = trim_copy(node->get_code());
		if (trimmed == "__compile_time__") {
			std::stringstream ss;
			auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			struct tm t;
			mylocaltime(&time, &t);
			ss << std::put_time(&t, "%X");
			return std::make_shared<AppendStringNode>(ss.str());
		}
		else if (trimmed == "__compile_date__") {
			std::stringstream ss;
			auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			struct tm t;
			mylocaltime(&time, &t);
			ss << std::put_time(&t, "%b %d %Y");
			return std::make_shared<AppendStringNode>(ss.str());
		}
		else if (trimmed == "__compile_datetime__") {
			std::stringstream ss;
			auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			struct tm t;
			mylocaltime(&time, &t);
			ss << std::put_time(&t, "%b %d %Y %X");
			return std::make_shared<AppendStringNode>(ss.str());
		}
		else if (trimmed == "__date__") {
			return std::make_shared<ExpressionNode>("__DATE__");
		}
		else if (trimmed == "__time__") {
			return std::make_shared<ExpressionNode>("__TIME__");
		}
		else if (trimmed == "__datetime__") {
			return std::make_shared<ExpressionNode>("std::string(__DATE__) + \" \" + __TIME__");
		}
		else if (trimmed == "__current_time__") {
			return std::make_shared<ExpressionNode>("strlocaltime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()), \"%X\")");
			//session.snippets.strlocaltime = true;
		}
		else if (trimmed == "__current_date__") {
			return std::make_shared<ExpressionNode>("strlocaltime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()), \"%b %d %Y\")");
			//session.snippets.strlocaltime = true;
		}
		else if (trimmed == "__current_datetime__") {
			return std::make_shared<ExpressionNode>("strlocaltime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()), \"%b %d %Y %X\")");
			//session.snippets.strlocaltime = true;
		}
		else if (trimmed == "__classname__") {
			return std::make_shared<AppendStringNode>(ast->get_classname());
		}
		return node;
	}

	std::string Generator::BuildActionRender(std::vector<NodePtr> nodes, ASTPtr ast, ASTPtr baseast, const std::string& cblock, size_t nindent)
	{
		std::string indent;
		for(size_t i=0; i<nindent; i++) indent+="\t";
		std::ostringstream impl;
		for(auto& onode : nodes) {
			auto node = ReplaceMacros(onode, ast);
			switch(node->get_type()) {
				case NodeType::AppendString:
					impl << indent << "str.append(\"" << SanitizePlainText(std::dynamic_pointer_cast<AppendStringNode>(node)->get_data()) << "\");" << std::endl;
					break;
				case NodeType::BlockCall:
					impl << indent << "renderBlock_" << std::dynamic_pointer_cast<BlockCallNode>(node)->get_block() << "(str, p);" << std::endl;
					break;
				case NodeType::BlockParentCall:
					impl << indent << baseast->get_classname() << "::renderBlock_" << std::dynamic_pointer_cast<BlockParentCallNode>(node)->get_block() << "(str, p);" << std::endl;
					break;
				case NodeType::Expression:
					impl << indent << "str.append(" << std::dynamic_pointer_cast<ExpressionNode>(node)->get_code() << ");" << std::endl;
					break;
				case NodeType::ForEachLoop: {
					auto l = std::dynamic_pointer_cast<ForEachLoopNode>(node);
					impl << indent << "for(auto& " << l->get_variable_name() << " : " << l->get_source() << ") {" << std::endl;
					impl << BuildActionRender(l->get_nodes(), ast, baseast, cblock, nindent + 1);
					impl << indent << "}" << std::endl;
					break;
				}
				case NodeType::Conditional: {
					auto cn = std::dynamic_pointer_cast<ConditionNode>(node);
					auto& branches = cn->get_branches();
					for(size_t i = 0; i< branches.size(); i++) {
						if(i != 0) impl << indent << "else ";
						else impl << indent;
						impl << "if (" << branches[i].first << ") {" << std::endl;
						impl << BuildActionRender(branches[i].second, ast, baseast, cblock, nindent + 1);
						impl << indent << "}";
					}
					auto belse = cn->get_else_branch();
					if(!belse.empty()) {
						impl << " else {" << std::endl;
						impl << BuildActionRender(belse, ast, baseast, cblock, nindent + 1);
						impl << indent << "}";
					}
					impl << std::endl;
					break;
				}
			}
		}
		return impl.str();
	}

	std::string Generator::GenerateImplementation(ASTPtr ast)
	{
		ASTPtr baseast;
		if(!ast->is_base_ast())
			baseast = std::dynamic_pointer_cast<ExtendingTemplateAST>(ast)->get_base_template_ast();

		const static std::string TAB = "\t";

		std::string line;
		std::ostringstream impl;

		impl << "#include \"" << ast->get_classname() << ".h\"" << std::endl;
		for(auto& s : ast->get_implementation_includes()) {
			impl << "#include " << s << std::endl;
		}
		if(ast->is_base_ast()) {
			impl << "#include <chrono>" << std::endl;
		}
		impl << "#include <typeinfo>" << std::endl;

		for(auto& ns : split(ast->get_namespace(), "::"))
		{
			impl << "namespace " << ns << " {" << std::endl;
		}

		impl << std::endl;

		impl << ast->get_classname() << "::" << ast->get_classname() << "()" << std::endl;
		impl << "{" << std::endl;
		{
			auto code = ast->get_codeblock("init");
			if(code) {
				std::istringstream iss(code->get_code());
				while(std::getline(iss, line)) impl << TAB << line << std::endl;
			}
		}
		impl << "}" << std::endl;
		impl << std::endl;
		impl << ast->get_classname() << "::~" << ast->get_classname() << "()" << std::endl;
		impl << "{" << std::endl;
		{
			auto code = ast->get_codeblock("deinit");
			if(code) {
				std::istringstream iss(code->get_code());
				while(std::getline(iss, line)) impl << TAB << line << std::endl;
			}
		}
		impl << "}" << std::endl;
		impl << std::endl;

		if(ast->is_base_ast()) {
			auto base = std::dynamic_pointer_cast<BaseTemplateAST>(ast);
			// Main render method, implemented using append render
			impl << "std::string " << ast->get_classname() << "::render(base_params& p) const" << std::endl;
			impl << "{" << std::endl;
			impl << TAB << "std::string res;" << std::endl;
			impl << TAB << "this->render(res, p);" << std::endl;
			impl << TAB << "return res;" << std::endl;
			impl << "}" << std::endl;
			impl << std::endl;
			// Render at the end of an existing string
			impl << "void " << ast->get_classname() << "::render(std::string& str, base_params& p) const" << std::endl;
			impl << "{" << std::endl;
			impl << TAB << "if(typeid(p) != get_param_type()) throw std::invalid_argument(\"invalid param struct\");" << std::endl;
			for(auto& p : ast->get_parameters()) {
				impl << TAB << "auto& " << p->get_name() << " = p." << p->get_name() << ";" << std::endl;
			}
			impl << TAB << "this->prerender(p);" << std::endl;

			impl << BuildActionRender(base->get_nodes(), ast, baseast, "", 1);

			impl << TAB << "this->postrender(p);" << std::endl;
			impl << "}" << std::endl;
			impl << std::endl;
		}

		impl << "const std::type_info& " << ast->get_classname() << "::get_param_type() const" << std::endl;
		impl << "{" << std::endl;
		impl << TAB << "return typeid(" << ast->get_classname() << "::params);" << std::endl;
		impl << "}" << std::endl;
		impl << std::endl;

		impl << "void " << ast->get_classname() << "::prerender(base_params& p) const" << std::endl;
		impl << "{" << std::endl;
		{
			auto code = ast->get_codeblock("prerender");
			if(code) {
				std::istringstream iss(code->get_code());
				while(std::getline(iss, line)) impl << TAB << line << std::endl;
			}
		}
		impl << "}" << std::endl;
		impl << std::endl;

		impl << "void " << ast->get_classname() << "::postrender(base_params& p) const" << std::endl;
		impl << "{" << std::endl;
		{
			auto code = ast->get_codeblock("postrender");
			if(code) {
				std::istringstream iss(code->get_code());
				while(std::getline(iss, line)) impl << TAB << line << std::endl;
			}
		}
		impl << "}" << std::endl;
		impl << std::endl;

		for (auto& e : ast->get_blocks()) {
			impl << "void " << ast->get_classname() << "::renderBlock_" << e->get_name() << "(std::string& str, base_params& p) const" << std::endl;
			impl << "{" << std::endl;
			if(ast->is_base_ast()) {
				for(auto& p : ast->get_parameters()) {
					impl << TAB << "auto& " << p->get_name() << " = p." << p->get_name() << ";" << std::endl;
				}
			} else {
				for(auto& p : ast->get_parameters()) {
					impl << TAB << "auto& " << p->get_name() << " = dynamic_cast<params&>(p)." << p->get_name() << ";" << std::endl;
				}
			}

			impl << BuildActionRender(e->get_nodes(), ast, baseast, e->get_name(), 1);

			impl << "}" << std::endl;
			impl << std::endl;
		}

		if(ast->is_base_ast()) {
			impl << "std::string " << ast->get_classname() << R"(::strlocaltime(time_t time, const char* fmt) {
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

		for(auto& ns : split(ast->get_namespace(), "::"))
		{
			impl << "} // namespace " << ns << std::endl;
		}

		return impl.str();
	}

	std::string Generator::GenerateHeader(ASTPtr ast) {
		ASTPtr baseast;
		if(!ast->is_base_ast())
			baseast = std::dynamic_pointer_cast<ExtendingTemplateAST>(ast)->get_base_template_ast();

		const static std::string TAB = "\t";
		std::ostringstream header;
		header << "#pragma once" << std::endl;
		if(baseast) header << "#include \"" << baseast->get_classname() << ".h\"" << std::endl;
		for (auto& incl : ast->get_header_includes()) {
			header << "#include " << incl << std::endl;
		}
		if(ast->get_header_includes().count("<string>") == 0)
			header << "#include <string>" << std::endl;
		
		for(auto& ns : split(ast->get_namespace(), "::"))
		{
			header << "namespace " << ns << " {" << std::endl;
		}
		header << "class " << ast->get_classname();
		if(!baseast) header << std::endl;
		else header << " : public " << baseast->get_classname() << std::endl;
		header << "{" << std::endl;
		header << TAB << "public:" << std::endl;
		if(baseast && ast->get_parameters().empty()) {
			header << TAB << TAB << "typedef struct " << baseast->get_classname() << "::params params;" << std::endl;
		} else{
			header << TAB << TAB << "struct params" << std::endl;
			if(baseast) header << TAB << TAB << TAB << ": " << baseast->get_classname() << "::params" << std::endl;
			header << TAB << TAB << "{" << std::endl;
			if(ast->is_base_ast()) // We need a virtual method for typeid to return the correct type
				header << TAB << TAB << TAB << "virtual ~params() {}" << std::endl;
			for (auto& param : ast->get_parameters()) {
				header << TAB << TAB << TAB << param->get_type() << " " << param->get_name() << ";" << std::endl;
			}
			header << TAB << TAB << "};" << std::endl;
		}
		if(ast->is_base_ast())
			header << TAB << TAB << "typedef struct params base_params;" << std::endl;
		header << std::endl;
		header << TAB << TAB << ast->get_classname() << "();" << std::endl;
		header << TAB << TAB << "virtual ~" << ast->get_classname() << "();" << std::endl;
		if(ast->is_base_ast()) {
			header << TAB << TAB << "std::string render(base_params& p) const;" << std::endl; // Main render method
			header << TAB << TAB << "void render(std::string& str, base_params& p) const;" << std::endl; // Render append
			header << std::endl;
		}
		for (auto& var : ast->get_variables()) {
			header << TAB << TAB << "void set" << var->get_function_name() << "(" << var->get_type() << " " << var->get_name() << ") { this->" << var->get_name() << " = " << var->get_name() << "; }" << std::endl;
			header << TAB << TAB << var->get_type() << " get" << var->get_function_name() << "() const { return this->" << var->get_name() << "; }" << std::endl;
		}
		header << TAB << "protected:" << std::endl;
		for (auto& var : ast->get_variables()) {
			header << TAB << TAB << var->get_type() << " " << var->get_name() << "; // " << var->get_function_name() << std::endl;
		}
		header << std::endl;
		// Code handlers
		header << TAB << TAB << "virtual const std::type_info& get_param_type() const;" << std::endl;
		header << TAB << TAB << "virtual void prerender(base_params& p) const;" << std::endl;
		header << TAB << TAB << "virtual void postrender(base_params& p) const;" << std::endl;

		for (auto& a : ast->get_blocks()) {
			header << TAB << TAB << "virtual void renderBlock_" << a->get_name() << "(std::string& str, base_params& p) const;" << std::endl;
		}

		header << TAB << TAB << "static std::string strlocaltime(time_t time, const char* fmt);" << std::endl;

		header << "};" << std::endl;
		
		for(auto& ns : split(ast->get_namespace(), "::"))
		{
			header << "} // namespace " << ns << std::endl;
		}

		return header.str();
	}
}