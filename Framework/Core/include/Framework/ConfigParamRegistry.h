// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
#ifndef O2_FRAMEWORK_CONFIGPARAMREGISTRY_H_
#define O2_FRAMEWORK_CONFIGPARAMREGISTRY_H_

#include "Framework/ParamRetriever.h"
#include "Framework/ConfigParamStore.h"
#include "Framework/Traits.h"

#include <boost/property_tree/ptree.hpp>
#include <memory>
#include <string>
#include <cassert>

namespace o2::framework
{

namespace
{
template <typename T>
std::vector<T> extractVector(boost::property_tree::ptree const& tree)
{
  std::vector<T> result(tree.size());
  auto count = 0u;
  for (auto& entry : tree) {
    result[count++] = entry.second.get_value<T>();
  }
  return result;
}

template <typename T>
o2::framework::Array2D<T> extractMatrix(boost::property_tree::ptree const& tree)
{
  std::vector<T> cache;
  uint32_t nrows = tree.size();
  uint32_t ncols = 0;
  bool first = true;
  auto irow = 0u;
  for (auto& row : tree) {
    if (first) {
      ncols = row.second.size();
      first = false;
    }
    auto icol = 0u;
    for (auto& entry : row.second) {
      cache.push_back(entry.second.get_value<T>());
      ++icol;
    }
    ++irow;
  }
  return o2::framework::Array2D<T>{cache, nrows, ncols};
}
} // namespace

class ConfigParamStore;

/// This provides unified access to the parameters specified in the workflow
/// specification.
/// The ParamRetriever is a concrete implementation of the registry which
/// will actually get the options. For example it could get them from the
/// FairMQ ProgOptions plugin or (to run "device-less", e.g. in batch simulation
/// jobs).
class ConfigParamRegistry
{
 public:
  ConfigParamRegistry(std::unique_ptr<ConfigParamStore> store)
    : mStore{std::move(store)}
  {
  }

  bool isSet(const char* key) const
  {
    return mStore->store().count(key);
  }

  bool isDefault(const char* key) const
  {
    return mStore->store().count(key) > 0 && mStore->provenance(key) != "default";
  }

  template <typename T>
  T get(const char* key) const
  {
    assert(mStore.get());
    try {
      if constexpr (std::is_same_v<T, int> ||
                    std::is_same_v<T, int64_t> ||
                    std::is_same_v<T, long> ||
                    std::is_same_v<T, float> ||
                    std::is_same_v<T, double> ||
                    std::is_same_v<T, bool>) {
        return mStore->store().get<T>(key);
      } else if constexpr (std::is_same_v<T, std::string>) {
        return mStore->store().get<std::string>(key);
      } else if constexpr (std::is_same_v<T, std::string_view>) {
        return std::string_view{mStore->store().get<std::string>(key)};
      } else if constexpr (is_base_of_template<std::vector, T>::value) {
        return extractVector<typename T::value_type>(mStore->store().get_child(key));
      } else if constexpr (is_base_of_template<o2::framework::Array2D, T>::value) {
        return extractMatrix<typename T::element_t>(mStore->store().get_child(key));
      } else if constexpr (std::is_same_v<T, boost::property_tree::ptree>) {
        return mStore->store().get_child(key);
      } else if constexpr (std::is_constructible_v<T, boost::property_tree::ptree>) {
        return T{mStore->store().get_child(key)};
      } else if constexpr (std::is_constructible_v<T, boost::property_tree::ptree> == false) {
        static_assert(std::is_constructible_v<T, boost::property_tree::ptree> == false,
                      "Not a basic type and no constructor from ptree provided");
      }
    } catch (std::exception& e) {
      throw std::invalid_argument(std::string("missing option: ") + key + " (" + e.what() + ")");
    } catch (...) {
      throw std::invalid_argument(std::string("error parsing option: ") + key);
    }
  }

 private:
  std::unique_ptr<ConfigParamStore> mStore;
};

} // namespace o2::framework

#endif // O2_FRAMEWORK_CONFIGPARAMREGISTRY_H_
