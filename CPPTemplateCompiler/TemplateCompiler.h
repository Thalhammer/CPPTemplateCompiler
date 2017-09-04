#pragma once
#include <string>

struct CompileSession;
struct TemplateAction;
class TemplateCompiler {
public:
	std::pair<std::string, std::string> compile(const std::string& input, const std::string & classname);
	void setRemoveExpressionOnlyLines(bool b) { _remove_expression_only_lines = b; }
	bool getRemoveExpressionOnlyLines() const { return _remove_expression_only_lines; }
private:
	bool _remove_expression_only_lines;

	static void ParseTemplate(const std::string& input, CompileSession& session);
	static std::string SanitizePlainText(const std::string& str);
	static void ReplaceMacros(TemplateAction& action, CompileSession& session);
};