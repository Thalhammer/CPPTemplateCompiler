#pragma once
#include <iosfwd>
#include <string>
#include <vector>
#include <memory>
#include <set>

namespace cpptemplate {
	class Variable;
	class Parameter;
	class CodeBlock;
	class Node;
	class AppendStringNode;
	class ForEachLoopNode;
	class ExpressionNode;
	class ConditionNode;
	class BlockCallNode;
	class BlockParentCallNode;
	class Block;
	class AST;
	class BaseTemplateAST;
	class ExtendingTemplateAST;
	typedef std::shared_ptr<Variable> VariablePtr;
	typedef std::shared_ptr<Parameter> ParameterPtr;
	typedef std::shared_ptr<CodeBlock> CodeBlockPtr;
	typedef std::shared_ptr<Node> NodePtr;
	typedef std::shared_ptr<AppendStringNode> AppendStringNodePtr;
	typedef std::shared_ptr<ForEachLoopNode> ForEachLoopNodePtr;
	typedef std::shared_ptr<ExpressionNode> ExpressionNodePtr;
	typedef std::shared_ptr<ConditionNode> ConditionNodePtr;
	typedef std::shared_ptr<BlockCallNode> BlockCallNodePtr;
	typedef std::shared_ptr<BlockParentCallNode> BlockParentCallNodePtr;
	typedef std::shared_ptr<Block> BlockPtr;
	typedef std::shared_ptr<AST> ASTPtr;
	typedef std::shared_ptr<BaseTemplateAST> BaseTemplateASTPtr;
	typedef std::shared_ptr<ExtendingTemplateAST> ExtendingTemplateASTPtr;

	class Variable {
		std::string name;
		std::string function_name;
		std::string type;
	public:
		Variable() {}
		Variable(std::string n, std::string fn, std::string t)
			: name(std::move(n)), function_name(std::move(fn)), type(std::move(t))
		{}

		const std::string& get_name() const { return name; }
		void set_name(std::string s) { name = std::move(s); }
		const std::string& get_function_name() const { return function_name; }
		void set_function_name(std::string s) { function_name = std::move(s); }
		const std::string& get_type() const { return type; }
		void set_type(std::string s) { type = std::move(s); }
	};
	class Parameter {
		std::string name;
		std::string type;
	public:
		Parameter() {}
		Parameter(std::string n, std::string t)
			: name(std::move(n)), type(std::move(t))
		{}

		const std::string& get_name() const { return name; }
		void set_name(std::string s) { name = std::move(s); }
		const std::string& get_type() const { return type; }
		void set_type(std::string s) { type = std::move(s); }
	};
	class CodeBlock {
		std::string name;
		std::string code;
	public:
		const std::string& get_name() const { return name; }
		void set_name(std::string s) { name = std::move(s); }
		const std::string& get_code() const { return code; }
		void set_code(std::string s) { code = std::move(s); }
	};
	enum class NodeType {
		AppendString,
		ForEachLoop,
		Expression,
		Conditional,
		BlockCall,
		BlockParentCall
	};
	class Node {
	public:
		virtual NodeType get_type() const = 0;
	};
	class AppendStringNode: public Node {
		std::string data;
	public:
		AppendStringNode() {}
		AppendStringNode(std::string str) : data(std::move(str)) {}
		NodeType get_type() const override { return NodeType::AppendString; }
		const std::string& get_data() const { return data; }
		void set_data(std::string d) { data = std::move(d); }
	};
	class ForEachLoopNode: public Node {
		std::string source;
		std::string varname;
		std::vector<NodePtr> nodes;
	public:
		NodeType get_type() const override { return NodeType::ForEachLoop; }
		const std::string& get_source() const { return source; }
		void set_source(std::string d) { source = std::move(d); }
		const std::string& get_variable_name() const { return varname; }
		void set_variable_name(std::string d) { varname = std::move(d); }
		const std::vector<NodePtr> get_nodes() const { return nodes; }
		void set_nodes(std::vector<NodePtr> n) { nodes = std::move(n); }
	};
	class ExpressionNode: public Node {
		std::string code;
	public:
		ExpressionNode() {}
		ExpressionNode(std::string c) : code(std::move(c)) {}
		NodeType get_type() const override { return NodeType::Expression; }
		const std::string& get_code() const { return code; }
		void set_code(std::string d) { code = std::move(d); }
	};
	class ConditionNode: public Node {
		std::vector<std::pair<std::string, std::vector<NodePtr>>> branches;
		std::vector<NodePtr> branch_else;
	public:
		NodeType get_type() const override { return NodeType::Conditional; }
		void set_else(std::vector<NodePtr> nodes) { branch_else = std::move(nodes); }
		void add_branch(std::string condition, std::vector<NodePtr> nodes) {
			branches.push_back({ condition, nodes });
		}
		const std::vector<std::pair<std::string, std::vector<NodePtr>>>& get_branches() const { return branches; }
		const std::vector<NodePtr>& get_else_branch() const { return branch_else; }
	};
	class BlockCallNode: public Node {
		std::string block;
	public:
		NodeType get_type() const override { return NodeType::BlockCall; }
		void set_block(std::string b) { block = b; }
		const std::string& get_block() const { return block; }
	};
	class BlockParentCallNode: public Node {
		std::string block;
	public:
		BlockParentCallNode() {}
		BlockParentCallNode(std::string s) : block(s) {}
		NodeType get_type() const override { return NodeType::BlockParentCall; }
		void set_block(std::string b) { block = b; }
		const std::string& get_block() const { return block; }
	};
	class Block {
		std::string name;
		std::vector<NodePtr> nodes;
	public:
		const std::string& get_name() const { return name; }
		void set_name(std::string n) { name = std::move(n); }
		void add_node(NodePtr node) { nodes.push_back(node); }
		const std::vector<NodePtr> get_nodes() const { return nodes; }
		void set_nodes(std::vector<NodePtr> n) { nodes = n;}
	};
	class AST {
		std::string filename;
		std::string classname;
		std::string t_namespace;
		std::vector<VariablePtr> variables;
		std::vector<ParameterPtr> parameters;
		std::vector<BlockPtr> blocks;
		std::vector<CodeBlockPtr> codeblocks;
		std::set<std::string> header_includes;
		std::set<std::string> impl_includes;
	public:
		void add_block(BlockPtr b) { blocks.push_back(b); }
		void add_parameter(ParameterPtr p) { parameters.push_back(p); }
		void add_variable(VariablePtr v) { variables.push_back(v); }
		void set_namespace(std::string ns) { t_namespace = std::move(ns); }
		const std::string& get_namespace() const { return t_namespace; }
		const std::vector<VariablePtr>& get_variables() const { return variables; }
		const std::vector<ParameterPtr>& get_parameters() const { return parameters; }
		const std::vector<BlockPtr>& get_blocks() const { return blocks; }
		const std::vector<CodeBlockPtr>& get_codeblocks() const { return codeblocks; }
		CodeBlockPtr get_codeblock(const std::string& id) const {
			for(auto& c : codeblocks)
				if(c->get_name() == id) return c;
			return nullptr;
		}
		const std::set<std::string>& get_header_includes() const { return header_includes; }
		const std::set<std::string>& get_implementation_includes() const { return impl_includes; }
		void set_variables(std::vector<VariablePtr> vars) { variables = std::move(vars); }
		void set_parameters(std::vector<ParameterPtr> params) { parameters = std::move(params); }
		void set_blocks(std::vector<BlockPtr> bls) { blocks = std::move(bls); }
		void add_header_include(std::string str) { header_includes.insert(str); }
		void add_implementation_include(std::string str) { impl_includes.insert(str); }
		void add_codeblock(CodeBlockPtr cb) { codeblocks.push_back(cb); }
		const std::string& get_filename() const { return filename; }
		void set_filename(std::string f) { filename = std::move(f); }
		const std::string& get_classname() const { return classname; }
		void set_classname(std::string f) { classname = std::move(f); }

		virtual bool is_base_ast() const = 0;
	};
	class BaseTemplateAST : public AST {
		std::vector<NodePtr> nodes;
	public:
		const std::vector<NodePtr>& get_nodes() const { return nodes; }
		void set_nodes(std::vector<NodePtr> n) { nodes = std::move(n); }
		void add_node(NodePtr n) { nodes.push_back(n); }
		virtual bool is_base_ast() const { return true; }
	};
	class ExtendingTemplateAST : public AST {
		std::string base_template;
		ASTPtr base_ast;
	public:
		void set_base_template(const std::string& bt) { base_template = bt; }
		const std::string& get_base_template() const { return base_template; }
		void set_base_template_ast(ASTPtr ast) { base_ast = ast; }
		ASTPtr get_base_template_ast() const { return base_ast; }

		virtual bool is_base_ast() const { return false; }
	};
}