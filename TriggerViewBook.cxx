/*
 *
 */

#include "TCanvas.h"
#include "TThread.h"
#include "TMutex.h"
#include "TH1F.h"
#include "TH2F.h"

#include "FilterHeader.h"
#include "GetTriggerInfo.cxx"

static TCanvas *gCan1 = new TCanvas ("DISPLAY", "Display");
static TH1F *gHHRTDC[2];
static TH2F *gH2HRTDC;
static TH1F *gHDiff;

static TH1F *gHTrig;
static std::vector<TH2F *> gH2TrigWindow;
static std::vector<TH2F *> gH2TrigWindowOne;
static std::vector<uint32_t> gTrigType;
static TH1F *gHElapse;
static TH1F *gHNTrig;

static const int gMaxTrigHist = 32;

struct trg_tdc {
	uint32_t trg_type;
	std::vector<int> tdc4n;
};
static std::vector<struct trg_tdc> gTrigTdc;

struct entry_tdc {
	std::vector<int> tdc4n;
	int      index;
	uint32_t module;
	int      channel;
	int      offset;
	int      type;
};
static std::vector<struct entry_tdc> gEntryTDC;

struct signal_id {
	int      index;
	uint32_t module;
	int      channel;
	int      offset;
};

static std::vector<struct signal_id> g_trg_sources;

#if 0
static std::vector<struct signal_id> g_trg_sources = {
	{ 0, 0xc0a802a9,  8,   0}, { 1, 0xc0a802a9, 10,   0},
	{ 2, 0xc0a802aa, 16, -12}, { 3, 0xc0a802aa, 17, -12}, { 4, 0xc0a802aa, 18, -12}, { 5, 0xc0a802aa, 19, -12},
	{ 6, 0xc0a802aa, 20, -12}, { 7, 0xc0a802aa, 21, -12}, { 8, 0xc0a802aa, 22, -12}, { 9, 0xc0a802aa, 23, -12},
	{10, 0xc0a802aa, 24, -12}, {11, 0xc0a802aa, 25, -12}, {12, 0xc0a802aa, 27, -12}, {13, 0xc0a802aa, 28, -12},
	{14, 0xc0a802a3,  4, -12}, {15, 0xc0a802a3, 16, -12}
};

static std::vector<struct signal_id> g_trg_sources = {
	{ 0, 0xc0a80a23, 31, 0}, { 1, 0xc0a80a23, 15, 0},
	{ 2, 0xc0a80a23, 63, 0}, { 3, 0xc0a80a24, 47, 0},
	{ 4, 0xc0a80a24,  0, 0}, { 5, 0xc0a80a24, 16, 0},
	{ 6, 0xc0a80a25,  0, 0}, { 7, 0xc0a80a25,  1, 0}, { 8, 0xc0a80a25,  2, 0}, { 9, 0xc0a80a25,  3, 0},
	{10, 0xc0a80a25,  4, 0}, {11, 0xc0a80a25,  5, 0}, {12, 0xc0a80a25,  6, 0}, {13, 0xc0a80a25,  7, 0},
	{14, 0xc0a80a25, 18, 0}, {15, 0xc0a80a25, 19, 0}
};


	{ 4, 0xc0a80a23, 15, 0}, { 5, 0xc0a80a23, 47, 0}, { 5, 0xc0a80a24, 16, 0},
	{ 6, 0xc0a80a23,  7, 0}, { 7, 0xc0a80a23, 24, 0},
	{16, 0xc0a80a25,  6, 0}, {17, 0xc0a80a25,  7, 0}, {18, 0xc0a80a25, 18, 0}, {19, 0xc0a80a25, 19, 0},
	{20, 0xc0a80a25, 20, 0}, {21, 0xc0a80a25, 21, 0}, {22, 0xc0a80a25, 22, 0}, {23, 0xc0a80a25, 23, 0},
	{24, 0xc0a80a25, 24, 0}, {25, 0xc0a80a25, 25, 0}, {26, 0xc0a80a25, 26, 0}, {27, 0xc0a80a25, 27, 0},
	{28, 0xc0a80a25, 28, 0}, {29, 0xc0a80a25, 29, 0}, {30, 0xc0a80a25, 30, 0}, {31, 0xc0a80a25, 31, 0}
};

#endif



//TMutex *gMtxDisp;

void gEventCycle(void *arg = NULL)
{
	(void)arg;

	while (true) {
		//gMtxDisp->Lock();
		gSystem->ProcessEvents();
		//gMtxDisp->UnLock();
		TThread::Sleep(0, 100'000'000);
	}
	return;
}

void gHistTrigSrcInit()
{
	for (auto &i : g_trg_sources) {
		struct entry_tdc src;
		src.index = i.index;
		src.module = i.module;
		src.channel = i.channel;
		src.offset = i.offset;
		src.type = 0;
		src.tdc4n.clear();
		src.tdc4n.resize(0);
		gEntryTDC.emplace_back(src);
	}

#if 0
	gTrigType.clear();
	gTrigType.emplace_back(0xaa000000);
	gTrigType.emplace_back(0xaa000001);
#endif

	return;
}

void gHistInit()
{
	#if 0
	gMtxDisp = new TMutex();
	#endif

	gHHRTDC[0] = new TH1F("HRTDC0", "HR-TDC0", 64, 0, 64);
	gHHRTDC[1] = new TH1F("HRTDC1", "HR-TDC1", 64, 0, 64);
	gH2HRTDC = new TH2F("HRTDC02D", "HR-TDC 2D", 64, 0, 64, 64, 0, 64);
	gHDiff = new TH1F("DIFF", "Diff", 1000, 0, 1000000);

	gHTrig = new TH1F("TRIG", "Trigger", 1000, 0, 150000);
	gHElapse = new TH1F("ELAPSE", "Elapse Time (ms)", 500, 0, 20000);
	gHNTrig = new TH1F("NTRIG", "Number of Trigger (/STF)", 500, 0, 1000);

	for (int i = 0 ; i < gMaxTrigHist ; i++) {
		std::ostringstream noss, toss;
		noss << "TWIN" << std::setw(3) << std::setfill('0') << i;
		toss << "Trigger Window " << std::setw(3) << std::setfill('0') << i;
		gH2TrigWindow.emplace_back(new TH2F(noss.str().c_str(), toss.str().c_str(),
			1000, -500., 500., 32, 0., 32.));

		std::ostringstream nooss, tooss;
		nooss << "TWONE" << std::setw(3) << std::setfill('0') << i;
		tooss << "Trigger Window One event " << std::setw(3) << std::setfill('0') << i;
		gH2TrigWindowOne.emplace_back(new TH2F(nooss.str().c_str(), tooss.str().c_str(),
			1000, -500., 500., 32, 0., 32.));
	}

	return;
}

void gHistWindowInit(std::string key, std::string redis_server)
{
        //std::vector< std::vector<uint32_t> > signals
	auto [signals, exprs] = GetTriggerSignals(key, redis_server);

	g_trg_sources.clear();
	g_trg_sources.resize(0);
	for (unsigned int i = 0 ; i < signals.size() ; i++) {
		if (signals[i].size() >= 3) {
			struct signal_id sig;
			sig.index = i;
			sig.module = signals[i][0];
			sig.channel = signals[i][1];
			sig.offset = static_cast<int>(signals[i][2]);
			g_trg_sources.emplace_back(sig);
		}
	}

	gTrigType.clear();
	gTrigType.resize(0);
	for (auto &t : exprs) gTrigType.emplace_back(t.type);

	gHistTrigSrcInit();

	gCan1->Clear();
	gCan1->Divide(2, 1 + gTrigType.size());
	gCan1->cd(1); gHElapse->Draw();
	gHNTrig->GetXaxis()->SetRangeUser(2., 1000.);
	gCan1->cd(2); gHNTrig->Draw();

	for (unsigned int i = 0 ; i < gTrigType.size() ; i++) {
		gH2TrigWindowOne[i]->GetXaxis()->SetRangeUser(-100., 100.);
		gH2TrigWindowOne[i]->GetYaxis()->SetRangeUser(0., 16.);
		gCan1->cd(3 + (2 * i)); gH2TrigWindowOne[i]->Draw("box");

		gH2TrigWindow[i]->GetXaxis()->SetRangeUser(-100., 100.);
		gH2TrigWindow[i]->GetYaxis()->SetRangeUser(0., 16.);
		gCan1->cd(4 + (2 * i))->SetLogz();
		gCan1->cd(4 + (2 * i)); gH2TrigWindow[i]->Draw("col2");
	}

	return;
}

void gHistDraw()
{

	for (unsigned int i = 0 ; i < 4 + (2 * gTrigType.size()) ; i++) {
		gCan1->cd(i + 1)->Modified();
	}

	//gMtxDisp->Lock();
	gCan1->Update();
	//gMtxDisp->UnLock();
	
	return;
}

void gHistReset()
{
	//gMtxDisp->Lock();

	gHHRTDC[0]->Reset();
	gHHRTDC[1]->Reset();
	gH2HRTDC->Reset();
	gHDiff->Reset();

	gHTrig->Reset();

	gHElapse->Reset();
	gHNTrig->Reset();

	for (unsigned int i = 0 ; i < gTrigType.size() ; i++) {
		gH2TrigWindow[i]->Reset();
		gH2TrigWindowOne[i]->Reset();
	}

	//gMtxDisp->UnLock();

	return;
}

void gHistFlt(struct Filter::Header *pflt)
{
	
	gHElapse->Fill(pflt->elapseTime);
	gHNTrig->Fill(pflt->numTrigs);

	return;
}

bool gTrigTdc_isvalid = false;
void gHistTrigTdc_clear()
{
	for (auto &i : gTrigTdc) {
		i.tdc4n.clear();
		i.tdc4n.resize(0);
	}
	gTrigTdc.clear();
	gTrigTdc.resize(0);
	gTrigTdc_isvalid = false;

	for (auto &i : gEntryTDC) {
		i.tdc4n.clear();
		i.tdc4n.resize(0);
	}
	
	return;
}

void gHistTrig(Filter::TrgTime *pdata, unsigned int len)
{
	//std::vector<int> trig_in_hbf;
	struct trg_tdc trig_in_hbf;

	Filter::TrgTime *top = reinterpret_cast<struct Filter::TrgTime *>(pdata);
	uint32_t trg_type = trig_in_hbf.trg_type = top->type;
	if (len < sizeof(struct Filter::TrgTime)) trig_in_hbf.trg_type = 0xffffffff;

	for (unsigned int i = 0 ; i < len ; i++) {
		Filter::TrgTime *t = reinterpret_cast<Filter::TrgTime *>(pdata + i);
		if (t->type == trg_type) {
			trig_in_hbf.tdc4n.emplace_back(t->time);
		} else {
			std::cout << "#E multipule Trigger type in FLT data "
				<< std::hex << t->type << ":" << trg_type
				<< std::endl;
		}
		gHTrig->Fill(t->time);
	}

	gTrigTdc.emplace_back(trig_in_hbf);
	gTrigTdc_isvalid = true;

	return;
}

void gHistEntryTDC(fair::mq::MessagePtr& msg, uint32_t id, int type)
{
	//unsigned int msize = msg->GetSize();
	//uint64_t msg_magic = *(reinterpret_cast<uint64_t *>(msg->GetData()));
	//HeartbeatFrame::Header *phbf = reinterpret_cast<HeartbeatFrame::Header *>(msg->GetData());

	unsigned char *pdata = reinterpret_cast<unsigned char *>(
		reinterpret_cast<unsigned char *>(msg->GetData()) + sizeof(HeartbeatFrame::Header));
	unsigned int dsize = msg->GetSize() - sizeof(HeartbeatFrame::Header);

	for (auto &mod : gEntryTDC) {
		if (id == mod.module) {
			mod.type = type;
			//std::vector<int> tdc4n;
			for (size_t i = 0 ; i < dsize ; i += sizeof(uint64_t)) {
				uint64_t *dword = reinterpret_cast<uint64_t *>(&(pdata[i]));
				int val_tdc4n = -1;
				int val_ch = -1;

				if (type == SubTimeFrame::TDC64H_V3) {
					struct TDC64H_V3::tdc64 tdc;
					TDC64H_V3::Unpack(*dword, &tdc);
					val_tdc4n = tdc.tdc4n;
					val_ch = tdc.ch;
				} else
				if (type == SubTimeFrame::TDC64L_V3) {
					struct TDC64L_V3::tdc64 tdc;
					TDC64L_V3::Unpack(*dword, &tdc);
					val_tdc4n = tdc.tdc4n;
					val_ch = tdc.ch;
				} else {
					break;
				}

				if (val_ch == mod.channel) {
					//tdc4n.emplace_back(val_tdc4n);
					mod.tdc4n.emplace_back(val_tdc4n);
				}

			}

			//mod.tdc4n.emplace_back(tdc4n);
		}
	}

	return;
}

void gHistBookTrigWinOne()
{
	static unsigned int tw_counter = 0;
	const unsigned int tw_pre_factor = 100;	

	for (unsigned int i = 0 ; i < gTrigTdc.size() ; i++) {
		for (unsigned int itt = 0 ; itt < gTrigType.size() ; itt++) {
			if (gTrigTdc[i].trg_type == gTrigType[itt]) {
				for (auto &trg : gTrigTdc[i].tdc4n) {
					bool is_draw = ((tw_counter++ % tw_pre_factor) == 0);
					if (is_draw) gH2TrigWindowOne[itt]->Reset();
					for (auto &mod : gEntryTDC) {
						for (auto &tdc4n : mod.tdc4n) {
							if (is_draw) {
								int diff = tdc4n - trg;
								if (std::abs(diff) < 500) {
									gH2TrigWindowOne[itt]->Fill(
										diff + mod.offset, mod.index);
								}
							}
						}
					}
					if (is_draw) gCan1->cd(3 + 2 * itt)->Modified();
				}
			}
		}
	}
	
	return;
}

void gHistBookTrigWin(fair::mq::MessagePtr& msg, uint32_t id, int type)
{
	unsigned int msize = msg->GetSize();
	unsigned char *pdata = reinterpret_cast<unsigned char *>(msg->GetData());

	if (! gTrigTdc_isvalid) return;

	for (size_t i = 0 ; i < msize ; i += sizeof(uint64_t)) {
		for (auto &sig : g_trg_sources) {

			int val_tdc4n = -1;
			int val_ch = -1;
			uint64_t *dword = reinterpret_cast<uint64_t *>(&(pdata[i]));

			if ((pdata[i + 7] & 0xfc) == (TDC64H_V3::T_TDC << 2) 
				&& (type == SubTimeFrame::TDC64H_V3)) {

				struct TDC64H_V3::tdc64 tdc;
				TDC64H_V3::Unpack(*dword, &tdc);
				val_tdc4n = tdc.tdc4n;
				val_ch = tdc.ch;
			} else 
			if ((pdata[i + 7] & 0xfc) == (TDC64L_V3::T_TDC << 2)
				&& (type == SubTimeFrame::TDC64L_V3)) {

				struct TDC64L_V3::tdc64 tdc;
				TDC64L_V3::Unpack(*dword, &tdc);
				val_tdc4n = tdc.tdc4n;
				val_ch = tdc.ch;
			} else {
				continue;
			}

			if ((id == sig.module) && (val_ch == sig.channel)) {
				for (auto &trigs : gTrigTdc) {
					for (unsigned int itt = 0 ; itt < gTrigType.size() ; itt++) {
						if (trigs.trg_type == gTrigType[itt]) {
							for (auto &trg : trigs.tdc4n) {
								int diff = val_tdc4n - trg;
								if (std::abs(diff) < 500) {
									gH2TrigWindow[itt]->Fill(
										diff + sig.offset, sig.index);
								}
							}
						}
					}
				}
			}

		}
	}

	return;
}


void gHistBook(fair::mq::MessagePtr& msg, uint32_t id, int type)
{
	unsigned int msize = msg->GetSize();
	unsigned char *pdata = reinterpret_cast<unsigned char *>(msg->GetData());
	//uint64_t msg_magic = *(reinterpret_cast<uint64_t *>(pdata));
	static unsigned int prescale = 0;

	for (size_t i = 0 ; i < msize ; i += sizeof(uint64_t)) {
		if ((id & 0x000000ff) == 169) {
			if ((pdata[i + 7] & 0xfc) == (TDC64H_V3::T_TDC << 2)) {

				uint64_t *dword = reinterpret_cast<uint64_t *>(&(pdata[i]));
				if (type == SubTimeFrame::TDC64H_V3) {
					struct TDC64H_V3::tdc64 tdc;
					TDC64H_V3::Unpack(*dword, &tdc);
					//gMtxDisp->Lock();
					gHHRTDC[0]->Fill(tdc.ch);
					//gMtxDisp->UnLock();
					#if 0
					std::cout << "FEM: " << (id & 0xff) << " TDC ";
					std::cout << "H :"
						<< " CH: " << std::dec << std::setw(3) << tdc.ch
						<< " TDC: " << std::setw(7) << tdc.tdc << std::endl;
					#else
					//std::cout << "*" << std::flush;
					#endif

				} else
				if (type == SubTimeFrame::TDC64L_V3) {
					struct TDC64L_V3::tdc64 tdc;
					TDC64L_V3::Unpack(*dword, &tdc);
					#if 0
					std::cout << "FEM: " << (id & 0xff) << " TDC ";
					std::cout << "L :"
						<< " CH: " << std::dec << std::setw(3) << tdc.ch
						<< " TDC: " << std::setw(7) << tdc.tdc << std::endl;
					#endif
				} else {
					std::cout << "UNKNOWN"<< std::endl;
				}
			}
		}

		if ((id & 0x000000ff) == 170) {
			if ((pdata[i + 7] & 0xfc) == (TDC64H_V3::T_TDC << 2)) {
	
				uint64_t *dword = reinterpret_cast<uint64_t *>(&(pdata[i]));
				if (type == SubTimeFrame::TDC64H_V3) {
					struct TDC64H_V3::tdc64 tdc;
					TDC64H_V3::Unpack(*dword, &tdc);
					//gMtxDisp->Lock();
					gHHRTDC[1]->Fill(tdc.ch);
					//gMtxDisp->UnLock();
					#if 0
					std::cout << "FEM: " << (id & 0xff) << " TDC ";
					std::cout << "H :"
						<< " CH: " << std::dec << std::setw(3) << tdc.ch
						<< " TDC: " << std::setw(7) << tdc.tdc << std::endl;
					#endif
				}
			}
		}


		for (auto &sig : g_trg_sources) {

			int val_tdc4n = -1;
			int val_ch = -1;
			uint64_t *dword = reinterpret_cast<uint64_t *>(&(pdata[i]));
			if ((pdata[i + 7] & 0xfc) == (TDC64H_V3::T_TDC << 2) 
				&& (type == SubTimeFrame::TDC64H_V3)) {

				struct TDC64H_V3::tdc64 tdc;
				TDC64H_V3::Unpack(*dword, &tdc);
				val_tdc4n = tdc.tdc4n;
				val_ch = tdc.ch;
			} else
			if ((pdata[i + 7] & 0xfc) == (TDC64L_V3::T_TDC << 2) 
				&& (type == SubTimeFrame::TDC64L_V3)) {

				struct TDC64L_V3::tdc64 tdc;
				TDC64L_V3::Unpack(*dword, &tdc);
				val_tdc4n = tdc.tdc4n;
				val_ch = tdc.ch;
			} else {
				continue;
			}

			if ((id == sig.module) && (val_ch == sig.channel)) {
				for (auto &trigs : gTrigTdc) {
					for (unsigned int itt = 0 ; itt < gTrigType.size() ; itt++) {
						if (trigs.trg_type == gTrigType[itt]) {
							for (auto &trg : trigs.tdc4n) {
								int diff = val_tdc4n - trg;
								if (std::abs(diff) < 500) {
									gH2TrigWindow[itt]->Fill(
										diff + sig.offset, sig.index);
								}
							}
						}
					}
				}
			}
		}


	}

#if 0
	uint64_t *pdata64 = reinterpret_cast<uint64_t *>(msg->GetData());
	if ((id & 0x000000ff) == 169) {
		for (size_t ii = 0 ; ii < (msize / sizeof(uint64_t)) ; ii++) {
			if (((pdata64[ii] & 0xfc00'0000'0000'0000) >> 58) == TDC64H_V3::T_TDC) {
				struct TDC64H_V3::tdc64 tdc;
				TDC64H_V3::Unpack(pdata64[ii], &tdc);
				auto chx = tdc.ch;
				auto tdcx = tdc.tdc;
				for (size_t j = 0 ; j < (msize / sizeof(uint64_t)) ; j++) {
					if (((pdata64[j] & 0xfc00'0000'0000'0000) >> 58) == TDC64H_V3::T_TDC) {
						TDC64H_V3::Unpack(pdata64[j], &tdc);
						auto chy = tdc.ch;
						auto tdcy = tdc.tdc;

						//std::cout << " " << abs(tdcx - tdcy);
						int diff = abs(tdcx - tdcy);
						if (diff > 0) {
							//gMtxDisp->Lock();
							gHDiff->Fill(diff);
							//gMtxDisp->UnLock();
							if (diff < 50'000) {
								if (chx == chy) {
									//std::cout << "#D " << chx
									//	<< " " << diff << std::endl;
								} else {
									//gMtxDisp->Lock();
									gH2HRTDC->Fill(chx, chy);
									//gMtxDisp->UnLock();
								}
							}
						}
					}
				}
			}
		}
	}
#endif

	prescale++;

	return;
}
