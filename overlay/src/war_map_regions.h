#pragma once

#include <array>
#include <string_view>

inline constexpr std::array<std::string_view, 41>
kWarMapRegionNames = {
    "guangdong_region",
    "shanxi_region",
    "yunnan_region",
    "guangxi_region",
    "xikang_region",
    "ningxia_region",
    "gansu_region",
    "qinghai_region",
    "chahar_region",
    "suiyuan_region",
    "sichuan_region",
    "guizhou_region",
    "shaanxi_region",
    "hebei_region",
    "east_hebei_region",
    "shandong_region",
    "fujian_region",
    "hunan_region",
    "jiangsu_region",
    "jiangxi_region",
    "chekiang_region",
    "anhui_region",
    "henan_region",
    "hubei_region",
    "xingan_region",
    "rehe_region",
    "fengtian_region",
    "liaonning_region",
    "andong_region",
    "nenjiang_region",
    "heihe_region",
    "heilongjiang_region",
    "songjiang_region",
    "jiandao_region",
    "jilin_region",
    "xinjiang_region",
	"utang_region",
	"taiwan_region",
	"Mongolia_Regions",
    "SF_Shanghai",
	"tannuuriankhai_region"
};

constexpr size_t kWarMapRegionCount =
    kWarMapRegionNames.size();
