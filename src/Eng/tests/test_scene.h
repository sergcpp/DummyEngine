#pragma once

#include <string_view>

namespace Sys {
class ThreadPool;
}

enum eImgTest { NoShadow, NoGI, NoGI_RTShadow, NoDiffGI, NoDiffGI_RTShadow, MedDiffGI, Full, Full_Ultra };

void run_image_test(Sys::ThreadPool &threads, std::string_view test_name, const double min_psnr,
                    const eImgTest img_test = eImgTest::NoShadow);