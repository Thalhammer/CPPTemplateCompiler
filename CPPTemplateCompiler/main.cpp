#include <iostream>
#include <fstream>
#include "TemplateCompiler.h"
#include <cassert>
#include <chrono>

std::string read_file(const std::string& fname)
{
	std::string res;
	std::ifstream file(fname, std::ios::binary);
	if (!file.good())
		throw std::runtime_error("Failed to open file");
	file.seekg(0, std::ios::end);
	res.resize(file.tellg());
	file.seekg(0, std::ios::beg);
	file.read((char*)res.data(), res.size());
	file.close();
	return res;
}

void write_file(const std::string& data, const std::string& fname)
{
	std::ofstream file(fname, std::ios::binary | std::ios::trunc);
	if (!file.good())
		throw std::runtime_error("Failed to open file");
	file.write(data.data(), data.size());
	file.flush();
	file.close();
}

class arg_parser {
	int _cidx;
	int _argc;
	const char** _argv;
	public:
		arg_parser(int argc, const char** argv)
			: _cidx(1), _argc(argc), _argv(argv)
		{}

		std::string next() {
			if(_cidx < _argc) {
				auto a = _argv[_cidx];
				_cidx++;
				return a;
			}
			throw std::runtime_error("Missing argument");
		}

		std::string peek() const {
			if(_cidx < _argc)
				return _argv[_cidx];
			throw std::runtime_error("Missing argument");
		}

		bool done() const {
			return _cidx >= _argc;
		}
};

int main(int argc, const char** argv)
{
	std::string templatefile;
	std::string classname;
	std::string outname;
	bool remove_expression_only_lines = false;
	bool print_source_line = false;

	std::vector<std::string> posargs;
	try {
		arg_parser parse(argc, argv);
		while(!parse.done()) {
			std::string p = parse.next();
			if(p.size() > 1 && p[0] == '-') {
				if(p == "-o") {
					outname = parse.next();
				} else if(p == "-r")
					remove_expression_only_lines = true;
				else if(p == "--source-lines")
					print_source_line = true;
				else throw std::runtime_error("Unknown arg:" + p);
			} else {
				posargs.push_back(p);
			}
		}
		if(templatefile.empty()) {
			if(posargs.size() < 1)
				throw std::runtime_error("Missing arg");
			templatefile = posargs[0];
			posargs.erase(posargs.begin());
		}
		if(classname.empty()) {
			if(posargs.size() < 1)
				throw std::runtime_error("Missing arg");
			classname = posargs[0];
			posargs.erase(posargs.begin());
		}
		if(outname.empty()) {
			if(posargs.size() < 1)
				throw std::runtime_error("Missing arg");
			outname = posargs[0];
			posargs.erase(posargs.begin());
		}
	}catch(const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return -1;
	}

	try {
		auto input = read_file(templatefile);

		TemplateCompiler compiler;
		compiler.setRemoveExpressionOnlyLines(remove_expression_only_lines);
		compiler.setPrintSourceLineComment(print_source_line);
		auto res = compiler.compile(input, classname);

		write_file(res.first, outname + ".h");
		write_file(res.second, outname + ".cpp");
		return 0;
	}
	catch (const std::exception& e) {
		std::cerr << "Error:" << e.what() << std::endl;
		return -2;
	}
}