#include <iostream>
#include <fstream>
#include "TemplateCompiler.h"
#include "CTestTemplate.h"
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
	std::string basename = argv[2];
	try {
		auto input = read_file(templatefile);

		TemplateCompiler compiler;
		compiler.setRemoveExpressionOnlyLines(true);
		auto res = compiler.compile(input, basename);

		/*std::cout << res.first << std::endl;
		std::cout << std::endl;
		std::cout << res.second << std::endl;*/

		/*write_file(res.first, basename + ".h");
		write_file(res.second, basename + ".cpp");*/

		CTestTemplate tpl;
		tpl.setNavigationItems({ {"http://google.de", "Google DE" }, { "http://google.com", "Google COM" }, { "http://google.at", "Google AT" } });
		tpl.setTestVariable("Hallo");
		tpl.render();
		size_t run_times = 1000000;
		std::string render;
		auto start = std::chrono::steady_clock::now();
		for (int i = 0; i < run_times; i++) {
			tpl.setIndex(i);
			render.clear();
			tpl.render(render);
			assert(render.size() != 0);
		}
		auto end = std::chrono::steady_clock::now();
		auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
		std::cout << "Took " << dur.count() << " ms total" << std::endl;
		std::cout << "Took " << ((double)dur.count()) / run_times << " ms per run" << std::endl;
		std::cin.get();
	}
	catch (const std::exception& e) {
		std::cerr << "Error:" << e.what() << std::endl;
		return -2;
	}
}