#include "CxFastMatchRuntimeCapture.h"

#include "FastMatch.h"
#include "Image.h"
#include "ParserDebugBridge.h"

#include <cmath>
#include <limits>
#include <array>
#include <map>
#include <sstream>

namespace
{

bool QueryGlobalInt(
    ParserDebugBridge& bridge,
    const std::string& name,
    int& value)
{
    double numeric = 0.0;

    if (!bridge.QueryDouble(name, numeric))
        return false;

    if (!std::isfinite(numeric))
        return false;

    const double rounded = std::round(numeric);

    if (std::abs(numeric - rounded) > 1e-9)
        return false;

    if (rounded < static_cast<double>(std::numeric_limits<int>::min()) ||
        rounded > static_cast<double>(std::numeric_limits<int>::max()))
        return false;

    value = static_cast<int>(rounded);
    return true;
}

}

bool CaptureFastMatchRuntime(
    ParserDebugBridge& bridge,
    const std::string& object_name,
    CxFastMatchRuntimeCapture& capture)
{
    capture = {};

    static constexpr std::array<const char*, 4> aliases = {
        "Match",
        "fastmatch",
        "FastMatch",
        "CFastMatch"
    };

    std::map<void*, std::string> unique_objects;

    for (const char* class_name : aliases)
    {
        for (const std::string& name :
             bridge.ListClassObjectNames(class_name))
        {
            if (name != object_name)
                continue;

            void* object =
                bridge.QueryClassObject(class_name, name);

            if (object == nullptr)
                continue;

            auto [it, inserted] =
                unique_objects.emplace(object, class_name);

            if (!inserted)
                it->second += std::string("|") + class_name;
        }
    }

    if (unique_objects.empty())
    {
        capture.failure_stage = "fastmatch_object_lookup";
        capture.reason = "runtime object not found: " + object_name;
        return false;
    }

    if (unique_objects.size() != 1)
    {
        std::ostringstream message;
        message << "multiple FastMatch runtime objects for "
                << object_name << ": ";

        for (const auto& item : unique_objects)
        {
            message << item.second << "@"
                    << item.first << " ";
        }

        capture.failure_stage = "fastmatch_object_ambiguity";
        capture.reason = message.str();
        return false;
    }

    capture.object_found = true;
    capture.object_unique = true;
    capture.object_address = reinterpret_cast<std::uintptr_t>(
        unique_objects.begin()->first);
    capture.object_aliases = unique_objects.begin()->second;

    auto* matcher =
        static_cast<fastmatch*>(unique_objects.begin()->first);

    capture.object_model_point_count =
        matcher->getmodelpointcount();
    capture.object_learn_a_count =
        matcher->getlearnacount();
    capture.object_learn_b_count =
        matcher->getlearnbcount();
    capture.object_learn_a2_count =
        matcher->getlearna2count();
    capture.object_learn_b2_count =
        matcher->getlearnb2count();
    capture.last_learn_argument =
        matcher->debuglastlearnargument();

    capture.global_model_found =
        QueryGlobalInt(
            bridge,
            "global_model_point_count",
            capture.global_model_point_count);

    capture.global_learn_a_found =
        QueryGlobalInt(
            bridge,
            "global_learn_a_count",
            capture.global_learn_a_count);

    capture.global_learn_b_found =
        QueryGlobalInt(
            bridge,
            "global_learn_b_count",
            capture.global_learn_b_count);

    auto* local_image = static_cast<Image*>(
        bridge.QueryClassObject("Image", "m_image"));

    auto* global_image = bridge.QueryImage("global_matInput");

    capture.local_image_address =
        reinterpret_cast<std::uintptr_t>(local_image);
    capture.global_image_address =
        reinterpret_cast<std::uintptr_t>(global_image);

    if (local_image != nullptr)
    {
        const cv::Mat& mat = local_image->getmat();

        capture.image_empty = mat.empty();
        capture.image_rows = mat.rows;
        capture.image_cols = mat.cols;
        capture.image_type = mat.empty() ? -1 : mat.type();
    }

    return true;
}