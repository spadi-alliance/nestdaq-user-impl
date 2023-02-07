#ifndef NESTDAQ_UTILITY_PROGRAM_OPTOINS_H_
#define NESTDAQ_UTILITY_PROGRAM_OPTOINS_H_

#include <boost/program_options.hpp>
#include <string_view>
#include <functional>

#include "utility/is_vector.h"

namespace nestdaq {
//______________________________________________________________________________
template <typename T>
void add_option(boost::program_options::options_description_easy_init &a, std::string_view name,
                boost::program_options::typed_value<T> *s, std::string_view description,
                const std::unordered_map<std::string, T> &defaultValues)
{
   if (defaultValues.count(name.data()) == 0) {
      a(name.data(), s, description.data());
   } else {
      a(name.data(), s->default_value(defaultValues.at(name.data())), description.data());
   }
}

//______________________________________________________________________________
template <typename T>
void add_option(boost::program_options::options_description_easy_init &a, std::string_view name,
                boost::program_options::typed_value<T> *s, std::string_view description,
                const std::unordered_map<std::string, T> &defaultValues, const T &t)
{
   if (defaultValues.count(name.data()) == 0) {
      a(name.data(), s->default_value(t), description.data());
   } else {
      a(name.data(), s->default_value(defaultValues.at(name.data())), description.data());
   }
}

//______________________________________________________________________________
template <typename T, typename DescriptionMap>
void add_options(boost::program_options::options_description &o, std::string_view s, const DescriptionMap &m)
{
   if constexpr (is_vector_v<T>) {
      o.add_options()(s.data(), boost::program_options::value<T>()->multitoken(), m.at(s).data());
   } else {
      o.add_options()(s.data(), boost::program_options::value<T>(), m.at(s).data());
   }
}

//______________________________________________________________________________
template <typename T, typename DescriptionMap>
void add_options(boost::program_options::options_description &o, std::string_view s, const DescriptionMap &m, T t)
{
   o.add_options()(s.data(), boost::program_options::value<T>()->default_value(t), m.at(s).data());
}

//______________________________________________________________________________
template <typename T, typename DescriptionMap>
void add_options(boost::program_options::options_description &o, std::string_view s, const DescriptionMap &m, T &r, T t)
{
   o.add_options()(s.data(), boost::program_options::value<T>(&r)->default_value(t), m.at(s).data());
}

//______________________________________________________________________________
template <typename DescriptionMap>
void add_options(boost::program_options::options_description &o, std::string_view s, const DescriptionMap &m)
{
   o.add_options()(s.data(), m.at(s).data());
}

//______________________________________________________________________________
int parse_command_line(int argc, char *argv[], boost::program_options::options_description &opt,
                       boost::program_options::variables_map &vm, std::function<void(void)> check = nullptr);

//______________________________________________________________________________
int parse_command_line(int argc, char *argv[], boost::program_options::options_description &opt,
                       boost::program_options::options_description &hOpt, boost::program_options::variables_map &vm,
                       std::function<void(void)> check = nullptr);
//______________________________________________________________________________
int parse_command_line(int argc, char *argv[],
                       const std::vector<std::pair<std::string, boost::program_options::options_description>> &options,
                       boost::program_options::variables_map &vm, std::function<void(void)> check = nullptr);

} // namespace nestdaq

#endif // NESTDAQ_UTILITY_PROGRAM_OPTOINS_H_
