#pragma once
#include <string>
#include <vector>

struct CompileSession;
struct TemplateAction;
class TemplateCompiler {
public:
	std::pair<std::string, std::string> compile(const std::string& input, const std::string & classname) const;
	void setRemoveExpressionOnlyLines(bool b) { _remove_expression_only_lines = b; }
	bool getRemoveExpressionOnlyLines() const { return _remove_expression_only_lines; }
	void setPrintSourceLineComment(bool b) { _print_source_line_comment = b; }
	bool getPrintSourceLineComment() const { return _print_source_line_comment; }
private:
	bool _remove_expression_only_lines;
	bool _print_source_line_comment;

	static void ParseTemplate(const std::string& input, CompileSession& session);
	static std::string SanitizePlainText(const std::string& str);
	static void ReplaceMacros(TemplateAction& action, CompileSession& session);
	static std::string BuildTemplateHeader(const CompileSession& session);
	static std::string BuildTemplateBody(const CompileSession& session);

	static std::string BuildActionRender(const std::vector<TemplateAction>& actions, const CompileSession& session, const std::string& block);
};