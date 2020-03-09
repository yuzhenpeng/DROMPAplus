/* Copyright(c)  Ryuichiro Nakato <rnakato@iam.u-tokyo.ac.jp>
 * All rights reserved.
 */
#ifndef _READMPBLDATA_HPP_
#define _READMPBLDATA_HPP_

#include "../submodules/SSP/common/BedFormat.hpp"
#include "../submodules/SSP/src/BpStatus.hpp"

std::vector<int32_t> readMpbl(const std::string &, const std::string &, const int32_t, const int32_t);
std::vector<BpStatus> readMpbl_binary(const std::string &, const std::string &, const int32_t);
void OverrideBedToArray(std::vector<BpStatus> &array, const std::string &chrname, const std::vector<bed> &vbed);

#endif // _READMPBLDATA_HPP_
