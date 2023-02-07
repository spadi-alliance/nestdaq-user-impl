#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <stdexcept>

#include "utility/program_options.h"

namespace bpo = boost::program_options;

//______________________________________________________________________________
int nestdaq::parse_command_line(int argc, char *argv[], bpo::options_description &opt, bpo::variables_map &vm,
                            std::function<void(void)> check)
{
   auto ret = EXIT_SUCCESS;
   try {
      bpo::store(bpo::parse_command_line(argc, argv, opt), vm);
      bpo::notify(vm);

      if (vm.count("help") != 0) {
         throw std::runtime_error("help");
      }

      if (check) {
         check();
      }

   } catch (const bpo::error_with_option_name &e) {
      std::cerr << "#Exception: boost program options error: " << e.what() << std::endl;
      ret = EXIT_FAILURE;
   } catch (const std::exception &e) {
      std::cerr << "#Exception: unhandled exception: " << e.what() << std::endl;
      ret = EXIT_FAILURE;
   } catch (...) {
      std::cerr << "#Exception: unknown exception instance ... " << std::endl;
      ret = EXIT_FAILURE;
   }

   if (ret != EXIT_SUCCESS || argc == 1) {
      std::cout << opt << std::endl;
      ret = EXIT_FAILURE;
   }

   return ret;
}

//______________________________________________________________________________
int nestdaq::parse_command_line(int argc, char *argv[], bpo::options_description &opt, bpo::options_description &hOpt,
                            bpo::variables_map &vm, std::function<void(void)> check)
{
   auto ret = EXIT_SUCCESS;
   bool helpAll = false;
   try {
      bpo::store(bpo::parse_command_line(argc, argv, opt), vm);
      bpo::notify(vm);

      if (vm.count("help") != 0) {
         throw std::runtime_error("help");
      } else if (vm.count("help-all")) {
         helpAll = true;
         throw std::runtime_error("help-all");
      }

      if (check) {
         check();
      }

   } catch (const bpo::error_with_option_name &e) {
      std::cerr << "#Exception: boost program options error: " << e.what() << std::endl;
      ret = EXIT_FAILURE;
   } catch (const std::exception &e) {
      std::cerr << "#Exception: unhandled exception: " << e.what() << std::endl;
      ret = EXIT_FAILURE;
   } catch (...) {
      std::cerr << "#Exception: unknown exception instance ... " << std::endl;
      ret = EXIT_FAILURE;
   }

   if (ret != EXIT_SUCCESS || argc == 1) {
      if (helpAll) {
         std::cout << opt << std::endl;
      } else {
         std::cout << hOpt << std::endl;
      }
      ret = EXIT_FAILURE;
   }

   return ret;
}

//______________________________________________________________________________
int nestdaq::parse_command_line(int argc, char *argv[],
                            const std::vector<std::pair<std::string, bpo::options_description>> &options,
                            bpo::variables_map &vm, std::function<void(void)> check)
{
   auto ret = EXIT_SUCCESS;
   std::string helpOption;
   try {
      bpo::options_description opt;
      for (const auto &[k, v] : options) {
         opt.add(v);
      }
      bpo::store(bpo::parse_command_line(argc, argv, opt), vm);
      bpo::notify(vm);

      if (vm.count("help") > 0) {
         helpOption = vm["help"].as<std::string>();
         ret = EXIT_FAILURE;
         throw std::string("print help");
      }

      if (check) {
         check();
      }
   } catch (const std::string &s) {
      std::cerr << "#Exception: string: " << s << " : " << helpOption << std::endl;
   } catch (const bpo::error_with_option_name &e) {
      std::cerr << "#Exception: boost program options error: " << e.what() << std::endl;
      ret = EXIT_FAILURE;
   } catch (const std::exception &e) {
      std::cerr << "#Exception: unhandled exception: " << e.what() << std::endl;
      ret = EXIT_FAILURE;
   } catch (...) {
      std::cerr << "#Exception: unknown exception instance ... " << std::endl;
      ret = EXIT_FAILURE;
   }

   if (ret != EXIT_SUCCESS || argc == 1) {
      if (helpOption == "all") {
         for (const auto &[k, v] : options) {
            std::cout << v << std::endl;
         }
      } else {
         auto itr = std::find_if(options.begin(), options.end(),
                                 [&helpOption](const auto &p) { return (p.first == helpOption); });
         if (itr != options.end()) {
            std::cout << itr->second << std::endl;
         } else if (!options.empty()) {
            std::cout << options[0].second << std::endl;
            std::cout << "  help options" << std::endl;
            for (const auto &[k, v] : options) {
               std::cout << "  --help " << k << std::endl;
            }
            std::cout << "  --help all" << std::endl;
         }
      }
      ret = EXIT_FAILURE;
   }

   return ret;
}