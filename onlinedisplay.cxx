/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <fairmq/Device.h>
#include <fairmq/runDevice.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "TSystem.h"
#include "TApplication.h"

#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "FilterHeader.h"
#include "UnpackTdc.h"
#include "KTimer.cxx"

#define USE_THREAD

static TApplication *gApp = new TApplication("App", nullptr, nullptr);

#include "gbookhist.cxx"

namespace bpo = boost::program_options;

struct OnlineDisplay : fair::mq::Device
{
	struct OptionKey {
		static constexpr std::string_view InputChannelName  {"in-chan-name"};
	};

	OnlineDisplay()
	{
		// register a handler for data arriving on "data" channel
		// OnData("in", &OnlineDisplay::HandleData);
		// LOG(info) << "Constructer Input Channel : " << fInputChannelName;
	}

	void InitTask() override
	{
		using opt = OptionKey;

		// Get the fMaxIterations value from the command line options (via fConfig)
		fMaxIterations = fConfig->GetProperty<uint64_t>("max-iterations");
		fInputChannelName  = fConfig->GetValue<std::string>(opt::InputChannelName.data());
		LOG(info) << "InitTask Input Channel : " << fInputChannelName;

		fKt1.SetDuration(100);
		fKt2.SetDuration(2000);
		fKt3.SetDuration(3000);

		#if 0
		#ifdef USE_THREAD
		static bool atFirst = true;
		if (atFirst) {
			TThread thEventCycle("EventCycle", &gEventCycle, NULL);
			thEventCycle.Run();
			atFirst = false;
		}
		#endif
		#endif

		gHistReset();

		//OnData(fInputChannelName, &OnlineDisplay::HandleData);
	}


	#if 0
	bool HandleData(fair::mq::MessagePtr& msg, int)
	{
		LOG(info) << "Received: \""
			<< std::string(static_cast<char*>(msg->GetData()), msg->GetSize())
			<< "\"";

		if (fMaxIterations > 0 && ++fNumIterations >= fMaxIterations) {
			LOG(info) << "Configured maximum number of iterations reached."
				<< " Leaving RUNNING state.";
			return false;
		}

		// return true if you want the handler to be called again
		// (otherwise return false go to the Ready state)
		return true;
	}
	#endif

	bool ConditionalRun() override;
	void PostRun() override;
	
private:
	bool CheckData(fair::mq::MessagePtr& msg);
	void BookData(fair::mq::MessagePtr& msg);

	uint64_t fMaxIterations = 0;
	uint64_t fNumIterations = 0;

	std::string fInputChannelName;

	struct STFBuffer {
		FairMQParts parts;
		std::chrono::steady_clock::time_point start;
	};

	std::unordered_map<uint32_t, std::vector<STFBuffer>> fTFBuffer;
	std::unordered_set<uint64_t> fDiscarded;
	int fNumSource = 0;
	int fFeType = 0;
	uint32_t fFEMId = 0;
	int fPrescale = 10;

	KTimer fKt1;
	KTimer fKt2;
	KTimer fKt3;
};

bool OnlineDisplay::CheckData(fair::mq::MessagePtr& msg)
{
	unsigned int msize = msg->GetSize();
	unsigned char *pdata = reinterpret_cast<unsigned char *>(msg->GetData());
	uint64_t msg_magic = *(reinterpret_cast<uint64_t *>(pdata));

	#if 0
	std::cout << "#Msg TopData(8B): " << std::hex << msg_magic
		<< " Size: " << std::dec << msize << std::endl;
	#endif

	if (msg_magic == Filter::Magic) {
		Filter::Header *pflt
			= reinterpret_cast<Filter::Header *>(pdata);
		std::cout << "#FLT Header "
			<< std::hex << std::setw(16) << std::setfill('0') <<  pflt->magic
			<< " len: " << std::dec << std::setw(8) <<  pflt->length
			<< " N trigs: " << std::setw(8) <<  pflt->numTrigs
			<< " Id: " << std::setw(8) << pflt->workerId
			<< " elapse: " << std::dec <<  pflt->elapseTime
			<< std::endl;

	} else if (msg_magic == TimeFrame::Magic) {
		TimeFrame::Header *ptf
			= reinterpret_cast<TimeFrame::Header *>(pdata);
		std::cout << "#TF Header "
			<< std::hex << std::setw(16) << std::setfill('0') <<  ptf->magic
			<< " id: " << std::setw(8) << std::setfill('0') <<  ptf->timeFrameId
			<< " Nsource: " << std::setw(8) << std::setfill('0') <<  ptf->numSource
			<< " len: " << std::dec <<  ptf->length
			<< std::endl;

	} else if (msg_magic == SubTimeFrame::Magic) {
		SubTimeFrame::Header *pstf
			= reinterpret_cast<SubTimeFrame::Header *>(pdata);
		std::cout << "#STF Header "
			<< std::hex << std::setw(8) << std::setfill('0') <<  pstf->magic
			<< " id: " << std::setw(8) << std::setfill('0') <<  pstf->timeFrameId
			//<< " res: " << std::setw(8) << std::setfill('0') <<  pstf->reserved
			<< " Type: " << std::setw(8) << std::setfill('0') <<  pstf->FEMType
			<< " FE: " << std::setw(8) << std::setfill('0') <<  pstf->FEMId
			<< std::endl << "# "
			<< " len: " << std::dec <<  pstf->length
			<< " nMsg: " << std::dec <<  pstf->numMessages
			<< std::endl << "# "
			<< " Ts: " << std::dec << pstf->time_sec
			<< " Tus: " << std::dec << pstf->time_usec
			<< std::endl;

		fFeType = pstf->FEMType;

	} else {

		#if 1
		for (unsigned int j = 0 ; j < msize ; j += 8) {
			std::cout << "# " << std::setw(8) << j << " : "
				<< std::hex << std::setw(2) << std::setfill('0')
				<< std::setw(2) << static_cast<unsigned int>(pdata[j + 7]) << " "
				<< std::setw(2) << static_cast<unsigned int>(pdata[j + 6]) << " "
				<< std::setw(2) << static_cast<unsigned int>(pdata[j + 5]) << " "
				<< std::setw(2) << static_cast<unsigned int>(pdata[j + 4]) << " "
				<< std::setw(2) << static_cast<unsigned int>(pdata[j + 3]) << " "
				<< std::setw(2) << static_cast<unsigned int>(pdata[j + 2]) << " "
				<< std::setw(2) << static_cast<unsigned int>(pdata[j + 1]) << " "
				<< std::setw(2) << static_cast<unsigned int>(pdata[j + 0]) << " : ";

			if        ((pdata[j + 7] & 0xfc) == (TDC64H::T_TDC << 2)) {
				std::cout << "TDC ";
				uint64_t *dword = reinterpret_cast<uint64_t *>(&(pdata[j]));
				if (fFeType == SubTimeFrame::TDC64H) {
					struct TDC64H::tdc64 tdc;
					TDC64H::Unpack(*dword, &tdc);
					std::cout << "H :"
						<< " CH: " << std::dec << std::setw(3) << tdc.ch
						<< " TDC: " << std::setw(7) << tdc.tdc << std::endl;
				} else
				if (fFeType == SubTimeFrame::TDC64L) {
					struct TDC64L::tdc64 tdc;
					TDC64L::Unpack(*dword, &tdc);
					std::cout << "L :"
						<< " CH: " << std::dec << std::setw(3) << tdc.ch
						<< " TDC: " << std::setw(7) << tdc.tdc << std::endl;
				} else {
					std::cout << "UNKNOWN"<< std::endl;
				}

			} else if ((pdata[j + 7] & 0xfc) == (TDC64H::T_HB << 2)) {
				std::cout << "Hart beat" << std::endl;

				uint64_t *dword = reinterpret_cast<uint64_t *>(&(pdata[j]));
				struct TDC64H::tdc64 tdc;
				TDC64H::Unpack(*dword, &tdc);
				int hbflag = tdc.flag;
			        if (hbflag > 0) {
					if ((hbflag & 0x200) == 0x200)
						std::cout << "#E HB Data lost" << std::endl;
					if ((hbflag & 0x100) == 0x100)
						std::cout << "#E HB Data confiliction" << std::endl;
					if ((hbflag & 0x080) == 0x080)
						std::cout << "#E HB LFN mismatch" << std::endl;
					if ((hbflag & 0x040) == 0x040)
						std::cout << "#E HB GFN mismatch" << std::endl;
				}

			} else if ((pdata[j + 7] & 0xfc) == (TDC64H::T_SPL_START << 2)) {
				std::cout << "SPILL Start" << std::endl;
			} else if ((pdata[j + 7] & 0xfc) == (TDC64H::T_SPL_END << 2)) {
				std::cout << "SPILL End" << std::endl;
			} else {
				std::cout << std::endl;
			}
		}
		std::cout <<  "#----" << std::endl;

		#else
		std::cout << "#Unknown Header " << std::hex << msg_magic << std::endl;
		#endif

	}

	#if 0
	for (unsigned int i = 0 ; i < msize; i++) {
		if ((i % 16) == 0) {
			if (i != 0) std::cout << std::endl;
			std::cout << "#" << std::setw(8) << std::setfill('0')
				<< i << " : ";
		}
		std::cout << " "
			<< std::hex << std::setw(2) << std::setfill('0')
			<< static_cast<unsigned int>(pdata[i]);
	}
	std::cout << std::endl;
	#endif


	return true;
}

void OnlineDisplay::BookData(fair::mq::MessagePtr& msg)
{
	//unsigned int msize = msg->GetSize();
	unsigned char *pdata = reinterpret_cast<unsigned char *>(msg->GetData());
	uint64_t msg_magic = *(reinterpret_cast<uint64_t *>(pdata));

	if (msg_magic == Filter::Magic) {
		#if 0
		Filter::Header *pflt
			= reinterpret_cast<Filter::Header *>(pdata);
		std::cout << "#FLT Header "
			<< std::hex << std::setw(16) << std::setfill('0') <<  pflt->magic
			<< " len: " << std::dec << std::setw(8) <<  pflt->length
			<< " N trigs: " << std::setw(8) <<  pflt->numTrigs
			<< " Id: " << std::setw(8) << pflt->workerId
			<< " elapse: " << std::dec <<  pflt->elapseTime
			<< std::endl;
		#endif

	} else if (msg_magic == TimeFrame::Magic) {
		#if 0
		TimeFrame::Header *ptf
			= reinterpret_cast<TimeFrame::Header *>(pdata);
		std::cout << "#TF Header "
			<< std::hex << std::setw(16) << std::setfill('0') <<  ptf->magic
			<< " id: " << std::setw(8) << std::setfill('0') <<  ptf->timeFrameId
			<< " Nsource: " << std::setw(8) << std::setfill('0') <<  ptf->numSource
			<< " len: " << std::dec <<  ptf->length
			<< std::endl;
		#endif

	} else if (msg_magic == SubTimeFrame::Magic) {
		SubTimeFrame::Header *pstf
			= reinterpret_cast<SubTimeFrame::Header *>(pdata);
		#if 0
		std::cout << "#STF Header "
			<< std::hex << std::setw(8) << std::setfill('0') <<  pstf->magic
			<< " id: " << std::setw(8) << std::setfill('0') <<  pstf->timeFrameId
			//<< " res: " << std::setw(8) << std::setfill('0') <<  pstf->reserved
			<< " Type: " << std::setw(8) << std::setfill('0') <<  pstf->FEMType
			<< " FE: " << std::setw(8) << std::setfill('0') <<  pstf->FEMId
			<< std::endl << "# "
			<< " len: " << std::dec <<  pstf->length
			<< " nMsg: " << std::dec <<  pstf->numMessages
			<< std::endl << "# "
			<< " Ts: " << std::dec << pstf->time_sec
			<< " Tus: " << std::dec << pstf->time_usec
			<< std::endl;
		#endif

		fFEMId = pstf->FEMId;
		fFeType = pstf->FEMType;

	} else {

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

		if        ((pdata[0 + 7] & 0xfc) == (TDC64H::T_TDC << 2)) {

			gHistBook(msg, fFEMId, fFeType);

			#if 0
			std::cout << "TDC ";
			uint64_t *dword = reinterpret_cast<uint64_t *>(&(pdata[0]));
			if (fFeType == SubTimeFrame::TDC64H) {
				struct TDC64H::tdc64 tdc;
				TDC64H::Unpack(*dword, &tdc);
				std::cout << "H :"
					<< " CH: " << std::dec << std::setw(3) << tdc.ch
					<< " TDC: " << std::setw(7) << tdc.tdc << std::endl;
			} else
			if (fFeType == SubTimeFrame::TDC64L) {
				struct TDC64L::tdc64 tdc;
				TDC64L::Unpack(*dword, &tdc);
				std::cout << "L :"
					<< " CH: " << std::dec << std::setw(3) << tdc.ch
					<< " TDC: " << std::setw(7) << tdc.tdc << std::endl;
			} else {
				std::cout << "UNKNOWN"<< std::endl;
			}
			#endif

		} else if ((pdata[0 + 7] & 0xfc) == (TDC64H::T_HB << 2)) {
			#if 0
			std::cout << "Hart beat" << std::endl;
			uint64_t *dword = reinterpret_cast<uint64_t *>(&(pdata[0]));
			struct TDC64H::tdc64 tdc;
			TDC64H::Unpack(*dword, &tdc);
			int hbflag = tdc.flag;
			if (hbflag > 0) {
				if ((hbflag & 0x200) == 0x200)
					std::cout << "#E HB Data lost" << std::endl;
				if ((hbflag & 0x100) == 0x100)
					std::cout << "#E HB Data confiliction" << std::endl;
				if ((hbflag & 0x080) == 0x080)
					std::cout << "#E HB LFN mismatch" << std::endl;
				if ((hbflag & 0x040) == 0x040)
					std::cout << "#E HB GFN mismatch" << std::endl;
			}
			#endif
		} else if ((pdata[0 + 7] & 0xfc) == (TDC64H::T_SPL_START << 2)) {
			#if 0
			std::cout << "SPILL Start" << std::endl;
			#endif
		} else if ((pdata[0 + 7] & 0xfc) == (TDC64H::T_SPL_END << 2)) {
			#if 0
			std::cout << "SPILL End" << std::endl;
			#endif
		} else {
			#if 0
			std::cout << std::endl;
			#endif
		}
	}

	return;
}

bool OnlineDisplay::ConditionalRun()
{

	//Receive
	FairMQParts inParts;
	if (Receive(inParts, fInputChannelName, 0, 1000) > 0) {
		//assert(inParts.Size() >= 2);

		static std::chrono::system_clock::time_point start
			= std::chrono::system_clock::now();
		static uint64_t counts = 0;
		static double freq = 0;

		const double kDURA = 10;
		auto now = std::chrono::system_clock::now();
		auto elapse = std::chrono::duration_cast<std::chrono::milliseconds>
			(now - start).count();
		if (elapse > (1000 * kDURA)) {
			freq = static_cast<double>(counts)
				/ static_cast<double>(elapse) * 1000;
			counts = 0;
			start = std::chrono::system_clock::now();
		}

		if (fKt3.Check()) {
			std::cout << "Nmsg: " << std::dec << inParts.Size();
			std::cout << "  Freq: " << freq << " el " << elapse
				<< " c " << counts  << std::endl;
		}

		#if 0
		static std::chrono::system_clock::time_point last;
		std::cout << " Elapsed time:"
			<< std::chrono::duration_cast<std::chrono::microseconds>
			(now - last).count();
		last = now;
		std::cout << std::endl;
		#endif

		#if 0
		for(auto& vmsg : inParts) CheckData(vmsg);
		#else
		if ((counts % 100) == 0) std::cout << "." << std::flush;
		if ((counts % fPrescale) == 0) for(auto& vmsg : inParts) BookData(vmsg);
		#endif

		#if 0
		auto tfHeader = reinterpret_cast<TimeFrame::Header*>(inParts.At(0)->GetData());

		auto stfHeader = reinterpret_cast<SubTimeFrame::Header*>(inParts.At(0)->GetData());
		auto stfId     = stfHeader->timeFrameId;

		if (fDiscarded.find(stfId) == fDiscarded.end()) {
			// accumulate sub time frame with same STF ID
			if (fTFBuffer.find(stfId) == fTFBuffer.end()) {
				fTFBuffer[stfId].reserve(fNumSource);
			}
			fTFBuffer[stfId].emplace_back(
				STFBuffer {std::move(inParts),
				std::chrono::steady_clock::now()});
		} else {
			// if received ID has been previously discarded.
			LOG(warn) << "Received part from an already discarded timeframe with id " << stfId;
		}
		#endif

		counts++;
	}

	#ifndef USE_THREAD
	if (fKt1.Check()) gSystem->ProcessEvents();
	#endif
	if (fKt2.Check()) gHistDraw();

	return true;
}

void OnlineDisplay::PostRun()
{
	LOG(info) << "Post Run";
	return;
}


#if 0
bool OnlineDisplay::HandleData(fair::mq::MessagePtr& msg, int val)
{
	(void)val;
	#if 0
	LOG(info) << "Received: \""
		<< std::string(static_cast<char*>(msg->GetData()), msg->GetSize())
		<< "\"";
	#endif

	#if 0
	LOG(info) << "Received: " << msg->GetSize() << " : " << val;
	#endif

	if (fMaxIterations > 0 && ++fNumIterations >= fMaxIterations) {
		LOG(info) << "Configured maximum number of iterations reached."
			<< " Leaving RUNNING state.";
		return false;
	}

	CheckData(msg);
	

	// return true if you want the handler to be called again
	// (otherwise return false go to the Ready state)
	return true;
}
#endif


void addCustomOptions(bpo::options_description& options)
{
	using opt = OnlineDisplay::OptionKey;

	options.add_options()
		("max-iterations", bpo::value<uint64_t>()->default_value(0),
		"Maximum number of iterations of Run/ConditionalRun/OnData (0 - infinite)")
		(opt::InputChannelName.data(),
			bpo::value<std::string>()->default_value("in"),
			"Name of the input channel");

}


std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/)
{
	gHistInit();

	#ifdef USE_THREAD
	TThread thEventCycle("EventCycle", &gEventCycle, NULL);
	thEventCycle.Run();
	#endif

	return std::make_unique<OnlineDisplay>();
}
