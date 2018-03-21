#pragma once
#include "AST.h"

namespace cpptemplate {
	class Parser {
		struct Token;
		static std::vector<Token> Tokenize(std::istream& is);
		static void CompactTokens(std::vector<Token>& tokens);
		static ASTPtr BuildAST(const std::vector<Token>& tokens);
		static ASTPtr BuildBaseAST(const std::vector<Token>& tokens);
		static ASTPtr BuildExtendingAST(const std::vector<Token>& tokens);

		static NodePtr BuildNode(std::vector<Token>::const_iterator& it, std::vector<Token>::const_iterator end);
		static ForEachLoopNodePtr BuildForEachNode(std::vector<Token>::const_iterator& it, std::vector<Token>::const_iterator end);
		static ConditionNodePtr BuildConditionNode(std::vector<Token>::const_iterator& it, std::vector<Token>::const_iterator end);

		static void DumpNode(std::ostream& str, NodePtr n, size_t indent);
	public:
		static ASTPtr ParseStream(std::istream& is, const std::string& fname);
		static ASTPtr ParseFile(const std::string& fname);

		static void DumpAST(std::ostream& str, ASTPtr ast);
	};
}