/*
 *
 *
 */

#include <iostream>
#include <iomanip>
#include <string>

#include <sw/redis++/redis++.h>
//#include <sw/redis++/patterns/redlock.h>
#include <sw/redis++/errors.h>

#include "SignalParser.cxx"
#include "ExprParser.cxx"


//std::vector< std::vector<uint32_t> > GetTriggerSignals(std::string key, std::string server_uri)
std::tuple< std::vector< std::vector<uint32_t> >, std::vector<struct ExprParser::TrgExpression> >
       	GetTriggerSignals(std::string key, std::string server_uri)
{
	std::vector< std::vector<uint32_t> > signals;
	std::vector<struct ExprParser::TrgExpression> exprs;

	std::shared_ptr<sw::redis::Redis> redis;
	try {
		redis = std::make_shared<sw::redis::Redis>(server_uri);
	} catch (const sw::redis::Error &err) {
		std::cerr << "Redis open error: " << err.what() << std::endl;
		return {signals, exprs};
	}
	std::unordered_map<std::string, std::string> hash_data;
	try {
		redis->hgetall(key, std::inserter(hash_data, hash_data.begin()));
	} catch (const sw::redis::Error &err) {
		std::cerr << "Redis hgetall error: " << err.what() << std::endl;
		return {signals, exprs};
	}

#if 0
	std::cout << "Hash data for key: " << key << " : " << hash_data.size() << std::endl;
	for (const auto& [field, value] : hash_data) {
		std::cout << "Field: " << field << ", Value: " << value << std::endl;
	}
#endif

	signals = SignalParser::Parsing(hash_data["trigger-signals"]);
	exprs = ExprParser::Parsing(hash_data["trigger-expression"]);

#if 0
	std::cout << "Signals: " << std::endl;;
	std::cout << std::hex;
	for (auto &vec : signals) {
		for (auto &val : vec) std::cout << " 0x" << val;
		std::cout << std::endl;
	}
#endif

	return {signals, exprs};
}

#ifdef TEST_MAIN_GETTRIGGERINFO
int main(int argc, char* argv[])  {

	//[std::vector< std::vector<uint32_t> > signals,
	//	std::vector<struct ExprParser::TrgExpression> exprs]
	auto [signals, exprs]
		= GetTriggerSignals("parameters:LogicFilter", "redis://localhost:6379/2");

	std::cout << "Signals: " << signals.size() << std::endl;
	std::cout << std::hex;
	for (auto &vec : signals) {
		for (auto &val : vec) std::cout << " 0x" << std::hex << val;
		std::cout << "  delay(singed int): " << std::dec << static_cast<int>(vec[2]);
		std::cout << std::endl;
	}
	std::cout << "Triggers: " << exprs.size() << std::endl;
	for (auto &t : exprs) {
		std::cout << "0x" << std::hex << t.type << " : " << t.expr << std::endl;
	}

	return 0;
}
#endif


#if 0
int main() {
	try {
		// Redis に接続
		auto redis = sw::redis::Redis("redis://127.0.0.1:6379/2");

		// ハッシュキー
		std::string hash_key = "parameters:LogicFilter";

		// ハッシュのすべてのフィールドと値を取得
		//auto hash_data = redis.hgetall(hash_key);

		std::unordered_map<std::string, std::string> m;
		redis.hgetall(hash_key, std::inserter(m, m.begin()));


		// 結果を出力
		std::cout << "Hash data for key: " << hash_key << " : " << m.size() << std::endl;
		for (const auto& [field, value] : m) {
			std::cout << "Field: " << field << ", Value: " << value << std::endl;
		}

	} catch (const sw::redis::Error &err) {
		std::cerr << "Redis error: " << err.what() << std::endl;
	}

	return 0;
}
#endif
