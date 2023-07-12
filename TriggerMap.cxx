/*
 *
 *
 */

#ifndef TriggerMap_cxx
#define TriggerMap_cxx

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <stack>

#include "LogiCalc.cxx"

class TriggerMap
{
public:
	TriggerMap(){};
	virtual ~TriggerMap(){};
	void MakeTable(std::string &);
	bool LookUp(uint32_t);
	void Dump();
protected:
private:
	bool ExtractBit(uint32_t, int);
	std::vector<bool> fLut;
	unsigned int fNsignal = 0;
	unsigned int fMapSize = 0;
};

bool TriggerMap::ExtractBit(uint32_t val, int digit)
{
	if (digit < 32) {
		uint32_t bit = (val >> digit) & 0x00000001;
		return (bit > 0) ? true : false;
	} else {
		return false;
	}
}

void TriggerMap::MakeTable(std::string & formula)
{
	LogiCalc calc;
	calc.SetFormula(formula);

	fNsignal = calc.GetSigMax() + 1;
	fMapSize = 0x1 << fNsignal;
	fLut.resize(fMapSize);


	for (uint32_t i = 0 ; i < fMapSize ; i++) {
	
		#if 0
		bool bval = true;
		for (unsigned int j = 0 ;  j < fNsignal ; j++) {
			bval =  bval && ExtractBit(i, j);
		}
		fLut[i] = bval;
		#endif

		#if 0
		bool bval = false;
		for (unsigned int j = 0 ;  j < (fNsignal / 2) ; j++) {
			bval |= ExtractBit(i, (j * 2)) && ExtractBit(i, (j * 2) + 1);
		}
		fLut[i] = bval;
		#endif

		#if 0
		bool bval = false;
		for (unsigned int j = 0 ;  j < fNsignal ; j++) {
			bval |= ExtractBit(i, j);
		}
		fLut[i] = bval;
		#endif

		#if 0
		bool dtof = false;
		bool utof = false;
		for (unsigned int j = 0 ;  j < 3 ; j++) {
			dtof |= ExtractBit(i, (j * 2)) && ExtractBit(i, (j * 2) + 1);
		}
		for (unsigned int j = 0 ;  j < 2 ; j++) {
			utof |= ExtractBit(i, (j * 2) + 6) && ExtractBit(i, (j * 2) + 6 + 1);
		}
		fLut[i] = utof && dtof;
		#endif

		fLut[i] = calc.Calc(i);

	}

	return;
}

bool TriggerMap::LookUp(uint32_t val)
{
	if (val < fMapSize) {
		return fLut[val];
	} else {
		return false;
	}
}

void TriggerMap::Dump()
{
	for (unsigned int i = 0 ;  i < fMapSize ; i++) {
		if ((i % 32) == 0) std::cout << std::endl << std::setw(6) << i << " : ";
		if ((i % 16) == 0) std::cout << " ";
		std::cout << fLut[i] << " ";
	}
	std::cout << std::endl;
	return;
}

#ifdef TRIGGERMAP_TEST_MAIN
int main(int argc, char *argv[])
{
	std::string form;
	if (argc > 1) {
		form = argv[1];
	} else {
		form = "0 1 & 2 3 & | 4 5 & | 6 7 & 8 9 & | &";
	}

	TriggerMap trig;

	trig.MakeTable(form);
	for (uint32_t i = 0 ; i < 32 ; i++) {
		std::cout  << trig.LookUp(i);
	}
	std::cout << std::endl;
	
	trig.Dump();
	
	return 0;
}
#endif

#endif //#ifdef TriggerMap_cxx
