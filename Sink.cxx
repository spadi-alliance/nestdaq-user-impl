#include <chrono>
#include <thread>
#include <vector>
#include <numeric>
#include <fairmq/runFairMQDevice.h>
#include <string>

#include "Sink.h"

static constexpr std::string_view MyClass{"Sink"};

namespace bpo = boost::program_options;

//_____________________________________________________________________________
void addCustomOptions(bpo::options_description &options)
{
    using opt = Sink::OptionKey;
    options.add_options()
    (opt::InputChannelName.data(), bpo::value<std::string>()->default_value(opt::InputChannelName.data()), "Name of input channel\n")
    //
    (opt::Multipart.data(), bpo::value<std::string>()->default_value("true"), "Handle multipart message\n");
}

//_____________________________________________________________________________
FairMQDevicePtr getDevice(const FairMQProgOptions &)
{
    return new Sink;
}

//_____________________________________________________________________________
void PrintConfig(const fair::mq::ProgOptions* config, std::string_view name, std::string_view funcname)
{
    auto c = config->GetPropertiesAsStringStartingWith(name.data());
    std::ostringstream ss;
    ss << funcname << "\n\t " << name << "\n";
    for (const auto &[k, v] : c) {
        ss << "\t key = " << k << ", value = " << v << "\n";
    }
    LOG(debug) << ss.str();
}

//_____________________________________________________________________________
bool Sink::HandleData(FairMQMessagePtr &msg, int index)
{
    const auto ptr = reinterpret_cast<unsigned char*>(msg->GetData());
    //  std::string s(ptr, ptr+msg->GetSize());
    //  LOG(debug) << __FUNCTION__ << " received = " << s << " [" << index << "] " << fNumMessages;

    LOG(debug) << __FUNCTION__ << " received = " << " [" << index << "] " << fNumMessages;

    for(size_t i = 0; i < msg->GetSize(); i++) {
        printf("%02x", ptr[i]);
        if( ( (i+1)%8 )==0 )
            printf("\n");
    }

    ++fNumMessages;
    return true;
}

//_____________________________________________________________________________
uint64_t Sink::TotalLength(const FairMQParts& parts)
{
    auto & c = const_cast<FairMQParts&>(parts);

    return std::accumulate(c.begin(), c.end(), static_cast<uint64_t>(0),
    [](uint64_t init, auto& m) -> uint64_t {
        return (!m) ? init : init + m->GetSize();
    });
}

//_____________________________________________________________________________
bool Sink::HandleMultipartData(FairMQParts &msgParts, int index)
{
    bool fSave = true;

    auto length = TotalLength(msgParts);
    std::vector<char> rawbuf;
    rawbuf.reserve(length);
    std::cout << "len: "<< length << std::endl;

    for (const auto& msg : msgParts) {

        if(!fSave) {
            const auto ptr = reinterpret_cast<unsigned char*>(msg->GetData());
            //    std::string s(ptr, ptr+msg->GetSize());
            //    LOG(debug) << __FUNCTION__ << " received = " << s << " [" << index << "] "
            //    << fNumMessages;
            //    LOG(debug) << s;

            LOG(debug) << __FUNCTION__ << " received = " << " [" << index << "] " << fNumMessages;

            size_t gsize = msg->GetSize();
            std::cout <<"GetSize: " << gsize << std::endl;

            for(size_t i = 0; i < msg->GetSize(); i++) {
                printf("%02x", ptr[i]);
                if( ( (i+1)%8 ) == 0 )
                    printf("\n");
            }

        } else if(fSave) {
            LOG(debug) << __FUNCTION__ << " received = " << " [" << index << "] " << fNumMessages;
            const auto ptr = reinterpret_cast<unsigned char*>(msg->GetData());
            const auto ptr_next = ptr + msg->GetSize();
            rawbuf.insert(rawbuf.end(),
                          std::make_move_iterator( reinterpret_cast<char*>(ptr) ),
                          std::make_move_iterator( reinterpret_cast<char*>(ptr_next) )
                         );

        }// save or print
        ++fNumMessages;
    }//msg

    if( fstr.is_open() && fopened ) {
        fstr.write(rawbuf.data(), length);
    }

    return true;
}

//_____________________________________________________________________________
void Sink::Init()
{
    PrintConfig(fConfig, "channel-config", __PRETTY_FUNCTION__);
    PrintConfig(fConfig, "chans.", __PRETTY_FUNCTION__);

    fNumMessages = 0;
    fopened = false;
}

//_____________________________________________________________________________
void Sink::InitTask()
{
    PrintConfig(fConfig, "channel-config", __PRETTY_FUNCTION__);
    PrintConfig(fConfig, "chans.", __PRETTY_FUNCTION__);

    LOG(debug) << MyClass << " InitTask";
    using opt = OptionKey;

    fInputChannelName = fConfig->GetProperty<std::string>(opt::InputChannelName.data());
    LOG(debug) << " input channel = " << fInputChannelName;

    //////
    /*  FairMQMessagePtr msginfo(NewMessage());
    int nrecv=0;
    while(true){
      if (Receive(msginfo, fInputChannelName) <= 0) {
        LOG(debug) << __func__ << " Trying to get FEMIfo " << nrecv;
        nrecv++;
      }else{
        break;
      }
    }

    const auto ptr = reinterpret_cast<unsigned char*>(msginfo->GetData());

    for(size_t i = 0; i < msginfo->GetSize(); i++){
      printf("%02x", ptr[i]);
      if( ( (i+1)%8 )==0 )
        printf("\n");
    }
    */
    ////

    const auto &isMultipart = fConfig->GetProperty<std::string>(opt::Multipart.data());
    if (isMultipart=="true" || isMultipart=="1") {
        LOG(warn) << " set multipart data handler";
        OnData(fInputChannelName, &Sink::HandleMultipartData);
    } else {
        LOG(warn) << " set data handler";
        OnData(fInputChannelName, &Sink::HandleData);
    }

}

//_____________________________________________________________________________
void Sink::PreRun()
{
    const auto rnumber = fConfig->GetProperty<std::string>("run_number");
    LOG(debug) << "run_number: " << rnumber;

    //  std::string filename = "run" + rnumber + ".dat";
    //  fstr.open(filename.data(), std::ios::binary);

    char filename[256];
    sprintf(filename, "./file/run%06d.dat", stoi(rnumber));
    fstr.open(filename, std::ios::binary);
    fopened = true;

}
//_____________________________________________________________________________
void Sink::PostRun()
{
    using opt = OptionKey;
    LOG(debug) << __func__;
    int nrecv=0;
    while (true) {
        const auto &isMultipart = fConfig->GetProperty<std::string>(opt::Multipart.data());
        if (isMultipart=="true" || isMultipart=="1") {
            FairMQParts parts;
            if (Receive(parts, fInputChannelName) <= 0) {
                LOG(debug) << __func__ << " no data received " << nrecv;
                ++nrecv;
                if (nrecv>10) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            } else {
                LOG(debug) << __func__ << " print data";
                HandleMultipartData(parts, 0);
            }
        } else {
            FairMQMessagePtr msg(NewMessage());
            if (Receive(msg, fInputChannelName) <= 0) {
                LOG(debug) << __func__ << " no data received " << nrecv;
                ++nrecv;
                if (nrecv>10) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            } else {
                LOG(debug) << __func__ << " print data";
                HandleData(msg, 0);
            }
        }
        LOG(debug) << __func__ << " done";
    }

    if( fstr.is_open() ) {
        fstr.flush();
        fstr.close();
    }

}
