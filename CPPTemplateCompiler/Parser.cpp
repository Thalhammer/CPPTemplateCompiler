#include "Parser.h"
#include "StringHelper.h"
#include <fstream>

namespace cpptemplate {
	struct Parser::Token {
		enum Type {
			APPENDSTRING = 0,
			FOREACH_LOOP,
			END_LOOP,
			EXPRESSION,
			CONDITIONAL,
			CONDITIONAL_ELSE,
			CONDITIONAL_ELSEIF,
			END_CONDITIONAL,
			BEGIN_BLOCK,
			END_BLOCK,
			BLOCK_PARENT,
			VARIABLE,
			PARAMETER,
			EXTENDS,
			NAMESPACE,
			INCLUDE_CPP_HEADER,
			INCLUDE_CPP_IMPL,
			COMMENT,
			CODE
		};
		Type type;
		std::vector<std::string> args;
		size_t source_line;
		size_t source_col;
	};

	std::vector<Parser::Token> Parser::Tokenize(std::istream& stream) {
		bool remove_expression_only_lines = true;

		std::string sline;
		size_t cnt_line = 0;
		bool in_comment_section = false;
		bool in_code_section = false;
		std::string cblock;
		std::vector<Token> tokens;
		while (std::getline(stream, sline)) {
			size_t offset = 0;
			while (offset < sline.size()) {
				if (!in_comment_section && !in_code_section) {
					auto pos = std::min(sline.find("{%", offset), std::min(sline.find("{{", offset), sline.find("{#", offset)));
					if (pos == std::string::npos) {
						tokens.push_back({ Token::APPENDSTRING, { sline.substr(offset) + "\n" }, cnt_line, offset });
						break;
					}
					if (sline.substr(pos, 2) == "{#") {
						in_comment_section = true;
						tokens.push_back({ Token::COMMENT, { "" }, cnt_line, offset });
						offset = pos + 2;
						continue;
					}

					bool is_cmd = sline.substr(pos, 2) == "{%";
					auto endpos = sline.find(is_cmd ? "%}" : "}}", pos);
					if (endpos == std::string::npos)
						throw std::runtime_error("Missing end of start tag at pos " + std::to_string(pos));
					if (pos != offset) {
						if (trim_copy(sline.substr(endpos + 2)).empty()) {
							if (!(remove_expression_only_lines && is_cmd && trim_copy(sline.substr(endpos + 2)).empty())) {
								std::string plain = sline.substr(offset, pos - offset);
								tokens.push_back({ Token::APPENDSTRING, { plain }, cnt_line, offset });
							}
						}
						else {
							std::string plain = sline.substr(offset, pos - offset);
							tokens.push_back({ Token::APPENDSTRING, { plain + "\n" }, cnt_line, offset });
						}
					}
					if (is_cmd) {
						std::string command = sline.substr(pos + 2, endpos - pos - 2);
						auto parts = split(command, " ");
						if (parts[0] == "variable") {
							tokens.push_back({ Token::VARIABLE, { parts[1], parts[2], join(" ", parts, 3) }, cnt_line, offset });
						}
						else if (parts[0] == "param") {
							tokens.push_back({ Token::PARAMETER, { parts[1], join(" ", parts, 2) }, cnt_line, offset });
						}
						else if (parts[0] == "extends") {
							tokens.push_back({ Token::EXTENDS, { parts[1] }, cnt_line, offset });
						}
						else if (parts[0] == "namespace") {
							tokens.push_back({ Token::NAMESPACE, { parts[1] }, cnt_line, offset });
						}
						else if (parts[0] == "#include") {
							tokens.push_back({ Token::INCLUDE_CPP_HEADER, { join(" ", parts, 1) }, cnt_line, offset });
						}
						else if (parts[0] == "#include_impl") {
							tokens.push_back({ Token::INCLUDE_CPP_IMPL, { join(" ", parts, 1) }, cnt_line, offset });
						}
						else if (parts[0] == "for") {
							tokens.push_back({ Token::FOREACH_LOOP, { parts[1], parts[3] }, cnt_line, offset });
						}
						else if (parts[0] == "endfor") {
							tokens.push_back({ Token::END_LOOP, {}, cnt_line, offset });
						}
						else if (parts[0] == "if") {
							tokens.push_back({ Token::CONDITIONAL, { join(" ", parts, 1) }, cnt_line, offset });
						}
						else if (parts[0] == "elif") {
							tokens.push_back({ Token::CONDITIONAL_ELSEIF, { join(" ", parts, 1) }, cnt_line, offset });
						}
						else if (parts[0] == "else") {
							tokens.push_back({ Token::CONDITIONAL_ELSE, {}, cnt_line, offset });
						}
						else if (parts[0] == "endif") {
							tokens.push_back({ Token::END_CONDITIONAL, {}, cnt_line, offset });
						}
						else if (parts[0] == "block") {
							cblock = parts[1];
							tokens.push_back({ Token::BEGIN_BLOCK, { parts[1] }, cnt_line, offset });
						}
						else if (parts[0] == "endblock") {
							cblock = "";
							tokens.push_back({ Token::END_BLOCK, {}, cnt_line, offset });
						}
						else if (parts[0] == "parent()" && !cblock.empty()) {
							tokens.push_back({ Token::BLOCK_PARENT, { cblock }, cnt_line, offset });
						}
						else if (parts[0] == "init" || parts[0] == "deinit" || parts[0] == "prerender" || parts[0] == "postrender") {
							in_code_section = true;
							tokens.push_back({ Token::CODE, { parts[0], "" }, cnt_line, offset });
						}
						else throw std::runtime_error("unknown token " + parts[0] + " at " + std::to_string(cnt_line+1) + ":" + std::to_string(offset));
						offset = endpos + 2;
					}
					else {
						tokens.push_back({ Token::EXPRESSION, { sline.substr(pos + 2, endpos - pos - 2) }, cnt_line, offset });
						offset = endpos + 2;
					}
				}
				else {
					if(in_comment_section) {
						if(tokens.back().type != Token::COMMENT)
							throw std::logic_error("Type of last token should have been comment");
						auto pos = sline.find("#}", offset);
						if (pos != std::string::npos) {
							tokens.back().args[0] += sline.substr(offset, pos - offset);
							offset = pos + 2;
							in_comment_section = false;
							continue;
						}
						tokens.back().args[0] += sline.substr(offset) + "\n";
						offset = pos;
					}
					if(in_code_section) {
						if(tokens.back().type != Token::CODE)
							throw std::logic_error("Type of last token should have been code");
						auto pos = sline.find("{%", offset);
						bool is_end = false;
						if (pos != std::string::npos) {
							auto moffset = pos + 2;
							auto endpos = sline.find("%}", moffset);
							if(endpos != std::string::npos)
							{
								auto parts = split(sline.substr(moffset, endpos - moffset), " ");
								if(parts.size() == 1 && (parts[0] == "end" + tokens.back().args[0] || parts[0] == "endcode")) {
									is_end = true;
								}
								offset = endpos + 2;
							} else {
								offset = moffset;
							}
						}
						if(!is_end) {
							tokens.back().args[1] += sline.substr(offset) + "\n";
							offset = sline.size();
						} else {
							tokens.back().args[1] += sline.substr(offset, pos - offset);
							in_code_section = false;
						}
					}
				}
			}
			cnt_line++;
		}
		return tokens;
	}

	void Parser::CompactTokens(std::vector<Token>& tokens) {
		for(size_t i=1; i< tokens.size(); i++) {
			if (tokens[i].type == Token::APPENDSTRING && tokens[i-1].type == Token::APPENDSTRING) {
				tokens[i-1].args[0] += tokens[i].args[0];
				tokens.erase(tokens.begin() + i);
				i--;
			}
		}
	}

	ASTPtr Parser::BuildAST(const std::vector<Token>& tokens) {
		bool is_base = std::count_if(tokens.cbegin(), tokens.cend(), [](auto& e) {
			return e.type == Token::EXTENDS;
		}) == 0;
		if(is_base) {
			return BuildBaseAST(tokens);
		} else {
			return BuildExtendingAST(tokens);
		}
	}

	ASTPtr Parser::BuildBaseAST(const std::vector<Token>& tokens) {
		auto ptr = std::make_shared<BaseTemplateAST>();
		for(auto it = tokens.begin(); it != tokens.end();) {
			if(it->type == Token::BEGIN_BLOCK) {
				auto block = std::make_shared<Block>();
				block->set_name(it->args[0]);
				it++;
				while(it != tokens.end()) {
					if(it->type == Token::END_BLOCK)
						break;
					else if(it->type == Token::BLOCK_PARENT) {
						throw std::runtime_error("parent call is only supported in extending templates");
					} else {
						auto node = BuildNode(it, tokens.end());
						block->add_node(node);
					}
				}
				ptr->add_block(block);
				auto bnode = std::make_shared<BlockCallNode>();
				bnode->set_block(block->get_name());
				ptr->add_node(bnode);
				it++;
			} else if(it->type == Token::NAMESPACE) {
				ptr->set_namespace(it->args[0]);
				it++;
			} else if(it->type == Token::VARIABLE) {
				auto var = std::make_shared<Variable>();
				var->set_name(it->args[0]);
				var->set_function_name(it->args[1]);
				var->set_type(it->args[2]);
				ptr->add_variable(var);
				it++;
			} else if(it->type == Token::PARAMETER) {
				auto param = std::make_shared<Parameter>();
				param->set_name(it->args[0]);
				param->set_type(it->args[1]);
				ptr->add_parameter(param);
				it++;
			} else if(it->type == Token::INCLUDE_CPP_HEADER) {
				ptr->add_header_include(it->args[0]);
				it++;
			} else if(it->type == Token::INCLUDE_CPP_IMPL) {
				ptr->add_implementation_include(it->args[0]);
				it++;
			} else if(it->type == Token::CODE) {
				auto code = std::make_shared<CodeBlock>();
				code->set_name(it->args[0]);
				code->set_code(it->args[1]);
				ptr->add_codeblock(code);
				it++;
			} else {
				auto node = BuildNode(it, tokens.end());
				ptr->add_node(node);
			}
		}
		return ptr;
	}

	ASTPtr Parser::BuildExtendingAST(const std::vector<Token>& tokens) {
		auto ptr = std::make_shared<ExtendingTemplateAST>();
		for(auto it = tokens.begin(); it != tokens.end();) {
			if(it->type == Token::BEGIN_BLOCK) {
				auto block = std::make_shared<Block>();
				block->set_name(it->args[0]);
				it++;
				while(it != tokens.end()) {
					if(it->type == Token::END_BLOCK)
						break;
					else if(it->type == Token::BLOCK_PARENT) {
						throw std::runtime_error("parent call is only supported in extending templates");
					} else {
						auto node = BuildNode(it, tokens.end());
						block->add_node(node);
					}
				}
				ptr->add_block(block);
				it++;
			} else if(it->type == Token::NAMESPACE) {
				ptr->set_namespace(it->args[0]);
				it++;
			} else if(it->type == Token::EXTENDS) {
				ptr->set_base_template(it->args[0]);
				it++;
			} else if(it->type == Token::VARIABLE) {
				auto var = std::make_shared<Variable>();
				var->set_name(it->args[0]);
				var->set_function_name(it->args[1]);
				var->set_type(it->args[2]);
				ptr->add_variable(var);
				it++;
			} else if(it->type == Token::PARAMETER) {
				auto param = std::make_shared<Parameter>();
				param->set_name(it->args[0]);
				param->set_type(it->args[1]);
				ptr->add_parameter(param);
				it++;
			} else if(it->type == Token::INCLUDE_CPP_HEADER) {
				ptr->add_header_include(it->args[0]);
				it++;
			} else if(it->type == Token::INCLUDE_CPP_IMPL) {
				ptr->add_implementation_include(it->args[0]);
				it++;
			} else if(it->type == Token::CODE) {
				auto code = std::make_shared<CodeBlock>();
				code->set_name(it->args[0]);
				code->set_code(it->args[1]);
				ptr->add_codeblock(code);
				it++;
			} else {
				throw std::runtime_error("extending templates do not allow free statements");
			}
		}
		return ptr;
	}

	NodePtr Parser::BuildNode(std::vector<Token>::const_iterator& it, std::vector<Token>::const_iterator end) {
		NodePtr ptr;
		switch(it->type) {
			case Token::APPENDSTRING: ptr = std::make_shared<AppendStringNode>(it->args[0]); it++; break;
			case Token::FOREACH_LOOP: ptr = BuildForEachNode(it, end); break;
			case Token::EXPRESSION: ptr = std::make_shared<ExpressionNode>(it->args[0]); it++; break;
			case Token::CONDITIONAL: ptr = BuildConditionNode(it, end); break;
			case Token::BLOCK_PARENT: ptr = std::make_shared<BlockParentCallNode>(it->args[0]); it++; break;
			default:
				throw std::runtime_error("Unknown block:" + std::to_string((int)it->type));
		}
		return ptr;
	}

	ForEachLoopNodePtr Parser::BuildForEachNode(std::vector<Token>::const_iterator& it, std::vector<Token>::const_iterator end) {
		auto ptr = std::make_shared<ForEachLoopNode>();
		ptr->set_source(it->args[1]);
		ptr->set_variable_name(it->args[0]);
		std::vector<NodePtr> nodes;
		it++;
		while(it != end) {
			if(it->type == Token::END_LOOP) {
				it++;
				break;
			}
			nodes.push_back(BuildNode(it, end));
		}
		ptr->set_nodes(nodes);
		return ptr;
	}

	ConditionNodePtr Parser::BuildConditionNode(std::vector<Token>::const_iterator& it, std::vector<Token>::const_iterator end) {
		auto ptr = std::make_shared<ConditionNode>();
		std::string condition = it->args[0];
		std::vector<NodePtr> nodes;
		it++;
		while(it != end) {
			if(it->type == Token::END_CONDITIONAL) {
				if(condition.empty()) {
					ptr->set_else(nodes);
				} else {
					ptr->add_branch(condition, nodes);
				}
				it++;
				break;
			} else if(it->type == Token::CONDITIONAL_ELSEIF) {
				if(condition.empty())
					throw std::runtime_error("Invalid conditional");
				ptr->add_branch(condition, nodes);
				condition = it->args[0];
				nodes.clear();
				it++;
			} else if(it->type == Token::CONDITIONAL_ELSE) {
				if(condition.empty())
					throw std::runtime_error("Invalid conditional");
				ptr->add_branch(condition, nodes);
				condition.clear();
				nodes.clear();
				it++;
			} else {
				auto node = BuildNode(it, end);
				nodes.push_back(node);
			}
		}
		return ptr;
	}

	void Parser::DumpNode(std::ostream& str, NodePtr n, size_t indent) {
		std::string tabs;
		for(size_t i=0; i<indent; i++) tabs += "\t";

		str << tabs << "|- ";
		switch(n->get_type()) {
			case NodeType::AppendString: {
				auto apn = std::dynamic_pointer_cast<AppendStringNode>(n);
				str << "AppendString (" << apn->get_data().size() << " bytes)";
				break;
			}
			case NodeType::Expression: {
				auto epn = std::dynamic_pointer_cast<ExpressionNode>(n);
				str << "Expression (" << epn->get_code().size() << " bytes code)";
				break;
			}
			case NodeType::BlockCall: {
				auto node = std::dynamic_pointer_cast<BlockCallNode>(n);
				str << "BlockCall " << node->get_block();
				break;
			}
			case NodeType::BlockParentCall: {
				auto node = std::dynamic_pointer_cast<BlockParentCallNode>(n);
				str << "BlockParentCall " << node->get_block();
				break;
			}
			case NodeType::ForEachLoop: {
				auto node = std::dynamic_pointer_cast<ForEachLoopNode>(n);
				str << "ForEachLoop " << node->get_variable_name() << " in " << node->get_source() << std::endl;
				for(auto& e: node->get_nodes())
					DumpNode(str, e, indent + 1);
				break;
			}
			case NodeType::Conditional: {
				auto node = std::dynamic_pointer_cast<ConditionNode>(n);
				str << "ConditionNode " << std::endl;
				for(auto& e : node->get_branches()) {
					str << tabs << "\tif " << e.first << std::endl;
					for(auto& x : e.second)
						DumpNode(str, x, indent + 1); 
				}
				if(node->get_else_branch().size() > 0) {
					str << tabs << "\telse" << std::endl;
					for(auto& e : node->get_else_branch())
						DumpNode(str, e, indent + 1);
				}
				break;
			}
		}
		str << std::endl;
	}

	ASTPtr Parser::ParseStream(std::istream& str, const std::string& fname) {
		auto tokens = Tokenize(str);
		CompactTokens(tokens);
		auto ast = BuildAST(tokens);
		ast->set_filename(fname);
		if(ast->get_classname().empty()) {
			auto parts = split(fname, ".");
			if(!parts.empty()) ast->set_classname(parts[0]);
		}
		if(!ast->is_base_ast()) {
			auto ext = std::dynamic_pointer_cast<ExtendingTemplateAST>(ast);
			std::string extname = ext->get_base_template();
			if(!startsWith(extname, "/")) {
				auto fnameparts = split(fname, "/", true);
				if(fnameparts.empty()) throw std::runtime_error("invalid template filename");
				fnameparts.erase(fnameparts.begin() + fnameparts.size() - 1); // Remove filename
				extname = join("/", fnameparts) + extname;
			}
			ext->set_base_template_ast(ParseFile(extname));
		}
		return ast;
	}

	ASTPtr Parser::ParseFile(const std::string& fname) {
		std::ifstream str(fname, std::ios::binary);
		return ParseStream(str, fname);
	}

	void Parser::DumpAST(std::ostream& str, ASTPtr ast) {
		str << "Filename:     " << ast->get_filename() << std::endl;
		str << "Type:         " << (ast->is_base_ast() ? "Base" : "Extending") << std::endl;
		if(!ast->is_base_ast()) {
			str << "Basetemplate: " << std::dynamic_pointer_cast<ExtendingTemplateAST>(ast)->get_base_template() << std::endl;
		}
		str << "Namespace:    " << ast->get_namespace() << std::endl;
		str << "Classname:    " << ast->get_classname() << std::endl;
		str << "Includes (Header):" << std::endl;
		for(auto& v : ast->get_header_includes())
			str << "\t" << v << std::endl;
		str << "Includes (Implementation):" << std::endl;
		for(auto& v : ast->get_implementation_includes())
			str << "\t" << v << std::endl;
		str << "Codeblocks:" << std::endl;
		for(auto& b : ast->get_codeblocks()) {
			str << "\t" << b->get_name() << " " << b->get_code().size() << " bytes" << std::endl;
		}
		str << "Variables:" << std::endl;
		for(auto& v : ast->get_variables())
			str << "\t" << v->get_name() << " (" << v->get_function_name() << ") " << v->get_type() << std::endl;
		str << "Parameters:" << std::endl;
		for(auto& p : ast->get_parameters())
			str << "\t" << p->get_name() << " " << p->get_type() << std::endl;
		str << "Blocks:" << std::endl;
		for(auto& b : ast->get_blocks()) {
			str << "\t" << b->get_name() << std::endl;
			for(auto& n : b->get_nodes())
				DumpNode(str, n, 1);
		}
		if(ast->is_base_ast()) {
			str << "Base block:" << std::endl;
			auto base_ast = std::dynamic_pointer_cast<BaseTemplateAST>(ast);
			for(auto b : base_ast->get_nodes()) {
				DumpNode(str, b, 1);
			}
		}
	}
}