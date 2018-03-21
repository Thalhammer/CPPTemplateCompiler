#include "Generator.h"
#include "Parser.h"
#include <iostream>
#include <fstream>

struct cmd_options {
	std::string template_filename;
	std::string output_filename;
	bool dump_only = false;

};
static std::string ParseCommandLine(int argc, const char** const argv, cmd_options& options);

int main(int argc, const char** const argv) {
	cmd_options options;
	auto err = ParseCommandLine(argc, argv, options);
	if(!err.empty()) {
		std::cerr << "Invalid commandline options: " << err << std::endl;
		return -1;
	}
	auto ast = cpptemplate::Parser::ParseFile(options.template_filename);
	if(options.dump_only) {
		cpptemplate::Parser::DumpAST(std::cout, ast);
	} else {
		if(options.output_filename.empty())
			options.output_filename = ast->get_classname();
		
		std::ofstream header(options.output_filename + ".h", std::ios::binary);
		std::ofstream impl(options.output_filename + ".cpp", std::ios::binary);
		if(!header || !impl) {
			std::cerr << "Could not open output files" << std::endl;
			return -2;
		}
		header << cpptemplate::Generator::GenerateHeader(ast);
		impl << cpptemplate::Generator::GenerateImplementation(ast);
		header.close();
		impl.close();
	}
}

static std::string ParseCommandLine(int argc, const char** const argv, cmd_options& options) {
	using namespace std::string_literals;
	for(int i=1; i<argc; i++) {
		if(argv[i] == "-o"s) {
			if(i == argc-1) return "Missing value after -o";
			options.output_filename = argv[++i];
		} else if(argv[i] == "-d"s) {
			options.dump_only = true;
		} else {
			if(!options.template_filename.empty()) return "Can only process one file per invocation";
			options.template_filename = argv[i];
		}
	}
	if(options.template_filename.empty())
		return "Missing template filename";
	return "";
}