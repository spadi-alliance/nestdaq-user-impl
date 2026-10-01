/*
 *
 *
 */
#include <iostream>
#include <vector>
#include <string>

#if 0
struct signal_prop {
	uint32_t id;
	uint32_t channel;
	uint32_t offset;
};

class SignalParser
{
public:
	SignalParser(){};
	virtual ~SignalParser(){};
	std::vector<struct signal_prop> & Parsing(std::string &);
protected:
private:
	std::vector<struct signal_prop> signals;
};

std::vector<struct signal_prop> & SignalParser::Parsing(std::string &sigstr)
{
	std::cout << sigstr << std::endl;
	return signals;
}
#endif

namespace SignalParser {

const std::string bra("(");
const std::string ket(")");
const std::string delim(" ");

std::vector<std::string> bksplit(const std::string str)
{
	char cbra = bra.c_str()[0];
	char cket = ket.c_str()[0];
	char cdelim = delim.c_str()[0];

	std::vector<std::string> words;

	std::string word;
	for (char ch : str) {
		if ((ch == cbra)  || (ch == cket)) {
			if (! word.empty()) {
				words.emplace_back(word);
				word.clear();
			}
			char temp_ch[2] = {0, 0};
			temp_ch[0] = ch;
			std::string braket(temp_ch);
			words.emplace_back(braket);
		} else
		if (ch == cdelim) {
			if (! word.empty()) {
				words.emplace_back(word);
				word.clear();
			}
		} else {
			word += ch;
		}
	}
	if (! word.empty()) words.emplace_back(word);

	return words;
}


std::vector<std::vector<uint32_t> > Parsing(const std::string &sigstr)
{
	std::vector<std::vector<uint32_t> > signals;
	#if 0
	std::cout << sigstr << std::endl;
	#endif

	std::vector<std::string> words = bksplit(sigstr);

	#if 0
	std::cout << "# " << words.size() ;
	for (auto &s : words)  std::cout << "_" << s;
	std::cout << std::endl;
	#endif

	int level = 0;
	std::vector<uint32_t> low;
	for (auto &v : words) {
		if (v == bra) {
			level++;
			low.clear();
			low.resize(0);
		} else
		if (v == ket) {
			level--;
			if (low.size() > 0) signals.emplace_back(low);
		} else {
			try {
				unsigned long ulval = std::stoul(v, nullptr, 0);
				uint32_t val = 0xffffffff & ulval;
				low.emplace_back(val);
			} catch (const std::invalid_argument &e) {
				std::cout << "#E Bad number word: " << v << " " << e.what() << std::endl;
				//throw e;
			} catch (const std::out_of_range &e) {
				std::cout << "#E Bad number word (out of range) : "
					<< v << " " << e.what() << std::endl;
				//throw e;
			}
			
		}
		
	}

	return signals;
}

} // namespace SignalParser

#ifdef SIGNALPARSER_TEST_MAIN
int main(int argc, char* argv[])
{
	std::string param("(0x1234 0 0) ( 0xc0a802a9 1 0 ) (0xa235 xxx1 1)");
	std::vector<std::vector<uint32_t> > signals
		= SignalParser::Parsing(param);

	std::cout << "String: " << param << std::endl;
	std::cout << std::hex;
	for (auto &vec : signals) {
		for (auto &val : vec) std::cout << " 0x" << val;
		std::cout << std::endl;
	}
	

	return 0;
}
#endif
