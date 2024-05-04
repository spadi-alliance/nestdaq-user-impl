#ifndef SLOWDASHIFY_H
#define SLOWDASHIFY_H

#include <string>
#include <iostream>
#include <sstream>

// - - - - - - - - - - - - - - - - 8< - - - - - - - - - - - - - - - - //

#if 0

#include <microbook.cxx>
std::string Slowdashify(const Histogram& hist) {
    std::ostringstream os;
    
    os << "{" << std::endl;
    os << "    \"bins\": { \"min\": " << hist.min << ", \"max\": " << hist.max << " }," << std::endl;
    os << "    \"counts\": [ ";
    for (unsigned i = 0; i < hist.size(); i++) {
        os << (i==0 ? "" : ", ") << hist[i];
    }
    os << " ]" << std::endl;
    os << "}" << std::endl;
    
    return os.str();
}
std::string Slowdashify(const Graph<XY>& graph) {
    std::ostringstream os;
    
    os << "{" << std::endl;
    os << "    \"x\": [";
    for (unsigned i = 0; i < graph.size(); i++) {
        os << (i==0 ? "" : ", ") << std::get<0>(graph[i]);
    }
    os << "]," << std::endl;
    os << "    \"y\": [";
    for (unsigned i = 0; i < graph.size(); i++) {
        os << (i==0 ? "" : ", ") << std::get<1>(graph[i]);
    }
    os << "]" << std::endl;
    os << "}" << std::endl;
    
    return os.str();
}
#endif

#if 1

#include <uhbook.cxx>
std::string Slowdashify(UH1Book& hist) {
    std::ostringstream os;
    
    os << "{" << std::endl;
    os << "    \"_attr\": { \"title\":     \"" << hist.GetTitle()      << "\", "  << std::endl;
    os << "                 \"xtitle\":    \"" << hist.GetXLabel()     << "\", "  << std::endl;
    os << "                 \"ytitle\":    \"" << hist.GetYLabel()     << "\" }," << std::endl;
    os << "    \"_stat\": { \"Entries\":   \"" << hist.GetEntries()    << "\", "  << std::endl;
    os << "                 \"Underflow\": \"" << hist.GetUnderflows() << "\", "  << std::endl;
    os << "                 \"Overflow\":  \"" << hist.GetOverflows()  << "\" }," << std::endl;
    os << "    \"bins\": { \"min\": " << hist.GetMinimum() << ", \"max\": " << hist.GetMaximum() << " }," << std::endl;
    os << "    \"counts\": [ ";
    for (int i = 1; i <= hist.GetNBins(); i++) {
      os << (i==1 ? "" : ", ") << hist.GetBinContent(i);
    }
    os << " ]" << std::endl;
    os << "}" << std::endl;
    
    return os.str();
}

std::string Slowdashify(UH2Book& hist) {
    std::ostringstream os;
    os << "{" << std::endl;
    os << "    \"_attr\": { \"title\":     \"" << hist.GetTitle()      << "\", "  << std::endl;
    os << "                 \"xtitle\":    \"" << hist.GetXLabel()     << "\", "  << std::endl;
    os << "                 \"ytitle\":    \"" << hist.GetYLabel()     << "\" }," << std::endl;
    os << "    \"_stat\": { \"Entries\":   " << hist.GetEntries()      << " },"   << std::endl;
    //os << "    \"_stat\": { \"Entries\":   \"" << hist.GetEntries()    << "\", " << std::endl;
    //os << "                 \"Underflow\": \"" << hist.GetUnderflows() << "\", " << std::endl;
    //os << "                 \"Overflow\":  \"" << hist.GetOverflows()  << "\" }," << std::endl;
    os << "    \"xbins\": { \"min\": " << hist.GetMinimumX() << ", \"max\": " << hist.GetMaximumX() << " }," << std::endl;
    os << "    \"ybins\": { \"min\": " << hist.GetMinimumY() << ", \"max\": " << hist.GetMaximumY() << " }," << std::endl;
    os << "    \"counts\": [";
    for (int j = 1; j <= hist.GetNBinsY(); j++) {
      os << (j==1 ? "" : ", ") << "  [ ";
      for (int i = 1; i <= hist.GetNBinsX(); i++) {
	os << (i==1 ? "" : ", ") << hist.GetBinContent(i,j);
      }
      os << " ]" << std::endl;
    }
    os << " ]" << std::endl;
    os << "}" << std::endl;
    return os.str();
}


#endif

#if 0
#include <TH1.h>
std::string Slowdashify(const TH1& hist) {
    //...
};
#include <TGraph.h>
std::string Slowdashify(const TGraph& graph) {
    //...
};
#endif

// - - - - - - - - - - - - - - - - 8< - - - - - - - - - - - - - - - - //

#endif
