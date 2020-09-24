#include "Generator.h"
#include "Parser.h"
#include "StringHelper.h"
#include <iostream>
#include <fstream>
#ifdef WITH_FS
#include <filesystem>
namespace fs = std::filesystem;
#elif WITH_FS_EXP
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

struct cmd_options {
	std::string template_filename {};
	std::string output_filename {};
	bool dump_only = false;
	bool print_help = false;

};
static std::string ParseCommandLine(int argc, const char** const argv, cmd_options& options);
static void PrintHelp();

int main(int argc, const char** const argv) try {
	cmd_options options;
	auto err = ParseCommandLine(argc, argv, options);
	if(!err.empty()) {
		std::cerr << "Invalid commandline options: " << err << std::endl;
		return -1;
	}
	if(options.print_help) {
		PrintHelp();
		return 0;
	}
	auto ast = cpptemplate::Parser::ParseFile(options.template_filename);
	if(options.dump_only) {
		cpptemplate::Parser::DumpAST(std::cout, ast);
	} else {
		if(options.output_filename.empty()) {
			auto parts = split(options.template_filename, "/");
			parts.erase(parts.begin() + parts.size() -1);
			parts.push_back(ast->get_classname());
			options.output_filename = join("/", parts);
		}

		auto dir = options.output_filename;
		dir = dir.substr(0, dir.find_last_of('/'));
		if(!dir.empty()) {
			fs::create_directories(dir);
		}
		
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
} catch(const std::exception& e) {
	std::cerr << "Error during execution: " << e.what() << std::endl;
	return -1;
}

static std::string ParseCommandLine(int argc, const char** const argv, cmd_options& options) {
	using namespace std::string_literals;
	for(int i=1; i<argc; i++) {
		if(argv[i] == "-o"s) {
			if(i == argc-1) return "Missing value after -o";
			options.output_filename = argv[++i];
		} else if(argv[i] == "-d"s) {
			options.dump_only = true;
		} else if(argv[i] == "-h"s || argv[i] == "--help"s) {
			options.print_help = true;
		} else {
			if(!options.template_filename.empty()) return "Can only process one file per invocation";
			options.template_filename = argv[i];
		}
	}
	if(options.template_filename.empty() && !options.print_help)
		return "Missing template filename";
	return "";
}

static void PrintHelp() {
	std::cout << "cpptemplate <infile> [options]" << std::endl;
	std::cout << "\t-o <outfile>     Set output filename" << std::endl;
	std::cout << "\t-d               Just dump AST" << std::endl;
	std::cout << "\t-h               Print help" << std::endl;
}