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

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::cerr << argv[0] << " <inputtemplate> <classname>" << std::endl;
		return -1;
	}
	std::string templatefile = argv[1];
	std::string classname = argv[2];
	std::string outname = classname;
	if(argc >= 4) {
		outname = argv[3];
	}

	try {
		auto input = read_file(templatefile);

		TemplateCompiler compiler;
		compiler.setRemoveExpressionOnlyLines(true);
		compiler.setPrintSourceLineComment(true);
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