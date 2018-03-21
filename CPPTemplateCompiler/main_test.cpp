#include "test_ext.h"
#include <iostream>

int main(int argc, const char** argv) {
	templates::test_ext t;
	templates::test_ext::params p;
	p.idx = 1;
	std::cout << t.render(p) << std::endl;
	return 0;
}