/*
 *
 *
 */

#ifndef TriggerMap_cxx
#define TriggerMap_cxx

#include <iostream>
#include <iomanip>
#include <vector>
#include <stack>

class TriggerMap
{
public:
	TriggerMap(){};
	virtual ~TriggerMap(){};
	void MakeTable(int);
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

void TriggerMap::MakeTable(int nsignal)
{
	fNsignal = nsignal;
	fMapSize = 0x1 << nsignal;
	fLut.resize(fMapSize);

	for (uint32_t i = 0 ; i < fMapSize ; i++) {
		#if 0
		fLut[i] = (ExtractBit(i, 0) && ExtractBit(i, 1))
			&& ((ExtractBit(i, 2) && ExtractBit(i, 3))
			 || (ExtractBit(i, 4) && ExtractBit(i, 5)));
		#else
		bool bval = true;
		for (unsigned int j = 0 ;  j < fNsignal ; j++) {
			bval =  bval && ExtractBit(i, j);
		}
		fLut[i] = bval;
		#endif
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
	const int nsignal = 8;
	TriggerMap trig;

	trig.MakeTable(nsignal);
	for (uint32_t i = 0 ; i < 32 ; i++) {
		std::cout  << trig.LookUp(i);
	}
	std::cout << std::endl;
	
	trig.Dump();
	
	return 0;
}
#endif

#endif //#ifdef TriggerMap_cxx
