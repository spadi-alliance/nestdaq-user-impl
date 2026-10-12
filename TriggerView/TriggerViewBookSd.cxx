/*
 *
 */

#include "uhbook.cxx"
#include "RedisDataStore.h"
#include "Slowdashify.h"

#include "FilterHeader.h"
#include "GetTriggerInfo.cxx"

static RedisDataStore *gDB;
static std::string gDbURL;
static std::string gDevName;
static std::string gNamePrefix;

static UH1Book *gHHRTDC[2];
static UH2Book *gH2HRTDC;
static UH1Book *gHDiff;

static UH1Book *gHTrig;
static std::vector<UH2Book *> gH2TrigWindow;
static std::vector<UH2Book *> gH2TrigWindowOne;
static std::vector<uint32_t> gTrigType;
static UH1Book *gHElapse;
static UH1Book *gHNTrig;

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


//TMutex *gMtxDisp;

void gEventCycle(void *arg = NULL)
{
#if 0
	(void)arg;

	while (true) {
		//gMtxDisp->Lock();
		gSystem->ProcessEvents();
		//gMtxDisp->UnLock();
		TThread::Sleep(0, 100'000'000);
	}
#endif
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

	gHHRTDC[0] = new UH1Book("HR-TDC0", 64, 0, 64);
	gHHRTDC[1] = new UH1Book("HR-TDC1", 64, 0, 64);
	gH2HRTDC = new UH2Book("HR-TDC 2D", 64, 0, 64, 64, 0, 64);
	gHDiff = new UH1Book("Diff", 1000, 0, 1000000);

	gHTrig = new UH1Book("Trigger", 1000, 0, 150000);
	gHElapse = new UH1Book("Elapse Time (ms)", 500, 0, 20000);
	gHNTrig = new UH1Book("Number of Trigger (/STF)", 500, 0, 1000);

	for (int i = 0 ; i < gMaxTrigHist ; i++) {
		std::ostringstream noss, toss;
		noss << "TWIN" << std::setw(3) << std::setfill('0') << i;
		toss << "Trigger Window " << std::setw(3) << std::setfill('0') << i;
		gH2TrigWindow.emplace_back(new UH2Book(toss.str().c_str(),
			1000, -500., 500., 32, 0., 32.));

		std::ostringstream nooss, tooss;
		nooss << "TWONE" << std::setw(3) << std::setfill('0') << i;
		tooss << "Trigger Window One event " << std::setw(3) << std::setfill('0') << i;
		gH2TrigWindowOne.emplace_back(new UH2Book(tooss.str().c_str(),
			1000, -500., 500., 32, 0., 32.));
	}

	if (gDevName.size() < 3) {
		gDB = new RedisDataStore("tcp://127.0.0.1:6379/3");
	} else {
		gDB = new RedisDataStore(gDbURL);
	}

	return;
}

void gHistInit(std::string &url, std::string &name, int num)
{
	gDbURL = url;
	gDevName = name;
	//gDevNumber = num;

	std::ostringstream oss;
	oss << num;
	gNamePrefix = "dqm:" + name + "-" + oss.str();

	gHistInit();

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

#if 0
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
#endif

	return;
}

void gUHistSlowdashify(UH1Book &hist)
{
	std::string hname = gNamePrefix + ":" + hist.GetTitle();
	gDB->write(hname.c_str(), Slowdashify(hist));

	return;
}

void gUHistSlowdashify(UH1Book *hist)
{
	std::string hname = gNamePrefix + ":" + hist->GetTitle();
	gDB->write(hname.c_str(), Slowdashify(*hist));

	return;
}

void gUHistSlowdashify(UH2Book &hist)
{
	std::string hname = gNamePrefix + ":" + hist.GetTitle();
	gDB->write(hname.c_str(), Slowdashify(hist));

	return;
}

void gUHistSlowdashify(UH2Book *hist)
{
	std::string hname = gNamePrefix + ":" + hist->GetTitle();
	gDB->write(hname.c_str(), Slowdashify(*hist));

	return;
}



void gHistDraw()
{

#if 0
	for (unsigned int i = 0 ; i < 4 + (2 * gTrigType.size()) ; i++) {
		gCan1->cd(i + 1)->Modified();
	}

	//gMtxDisp->Lock();
	gCan1->Update();
	//gMtxDisp->UnLock();
#endif
	
#if 0
	gHElapse->Draw();
	gHNTrig->Draw();

	for (unsigned int i = 0 ; i < gTrigType.size() ; i++) {
		gH2TrigWindowOne[i]->Draw("box");
		gH2TrigWindow[i]->Draw("col2");
	}
#endif

	gUHistSlowdashify(gHElapse);
	gUHistSlowdashify(gHNTrig);

	for (unsigned int i = 0 ; i < gTrigType.size() ; i++) {
		gUHistSlowdashify(gH2TrigWindowOne[i]);
		gUHistSlowdashify(gH2TrigWindow[i]);
	}

	
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
	
	gHElapse->Fill(static_cast<double>(pflt->elapseTime));
	gHNTrig->Fill(static_cast<double>(pflt->numTrigs));

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
		gHTrig->Fill(static_cast<double>(t->time));
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
					//if (is_draw) gCan1->cd(3 + 2 * itt)->Modified();
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
