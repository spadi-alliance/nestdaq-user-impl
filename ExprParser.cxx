/*
 *
 *
 */
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>


namespace ExprParser {

const std::string kBRA("(");
const std::string kKET(")");
const std::string kDELIM(" ");
const std::string kPAIR(":");
const std::string kSEPA(";");

struct TrgExpression {
	uint32_t type;
	std::string expr;
};


int StripSpaces(std::string &str)
{
	while ((! str.empty())
		&& (str.back() == ' '
		 || str.back() == '\t'
		 || str.back() == '\n')) {
		str.pop_back();
	}
	while ((! str.empty())
		&& (str.front() == ' '
		 || str.front() == '\t'
		 || str.front() == '\n')) {
		str.erase(0, 1);
	}

	return 0;
}

std::vector<std::string> LineSeparation(std::string param)
{
	std::vector<std::string> lines;

	std::string oneline;
	const char *c_sepa = kSEPA.c_str();
	for (char c : param) {
		if (c == *c_sepa) {
			StripSpaces(oneline);
			if (! oneline.empty()) lines.emplace_back(oneline);
			oneline.clear();
		} else {
			oneline += c;
		}
	}

	StripSpaces(oneline);
	if (! oneline.empty()) {
		lines.emplace_back(oneline);
	}

#if 0
	for (auto &one : lines) std::cout << one << ":" << std::endl;
#endif

	return lines;
}

std::vector<struct TrgExpression> Parsing(const std::string &expression)
{
	std::vector<struct TrgExpression> trig;
	std::vector<std::string> lines = LineSeparation(expression);

	const char *c_pair = kPAIR.c_str();
	for (auto &line : lines) {
		std::vector<std::string> words;
		std::string word;
		for (char c : line) {
			if (c == *c_pair) {
				StripSpaces(word);
				if (! word.empty()) words.emplace_back(word);
				word.clear();
			} else {
				word += c;
			}
		}
		StripSpaces(word);
		if (! word.empty()) words.emplace_back(word);

		struct TrgExpression t;
		try {
			unsigned long ll = std::stoul(words[0], nullptr, 0);
			t.type = 0xffffffff & ll;
		} catch (const std::invalid_argument &e) {
			std::cerr << "#E invalid argument " << e.what() << " " << words[0] << std::endl;
			break;
		} catch (const std::out_of_range &e) {
			std::cerr << "#E out of range " << e.what() << " " << words[0] << std::endl;
			break;
		}
		t.expr = words[1];
		trig.emplace_back(t);
	}

	return trig;
}

} // namespace ExprParser


#ifdef EXPRPARSER_TEST_MAIN
int main(int argc, char* argv[])
{
	std::string param(
		"0x55000000 : (0 & 1) | (2 & 3) ; 0xaa000001 : (1 & 4) & (5 & 6) ; "
		"         1 : (0 & 1) | (2 & 3) ; 0xaa000001 : (1 & 4) & (5 & 6) ;\n"
		"       123 : (0 & 1) | (2 & 3) ; 0xaa000001 : (1 & 4) &   (5 & 6) ; "
		"0xaa000003 : (0 & 1) | (2 & 3) ; 0xaa000001 : (1 & 4) & (5 & 6) ;   ");

	//param = "0xaa000000 : (0 & 1) ;";

	std::vector<struct ExprParser::TrgExpression> expressions = ExprParser::Parsing(param);

	for (auto &t : expressions) {
		std::cout << std::setw(8) << std::hex << t.type << " :" << t.expr << ":" << std::endl;
	}

	return 0;
}
#endif
