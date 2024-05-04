/*
 *
 *
 */

#include <iostream>
#include <iomanip>


namespace TDC40 {

static constexpr unsigned int T_TDC = 0xd;
static constexpr unsigned int T_HB = 0xf;
static constexpr unsigned int T_ERROR = 0xe;
static constexpr unsigned int T_SPL_START = 0x1;
static constexpr unsigned int T_SPL_END = 0x4;

struct tdc40 {
	int type;
	int tot;
	int flag;
	int ch;
	int tdc;
	int hartbeat;
};

void Rev5(unsigned char *val, unsigned char *rval)
{
	rval[4] = val[0];
	rval[3] = val[1];
	rval[2] = val[2];
	rval[1] = val[3];
	rval[0] = val[4];

	return;
}

int Unpack(unsigned char *data, struct tdc40 *tdc)
{
	tdc->type = (data[4] & 0xf0) >> 4;
	if (tdc->type == T_TDC) {
		tdc->tot  = ((data[4] & 0x07) << 5) | ((data[3] & 0xf8) >> 3);
		tdc->flag = (data[3] & 0x06) >> 1;
		tdc->ch   = ((data[3] & 0x01) << 5) | (data[2] & 0xf8) >> 3;
		tdc->tdc  = ((data[2] & 0x07) << 16) | (data[1] << 8) | data[0];
		tdc->hartbeat = 0;
	} else 
	if ((tdc->type == T_HB) || (tdc->type == T_ERROR)) {
		tdc->tot  = -1;
		tdc->flag = -1;
		tdc->ch   = -1;
		tdc->tdc  = -1;
		tdc->hartbeat = ((data[1] << 8) | data[0]);
	} else {
		tdc->tot  = -1;
		tdc->flag = -1;
		tdc->ch   = -1;
		tdc->tdc  = -1;
		tdc->hartbeat = 0;
	}

	return tdc->type;
}
} //namespace TDC40


namespace TDC64H {

struct tdc64 {
	int type;
	int ch;
	int tot;
	int tdc;
	int tdc4n;
	int flag;
	int spill;
	int hartbeat;
};

static constexpr unsigned int T_TDC        = (0x2c >> 2);
static constexpr unsigned int T_TDC_L      = (0x2c >> 2);
static constexpr unsigned int T_TDC_T      = (0x34 >> 2);
static constexpr unsigned int T_HB         = (0x70 >> 2);
static constexpr unsigned int T_THR1_START = (0x64 >> 2);
static constexpr unsigned int T_THR1_END   = (0x44 >> 2);
static constexpr unsigned int T_THR2_START = (0x68 >> 2);
static constexpr unsigned int T_THR2_END   = (0x48 >> 2);
static constexpr unsigned int T_SPL_START  = (0x60 >> 2);
static constexpr unsigned int T_SPL_END    = (0x50 >> 2);

int Unpack(uint64_t data, struct tdc64 *tdc)
{
	//unsigned char *cdata = reinterpret_cast<unsigned char *>(&data);

	tdc->type = (data & 0xfc00'0000'0000'0000) >> 58;
	if (tdc->type == T_TDC) {
		tdc->ch	      = (data & 0x03f8'0000'0000'0000) >> 51;
		tdc->tot      = (data & 0x0007'ffff'e000'0000) >> 29;
		tdc->tdc      = (data & 0x0000'0000'1fff'ffff);
		tdc->tdc4n    = (data & 0x0000'0000'1fff'ffff) >> 12;
		tdc->spill    = -1;
		tdc->hartbeat = -1;
	} else
	if (tdc->type == T_HB) {
		tdc->ch	= -1;
		tdc->tot      = -1;
		tdc->tdc      = -1;
		tdc->tdc4n    = -1;
		tdc->flag     = (data & 0x03ff'0000'0000'0000) >> 48;
		tdc->spill    = (data & 0x0000'ff00'0000'0000) >> 40;
		tdc->hartbeat = (data & 0x0000'00ff'ff00'0000) >> 24;
	} else
	if ((tdc->type == T_SPL_START) || (tdc->type == T_SPL_END)) {
		tdc->ch	= -1;
		tdc->tot      = -1;
		tdc->tdc      = -1;
		tdc->tdc4n    = -1;
		tdc->flag     = (data & 0x03ff'0000'0000'0000) >> 48;
		tdc->spill    = (data & 0x0000'ff00'0000'0000) >> 40;
		tdc->hartbeat = (data & 0x0000'00ff'ff00'0000) >> 24;
	} else {
		tdc->ch	= -1;
		tdc->tot      = -1;
		tdc->tdc      = -1;
		tdc->tdc4n    = -1;
		tdc->flag     = -1;
		tdc->spill    = -1;
		tdc->hartbeat = -1;
	}

	return tdc->type;
}

#if 0
int Unpack(uint64_t data, struct tdc64 *tdc)
{
	unsigned char *cdata = reinterpret_cast<unsigned char *>(&data);
	return Unpack(cdata, tdc);
}
#endif

int Unpack(unsigned char *data, struct tdc64 *tdc)
{
	uint64_t *pdata = reinterpret_cast<uint64_t *>(data);
	return Unpack(*pdata, tdc);
}

int GetHBFrame(unsigned char *pdata, unsigned char *pend, unsigned char **ppnext)
{
	uint64_t *tdcword = reinterpret_cast<uint64_t *>(pdata);
	struct tdc64 tdc;
	int i = 0;
	while (true) {
		if (Unpack(tdcword[i++], &tdc) == T_HB) break;
		if (reinterpret_cast<unsigned char*>(&(tdcword[i])) > pend) {
			*ppnext = nullptr;
			return 0;
		}
	}
	i++;
	*ppnext = reinterpret_cast<unsigned char *>(tdcword + i);

	return i * sizeof(uint64_t);
}
} //namespace TDC64H

namespace TDC64L {

struct tdc64 {
	int type;
	int ch;
	int tot;
	int tdc;
	int tdc4n;
	int flag;
	int spill;
	int hartbeat;
};

namespace v1 {
static constexpr unsigned int T_TDC       = (0x2c >> 2);
static constexpr unsigned int T_HB        = (0x70 >> 2);
static constexpr unsigned int T_SPL_START = (0x60 >> 2);
static constexpr unsigned int T_SPL_END   = (0x50 >> 2);

int Unpack(uint64_t data, struct tdc64 *tdc)
{
	//unsigned char *cdata = reinterpret_cast<unsigned char *>(&data);

	tdc->type = (data & 0xfc00'0000'0000'0000) >> 58;
	if (tdc->type == T_TDC) {
		tdc->ch	= (data & 0x03f8'0000'0000'0000) >> 51;
		tdc->tot      = (data & 0x0007'f800'0000'0000) >> 43;
		tdc->tdc      = (data & 0x0000'07ff'ff00'0000) >> 24;
		tdc->tdc4n    = (data & 0x0000'07ff'ff00'0000) >> 26;
		tdc->flag     = -1;
		tdc->spill    = -1;
		tdc->hartbeat = -1;
	} else
	if (tdc->type == T_HB) {
		tdc->ch	= -1;
		tdc->tot      = -1;
		tdc->tdc      = -1;
		tdc->tdc4n    = -1;
		tdc->flag     = (data & 0x03ff'0000'0000'0000) >> 48;
		tdc->spill    = (data & 0x0000'ff00'0000'0000) >> 40;
		tdc->hartbeat = (data & 0x0000'00ff'ff00'0000) >> 24;
	} else
	if ((tdc->type == T_SPL_START) || (tdc->type == T_SPL_END)) {
		tdc->ch	= -1;
		tdc->tot      = -1;
		tdc->tdc      = -1;
		tdc->tdc4n    = -1;
		tdc->flag     = (data & 0x03ff'0000'0000'0000) >> 48;
		tdc->spill    = (data & 0x0000'ff00'0000'0000) >> 40;
		tdc->hartbeat = (data & 0x0000'00ff'ff00'0000) >> 24;
	} else {
		tdc->ch	= -1;
		tdc->tot      = -1;
		tdc->tdc      = -1;
		tdc->tdc4n    = -1;
		tdc->flag     = -1;
		tdc->spill    = -1;
		tdc->hartbeat = -1;
	}

	return tdc->type;
}
} //namespace v1

inline namespace v2 {
static constexpr unsigned int T_TDC        = (0x2c >> 2);
static constexpr unsigned int T_TDC_L      = (0x2c >> 2);
static constexpr unsigned int T_TDC_T      = (0x34 >> 2);
static constexpr unsigned int T_THR1_START = (0x64 >> 2);
static constexpr unsigned int T_THR1_END   = (0x44 >> 2);
static constexpr unsigned int T_THR2_START = (0x68 >> 2);
static constexpr unsigned int T_THR2_END   = (0x48 >> 2);
static constexpr unsigned int T_HB         = (0x70 >> 2);
static constexpr unsigned int T_HB1        = (0x70 >> 2);
static constexpr unsigned int T_HB2        = (0x78 >> 2);
static constexpr unsigned int T_SPL_START  = (0x60 >> 2);
static constexpr unsigned int T_SPL_END    = (0x50 >> 2);

int Unpack(uint64_t data, struct tdc64 *tdc)
{
	//unsigned char *cdata = reinterpret_cast<unsigned char *>(&data);

	tdc->type = (data & 0xfc00'0000'0000'0000) >> 58;
	if (  (tdc->type == T_TDC_L)     || (tdc->type == T_TDC_T)
	   || (tdc->type == T_THR1_START) || (tdc->type == T_THR1_END)
	   || (tdc->type == T_THR2_START) || (tdc->type == T_THR2_END)  ) {
		tdc->ch	      = (data & 0x03f8'0000'0000'0000) >> 51;
		tdc->tot      = (data & 0x0007'fff8'0000'0000) >> 35;
		tdc->tdc      = (data & 0x0000'0007'ffff'0000) >> 16;
		tdc->tdc4n    = (data & 0x0000'0007'fffc'0000) >> 18;
		tdc->flag     = -1;
		tdc->spill    = -1;
		tdc->hartbeat = -1;
	} else
	if (tdc->type == T_HB) {
		tdc->ch	      = -1;
		tdc->tot      = -1;
		tdc->tdc      = -1;
		tdc->tdc4n    = -1;
		tdc->flag     = (data & 0x03ff'0000'0000'0000) >> 48;
		tdc->spill    = (data & 0x0000'ff00'0000'0000) >> 40;
		tdc->hartbeat = (data & 0x0000'00ff'ff00'0000) >> 24;
	} else
	if ((tdc->type == T_SPL_START) || (tdc->type == T_SPL_END)) {
		tdc->ch	= -1;
		tdc->tot      = -1;
		tdc->tdc      = -1;
		tdc->tdc4n    = -1;
		tdc->flag     = (data & 0x03ff'0000'0000'0000) >> 48;
		tdc->spill    = (data & 0x0000'ff00'0000'0000) >> 40;
		tdc->hartbeat = (data & 0x0000'00ff'ff00'0000) >> 24;
	} else {
		tdc->ch	= -1;
		tdc->tot      = -1;
		tdc->tdc      = -1;
		tdc->tdc4n    = -1;
		tdc->flag     = -1;
		tdc->spill    = -1;
		tdc->hartbeat = -1;
	}

	return tdc->type;
}
} //namespace v2

} //namespace TDC64L
 

namespace TDC64H_V3 {

struct tdc64 {
	int type;
	int ch;
	int tot;
	int tdc;
	int tdc4n;
	int flag;
	int toffset;
	int genesize;
	int transize;
	int hartbeat;
};

inline namespace v2 {
static constexpr unsigned int T_TDC        = (0x2c >> 2);
static constexpr unsigned int T_TDC_L      = (0x2c >> 2);
static constexpr unsigned int T_TDC_T      = (0x34 >> 2);
static constexpr unsigned int T_THR1_START = (0x64 >> 2);
static constexpr unsigned int T_THR1_END   = (0x44 >> 2);
static constexpr unsigned int T_THR2_START = (0x68 >> 2);
static constexpr unsigned int T_THR2_END   = (0x48 >> 2);
static constexpr unsigned int T_HB         = (0x70 >> 2);
static constexpr unsigned int T_HB1        = (0x70 >> 2);
static constexpr unsigned int T_HB2        = (0x78 >> 2);
static constexpr unsigned int T_SPL_START  = (0x60 >> 2);
static constexpr unsigned int T_SPL_END    = (0x50 >> 2);

int Unpack(uint64_t data, struct tdc64 *tdc)
{
	//unsigned char *cdata = reinterpret_cast<unsigned char *>(&data);

	tdc->type = (data & 0xfc00'0000'0000'0000) >> 58;
	if (  (tdc->type == T_TDC_L)     || (tdc->type == T_TDC_T)
	   || (tdc->type == T_THR1_START) || (tdc->type == T_THR1_END)
	   || (tdc->type == T_THR2_START) || (tdc->type == T_THR2_END)  ) {
		tdc->ch	      = (data & 0x03f8'0000'0000'0000) >> 51;
		tdc->tot      = (data & 0x0007'ffff'e000'0000) >> 29;
		tdc->tdc      = (data & 0x0000'0000'1fff'ffff);
		tdc->tdc4n    = (data & 0x0000'0000'1fff'ffff) >> 12;
		tdc->flag     = -1;
		tdc->toffset  = -1;
		tdc->hartbeat = -1;
		tdc->genesize = -1;
		tdc->transize = -1;
	} else
	if (tdc->type == T_HB1) {
		tdc->ch	      = -1;
		tdc->tot      = -1;
		tdc->tdc      = -1;
		tdc->tdc4n    = -1;
		tdc->flag     = (data & 0x00ff'ff00'0000'0000) >> 40;
		tdc->toffset  = (data & 0x0000'00ff'ff00'0000) >> 24;
		tdc->hartbeat = (data & 0x0000'0000'00ff'ffff) >>  0;
		tdc->genesize = -1;
		tdc->transize = -1;
	} else
	if (tdc->type == T_HB2) {
		tdc->ch	      = -1;
		tdc->tot      = -1;
		tdc->tdc      = -1;
		tdc->tdc4n    = -1;
		tdc->flag     = (data & 0x00ff'ff00'0000'0000) >> 40;
		tdc->toffset  = -1;
		tdc->hartbeat = -1;
		tdc->genesize = (data & 0x0000'00ff'fff0'0000) >> 20;
		tdc->transize = (data & 0x0000'0000'000f'ffff) >>  0;
	} else
	{
		tdc->ch       = -1;
		tdc->tot      = -1;
		tdc->tdc      = -1;
		tdc->tdc4n    = -1;
		tdc->flag     = -1;
		tdc->toffset  = -1;
		tdc->hartbeat = -1;
		tdc->genesize = -1;
		tdc->transize = -1;
	}

	return tdc->type;
}
} //namespace v2

} //namespace TDC64H_V3
 
namespace TDC64L_V3 {

struct tdc64 {
	int type;
	int ch;
	int tot;
	int tdc;
	int tdc4n;
	int flag;
	int toffset;
	int genesize;
	int transize;
	int hartbeat;
};

inline namespace v2 {
static constexpr unsigned int T_TDC        = (0x2c >> 2);
static constexpr unsigned int T_TDC_L      = (0x2c >> 2);
static constexpr unsigned int T_TDC_T      = (0x34 >> 2);
static constexpr unsigned int T_THR1_START = (0x64 >> 2);
static constexpr unsigned int T_THR1_END   = (0x44 >> 2);
static constexpr unsigned int T_THR2_START = (0x68 >> 2);
static constexpr unsigned int T_THR2_END   = (0x48 >> 2);
static constexpr unsigned int T_HB         = (0x70 >> 2);
static constexpr unsigned int T_HB1        = (0x70 >> 2);
static constexpr unsigned int T_HB2        = (0x78 >> 2);
static constexpr unsigned int T_SPL_START  = (0x60 >> 2);
static constexpr unsigned int T_SPL_END    = (0x50 >> 2);

int Unpack(uint64_t data, struct tdc64 *tdc)
{
	//unsigned char *cdata = reinterpret_cast<unsigned char *>(&data);

	tdc->type = (data & 0xfc00'0000'0000'0000) >> 58;
	if (  (tdc->type == T_TDC_L)     || (tdc->type == T_TDC_T)
	   || (tdc->type == T_THR1_START) || (tdc->type == T_THR1_END)
	   || (tdc->type == T_THR2_START) || (tdc->type == T_THR2_END)  ) {
		tdc->ch	      = (data & 0x03fc'0000'0000'0000) >> 51;
		tdc->tot      = (data & 0x0003'fffc'0000'0000) >> 35;
		tdc->tdc      = (data & 0x0000'0003'ffff'8000) >> 15;
		tdc->tdc4n    = (data & 0x0000'0003'fffe'0000) >> 17;
		tdc->flag     = -1;
		tdc->toffset  = -1;
		tdc->genesize = -1;
		tdc->transize = -1;
		tdc->hartbeat = -1;
	} else
	if (tdc->type == T_HB1) {
		tdc->ch	      = -1;
		tdc->tot      = -1;
		tdc->tdc      = -1;
		tdc->tdc4n    = -1;
		tdc->flag     = (data & 0x00ff'ff00'0000'0000) >> 40;
		tdc->toffset  = (data & 0x0000'00ff'ff00'0000) >> 24;
		tdc->hartbeat = (data & 0x0000'0000'00ff'ffff) >>  0;
		tdc->genesize = -1;
		tdc->transize = -1;
	} else
	if (tdc->type == T_HB2) {
		tdc->ch	      = -1;
		tdc->tot      = -1;
		tdc->tdc      = -1;
		tdc->tdc4n    = -1;
		tdc->flag     = (data & 0x00ff'ff00'0000'0000) >> 40;
		tdc->toffset  = -1;
		tdc->hartbeat = -1;
		tdc->genesize   = (data & 0x0000'00ff'fff0'0000) >> 20;
		tdc->transize   = (data & 0x0000'0000'000f'ffff) >>  0;
	} else
	{
		tdc->ch       = -1;
		tdc->tot      = -1;
		tdc->tdc      = -1;
		tdc->tdc4n    = -1;
		tdc->flag     = -1;
		tdc->toffset  = -1;
		tdc->hartbeat = -1;
		tdc->genesize = -1;
		tdc->transize = -1;
	}

	return tdc->type;
}
} //namespace v2

} //namespace TDC64L3
 

#ifdef TEST_MAIN_UNPACKTDC
//#if 0

int tdc64h_dump(uint64_t data)
{
	struct TDC64H::tdc64 tdc;
	int type = TDC64H::Unpack(data, &tdc);
	if (type == TDC64H::T_TDC) {
		std::cout << "TDC " << std::dec
			<< " CH:" << tdc.ch 
			<< " TOT:" << tdc.tot 
			<< " TDC:" << tdc.tdc 
			<< " TDC4n:" << tdc.tdc4n 
			<< " : " << std::hex << data
			<< std::endl;
	} else
	if ((type == TDC64H::T_HB)
		|| (tdc.type == TDC64H::T_SPL_START)
		|| (tdc.type == TDC64H::T_SPL_END)) {
		if (type ==TDC64H::T_HB) std::cout << "HB ";
		if (type ==TDC64H::T_SPL_START) std::cout << "S_STA ";
		if (type ==TDC64H::T_SPL_END) std::cout << "S_END ";
		std::cout  << std::hex
			<< " FLAG: " << tdc.flag
			<< " SPILL: " << tdc.spill
			<< " HERTBEAT: " << tdc.hartbeat
			<< " : " << std::hex << data
			<< std::dec << std::endl;
	} else {
		std::cerr << "Invalid data : "
			<< std::hex << data << std::dec << std::endl;
	}

	return type;
}

int main(int argc, char* argv[])
{
	static char cbuf[16];
	uint64_t *pdata = reinterpret_cast<uint64_t *>(cbuf);

	uint64_t *buf = new uint64_t[1024*1024*8];

	int i = 0;
	while (true) {
		std::cin.read(cbuf, 8);
		//std::cin >> *pdata; 
		if (std::cin.eof()) break;
		buf[i++] = *pdata;
		#if 0
		std::cout << "\r "  << i << ": " << *pdata << "  " << std::flush;
		#endif
	
		tdc64h_dump(*pdata);

	}

	unsigned char *pcurr = reinterpret_cast<unsigned char *>(buf);
	unsigned char *pend = reinterpret_cast<unsigned char *>(buf + i);
	unsigned char *pnext = nullptr;
	while (true) {
		int len = TDC64H::GetHBFrame(pcurr, pend, &pnext);
		if (len <= 0) break;

		/*
		std::cout << "#D HB frame size: " << std::dec << len
			<< " curr: " << std::hex
			<< reinterpret_cast<uintptr_t>(pcurr)
			<< " next: "
			<< reinterpret_cast<uintptr_t>(pnext) << std::endl;
		*/

		pdata = reinterpret_cast<uint64_t *>(pcurr);

		for (unsigned int j = 0 ; j < len / sizeof(uint64_t) ; j++) {
			tdc64h_dump(pdata[j]);
		}
		pcurr = pnext;
	}

	return 0;
}
#endif

#if 0
int main(int argc, char* argv[])
{
	static char cbuf[16];
	uint64_t *pdata = reinterpret_cast<uint64_t *>(cbuf);
	struct TDC64H::tdc64 tdc;

	while (true) {
		std::cin.read(cbuf, 8);
		//std::cin >> *pdata; 
		if (std::cin.eof()) break;

		int type = TDC64H::Unpack(*pdata, &tdc);
		if (type == TDC64H::T_TDC) {
			std::cout << "TDC " << std::dec
				<< " CH:" << tdc.ch 
				<< " TOT:" << tdc.tot 
				<< " TDC:" << tdc.tdc 
				<< " : " << std::hex << *pdata
				<< std::endl;
		} else
		if ((type == TDC64H::T_HB)
			|| (tdc.type == TDC64H::T_SPL_START)
			|| (tdc.type == TDC64H::T_SPL_END)) {
			if (type ==TDC64H::T_HB) std::cout << "HB ";
			if (type ==TDC64H::T_SPL_START) std::cout << "S_STA ";
			if (type ==TDC64H::T_SPL_END) std::cout << "S_END ";
			std::cout  << std::hex
				<< " FLAG: " << tdc.flag
				<< " SPILL: " << tdc.spill
				<< " HERTBEAT: " << tdc.hartbeat
				<< " : " << std::hex << *pdata
				<< std::dec << std::endl;
		} else {
			std::cerr << "Invalid data : "
				<< std::hex << *pdata << std::dec << std::endl;
			break;
		}
	}

	return 0;
}
#endif

#if 0
int main(int argc, char* argv[])
{
	static unsigned char cbuf[16];
	//uint64_t *pdata = reinterpret_cast<uint64_t *>(cbuf);
	struct TDC40::tdc40 tdc;

	std::cin.read(reinterpret_cast<char *>(cbuf), 13);
	while (true) {
		std::cin.read(reinterpret_cast<char *>(cbuf), 5);
		//std::cin >> *pdata; 
		if (std::cin.eof()) break;

		int type = TDC40::Unpack(cbuf, &tdc);
		if (type == TDC40::T_TDC) {
			std::cout << "TDC " << std::dec
				<< " CH:" << std::setw(2) << tdc.ch 
				<< " TOT:" << std::setw(3) << tdc.tot 
				<< " TDC:" << std::setw(7) << tdc.tdc 
				<< " : " << std::hex
				<< std::setw(2) << static_cast<unsigned int>(cbuf[4])
				<< std::setw(2) << static_cast<unsigned int>(cbuf[3])
				<< std::setw(2) << static_cast<unsigned int>(cbuf[2])
				<< std::setw(2) << static_cast<unsigned int>(cbuf[1])
				<< std::setw(2) << static_cast<unsigned int>(cbuf[0])
				<< std::endl;
		} else
		if ((type == TDC40::T_HB)
			|| (tdc.type == TDC40::T_ERROR)
			|| (tdc.type == TDC40::T_SPL_START)
			|| (tdc.type == TDC40::T_SPL_END)) {
			if (type ==TDC40::T_HB) std::cout << "HB ";
			if (type ==TDC40::T_ERROR) std::cout << "ERR ";
			if (type ==TDC40::T_SPL_START) std::cout << "S_STA ";
			if (type ==TDC40::T_SPL_END) std::cout << "S_END ";
			std::cout  << std::dec
				//<< " FLAG: " << tdc.flag
				//<< " SPILL: " << tdc.spill
				<< " HERTBEAT: " << tdc.hartbeat
				<< " : " << std::hex
				<< std::setw(2) << static_cast<unsigned int>(cbuf[4])
				<< std::setw(2) << static_cast<unsigned int>(cbuf[3])
				<< std::setw(2) << static_cast<unsigned int>(cbuf[2])
				<< std::setw(2) << static_cast<unsigned int>(cbuf[1])
				<< std::setw(2) << static_cast<unsigned int>(cbuf[0])
				<< std::dec << std::endl;
		} else {
			std::cout << "Invalid data : " << std::hex
				<< std::setw(2) << static_cast<unsigned int>(cbuf[4])
				<< std::setw(2) << static_cast<unsigned int>(cbuf[3])
				<< std::setw(2) << static_cast<unsigned int>(cbuf[2])
				<< std::setw(2) << static_cast<unsigned int>(cbuf[1])
				<< std::setw(2) << static_cast<unsigned int>(cbuf[0])
				<< std::dec << std::endl;
			//break;
		}
	}

	return 0;
}
#endif

#if 0
int main(int argc, char* argv[])
{
	struct TDC40::tdc40 tdc;

	unsigned char rawdata[] = {
		0x00, 0x00, 0x80, 0x40, 0xd0,
		0x11, 0x00, 0x80, 0x40, 0xd0,
		0x22, 0x00, 0x80, 0x40, 0xd0,
		0x33, 0x00, 0x80, 0x40, 0xd0,
		0x44, 0x00, 0x80, 0x40, 0xd0,
		0x02, 0x00, 0x00, 0x00, 0xf0		
	};


	for (int i = 0 ; i < 30 ; i += 5) {
		TDC40::Unpack((rawdata + i), &tdc);
		std::cout << " Head: " << std::hex << tdc.type
			<< " TOT:  " << std::hex << tdc.tot
			<< " TYPE: " << std::hex << tdc.flag
			<< " CH:   " << std::hex << tdc.ch
			<< " TDC:  " << std::hex << tdc.tdc
			<< " HB:  " << std::hex << tdc.hartbeat
			<< std::endl;
	}
	

	return 0;
}
#endif
