#ifndef SIGNALPARSER_H
#define SIGNALPARSER_H

#include <iostream>
#include <vector>
#include <string>

#include <cstdint>



namespace SignalParser{
struct psig{
	uint32_t group_id;
	uint32_t subgroup_id;
	uint32_t femId;
	uint32_t channel;
	uint32_t offset;

    void print() const {
        std::cout << "group_id: " << group_id
                  << ", subgroup_id: " << subgroup_id
                  << ", femId: " << femId
                  << ", channel: " << channel
                  << ", offset: " << offset
                  << std::endl;
    }
};

constexpr uint32_t NO_SUBGROUP = UINT32_MAX;
const std::string bra("(");
const std::string ket(")");
const std::string delim(" ");

std::vector<std::string> bksplit(const std::string str);
std::vector<struct psig> Parsing(const std::string &sigstr);
} // namespace SignalParser

#endif