/*
 *
 *
 */
#ifndef LOGICALC_CXX
#define LOGICALC_CXX

#include <iostream>
#include <iomanip>
#include <vector>
#include <stack>

#include <algorithm>


class LogiCalc
{
public:
	LogiCalc(){};
	virtual ~LogiCalc(){};
	std::vector<std::string> & SetFormula(std::string);
	std::string& GetFormula();
	int GetSigMax(){return fSigMax;};
	bool Calc(uint32_t);
	void Dump(){};
protected:
private:
	bool ExtractBit(uint32_t, int);
	//std::vector<std::string> &Split(std::string);
	int ComPushBack(std::string &);

	std::vector<std::string> fCommands;
	std::stack<bool> fStack;
	int fNsigMax = 32;
	int fSigMax = 0;
	const std::vector<std::string> fFuncs = {"&", "|", "!", "x", "d"};
	const std::string fSeparator = std::string(" ");

};

bool LogiCalc::ExtractBit(uint32_t val, int digit)
{
	if (digit < 32) {
		uint32_t bit = (val >> digit) & 0x00000001;
		return (bit > 0) ? true : false;
	} else {
		return false;
	}
}

int LogiCalc::ComPushBack(std::string &word)
{
	if (word.length() > 0) {
		if (std::all_of(word.cbegin(), word.cend(), isdigit)) {
			int sig = atoi(word.c_str());
			if ((sig >= 0) && (sig < fNsigMax)) {
				fCommands.push_back(word);
				if (sig > fSigMax) fSigMax = sig;
			} else {
				std::cout << "#E Over range : " << sig << std::endl;
			}
		} else {
			bool hit = false;
			for (auto & fword : fFuncs) {
				if (word == fword) {
					fCommands.push_back(word);
					hit = true;
					break;
				}
			}
			if (! hit) std::cout << "#E Unknown talken : " << word << std::endl;
		}
	}
	return fCommands.size();
}

std::vector<std::string> & LogiCalc::SetFormula(std::string sform)
{
	fCommands.clear();
	fCommands.resize(0);
	fSigMax = 0;

	int slen = fSeparator.length();
	auto offset = std::string::size_type(0);
	while (true) {
		auto pos = sform.find(fSeparator, offset);
		if (pos == std::string::npos) {
			auto word = sform.substr(offset);
			if (word.length() > 0) ComPushBack(word);
			break;
		}
		auto word = sform.substr(offset, pos - offset);
		if (word.length() > 0) ComPushBack(word);
		offset = pos + slen;
	}
	
	return fCommands;
}

bool LogiCalc::Calc(uint32_t val)
{
	while (! fStack.empty()) fStack.pop();

	for (auto & com : fCommands) {
		if (std::all_of(com.cbegin(), com.cend(), isdigit)) {
			int sig = atoi(com.c_str());
			fStack.push(ExtractBit(val, sig));
		} else {
			if (fStack.size() > 1) {
				if ((com == "&") || (com == "*")) {
					auto val1 = fStack.top(); fStack.pop();
					auto val2 = fStack.top(); fStack.pop();
					fStack.push(val1 & val2);
				}
				if ((com == "|") || (com == "+")) {
					auto val1 = fStack.top(); fStack.pop();
					auto val2 = fStack.top(); fStack.pop();
					fStack.push(val1 | val2);
				}
				if (com == "x") {
					auto val1 = fStack.top(); fStack.pop();
					auto val2 = fStack.top(); fStack.pop();
					fStack.push(val1);
					fStack.push(val2);
				}
			}
			if ((com == "!") || (com == "^")) {
				auto vtop = fStack.top(); fStack.pop();
				fStack.push(! vtop);
			}
			if ((com == "d") || (com == "p")) {
				if (fStack.size() > 0) {
					fStack.pop();
				} else {
					std::cout << "#E empty stack" << std::endl;
				}
			}
		}
	}

	bool ret;
	if (fStack.size() > 0) {
		ret = fStack.top(); fStack.pop();
	} else {
		std::cout << "#E empty stack result" << std::endl;
		ret = false;
	}
	return ret;
}


#ifdef LOGICALC_TEST_MAIN
int main(int argc, char *argv[])
{
	std::string formula;
	if (argc > 1) {
		formula = argv[1];
	} else {
		formula = "0 1 & 2 3 & | 4 5 & 6 7 & | &";
	}

	LogiCalc calc;

	#if 0
	const std::vector<std::string> form = {
		"1  2 & 3 4 & |",
		"32 && 3 4 & | *"};
	for (auto & f : form) {
		auto & commands = calc.SetFormula(f);
		for (auto & com : commands) {
			std::cout  << "-" << com;
		}
		std::cout << std::endl;
	}
	#endif

	auto & commands = calc.SetFormula(formula);

	std::cout << "Formula(RPN): ";
	for (auto & com : commands) std::cout  << " " << com;

	uint32_t range = 1 << (calc.GetSigMax() +1);
	for (uint32_t i = 0 ; i < range ; i++) {
		if ((i % 16) == 0) std::cout << std::endl;
		std::cout << " " << calc.Calc(i);
	}

	#if 1
	for (uint32_t i = 0 ; i < range ; i++) {
		if ((i % 16) == 0) std::cout << std::endl;
		std::cout << " " << std::hex << std::setw(2) << i << ":" << calc.Calc(i);
	}
	#endif

	std::cout << std::dec << std::endl;
	
	return 0;
}
#endif

#endif //LOGICALC_CXX
