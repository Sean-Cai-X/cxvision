#include "muParser.h"
#include "CxScriptStage25Manifest.h"
#include "CxScriptStage25Runner.h"

Stage25Manifest g_stage25_manifest;

namespace
{
    Stage25ImageCase* g_current_image = nullptr;
    Stage25FindlineProfile* g_current_findline_profile = nullptr;
    Stage25FindcircleProfile* g_current_findcircle_profile = nullptr;
    Stage25EvidenceProfile* g_current_evidence_profile = nullptr;
}

class Stage25ManifestBinding
{
public:
    void setname(mu::charpvect& params)
    {
        if (params.size() > 0)
            g_stage25_manifest.name = params[0];
    }

    void setoutroot(mu::charpvect& params)
    {
        if (params.size() > 0)
            g_stage25_manifest.outroot = params[0];
    }

    void setimageroot(mu::charpvect& params)
    {
        if (params.size() > 0)
            g_stage25_manifest.imageroot = params[0];
    }

    void setminimagesforstability(int value)
    {
        g_stage25_manifest.min_images_for_stability = value;
    }

    void setminlevelsforstability(int value)
    {
        g_stage25_manifest.min_levels_for_stability = value;
    }

    int addimage(mu::charpvect& params)
    {
        if (params.size() >= 3)
        {
            Stage25ImageCase img;
            img.image_id = params[0];
            img.level = params[1];
            img.path = params[2];
            g_stage25_manifest.images.push_back(img);
            g_current_image = &g_stage25_manifest.images.back();
        }
        return 0;
    }

    int addfindlineprofile(mu::charpvect& params)
    {
        if (params.size() >= 1)
        {
            Stage25FindlineProfile profile;
            profile.profile_id = params[0];
            g_stage25_manifest.findline_profiles.push_back(profile);
            g_current_findline_profile = &g_stage25_manifest.findline_profiles.back();
        }
        return 0;
    }

    int addfindcircleprofile(mu::charpvect& params)
    {
        if (params.size() >= 1)
        {
            Stage25FindcircleProfile profile;
            profile.profile_id = params[0];
            g_stage25_manifest.findcircle_profiles.push_back(profile);
            g_current_findcircle_profile = &g_stage25_manifest.findcircle_profiles.back();
        }
        return 0;
    }

    int addevidenceprofile(mu::charpvect& params)
    {
        if (params.size() >= 1)
        {
            Stage25EvidenceProfile profile;
            profile.name = params[0];
            g_stage25_manifest.evidence_profiles.push_back(profile);
            g_current_evidence_profile = &g_stage25_manifest.evidence_profiles.back();
        }
        return 0;
    }
};

class Stage25ImageSetBinding
{
public:
    void addtag(mu::charpvect& params)
    {
        if (g_current_image && params.size() > 0)
            g_current_image->tags.push_back(params[0]);
    }

    void setexpectededge(mu::charpvect& params)
    {
        if (g_current_image && params.size() > 0)
            g_current_image->expected_edge = params[0];
    }

    void setlighting(mu::charpvect& params)
    {
        if (g_current_image && params.size() > 0)
            g_current_image->lighting = params[0];
    }

    void setcontrast(mu::charpvect& params)
    {
        if (g_current_image && params.size() > 0)
            g_current_image->contrast = params[0];
    }

    void addfindlinetarget(mu::charpvect& params)
    {
        if (g_current_image && params.size() >= 7)
        {
            Stage25ImageTarget target;
            target.target_id = params[0];
            target.tool = "Findline";
            target.x0 = std::stoi(params[1]);
            target.y0 = std::stoi(params[2]);
            target.x1 = std::stoi(params[3]);
            target.y1 = std::stoi(params[4]);
            target.wgap = std::stoi(params[5]);
            target.hgap = std::stoi(params[6]);
            g_current_image->targets.push_back(target);
        }
    }

    void addfindcircletarget(mu::charpvect& params)
    {
        if (g_current_image && params.size() >= 6)
        {
            Stage25ImageTarget target;
            target.target_id = params[0];
            target.tool = "Findcircle";
            target.cx = std::stoi(params[1]);
            target.cy = std::stoi(params[2]);
            target.px = std::stoi(params[3]);
            target.py = std::stoi(params[4]);
            target.gap = std::stoi(params[5]);
            target.linegap = std::stoi(params[6]);
            g_current_image->targets.push_back(target);
        }
    }
};

class Stage25FindlineProfileBinding
{
public:
    void setmethod(int value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->method = value;
    }

    void setthreshold(int value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->threshold = value;
    }

    void setlinegap(int value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->linegap = value;
    }

    void setfitmode(int value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->fitmode = value;
    }

    void setscript_scale(int value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->script_scale = value;
    }

    void setfilterprofile(int value)
    {
        if (g_current_findline_profile)
            g_current_findline_profile->filter_profile = value;
    }

    void setobjfilter(int value)
    {
        if (g_current_findline_profile)
        {
            g_current_findline_profile->has_explicit_filter = true;
            g_current_findline_profile->objfilter = value;
        }
    }

    void setfilter(mu::charpvect& params)
    {
        if (g_current_findline_profile && params.size() >= 3)
        {
            g_current_findline_profile->has_explicit_filter = true;
            g_current_findline_profile->filter_borw = std::stoi(params[0]);
            g_current_findline_profile->filter_min = std::stoi(params[1]);
            g_current_findline_profile->filter_max = std::stoi(params[2]);
        }
    }

    void setgamarate(int value)
    {
        if (g_current_findline_profile)
        {
            g_current_findline_profile->has_gamma = true;
            g_current_findline_profile->gamma = value;
        }
    }

    void setpolicy(mu::charpvect& params)
    {
        if (g_current_findline_profile && params.size() > 0)
            g_current_findline_profile->policy = params[0];
    }
};

class Stage25FindcircleProfileBinding
{
public:
    void setmethod(int value)
    {
        if (g_current_findcircle_profile)
            g_current_findcircle_profile->method = value;
    }

    void setthreshold(int value)
    {
        if (g_current_findcircle_profile)
            g_current_findcircle_profile->threshold = value;
    }

    void setgap(int value)
    {
        if (g_current_findcircle_profile)
            g_current_findcircle_profile->gap = value;
    }

    void setlinegap(int value)
    {
        if (g_current_findcircle_profile)
            g_current_findcircle_profile->linegap = value;
    }

    void setfindsetting(int value)
    {
        if (g_current_findcircle_profile)
        {
            g_current_findcircle_profile->has_filter = true;
            g_current_findcircle_profile->findsetting = value;
        }
    }

    void setfilter(mu::charpvect& params)
    {
        if (g_current_findcircle_profile && params.size() >= 3)
        {
            g_current_findcircle_profile->has_filter = true;
            g_current_findcircle_profile->filter_borw = std::stoi(params[0]);
            g_current_findcircle_profile->filter_min = std::stoi(params[1]);
            g_current_findcircle_profile->filter_max = std::stoi(params[2]);
        }
    }

    void setsamplerate(double value)
    {
        if (g_current_findcircle_profile)
        {
            g_current_findcircle_profile->has_samplerate = true;
            g_current_findcircle_profile->samplerate = value;
        }
    }

    void setpolicy(mu::charpvect& params)
    {
        if (g_current_findcircle_profile && params.size() > 0)
            g_current_findcircle_profile->policy = params[0];
    }
};

class Stage25EvidenceProfileBinding
{
public:
    void setnearestpointsupportpx(double value)
    {
        if (g_current_evidence_profile)
            g_current_evidence_profile->nearest_point_support_px = value;
    }

    void setlinedistancesupportpx(double value)
    {
        if (g_current_evidence_profile)
            g_current_evidence_profile->line_distance_support_px = value;
    }

    void setmingradient(double value)
    {
        if (g_current_evidence_profile)
            g_current_evidence_profile->min_gradient = value;
    }

    void setmingradientratio(double value)
    {
        if (g_current_evidence_profile)
            g_current_evidence_profile->min_gradient_ratio = value;
    }
};

mu::value_type RunStage25Manifest(const mu::value_type* params, int num_params)
{
    (void)params;
    (void)num_params;
    
    Stage25RunOptions options;
    options.out_root = g_stage25_manifest.outroot;
    options.manifest_path = g_stage25_manifest.outroot;
    
    Stage25RunResult result;
    RunStage25ManifestFile(options, result);
    return result.ok ? 1.0 : 0.0;
}

void RegisterStage25CxScriptBindings(mu::Parser& parser)
{
    Stage25ManifestBinding* manifest = nullptr;
    parser.DefineClass("Stage25Manifest", manifest);
    parser.DefineClassFun("Stage25Manifest", manifest, "setname", &Stage25ManifestBinding::setname);
    parser.DefineClassFun("Stage25Manifest", manifest, "setoutroot", &Stage25ManifestBinding::setoutroot);
    parser.DefineClassFun("Stage25Manifest", manifest, "setimageroot", &Stage25ManifestBinding::setimageroot);
    parser.DefineClassFun("Stage25Manifest", manifest, "setminimagesforstability", &Stage25ManifestBinding::setminimagesforstability);
    parser.DefineClassFun("Stage25Manifest", manifest, "setminlevelsforstability", &Stage25ManifestBinding::setminlevelsforstability);
    parser.DefineClassFun("Stage25Manifest", manifest, "addimage", &Stage25ManifestBinding::addimage);
    parser.DefineClassFun("Stage25Manifest", manifest, "addfindlineprofile", &Stage25ManifestBinding::addfindlineprofile);
    parser.DefineClassFun("Stage25Manifest", manifest, "addfindcircleprofile", &Stage25ManifestBinding::addfindcircleprofile);
    parser.DefineClassFun("Stage25Manifest", manifest, "addevidenceprofile", &Stage25ManifestBinding::addevidenceprofile);

    Stage25ImageSetBinding* image_set = nullptr;
    parser.DefineClass("ImageSet", image_set);
    parser.DefineClassFun("ImageSet", image_set, "addtag", &Stage25ImageSetBinding::addtag);
    parser.DefineClassFun("ImageSet", image_set, "setexpectededge", &Stage25ImageSetBinding::setexpectededge);
    parser.DefineClassFun("ImageSet", image_set, "setlighting", &Stage25ImageSetBinding::setlighting);
    parser.DefineClassFun("ImageSet", image_set, "setcontrast", &Stage25ImageSetBinding::setcontrast);
    parser.DefineClassFun("ImageSet", image_set, "addfindlinetarget", &Stage25ImageSetBinding::addfindlinetarget);
    parser.DefineClassFun("ImageSet", image_set, "addfindcircletarget", &Stage25ImageSetBinding::addfindcircletarget);

    Stage25FindlineProfileBinding* fl_profile = nullptr;
    parser.DefineClass("FindlineProfile", fl_profile);
    parser.DefineClassFun("FindlineProfile", fl_profile, "setmethod", &Stage25FindlineProfileBinding::setmethod);
    parser.DefineClassFun("FindlineProfile", fl_profile, "setthreshold", &Stage25FindlineProfileBinding::setthreshold);
    parser.DefineClassFun("FindlineProfile", fl_profile, "setlinegap", &Stage25FindlineProfileBinding::setlinegap);
    parser.DefineClassFun("FindlineProfile", fl_profile, "setfitmode", &Stage25FindlineProfileBinding::setfitmode);
    parser.DefineClassFun("FindlineProfile", fl_profile, "setscript_scale", &Stage25FindlineProfileBinding::setscript_scale);
    parser.DefineClassFun("FindlineProfile", fl_profile, "setfilterprofile", &Stage25FindlineProfileBinding::setfilterprofile);
    parser.DefineClassFun("FindlineProfile", fl_profile, "setobjfilter", &Stage25FindlineProfileBinding::setobjfilter);
    parser.DefineClassFun("FindlineProfile", fl_profile, "setfilter", &Stage25FindlineProfileBinding::setfilter);
    parser.DefineClassFun("FindlineProfile", fl_profile, "setgamarate", &Stage25FindlineProfileBinding::setgamarate);
    parser.DefineClassFun("FindlineProfile", fl_profile, "setpolicy", &Stage25FindlineProfileBinding::setpolicy);

    Stage25FindcircleProfileBinding* fc_profile = nullptr;
    parser.DefineClass("FindcircleProfile", fc_profile);
    parser.DefineClassFun("FindcircleProfile", fc_profile, "setmethod", &Stage25FindcircleProfileBinding::setmethod);
    parser.DefineClassFun("FindcircleProfile", fc_profile, "setthreshold", &Stage25FindcircleProfileBinding::setthreshold);
    parser.DefineClassFun("FindcircleProfile", fc_profile, "setgap", &Stage25FindcircleProfileBinding::setgap);
    parser.DefineClassFun("FindcircleProfile", fc_profile, "setlinegap", &Stage25FindcircleProfileBinding::setlinegap);
    parser.DefineClassFun("FindcircleProfile", fc_profile, "setfindsetting", &Stage25FindcircleProfileBinding::setfindsetting);
    parser.DefineClassFun("FindcircleProfile", fc_profile, "setfilter", &Stage25FindcircleProfileBinding::setfilter);
    parser.DefineClassFun("FindcircleProfile", fc_profile, "setsamplerate", &Stage25FindcircleProfileBinding::setsamplerate);
    parser.DefineClassFun("FindcircleProfile", fc_profile, "setpolicy", &Stage25FindcircleProfileBinding::setpolicy);

    Stage25EvidenceProfileBinding* ev_profile = nullptr;
    parser.DefineClass("EvidenceProfile", ev_profile);
    parser.DefineClassFun("EvidenceProfile", ev_profile, "setnearestpointsupportpx", &Stage25EvidenceProfileBinding::setnearestpointsupportpx);
    parser.DefineClassFun("EvidenceProfile", ev_profile, "setlinedistancesupportpx", &Stage25EvidenceProfileBinding::setlinedistancesupportpx);
    parser.DefineClassFun("EvidenceProfile", ev_profile, "setmingradient", &Stage25EvidenceProfileBinding::setmingradient);
    parser.DefineClassFun("EvidenceProfile", ev_profile, "setmingradientratio", &Stage25EvidenceProfileBinding::setmingradientratio);

    parser.DefineFun("RunStage25Manifest", (mu::multfun_type)RunStage25Manifest);
}