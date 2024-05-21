/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *									      *
 *	      This software is distributed under the terms of the	     *
 *	      GNU Lesser General Public Licence (LGPL) version 3,	     *
 *		  copied verbatim in the file "LICENSE"		       *
 ********************************************************************************/

#include <fairmq/Device.h>
#include <fairmq/runDevice.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <unordered_set>

//#include "HulStrTdcData.h"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "FilterHeader.h"
#include "HeartbeatFrameHeader.h"
#include "UnpackTdc.h"
#include "KTimer.cxx"

namespace bpo = boost::program_options;

struct TFdump : fair::mq::Device
{
	struct OptionKey {
		static constexpr std::string_view InputChannelName  {"in-chan-name"};
		static constexpr std::string_view ShrinkMode	{"shrink"};
		static constexpr std::string_view Interval	  {"interval"};
	};

	TFdump()
	{
		// register a handler for data arriving on "data" channel
		//OnData("in", &TFdump::HandleData);
		//LOG(info) << "Constructer Input Channel : " << fInputChannelName;
	}

	void InitTask() override
	{
		using opt = OptionKey;

		// Get the fMaxIterations value from the command line options (via fConfig)
		fMaxIterations = fConfig->GetProperty<uint64_t>("max-iterations");
		fInputChannelName  = fConfig->GetValue<std::string>(opt::InputChannelName.data());
		LOG(info) << "Input Channel : " << fInputChannelName;

		auto sShrinkMode = fConfig->GetProperty<std::string>(opt::ShrinkMode.data());
		fIsShrink = (sShrinkMode == "true") ? true : false;
		LOG(info) << "Shrink Mode : " << fIsShrink;
		fInterval = std::stoi(fConfig->GetProperty<std::string>(opt::Interval.data()));
		fInterval = fInterval * 1000;
		LOG(info) << "Interval : " << fInterval;

		fKt1 = new KTimer(fInterval);

		//OnData(fInputChannelName, &TFdump::HandleData);
	}

	bool CheckData(fair::mq::MessagePtr& msg);

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
	int fFE_type = 0;

	bool fIsShrink;
	KTimer *fKt1;
	int fInterval = 0;
};

#if 1
bool TFdump::CheckData(fair::mq::MessagePtr& msg)
{
	unsigned int msize = msg->GetSize();
	unsigned char *pdata = reinterpret_cast<unsigned char *>(msg->GetData());
	uint64_t msg_magic = *(reinterpret_cast<uint64_t *>(pdata));

	//std::cout << "#D Msize: " << msize << std::endl;
	if (msize <= 0) {
		//std::cout << "#D size Zero, Pointer of MAGIC : 0x" << std::hex <<
		//	reinterpret_cast<unsigned long>(pdata) << std::endl;
		return false;
	} else {
		//std::cout << "#D Pointer of MAGIC : 0x" << std::hex <<
		//	reinterpret_cast<unsigned long>(pdata) << std::endl;
	}

	std::cout << std::dec;

	#if 0
	std::cout << "#Msg TopData(8B): " << std::hex << msg_magic
		<< " Size: " << std::dec << msize << std::endl;
	#endif

	static int tf_nstf = 0;
	static int ac_nstf = 0;
	static int ac_nhbf = 0;

	constexpr int DM_OTHER = 0;
	constexpr int DM_TDC   = 1;
	constexpr int DM_FLT   = 16;
	static int data_mode   = 0;

	if (msg_magic == Filter::MAGIC) {
		if (fIsShrink) {
			std::cout << "F";
		} else {
			Filter::Header *pflt
			= reinterpret_cast<Filter::Header *>(pdata);
			std::cout << "#FLT Header (" << std::dec << ac_nstf << ") "
			<< std::hex << std::setw(16) << std::setfill('0') <<  pflt->magic
			<< " len: " << std::dec << std::setw(6) << std::setfill(' ') <<  pflt->length
			<< " tId: " << std::setw(6) << pflt->timeFrameId
			<< " Ntrigs: " << std::setw(6) << pflt->numTrigs
			<< " wId: " << std::setw(6) << pflt->workerId
			<< " nMsg: " << std::dec <<  pflt->numMessages
			<< " elapse: " << std::dec <<  pflt->elapseTime
			<< std::endl;
		}
		data_mode = DM_FLT;

	} else if (msg_magic == Filter::TDC_MAGIC) {
		Filter::TrgTimeHeader *ptrg
			= reinterpret_cast<Filter::TrgTimeHeader *>(pdata);

		if (fIsShrink) {
			std::cout << "t";
		} else {
			std::cout << "#FLT Trigger len: " << std::dec << ptrg->length
				<< " Msize: " << msize << std::endl;
			if (msize > sizeof(Filter::TrgTimeHeader) ) {
				for (unsigned int j = 0 ; j < msize ; j ++) {
					if ((j % 16) == 0) {
						if (j != 0) std::cout << std::endl;
						std::cout << "#FLT DUMP " << std::hex << std::setw(8) 
							<< std::setfill('0') << j << " : ";
					}
					std::cout << " " << std::setw(2) << std::setfill('0')
						<< static_cast<unsigned int>(pdata[j]);
				}
				std::cout << std::endl;
			}
		}

	} else if ((msg_magic == TimeFrame::MAGIC)
			|| (msg_magic == TimeFrame::v1::MAGIC)) {
		TimeFrame::Header *ptf
			= reinterpret_cast<TimeFrame::Header *>(pdata);
		if (fIsShrink) {
			std::cout << "T";
		} else {
			std::cout << "#TF Header "
			<< std::hex << std::setw(16) << std::setfill('0') <<  ptf->magic
			<< " id: " << std::setw(8) << std::setfill('0') <<  ptf->timeFrameId
			<< " Nsource: " << std::setw(8) << std::setfill('0') <<  ptf->numSource
			<< " len: " << std::dec <<  ptf->length
			<< std::endl;
		}

		if (tf_nstf != ac_nstf) {
			std::cout << "### Wrong Nsource: TFH: " << std::dec << tf_nstf
				<< " Real: " << ac_nstf << std::endl;
		}
		tf_nstf = ptf->numSource;
		ac_nstf = 0;
		data_mode = DM_OTHER;

	} else if ((msg_magic == SubTimeFrame::MAGIC)
			|| (msg_magic == SubTimeFrame::v1::MAGIC)) {
		SubTimeFrame::Header *pstf
			= reinterpret_cast<SubTimeFrame::Header *>(pdata);

		if (fIsShrink) {
			std::cout << "S";
		} else {
			std::cout << "#STF Header (" << std::dec << ac_nstf << ") "
			<< std::hex << std::setw(8) << std::setfill('0') <<  pstf->magic
			<< " id: " << std::setw(8) << std::setfill('0') <<  pstf->timeFrameId
			//<< " res: " << std::setw(8) << std::setfill('0') <<  pstf->reserved
			<< " Type: " << std::setw(8) << std::setfill('0') <<  pstf->femType
			<< " FE: " << std::setw(8) << std::setfill('0') <<  pstf->femId
			<< std::endl << "# "
			<< " len: " << std::dec <<  pstf->length
			<< " nMsg: " << std::dec <<  pstf->numMessages
			<< std::endl << "# "
			<< " Ts: " << std::dec << pstf->timeSec
			<< " Tus: " << std::dec << pstf->timeUSec
			<< std::endl;

			fFE_type = pstf->femType;
		}

		if ((pstf->femType) == SubTimeFrame::TDC64H) {
			data_mode = DM_TDC;
			std::cout << "#DM TDC(H)" << std::endl;
		} else
		if ((pstf->femType) == SubTimeFrame::TDC64L) {
			data_mode = DM_TDC;
			std::cout << "#DM TDC(L)" << std::endl;
		}
		ac_nstf++;
		ac_nhbf = 0;


	//} else if (data_mode == DM_TDC) {
	} else if (msg_magic == HeartbeatFrame::MAGIC) {
		//std::cout << "#D DMODE: TDC" << std::endl;
		if (fIsShrink) {
			//std::cout << "#Unknown Header " << std::hex << msg_magic << std::endl;
			std::cout << "H";
		} else {
			std::cout << "#D HBF" << std::endl;

			HeartbeatFrame::Header *phbf
				= reinterpret_cast<HeartbeatFrame::Header *>(pdata);

			std::cout << "#HBF Header (" << std::dec << ac_nhbf << ") "
				<< std::hex << std::setw(8) << std::setfill('0') <<  phbf->magic
				<< " len: " << std::setw(8) << std::setfill('0') <<  phbf->length
				<< std::endl;
			ac_nhbf++;


			//std::cout << "#DMODE: TDC" << std::endl;
			for (unsigned int j = 16 ; j < msize ; j += 8) {
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

				if  (  ((pdata[j + 7] & 0xfc) == (TDC64H::T_TDC << 2))
				    || ((pdata[j + 7] & 0xfc) == (TDC64H::T_TDC_T << 2))) {
					std::cout << "TDC ";
					uint64_t *dword = reinterpret_cast<uint64_t *>(&(pdata[j]));
					if (fFE_type == SubTimeFrame::TDC64H) {
						struct TDC64H::tdc64 tdc;
						TDC64H::Unpack(*dword, &tdc);
						std::cout << "H :"
						<< " CH: " << std::dec << std::setw(3) << tdc.ch
						<< " TDC: " << std::setw(7) << tdc.tdc << std::endl;
					} else
					if (fFE_type == SubTimeFrame::TDC64L) {
						struct TDC64L::tdc64 tdc;
						TDC64L::Unpack(*dword, &tdc);
						std::cout << "L :"
						<< " CH: " << std::dec << std::setw(3) << tdc.ch
						<< " TDC: " << std::setw(7) << tdc.tdc << std::endl;
					} else
					if (fFE_type == SubTimeFrame::TDC64H_V3) {
						struct TDC64H_V3::tdc64 tdc;
						TDC64H_V3::Unpack(*dword, &tdc);
						std::cout << "H :"
						<< " CH: " << std::dec << std::setw(3) << tdc.ch
						<< " TDC: " << std::setw(7) << tdc.tdc << std::endl;
					} else
					if (fFE_type == SubTimeFrame::TDC64L_V3) {
						struct TDC64L_V3::tdc64 tdc;
						TDC64L_V3::Unpack(*dword, &tdc);
						std::cout << "L :"
						<< " CH: " << std::dec << std::setw(3) << tdc.ch
						<< " TDC: " << std::setw(7) << tdc.tdc << std::endl;
					} else {
						std::cout << "UNKNOWN"<< std::endl;
					}

				} else if (
					   (pdata[j + 7] & 0xfc) == (TDC64H_V3::T_HB  << 2)
					|| (pdata[j + 7] & 0xfc) == (TDC64H_V3::T_HB1 << 2)
					|| (pdata[j + 7] & 0xfc) == (TDC64H_V3::T_HB2 << 2)) {
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
						if ((hbflag & 0x020) == 0x020)
							std::cout << "#W I throttling 1" << std::endl;
						if ((hbflag & 0x010) == 0x010)
							std::cout << "#W I Throttling 2" << std::endl;
						if ((hbflag & 0x008) == 0x008)
							std::cout << "#W O Throttling" << std::endl;
						if ((hbflag & 0x004) == 0x004)
							std::cout << "#W HBF Throttling" << std::endl;
					}

				} else if ((pdata[j + 7] & 0xfc) == (TDC64H::T_SPL_START << 2)) {
					std::cout << "SPILL Start" << std::endl;
				} else if ((pdata[j + 7] & 0xfc) == (TDC64H::T_SPL_END << 2)) {
					std::cout << "SPILL End" << std::endl;
				} else {
					std::cout << "UNKNOWN" << std::endl;
				}
			}
			std::cout <<  "#----" << std::endl;
		}
	} else if (data_mode == DM_OTHER) {
		std::cout << "#E Unkown data message" << std::endl;
		std::cout << "#D " << reinterpret_cast<char *>(&msg_magic)
			<< " 0x" << std::hex << msg_magic << std::dec << std::endl;

		#if 1
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

	}

	return true;
}

#else

bool TFdump::CheckData(fair::mq::MessagePtr& msg)
{
	//unsigned int msize = msg->GetSize();
	unsigned char *pdata = reinterpret_cast<unsigned char *>(msg->GetData());
	uint64_t msg_magic = *(reinterpret_cast<uint64_t *>(pdata));
	int msize = msg->GetSize();

	static TimeFrame::Header ltimeframe;
	static SubTimeFrame::Header lsubtimeframe;
	static int nsubtimeframe = 0;

	#if 0
	std::cout << "#Msg MAGIC: " << std::hex << msg_magic
		<< " Size: " << std::dec << msize << std::endl;
	#endif

	if (msg_magic == TimeFrame::MAGIC) {
		TimeFrame::Header *ptf
			= reinterpret_cast<TimeFrame::Header *>(pdata);
		ltimeframe.magic = ptf->magic;
		ltimeframe.timeFrameId = ptf->timeFrameId;
		ltimeframe.numSource = ptf->numSource;
		ltimeframe.length = ptf->length;

		std::cout << "#TF Header "
			<< std::hex << std::setw(16) << std::setfill('0') <<  ptf->magic
			<< " id: " << std::setw(8) << std::setfill('0') <<  ptf->timeFrameId
			<< " Nsource: " << std::setw(8) << std::setfill('0') <<  ptf->numSource
			<< " len: " << std::dec <<  ptf->length
			<< std::endl;

	} else if (msg_magic == SubTimeFrame::MAGIC) {
		SubTimeFrame::Header *pstf
			= reinterpret_cast<SubTimeFrame::Header *>(pdata);
		if (nsubtimeframe != 0) {
			std::cout << "#E lost SubTimeFrame Msg" << std::endl;
		}

		lsubtimeframe.magic = pstf->magic;
		lsubtimeframe.timeFrameId = pstf->timeFrameId;
		lsubtimeframe.femId = pstf->femId;
		lsubtimeframe.length = pstf->length;
		lsubtimeframe.numMessages = pstf->numMessages;
		nsubtimeframe = lsubtimeframe.numMessages - 1;

		std::cout << "#STF Header "
			<< std::hex << std::setw(8) << std::setfill('0') <<  pstf->magic
			<< " id: " << std::setw(8) << std::setfill('0') <<  pstf->timeFrameId
			<< " FE: " << std::setw(8) << std::setfill('0') <<  pstf->femId
			<< " len: " << std::dec <<  pstf->length
			<< " nMsg: " << std::dec <<  pstf->numMessages
			<< std::endl;

	} else {
		if (nsubtimeframe > 0) {
			std::cout << "#TDC body : " << msize << std::endl;

			nsubtimeframe--;
		} else {
			std::cout << "#Unknown Header " << std::hex << msg_magic << std::endl;
		}
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
#endif

bool TFdump::ConditionalRun()
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

		#if 0
		static std::chrono::system_clock::time_point last;
		std::cout << " Elapsed time:"
			<< std::chrono::duration_cast<std::chrono::microseconds>
			(now - last).count();
		last = now;
		std::cout << std::endl;
		#endif

		if ((fInterval == 0) || (fKt1->Check())) {
			std::cout << "Nmsg: " << std::dec << inParts.Size();
			std::cout << "  Freq: " << freq << "Hz  el: " << elapse
				<< " C: " << counts  << std::endl;
			for(auto& vmsg : inParts) CheckData(vmsg);
		}

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


	return true;
}

void TFdump::PostRun()
{
	LOG(info) << "Post Run";
	return;
}


#if 0
bool TFdump::HandleData(fair::mq::MessagePtr& msg, int val)
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
	using opt = TFdump::OptionKey;

	options.add_options()
		("max-iterations", bpo::value<uint64_t>()->default_value(0),
			"Maximum number of iterations of Run/ConditionalRun/OnData (0 - infinite)")
		(opt::InputChannelName.data(), bpo::value<std::string>()->default_value("in"),
			"Name of the input channel")
		(opt::ShrinkMode.data(), bpo::value<std::string>()->default_value("false"),
			"Shrink mode flag")
		(opt::Interval.data(), bpo::value<std::string>()->default_value("0"),
			"Display interval period (s)")
	;
}


std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/)
{
	return std::make_unique<TFdump>();
}
