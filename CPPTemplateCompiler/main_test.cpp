#include <iostream>
#include "test.h"

int main() {
	std::cout<< "main" << std::endl;
	{
		templates::test tmpl;
		std::cout << "main pre render" <<std::endl;
		templates::test::params_t params;
		params["idx"] = int(10);
		tmpl.render(params);
		std::cout << "main post render" << std::endl;
	}
	std::cout << "main end" << std::endl;
}