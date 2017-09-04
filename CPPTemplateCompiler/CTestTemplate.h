#include "NavItem.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>
class CTestTemplate
{
	public:
		std::string render() const;
		void render(std::string& str) const;
		void setNavigationItems(std::vector<NavItem> navigation) { this->navigation = navigation; }
		std::vector<NavItem> getNavigationItems() const { return this->navigation; }
		void setTestVariable(std::string a_variable) { this->a_variable = a_variable; }
		std::string getTestVariable() const { return this->a_variable; }
		void setIndex(int index) { this->index = index; }
		int getIndex() const { return this->index; }
	protected:
		std::vector<NavItem> navigation; // NavigationItems
		std::string a_variable; // TestVariable
		int index; // Index
		static std::string strlocaltime(time_t time, const char* fmt);
		virtual void renderBlock_testBlock(std::string& str) const;
};