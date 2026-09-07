#ifndef SIGNALPARSER_H
#define SIGNALPARSER_H

#include <iostream>
#include <vector>
#include <string>

#include <cstdint>

struct psig{
	uint32_t group_id;
	uint32_t subgroup_id;
	uint32_t femId;
	uint32_t channel;
	uint32_t offset;
};

namespace SignalParser{

constexpr uint32_t NO_SUBGROUP = UINT32_MAX;
const std::string bra("(");
const std::string ket(")");
const std::string delim(" ");

std::vector<std::string> bksplit(const std::string str);
std::vector<struct psig> Parsing(const std::string &sigstr);
} // namespace SignalParser

#endif