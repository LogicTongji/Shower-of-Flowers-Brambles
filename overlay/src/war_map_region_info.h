#pragma once

#include <string_view>
#include <unordered_map>

struct RegionStaticInfo
{
    std::string_view chineseName;
    std::string_view capitalName;
};

inline const std::unordered_map<
    std::string_view,
    RegionStaticInfo
> kRegionStaticInfo = {
    {"guangdong_region", {"广东省", "广州"}},
    {"shanxi_region", {"山西省", "太原"}},
    {"yunnan_region", {"云南省", "昆明"}},
    {"guangxi_region", {"广西省", "南宁"}},
    {"sichuan_region", {"四川省", "成都"}},
    {"guizhou_region", {"贵州省", "贵阳"}},
    {"shaanxi_region", {"陕西省", "西安"}},
    {"hebei_region", {"河北省", "保定"}},
    {"shandong_region", {"山东省", "济南"}},
    {"fujian_region", {"福建省", "福州"}},
    {"hunan_region", {"湖南省", "长沙"}},
    {"jiangsu_region", {"江苏省", "南京"}},
    {"jiangxi_region", {"江西省", "南昌"}},
    {"chekiang_region", {"浙江省", "杭州"}},
    {"anhui_region", {"安徽省", "合肥"}},
    {"henan_region", {"河南省", "开封"}},
    {"hubei_region", {"湖北省", "武汉"}},
    {"rehe_region", {"热河省", "承德"}},
    {"fengtian_region", {"奉天省", "沈阳"}},
    {"liaonning_region", {"辽宁省", "沈阳"}},
    {"jilin_region", {"吉林省", "吉林"}},
    {"heilongjiang_region", {"黑龙江省", "哈尔滨"}},
    {"xingan_region", {"兴安省", "海拉尔"}},
    {"nenjiang_region", {"嫩江省", "齐齐哈尔"}},
    {"heihe_region", {"黑河省", "黑河"}},
    {"songjiang_region", {"松江省", "哈尔滨"}},
    {"jiandao_region", {"间岛省", "延吉"}},
    {"andong_region", {"安东省", "安东"}},
    {"chahar_region", {"察哈尔省", "张家口"}},
    {"suiyuan_region", {"绥远省", "呼和浩特"}},
    {"qinghai_region", {"青海省", "西宁"}},
    {"gansu_region", {"甘肃省", "兰州"}},
    {"ningxia_region", {"宁夏省", "银川"}},
    {"xinjiang_regions", {"新疆省", "迪化"}},
    {"Mongolia_Regions", {"蒙古地方", "乌兰巴托"}},
    {"tannuuriankhai_region", {"唐努乌梁海", "乌里雅苏台"}},
    {"utang_region", {"卫藏地方", "拉萨"}},
    {"taiwan_region", {"台湾省", "台北"}}
};

inline RegionStaticInfo GetRegionStaticInfo(
    std::string_view regionId
)
{
    const auto iterator =
        kRegionStaticInfo.find(regionId);

    if (iterator != kRegionStaticInfo.end())
    {
        return iterator->second;
    }

    return {regionId, "待补充"};
}
