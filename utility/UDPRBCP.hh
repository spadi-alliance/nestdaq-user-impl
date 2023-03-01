#ifndef UDPRBCP_
#define UDPRBCP_

#include<vector>
#include<algorithm>

#include"Uncopyable.hh"

struct rbcp_header;

class UDPRBCP
    : Uncopyable<UDPRBCP>
{
public:
    static const unsigned int udp_buf_size_ = 4096;
    static const unsigned int rbcp_ver_     = 0xFF;
    enum rbcp_debug_mode
    {
        disp_no, disp_interactive, disp_debug,
        size_rbcp_debug_mode
    };

private:
    static const unsigned int rbcp_cmd_wr_  = 0x80;
    static const unsigned int rbcp_cmd_rd_  = 0xC0;

    const char*  ipAddr_;
    unsigned int port_;
    rbcp_header* sendHeader_;

    int  length_rd_;
    char          wd_buffer_[udp_buf_size_];
    unsigned char rd_buffer_[udp_buf_size_];

    rbcp_debug_mode mode_;

public:
    UDPRBCP(const char* ipAddr, unsigned int port, rbcp_header* sendHeader,
            rbcp_debug_mode mode = disp_interactive);
    virtual ~UDPRBCP();

    void SetDispMode(rbcp_debug_mode mode);
    void SetWD(unsigned int address, unsigned int length, char* sendData);
    void SetRD(unsigned int address, unsigned int legnth);
    int  DoRBCP();

    void CopyRD(std::vector<unsigned char>& buffer);
};

inline void UDPRBCP::CopyRD(std::vector<unsigned char>& buffer) {
    std::copy(rd_buffer_, rd_buffer_ +length_rd_,
              back_inserter(buffer)
             );
}

#endif
