#pragma once
#include "AST.h"

namespace cpptemplate {
	class Generator {
		static std::string SanitizePlainText(const std::string& str);
		static NodePtr ReplaceMacros(NodePtr n, ASTPtr ast);
		static std::string BuildActionRender(std::vector<NodePtr> nodes, ASTPtr ast, ASTPtr baseast, const std::string& cblock = "", size_t nindent = 0);
	public:
		static std::string GenerateImplementation(ASTPtr ast);
		static std::string GenerateHeader(ASTPtr ast);
	};
}