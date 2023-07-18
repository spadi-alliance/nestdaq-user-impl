/*
 *
 */

#include "TCanvas.h"
#include "TThread.h"
#include "TMutex.h"
#include "TH1F.h"
#include "TH2F.h"

static TCanvas *gCan1 = new TCanvas ("DISPLAY", "Display");
static TCanvas *gCan2 = new TCanvas ("DISPLAY2", "Display2");
static TH1F *gHHRTDC[2];
static TH2F *gH2HRTDC;
static TH1F *gHDiff;

static TH1F *gHBDC[2];
static TH1F *gHSFT[3];
static TH1F *gHKLDC[2];

static std::vector< std::vector<int> > gIpBDC = {
	{161, 162, 163, 164},
	{166, 166, 166, 168}};
static std::vector<int> gIpSFT = {173, 174, 175};
static std::vector< std::vector<int> > gIpKLDC = {
	{173, 174}, {175, 176}};

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

void gHistInit()
{
	//gMtxDisp = new TMutex();

	gHHRTDC[0] = new TH1F("HRTDC0", "HR-TDC0", 64, 0, 64);
	gHHRTDC[1] = new TH1F("HRTDC1", "HR-TDC1", 64, 0, 64);
	gH2HRTDC = new TH2F("HRTDC02D", "HR-TDC 2D", 64, 0, 64, 64, 0, 64);
	gHDiff = new TH1F("DIFF", "Diff", 1000, 0, 1000000);

	gCan1->Divide(2, 2);
	gCan1->cd(1)->SetLogy();
	gCan1->cd(1); gHHRTDC[0]->Draw();
	gCan1->cd(2)->SetLogy();
	gCan1->cd(2); gHHRTDC[1]->Draw();
	gCan1->cd(3)->SetLogz();
	gCan1->cd(3); gH2HRTDC->Draw("col2");
	gCan1->cd(4); gHDiff->Draw();

	gHBDC[0] = new TH1F("BDC1", "BDC1", 512, 0, 512);
	gHBDC[1] = new TH1F("BDC2", "BDC2", 512, 0, 512);
	gHSFT[0] = new TH1F("SFT1", "SFT", 128, 0, 128);
	gHSFT[1] = new TH1F("SFT2", "SFT", 128, 0, 128);
	gHSFT[2] = new TH1F("SFT3", "SFT", 128, 0, 128);
	gHKLDC[0] = new TH1F("KLDC1", "KLDC1", 256, 0, 256);
	gHKLDC[1] = new TH1F("KLDC2", "KLDC2", 256, 0, 256);

	gCan2->Divide(2, 4);
	gCan2->cd(1); gHBDC[0]->Draw();
	gCan2->cd(2); gHBDC[1]->Draw();
	gCan2->cd(3); gHSFT[0]->Draw();
	gCan2->cd(4); gHSFT[1]->Draw();
	gCan2->cd(5); gHSFT[2]->Draw();
	gCan2->cd(7); gHKLDC[0]->Draw();
	gCan2->cd(8); gHKLDC[1]->Draw();

	return;
}

void gHistDraw()
{

	for (int i = 0 ; i < 4 ; i++) gCan1->cd(i + 1)->Modified();
	for (int i = 0 ; i < 5 ; i++) gCan2->cd(i + 1)->Modified();
	for (int i = 0 ; i < 2 ; i++) gCan2->cd(i + 7)->Modified();

	//gMtxDisp->Lock();
	gCan1->Update();
	gCan2->Update();
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

	gHBDC[0]->Reset();
	gHBDC[1]->Reset();
	gHSFT[0]->Reset();
	gHSFT[1]->Reset();
	gHSFT[2]->Reset();
	gHKLDC[0]->Reset();
	gHKLDC[1]->Reset();

	//gMtxDisp->UnLock();

	return;
}


void gHistBook(fair::mq::MessagePtr& msg, int id, int type)
{
	unsigned int msize = msg->GetSize();
	unsigned char *pdata = reinterpret_cast<unsigned char *>(msg->GetData());
	//uint64_t msg_magic = *(reinterpret_cast<uint64_t *>(pdata));

	static unsigned int prescale = 0;


	#if 0
	std::cout << "# " << std::setw(8) << j << " : "
		<< std::hex << std::setw(2) << std::setfill('0')
		<< std::setw(2) << static_cast<unsigned int>(pdata[0 + 7]) << " "
		<< std::setw(2) << static_cast<unsigned int>(pdata[0 + 6]) << " "
		<< std::setw(2) << static_cast<unsigned int>(pdata[0 + 5]) << " "
		<< std::setw(2) << static_cast<unsigned int>(pdata[0 + 4]) << " "
		<< std::setw(2) << static_cast<unsigned int>(pdata[0 + 3]) << " "
		<< std::setw(2) << static_cast<unsigned int>(pdata[0 + 2]) << " "
		<< std::setw(2) << static_cast<unsigned int>(pdata[0 + 1]) << " "
		<< std::setw(2) << static_cast<unsigned int>(pdata[0 + 0]) << " : ";
	#endif
	//std::cout << " " << (id & 0xff);

	for (size_t i = 0 ; i < msize ; i += sizeof(uint64_t)) {
		if ((id & 0x000000ff) == 169) {
			if ((pdata[i + 7] & 0xfc) == (TDC64H::T_TDC << 2)) {

				uint64_t *dword = reinterpret_cast<uint64_t *>(&(pdata[i]));
				if (type == SubTimeFrame::TDC64H) {
					struct tdc64 tdc;
					TDC64H::Unpack(*dword, &tdc);
					//gMtxDisp->Lock();
					gHHRTDC[0]->Fill(tdc.ch);
					//gMtxDisp->UnLock();
					#if 0
					std::cout << "FEM: " << (id & 0xff) << " TDC ";
					std::cout << "H :"
						<< " CH: " << std::dec << std::setw(3) << tdc.ch
						<< " TDC: " << std::setw(7) << tdc.tdc << std::endl;
					#else
					std::cout << "*" << std::flush;
					#endif
				} else
				if (type == SubTimeFrame::TDC64L) {
					struct tdc64 tdc;
					TDC64L::Unpack(*dword, &tdc);
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
			if ((pdata[i + 7] & 0xfc) == (TDC64H::T_TDC << 2)) {
	
				uint64_t *dword = reinterpret_cast<uint64_t *>(&(pdata[i]));
				if (type == SubTimeFrame::TDC64H) {
					struct tdc64 tdc;
					TDC64H::Unpack(*dword, &tdc);
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
	}

	uint64_t *pdata64 = reinterpret_cast<uint64_t *>(msg->GetData());
	if ((id & 0x000000ff) == 169) {
		for (size_t i = 0 ; i < (msize / sizeof(uint64_t)) ; i++) {
			if (((pdata64[i] & 0xfc00'0000'0000'0000) >> 58) == TDC64H::T_TDC) {
				struct tdc64 tdc;
				TDC64H::Unpack(pdata64[i], &tdc);
				auto chx = tdc.ch;
				auto tdcx = tdc.tdc;
				for (size_t j = 0 ; j < (msize / sizeof(uint64_t)) ; j++) {
					if (((pdata64[j] & 0xfc00'0000'0000'0000) >> 58) == TDC64H::T_TDC) {
						TDC64H::Unpack(pdata64[j], &tdc);
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


	if ((prescale % 10) == 0) {

	int offset = 0;
	TH1F *hist;
	bool isBook = false;
	switch (id & 0x000000ff) {
		case 161 : hist = gHBDC[0] ; offset =   0; isBook = true; break;
		case 162 : hist = gHBDC[0] ; offset = 128; isBook = true; break;
		case 163 : hist = gHBDC[0] ; offset = 256; isBook = true; break;
		case 164 : hist = gHBDC[0] ; offset = 384; isBook = true; break;

		case 165 : hist = gHBDC[1] ; offset =   0; isBook = true; break;
		case 166 : hist = gHBDC[1] ; offset = 128; isBook = true; break;
		case 167 : hist = gHBDC[1] ; offset = 256; isBook = true; break;
		case 168 : hist = gHBDC[1] ; offset = 384; isBook = true; break;

		case 173 : hist = gHSFT[0] ; offset =   0; isBook = true; break;
		case 174 : hist = gHSFT[1] ; offset =   0; isBook = true; break;
		case 175 : hist = gHSFT[2] ; offset =   0; isBook = true; break;

		case 176 : hist = gHKLDC[0] ; offset =   0; isBook = true; break;
		case 177 : hist = gHKLDC[0] ; offset = 128; isBook = true; break;
		case 178 : hist = gHKLDC[1] ; offset =   0; isBook = true; break;
		case 179 : hist = gHKLDC[1] ; offset = 128; isBook = true; break;

		default : isBook = false; break;
	}
	//std::cout << "#D id:" << (id & 0x000000ff) <<  " " << isBook << std::endl;

	if (isBook) {
		for (size_t i = 0 ; i < (msize / sizeof(uint64_t)) ; i++) {
			if (((pdata64[i] & 0xfc00'0000'0000'0000) >> 58) == TDC64L::T_TDC) {
				if (type == SubTimeFrame::TDC64L) {
					struct tdc64 tdc;
					TDC64L::Unpack(pdata64[i], &tdc);
					//gMtxDisp->Lock();
					hist->Fill(tdc.ch + offset);
					//gMtxDisp->UnLock();
				}
			}
		}
	}

	}

	prescale++;

	return ;
}
