#ifndef NESTDAQ_UTILITY_string_to_container_h
#define NESTDAQ_UTILITY_string_to_container_h

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>

namespace nestdaq {

template <class Container>
Container ToSequence(std::string_view line, std::string_view delim = ", ")
{
   Container ret;
   std::string l = line.data();
   boost::trim_if(l, boost::is_space());
   boost::split(ret, l, boost::is_any_of(delim.data()), boost::token_compress_on);
   return ret;
}

//_____________________________________________________________________________
template <class Container>
Container ToMap(std::string_view line, std::string_view comma = ", ", std::string_view eq = "= ")
{
   std::string l = line.data();
   boost::trim_if(l, boost::is_space());
   auto v = ToSequence<std::vector<std::string>>(l, comma);
   Container ret;
   for (auto &x : v) {
      boost::trim_if(x, boost::is_space());
      std::vector<std::string> s;
      boost::split(s, x, boost::is_any_of(eq.data()), boost::token_compress_on);
      if (s.size() == 2) {
         ret.emplace(s[0], s[0]);
      }
   }
   return ret;
}

} // namespace nestdaq

#endif