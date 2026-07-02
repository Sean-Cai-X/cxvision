#include "muParserDef.h"
#include "muParserTest.h" 
#include "ParserClass.h"

#include "shapebase.h"
#include "Shape.h"
#include "imagemanager.h"
 
#include "Findline.h"
#include "Findcircle.h"
#include "Findellipse.h"
#include "findobject.h"
#include "FastMatch.h"
#include "CxScriptDirectBindings.h"

//#include "gridobject.h"
//#include "imageroi.h"
 
#include "Run.h"

//#include "tts.h"
//#include "asr.h"



typedef std::vector<double> vectordouble;
typedef std::vector<int> vectorint;
string  getlocationstringx0(const string &strfile)
{
    (void)strfile;
    string locationstring;
     return locationstring;
}


class SmartDouble
{
    vectordouble m_vectresult;
    vectordouble m_vectdouble;
public:
    SmartDouble(){}
    ~SmartDouble(){}
    void push(double dvalue,double dresult){m_vectdouble.push_back(dvalue);m_vectresult.push_back(dresult);}
    double getvalue(int inum){return m_vectdouble[inum];}
    double getresult(int inum){return m_vectresult[inum];}

    void set(double inum,double dvalue,double dresult)
    {m_vectdouble[((int)inum)]=dvalue;m_vectresult[((int)inum)]=dresult;}
    void clear(){m_vectdouble.clear();m_vectresult.clear();}
    int size(){return static_cast<int>(m_vectdouble.size());}
    int getvaluetimes(double dvalue)
    {
        int itimes=0;
        for(int i =0;i<static_cast<int>(m_vectdouble.size());i++)
        {
            if(m_vectdouble[i]==dvalue)
                itimes=itimes+1;
        }
        return itimes;
    }
    double average()
    {
        double daverage=0;
        for(int i=0;i<static_cast<int>(m_vectdouble.size());i++)
        {
            daverage +=m_vectdouble[i];
        }
        return daverage/m_vectdouble.size();
    }
    double maxvalue()
    {
        double dmax=-11111;
        for(int i=0;i<static_cast<int>(m_vectdouble.size());i++)
        {
            if(dmax<m_vectdouble[i])
                dmax =m_vectdouble[i];
        }
        return dmax;
    }
    int maxnum()
    {
        double dmax=-11111;int i;
        for(i=0;i<static_cast<int>(m_vectdouble.size());i++)
        {
            if(dmax<m_vectdouble[i])
                dmax =m_vectdouble[i];
        }
        return i;
    }
    double minvalue()
    {
        double dmin=9999;
        for(int i=0;i<m_vectdouble.size();i++)
        {
            if(dmin<m_vectdouble[i])
                dmin =m_vectdouble[i];
        }
        return dmin;
    }
    void save(const char * pchar)
    {
        int isize = static_cast<int>(m_vectdouble.size());
        if(isize<=0)
            return;
        FILE* rf = nullptr;
        if (fopen_s(&rf, pchar, "w+") != 0 || rf == nullptr)
           return;
       rewind(rf);
       for(int i=0;i<isize-1;i++)
       {
           double dv = m_vectdouble[i];
           double dr = m_vectresult[i];
           fprintf(rf,"%f$%f,",dv,dr);
       }

       fprintf(rf,"%f",m_vectdouble[isize-1]);
       fclose(rf);
    }
    std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(str);

        while (getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }

        return tokens;
    }
    void load(const char * pchar)
    {

        clear();
        FILE* rf = nullptr;
        if (fopen_s(&rf, pchar, "rb") != 0 || rf == nullptr)
            return;
        fseek(rf,0,SEEK_END);
        int filesize = ftell(rf);
        char *pcharget =new char[filesize+10];
        memset(pcharget,0,filesize+10);
        rewind(rf);
        fread((char*)(pcharget),filesize,1,rf);

        string astr = pcharget;
        vector<string> strnumlist = split(astr, ',');
        for(int i=0;i<static_cast<int>(strnumlist.size());i++)
        {
            vector<string> strdr = split(strnumlist[i], '$');
            double dvalue = std::stod(strdr.at(0));
            double dresult = std::stod(strdr.at(1)); 
            push(dvalue,dresult);
        }
        delete []pcharget;
        fclose(rf);
    }

};
class SmartTable
{
    map<int, double> m_tabmap0;
    vector<int> m_sort0;

public:
    SmartTable(){}
    ~SmartTable(){}

    void valuescale(double vala,double valb)
    {
        if(vala<valb)
         for(int iv=static_cast<int>(vala);iv<static_cast<int>(valb);iv++)
         {
               m_tabmap0[iv]=0;
         }
    }
    void addvalue(double val)
    {
         m_tabmap0[static_cast<int>(val)]=m_tabmap0[static_cast<int>(val)]+1;
    }
    void push(double did,double dvalue1)
    {
        int id = static_cast<int>(did);
        m_tabmap0[id]=dvalue1;

    }
    double getmap(int inum){return m_tabmap0[inum];}

    void sort()//m_tabmap0,m_sort0
    {
        /*
        m_sort0 = m_tabmap0.key_comp();
        list<double> dvalues = m_tabmap0.values();
        int irun = 0 ;
        for(int it=0;it<dvalues.size()-1;it++)
        {
            for(int is=it;is>0;is--)
            {
                if(is-1>0)
                {
                    irun = is-1;
                     if(dvalues[is]>dvalues[irun])
                     {
                         dvalues[irun]=dvalues[is];
                         int id0=m_sort0[irun];
                         m_sort0[irun]=m_sort0[is];
                         m_sort0[is]=id0;
                     }
                }
            }
        }
        */
    }

    double getsort(int inum){return m_sort0[inum];}

    void clear()
    {
       m_tabmap0.clear();
       m_sort0.clear();
    }

void print()
{ 
       
}

};

namespace mu
{
    CxParserRuntime::CxParserRuntime():
    m_iVal(0)
    {
        g_testcal = 0;
    }
    void CxParserRuntime::ParserInitialClassFunction(int iusing)
    {
        switch (iusing)
        {
        case 0:
        {
            double* apdouble = 0;
            m_parser.DefineOrgClass("double", apdouble);
             
                RunClass* prun = 0;
            m_parser.DefineClass("TestRun", prun);
            m_parser.DefineClassFun("TestRun", prun, "run", &RunClass::Run);
            m_parser.DefineClassFun("TestRun", prun, "testrun", &RunClass::testrun);
             
            ImageManager* pmodule = 0;
            m_parser.DefineClass("Module", pmodule);
            m_parser.DefineClassFun("Module", pmodule, "Show", &ImageManager::setshow);
            m_parser.DefineClassFun("Module", pmodule, "setobjectshow", &ImageManager::setobjectshow);

            SmartDouble* psmartd = 0;
            m_parser.DefineClass("SmartDouble", psmartd);
            m_parser.DefineClassFun("SmartDouble", psmartd, "set", &SmartDouble::set);
            m_parser.DefineClassFun("SmartDouble", psmartd, "save", &SmartDouble::save);
            m_parser.DefineClassFun("SmartDouble", psmartd, "load", &SmartDouble::load);
            m_parser.DefineClassFun("SmartDouble", psmartd, "getvalue", &SmartDouble::getvalue);
            
            SmartTable* psmart = 0;
            m_parser.DefineClass("SmartTable", psmart);
            m_parser.DefineClassFun("SmartTable", psmart, "push", &SmartTable::push);
            m_parser.DefineClassFun("SmartTable", psmart, "getmap", &SmartTable::getmap);
            m_parser.DefineClassFun("SmartTable", psmart, "sort", &SmartTable::sort);
            m_parser.DefineClassFun("SmartTable", psmart, "getsort", &SmartTable::getsort);
            m_parser.DefineClassFun("SmartTable", psmart, "clear", &SmartTable::clear);
            m_parser.DefineClassFun("SmartTable", psmart, "print", &SmartTable::print);
            m_parser.DefineClassFun("SmartTable", psmart, "valuescale", &SmartTable::valuescale);
            m_parser.DefineClassFun("SmartTable", psmart, "addvalue", &SmartTable::addvalue);

            Image* pimage = 0;
            m_parser.DefineClass("Image", pimage);
            m_parser.DefineClassFun("Image", pimage, "blur", &Image::blur);
            m_parser.DefineClassFun("Image", pimage, "load", &Image::load);
            m_parser.DefineClassFun("Image", pimage, "save", &Image::savefile);
            m_parser.DefineClassFun("Image", pimage, "getshow", &Image::getshow);
            m_parser.DefineClassFun("Image", pimage, "Show", &Image::setshow);
            m_parser.DefineClassFun("Image", pimage, "setroi", &Image::setroi);
            m_parser.DefineClassFun("Image", pimage, "Or", &Image::bitwiseOr);
            m_parser.DefineClassFun("Image", pimage, "And", &Image::bitwiseAnd); 
            m_parser.DefineClassFun("Image", pimage, "roi_7blur_gap_mud_thre_bw", &Image::roi_7blur_gap_mud_thre_bw);
            m_parser.DefineClassFun("Image", pimage, "roi_7blur_gap_mud_thre_bw_h", &Image::roi_7blur_gap_mud_thre_bw_h);
            m_parser.DefineClassFun("Image", pimage, "pyramidThresholding", &Image::pyramidThresholding);
            m_parser.DefineClassFun("Image", pimage, "colorizeROI", &Image::colorizeROI);
            m_parser.DefineClassFun("Image", pimage, "OrROI", &Image::bitwiseOrROI);
            m_parser.DefineClassFun("Image", pimage, "AndROI", &Image::bitwiseAndROI);
            m_parser.DefineClassFun("Image", pimage, "CopyFrom", &Image::CopyFrom);
            m_parser.DefineClassFun("Image", pimage, "copyFromMat", &Image::CopyFrom);
            m_parser.DefineClassFun("Image", pimage, "erodeROI", &Image::erodeROI);
            m_parser.DefineClassFun("Image", pimage, "erodeVerticalROI", &Image::erodeVerticalROI);
            m_parser.DefineClassFun("Image", pimage, "erodeHorizontalROI", &Image::erodeHorizontalROI);
           // m_parser.DefineClassFun("Image", pimage, "test", &Image::TestROI);
            m_parser.DefineClassFun("Image", pimage, "roieasythre", &Image::ROIEasyThre);
            m_parser.DefineClassFun("Image", pimage, "roidenoising", &Image::ROIDenoising);
            m_parser.DefineClassFun("Image", pimage, "roidenoisingmulti", &Image::ROIDenoisingMulti);
            m_parser.DefineClassFun("Image", pimage, "pyrdown", &Image::pyrDown);
            m_parser.DefineClassFun("Image", pimage, "roipyrdown", &Image::ROIpyrDown);
            m_parser.DefineClassFun("Image", pimage, "getshape", &Image::getshape);
            m_parser.DefineClassFun("Image", pimage, "loadfiles", &Image::loadfiles);
            m_parser.DefineClassFun("Image", pimage, "reload", &Image::reload); 
            m_parser.DefineClassFun("Image", pimage, "rotate", &Image::rotateImage);
            m_parser.DefineClassFun("Image", pimage, "roisobel", &Image::ROISobel);
            m_parser.DefineClassFun("Image", pimage, "roischarr", &Image::ROIScharr);

            m_parser.DefineClassFun("Image", pimage, "roi_5bgmb", &Image::roi_5blur_gap_mud_bw);
            m_parser.DefineClassFun("Image", pimage, "roi_7bgmb", &Image::roi_7blur_gap_mud_bw);
            m_parser.DefineClassFun("Image", pimage, "roi_5bgmbh", &Image::roi_5blur_gap_mud_bw_h);
            m_parser.DefineClassFun("Image", pimage, "roi_7bgmbh", &Image::roi_7blur_gap_mud_bw_h);
            m_parser.DefineClassFun("Image", pimage, "roimean", &Image::roimean);
            m_parser.DefineClassFun("Image", pimage, "roimagnitude", &Image::roimagnitude);
             
            m_parser.DefineClassFun("Image", pimage, "test", &Image::Test);


            

            
             
            Shape* pshape = nullptr;
            m_parser.DefineClass("Shape", pshape);
            m_parser.DefineClassFun("Shape", pshape, "settype", &Shape::settype);
            m_parser.DefineClassFun("Shape", pshape, "setname", &Shape::setname);
            m_parser.DefineClassFun("Shape", pshape, "setrect", &Shape::setrect);
            m_parser.DefineClassFun("Shape", pshape, "setcolor", &Shape::setcolor);
            m_parser.DefineClassFun("Shape", pshape, "setfont", &Shape::setfont);
            m_parser.DefineClassFun("Shape", pshape, "translate", &Shape::translate);
            m_parser.DefineClassFun("Shape", pshape, "Show", &Shape::setshow);
            m_parser.DefineClassFun("Shape", pshape, "getshape", &Shape::shapesetroi);
            m_parser.DefineClassFun("Shape", pshape, "cutedge", &Shape::cutedge);
            

            

            ShapeBase* pshapebase = nullptr;
            m_parser.DefineClass("ShapeBase", pshapebase);
            m_parser.DefineClassFun("ShapeBase", pshapebase, "setshape", &ShapeBase::setShape);
            m_parser.DefineClassFun("ShapeBase", pshapebase, "Show", &ShapeBase::setshow);
                
            PointsShape* apoints = nullptr;
            m_parser.DefineClass("PointsShape", apoints);
            m_parser.DefineClassFun("PointsShape", apoints, "Show", &PointsShape::setshow);
            m_parser.DefineClassFun("PointsShape", apoints, "setcolor", &PointsShape::setcolor);
            m_parser.DefineClassFun("PointsShape", apoints, "addpoint", &PointsShape::addpoint);
            m_parser.DefineClassFun("PointsShape", apoints, "clear", &PointsShape::clear);
            m_parser.DefineClassFun("PointsShape", apoints, "calibration", &PointsShape::calibration);
            m_parser.DefineClassFun("PointsShape", apoints, "size", &PointsShape::size);
            m_parser.DefineClassFun("PointsShape", apoints, "getx", &PointsShape::getx);
            m_parser.DefineClassFun("PointsShape", apoints, "gety", &PointsShape::gety);
            m_parser.DefineClassFun("PointsShape", apoints, "crosspoint", &PointsShape::crosspoint);
            m_parser.DefineClassFun("PointsShape", apoints, "pointsABadd", &PointsShape::pointsABadd);
            m_parser.DefineClassFun("PointsShape", apoints, "gridpoints", &PointsShape::gridpoints);
            m_parser.DefineClassFun("PointsShape", apoints, "gridrightline", &PointsShape::gridrightline);
            m_parser.DefineClassFun("PointsShape", apoints, "pointslineadd", &PointsShape::pointslineadd);
            m_parser.DefineClassFun("PointsShape", apoints, "circlepoints", &PointsShape::circlepoints);
            m_parser.DefineClassFun("PointsShape", apoints, "halfcircle", &PointsShape::halfcircle);
            m_parser.DefineClassFun("PointsShape", apoints, "ellipsepoints", &PointsShape::ellipsepointsx);
            m_parser.DefineClassFun("PointsShape", apoints, "ratetopoint", &PointsShape::ratetopoint);
            m_parser.DefineClassFun("PointsShape", apoints, "sample", &PointsShape::sample);
            m_parser.DefineClassFun("PointsShape", apoints, "part", &PointsShape::part);
            m_parser.DefineClassFun("PointsShape", apoints, "makeshape", &PointsShape::MakeShape);
            m_parser.DefineClassFun("PointsShape", apoints, "load", &PointsShape::load);
            m_parser.DefineClassFun("PointsShape", apoints, "save", &PointsShape::save);
            
            m_parser.DefineClassFun("PointsShape", apoints, "makeshape", &PointsShape::MakeShape);

            m_parser.DefineClassFun("PointsShape", apoints, "clusterA", &PointsShape::ClusterPointCloud);
            
            m_parser.DefineClassFun("PointsShape", apoints, "aptfilter", &PointsShape::AdaptiveDistfilter);
            m_parser.DefineClassFun("PointsShape", apoints, "obbanglecenter", &PointsShape::OBBCenterAngleSort);
            m_parser.DefineClassFun("PointsShape", apoints, "sortpoints", &PointsShape::SortPoints);
            
            m_parser.DefineClassFun("PointsShape", apoints, "cluster", &PointsShape::ClusterPoints);
            m_parser.DefineClassFun("PointsShape", apoints, "filter", &PointsShape::FilterPoints);

            m_parser.DefineClassFun("PointsShape", apoints, "findcross", &PointsShape::FindCrossPoints);


            

            LineShape* plineshape = nullptr;
            m_parser.DefineClass("LineShape", plineshape);
            m_parser.DefineClassFun("LineShape", plineshape, "setline", &LineShape::setline);
            m_parser.DefineClassFun("LineShape", plineshape, "Show", &LineShape::setshow);
            m_parser.DefineClassFun("LineShape", plineshape, "move", &LineShape::Move);
            m_parser.DefineClassFun("LineShape", plineshape, "rotate", &LineShape::Rotate);
            m_parser.DefineClassFun("LineShape", plineshape, "zoom", &LineShape::Zoom);
            m_parser.DefineClassFun("LineShape", plineshape, "setpenw", &LineShape::setpenw);
            m_parser.DefineClassFun("LineShape", plineshape, "lineaex", &LineShape::lineaex);
            m_parser.DefineClassFun("LineShape", plineshape, "linebex", &LineShape::linebex);
            m_parser.DefineClassFun("LineShape", plineshape, "linecv", &LineShape::linecv);

            Findcircle* pfindcircle = nullptr;
            m_parser.DefineClass("Findcircle", pfindcircle);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "setcircle2", &Findcircle::setcircle2);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "setcircle", &Findcircle::setcircle);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "setgap", &Findcircle::Setgap);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "Setgap", &Findcircle::Setgap); 
            m_parser.DefineClassFun("Findcircle", pfindcircle, "Show", &Findcircle::setshow);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "measure", &Findcircle::measure);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "setlinegap", &Findcircle::setlinegap);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "setmethod", &Findcircle::setmethod);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "setthre", &Findcircle::setthre);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "getshape", &Findcircle::getshape);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "setcirclegap", &Findcircle::setcirclegap);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "fitcircle", &Findcircle::fitcircle);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "getavgdist", &Findcircle::getavgdist);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "getresultcentx", &Findcircle::getresultcentx);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "getresultcenty", &Findcircle::getresultcenty);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "getradius", &Findcircle::getradius);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "getavgdist", &Findcircle::getavgdist);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "setfitmeasuregap", &Findcircle::setfitmeasuregap);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "fitmeasure", &Findcircle::FitResultMeasure);
            m_parser.DefineClassFun("Findcircle", pfindcircle, "FitResultMeasure", &Findcircle::FitResultMeasure);
            
            

            Findellipse* pfindellipse = nullptr;
            m_parser.DefineClass("Findellipse", pfindellipse);
            m_parser.DefineClassFun("Findellipse", pfindellipse, "setellipse2", &Findellipse::setellipse2);
            m_parser.DefineClassFun("Findellipse", pfindellipse, "setellipse", &Findellipse::setellipse);
            m_parser.DefineClassFun("Findellipse", pfindellipse, "setgap", &Findellipse::Setgap);
            m_parser.DefineClassFun("Findellipse", pfindellipse, "Show", &Findellipse::setshow);
            m_parser.DefineClassFun("Findellipse", pfindellipse, "setlinegap", &Findellipse::setlinegap);
            m_parser.DefineClassFun("Findellipse", pfindellipse, "setmethod", &Findellipse::setmethod);
            m_parser.DefineClassFun("Findellipse", pfindellipse, "setthre", &Findellipse::setthre);

            Findline* pfindline = nullptr;
            m_parser.DefineClass("Findline", pfindline);
            m_parser.DefineClassFun("Findline", pfindline, "setrect", &Findline::setrect);
            m_parser.DefineClassFun("Findline", pfindline, "setline", &Findline::setline);
            m_parser.DefineClassFun("Findline", pfindline, "fitline", &Findline::FitLine);
            m_parser.DefineClassFun("Findline", pfindline, "setfitmode", &Findline::setfitmode);
            m_parser.DefineClassFun("Findline", pfindline, "setfitpointweight", &Findline::setfitpointweight);
            m_parser.DefineClassFun("Findline", pfindline, "translate", &Findline::translate);
            m_parser.DefineClassFun("Findline", pfindline, "Show", &Findline::setshow);
            m_parser.DefineClassFun("Findline", pfindline, "clear", &Findline::clear);
            m_parser.DefineClassFun("Findline", pfindline, "setwhgap", &Findline::SetWHgap);
            m_parser.DefineClassFun("Findline", pfindline, "measure", &Findline::measure);
            m_parser.DefineClassFun("Findline", pfindline, "setlinesample", &Findline::setlinesamplerate);
            m_parser.DefineClassFun("Findline", pfindline, "setlinegap", &Findline::setlinegap);
            m_parser.DefineClassFun("Findline", pfindline, "setmethod", &Findline::setmethod);
            m_parser.DefineClassFun("Findline", pfindline, "setthre", &Findline::setthre);
            m_parser.DefineClassFun("Findline", pfindline, "setgama", &Findline::setgamarate);
            m_parser.DefineClassFun("Findline", pfindline, "setobjfilter", &Findline::setobjfilter);
            m_parser.DefineClassFun("Findline", pfindline, "setfilter", &Findline::setfilter);
            m_parser.DefineClassFun("Findline", pfindline, "findpattern", &Findline::findpattern);
            m_parser.DefineClassFun("Findline", pfindline, "setcompgap", &Findline::setcomparegap);
            m_parser.DefineClassFun("Findline", pfindline, "shapesetroi", &Findline::shapesetroi);
            m_parser.DefineClassFun("Findline", pfindline, "setselectedgenum", &Findline::setselectedgenum);
            m_parser.DefineClassFun("Findline", pfindline, "patternfilter", &Findline::patternfilter);
            m_parser.DefineClassFun("Findline", pfindline, "getshape", &Findline::getshape);
            m_parser.DefineClassFun("Findline", pfindline, "sfilter", &Findline::SmartFilter); 
            m_parser.DefineClassFun("Findline", pfindline, "inflectionpoint", &Findline::InflectionPoint);

           // m_parser.DefineClassFun("Findline", pfindline, "setlinesegment", &Findline::setlinesegment);
            
 
            FindObject* pfobj = nullptr; 
            m_parser.DefineClass("Findobject", pfobj);
            m_parser.DefineClassFun("Findobject", pfobj, "setrect", &FindObject::setrect);
            m_parser.DefineClassFun("Findobject", pfobj, "measure", &FindObject::measure);
            m_parser.DefineClassFun("Findobject", pfobj, "Show", &FindObject::setshow);
            m_parser.DefineClassFun("Findobject", pfobj, "measurex", &FindObject::measurex);
            m_parser.DefineClassFun("Findobject", pfobj, "sethsogap", &FindObject::sethsogap);
            m_parser.DefineClassFun("Findobject", pfobj, "setminmax", &FindObject::setminmaxarea);
            m_parser.DefineClassFun("Findobject", pfobj, "setminmaxwh", &FindObject::setminmaxwh);
            m_parser.DefineClassFun("Findobject", pfobj, "setbrow", &FindObject::setbrow);
            m_parser.DefineClassFun("Findobject", pfobj, "setdistance", &FindObject::setdistance);
            m_parser.DefineClassFun("Findobject", pfobj, "setsearchtype", &FindObject::setsearchtype);
            m_parser.DefineClassFun("Findobject", pfobj, "edgeimage", &FindObject::edgeimage);
            m_parser.DefineClassFun("Findobject", pfobj, "setedgeoi", &FindObject::setedgeoi);
            m_parser.DefineClassFun("Findobject", pfobj, "setoffset", &FindObject::setoffset);
            m_parser.DefineClassFun("Findobject", pfobj, "getresultcentx", &FindObject::getresultcentx);
            m_parser.DefineClassFun("Findobject", pfobj, "getresultcenty", &FindObject::getresultcenty);
            m_parser.DefineClassFun("Findobject", pfobj, "getresultx", &FindObject::getresultx);
            m_parser.DefineClassFun("Findobject", pfobj, "getresulty", &FindObject::getresulty);
            m_parser.DefineClassFun("Findobject", pfobj, "getresultw", &FindObject::getresultw);
            m_parser.DefineClassFun("Findobject", pfobj, "getresulth", &FindObject::getresulth);
            m_parser.DefineClassFun("Findobject", pfobj, "getresultsize", &FindObject::getresultsize);
            m_parser.DefineClassFun("Findobject", pfobj, "getresultobjsnum", &FindObject::getresultobjsnum);
            m_parser.DefineClassFun("Findobject", pfobj, "objectgrid", &FindObject::objectgrid);
            m_parser.DefineClassFun("Findobject", pfobj, "setobjectgrid", &FindObject::setobjectgrid);
            m_parser.DefineClassFun("Findobject", pfobj, "objectsort", &FindObject::objectsort);
            m_parser.DefineClassFun("Findobject", pfobj, "edge", &FindObject::Edge);
            m_parser.DefineClassFun("Findobject", pfobj, "setrelresultnum", &FindObject::setrelationrectfromresultnum);
            m_parser.DefineClassFun("Findobject", pfobj, "setrelmatch", &FindObject::setrelationrectfrom_matchresult);
            m_parser.DefineClassFun("Findobject", pfobj, "setrelxy", &FindObject::setrelationxy);
            m_parser.DefineClassFun("Findobject", pfobj, "setrelzoom", &FindObject::setrelationzoom);
            m_parser.DefineClassFun("Findobject", pfobj, "setreltorect", &FindObject::setrelationtorect);
            m_parser.DefineClassFun("Findobject", pfobj, "setcolor", &FindObject::setcolorstyle);
            m_parser.DefineClassFun("Findobject", pfobj, "setroithre", &FindObject::SetImageROIthre);
            m_parser.DefineClassFun("Findobject", pfobj, "setroiincrease", &FindObject::SetImageROIincrease);
            m_parser.DefineClassFun("Findobject", pfobj, "setroicomparegap", &FindObject::SetImageROIcomparegap);
            m_parser.DefineClassFun("Findobject", pfobj, "setroifindborw", &FindObject::SetImageROIfindBorW);
            m_parser.DefineClassFun("Findobject", pfobj, "setroiedge5o7", &FindObject::SetImageROIedge_5o7);
            m_parser.DefineClassFun("Findobject", pfobj, "roithre", &FindObject::ImageROIthre);
            m_parser.DefineClassFun("Findobject", pfobj, "roiedge", &FindObject::ImageROIedge);
            m_parser.DefineClassFun("Findobject", pfobj, "roiedgeh", &FindObject::ImageROIedgeH);
            m_parser.DefineClassFun("Findobject", pfobj, "shapesetroi", &FindObject::shapesetroi);
            m_parser.DefineClassFun("Findobject", pfobj, "getshape", &FindObject::getshape);
 
            fastmatch* pfastmatch = nullptr;
            m_parser.DefineClass("Match", pfastmatch);
            m_parser.DefineClass("fastmatch", pfastmatch);
            m_parser.DefineClassFun("Match", pfastmatch, "setrect", &fastmatch::setrect);
            m_parser.DefineClassFun("Match", pfastmatch, "Show", &fastmatch::setshow);
            m_parser.DefineClassFun("Match", pfastmatch, "learn", &fastmatch::learn);
            m_parser.DefineClassFun("Match", pfastmatch, "setcompgap", &fastmatch::setcomparegap);
            m_parser.DefineClassFun("Match", pfastmatch, "setwhgap", &fastmatch::SetWHgap);
            m_parser.DefineClassFun("Match", pfastmatch, "setlinesample", &fastmatch::setlinesamplerate);
            m_parser.DefineClassFun("Match", pfastmatch, "setlinegap", &fastmatch::setlinegap);
            m_parser.DefineClassFun("Match", pfastmatch, "setmethod", &fastmatch::setmethod);
            m_parser.DefineClassFun("Match", pfastmatch, "setthre", &fastmatch::setthre);
            m_parser.DefineClassFun("Match", pfastmatch, "setobjfilter", &fastmatch::setobjfilter);
            m_parser.DefineClassFun("Match", pfastmatch, "setfilter", &fastmatch::setfilter);
            m_parser.DefineClassFun("Match", pfastmatch, "savemodel", &fastmatch::savemodelfile);
            m_parser.DefineClassFun("Match", pfastmatch, "loadmodel", &fastmatch::loadmodelfile);
            m_parser.DefineClassFun("Match", pfastmatch, "modelzero", &fastmatch::ZeroPOS);
            m_parser.DefineClassFun("Match", pfastmatch, "modelrotate", &fastmatch::modelrotate);
            m_parser.DefineClassFun("Match", pfastmatch, "modelzoom", &fastmatch::modelzoom);
            m_parser.DefineClassFun("Match", pfastmatch, "modelzeroposition", &fastmatch::modelzeroposition);
            m_parser.DefineClassFun("Match", pfastmatch, "match", &fastmatch::match);
            m_parser.DefineClassFun("Match", pfastmatch, "matchmore", &fastmatch::matchmore);
            m_parser.DefineClassFun("Match", pfastmatch, "setmatchrect", &fastmatch::setmatchrect);
            m_parser.DefineClassFun("Match", pfastmatch, "setminscore", &fastmatch::setminscore);
            m_parser.DefineClassFun("Match", pfastmatch, "matchstepgap", &fastmatch::matchstepgap);
            m_parser.DefineClassFun("Match", pfastmatch, "patternrootgrid", &fastmatch::patternrootgrid);
            m_parser.DefineClassFun("Match", pfastmatch, "patternzoom", &fastmatch::patternzoom);
            m_parser.DefineClassFun("Match", pfastmatch, "modeltranform", &fastmatch::patterntranform);
            m_parser.DefineClassFun("Match", pfastmatch, "rotatematch", &fastmatch::rotatematch);
            m_parser.DefineClassFun("Match", pfastmatch, "rotatematchAB", &fastmatch::rotatematchAB);
            m_parser.DefineClassFun("Match", pfastmatch, "rotatematch_upgrade", &fastmatch::rotatematchAB_upgrade);
            m_parser.DefineClassFun("Match", pfastmatch, "rotatematchAB05_upgrade", &fastmatch::rotatematchAB05_upgrade);
            m_parser.DefineClassFun("Match", pfastmatch, "rotatematchAB025_upgrade", &fastmatch::rotatematchAB025_upgrade);
            m_parser.DefineClassFun("Match", pfastmatch, "setupgradenum", &fastmatch::setupgradenum);
            m_parser.DefineClassFun("Match", pfastmatch, "patterngap", &fastmatch::patternABgap2gap);
            m_parser.DefineClassFun("Match", pfastmatch, "patternsample", &fastmatch::patternABsample);
            m_parser.DefineClassFun("Match", pfastmatch, "pattern2org", &fastmatch::pattern2org);
            m_parser.DefineClassFun("Match", pfastmatch, "reorgpattern", &fastmatch::org2pattern);
            m_parser.DefineClassFun("Match", pfastmatch, "patternsize", &fastmatch::ABpatternsize);
            m_parser.DefineClassFun("Match", pfastmatch, "samplemodel", &fastmatch::samplemodelAB); 
            m_parser.DefineClassFun("Match", pfastmatch, "loadrotatemodel", &fastmatch::loadrotatemodelfile);
            m_parser.DefineClassFun("Match", pfastmatch, "loadrotate05model", &fastmatch::loadrotate05modelfile);
            m_parser.DefineClassFun("Match", pfastmatch, "loadrotate025model", &fastmatch::loadrotate025modelfile);
            m_parser.DefineClassFun("Match", pfastmatch, "setrotateangle", &fastmatch::setrotateangle);
            m_parser.DefineClassFun("Match", pfastmatch, "setanglescale", &fastmatch::setrotateanglescale);
            m_parser.DefineClassFun("Match", pfastmatch, "multimatch", &fastmatch::multimatch);
            m_parser.DefineClassFun("Match", pfastmatch, "setmultimatchrect", &fastmatch::setmultimatchrect);
            m_parser.DefineClassFun("Match", pfastmatch, "setmatchrectnum", &fastmatch::setmatchrectnum);
            m_parser.DefineClassFun("Match", pfastmatch, "imagelearn", &fastmatch::imagelearn);
            m_parser.DefineClassFun("Match", pfastmatch, "imagematch", &fastmatch::imagematch);
            m_parser.DefineClassFun("Match", pfastmatch, "imagemodelcompareshow", &fastmatch::imagemodelcompareshow);
            m_parser.DefineClassFun("Match", pfastmatch, "savematchroi", &fastmatch::savematchroi);
            m_parser.DefineClassFun("Match", pfastmatch, "loadmapmodel", &fastmatch::loadfastimagemodel);
            m_parser.DefineClassFun("Match", pfastmatch, "savemapmodel", &fastmatch::savefastimagemodel);
            m_parser.DefineClassFun("Match", pfastmatch, "loadcalibration", &fastmatch::loadcalibration);
            m_parser.DefineClassFun("Match", pfastmatch, "savecalibration", &fastmatch::savecalibration);
            m_parser.DefineClassFun("Match", pfastmatch, "getimagemodelreslut", &fastmatch::getimagemodelreslut);
            m_parser.DefineClassFun("Match", pfastmatch, "setclustergap", &fastmatch::setclustergap);
            m_parser.DefineClassFun("Match", pfastmatch, "saveimagemodel", &fastmatch::savematchimagemodel);
            m_parser.DefineClassFun("Match", pfastmatch, "setmatchthre", &fastmatch::setmatchthre);
            m_parser.DefineClassFun("Match", pfastmatch, "setfindnum", &fastmatch::setfindnum);
            m_parser.DefineClassFun("Match", pfastmatch, "getresultnum", &fastmatch::getresultnum);
            m_parser.DefineClassFun("Match", pfastmatch, "getresultcentx", &fastmatch::getresultcentx);
            m_parser.DefineClassFun("Match", pfastmatch, "getresultcenty", &fastmatch::getresultcenty);
            m_parser.DefineClassFun("Match", pfastmatch, "getmaxresult", &fastmatch::getmaxresult);
            m_parser.DefineClassFun("Match", pfastmatch, "setspecshow", &fastmatch::setspecshow);
            m_parser.DefineClassFun("Match", pfastmatch, "setrelresultnum", &fastmatch::setrelationrectfromresultnum);
            m_parser.DefineClassFun("Match", pfastmatch, "setrelmatch", &fastmatch::setrelationrectfrom_matchresult);
            m_parser.DefineClassFun("Match", pfastmatch, "setrelxy", &fastmatch::setrelationxy);
            m_parser.DefineClassFun("Match", pfastmatch, "setrelzoom", &fastmatch::setrelationzoom);
            m_parser.DefineClassFun("Match", pfastmatch, "setreltorect", &fastmatch::setrelationtorect);
            m_parser.DefineClassFun("Match", pfastmatch, "setcolor", &fastmatch::setcolorstyle);
            m_parser.DefineClassFun("Match", pfastmatch, "shapesetroi", &fastmatch::shapesetroi);
            m_parser.DefineClassFun("Match", pfastmatch, "getrotateresultx", &fastmatch::getrotateresultx);
            m_parser.DefineClassFun("Match", pfastmatch, "getrotateresulty", &fastmatch::getrotateresulty);
            m_parser.DefineClassFun("Match", pfastmatch, "getrotateresulta", &fastmatch::getrotateresulta);
            m_parser.DefineClassFun("Match", pfastmatch, "getrotateresultscore", &fastmatch::getrotateresultscore);
            m_parser.DefineClassFun("Match", pfastmatch, "getrotateresultscoreA", &fastmatch::getrotateresultscoreA);
            m_parser.DefineClassFun("Match", pfastmatch, "getrotateresultcentx", &fastmatch::getrotateresultcentx);
            m_parser.DefineClassFun("Match", pfastmatch, "getrotateresultcenty", &fastmatch::getrotateresultcenty);
            m_parser.DefineClassFun("Match", pfastmatch, "rotateresultsfilter", &fastmatch::rotateresultsortfilter);
            m_parser.DefineClassFun("Match", pfastmatch, "rotateresultsfilterA", &fastmatch::rotateresultsortfilterA);
            m_parser.DefineClassFun("Match", pfastmatch, "rotateresultsize", &fastmatch::rotateresultsize);
            m_parser.DefineClassFun("Match", pfastmatch, "setupgradescale", &fastmatch::Setupgradescale);
            m_parser.DefineClassFun("Match", pfastmatch, "setshownum", &fastmatch::setshownum);
            m_parser.DefineClassFun("Match", pfastmatch, "getresultcentpoints", &fastmatch::getresultcentpoints);
            m_parser.DefineClassFun("Match", pfastmatch, "getshape", &fastmatch::getshape);
            m_parser.DefineClassFun("fastmatch", pfastmatch, "setrect", &fastmatch::setrect);
            m_parser.DefineClassFun("fastmatch", pfastmatch, "setthre", &fastmatch::setthre);
            m_parser.DefineClassFun("fastmatch", pfastmatch, "setlinegap", &fastmatch::setlinegap);
            m_parser.DefineClassFun("fastmatch", pfastmatch, "learn", &fastmatch::learn);
            m_parser.DefineClassFun("fastmatch", pfastmatch, "setmatchrect", &fastmatch::setmatchrect);
            m_parser.DefineClassFun("fastmatch", pfastmatch, "match", &fastmatch::match);
            m_parser.DefineClassFun("fastmatch", pfastmatch, "getmaxresult", &fastmatch::getmaxresult);

            RegisterPendingDirectCxScriptBindings(m_parser);
            
            SmartDouble* avect = nullptr;
            m_parser.DefineClass("vector", avect);
            m_parser.DefineClassFun("vector", avect, "push", &SmartDouble::push);
            m_parser.DefineClassFun("vector", avect, "get", &SmartDouble::getvalue);
            m_parser.DefineClassFun("vector", avect, "get", &SmartDouble::getresult); 
            m_parser.DefineClassFun("vector", avect, "set", &SmartDouble::set);
            m_parser.DefineClassFun("vector", avect, "clear", &SmartDouble::clear);
            m_parser.DefineClassFun("vector", avect, "size", &SmartDouble::size);
            m_parser.DefineClassFun("vector", avect, "average", &SmartDouble::average);
            m_parser.DefineClassFun("vector", avect, "maxvalue", &SmartDouble::maxvalue);
            m_parser.DefineClassFun("vector", avect, "minvalue", &SmartDouble::minvalue);
            m_parser.DefineClassFun("vector", avect, "maxnum", &SmartDouble::maxnum);
            m_parser.DefineClassFun("vector", avect, "save", &SmartDouble::save);
            m_parser.DefineClassFun("vector", avect, "load", &SmartDouble::load);


        }
        m_parser.UsingClass(true);
        m_pdoubleclass = (classbase*)GetClass(string("double"));
        break;
        }
        m_parser.UsingClass(true);
    }
    void CxParserRuntime::ParserElementShow(int ishow)
    {
        (void)ishow;
        mu::Parser &Pparser=m_parser;
        mu::classbasemap_type classmap = Pparser.GetClassMap();
        if(!classmap.size())
            return;
        classbasemap_type::const_iterator item = classmap.end();
        for (; item!=classmap.begin(); )
        {
            --item;
            if("CFindPointEx"==item->first)
            {
#ifdef USE_CROIMeasurePointEx
                CROIMeasurePointEx *apimage=0;
                for(int i=0;i<pclass->size();i++)
                {
                    apimage=(CROIMeasurePointEx *)pclass->getvarpoint(i);
                    if(ishow>0)
                    apimage->SetShow(35);
                    else
                    apimage->SetShow(0);
                }
#endif
            }
         }
    }
    void * CxParserRuntime::GetClass(const string & strclass)
    {
        mu::classbasemap_type classmap = m_parser.GetClassMap();
        classbasemap_type::const_iterator item = classmap.find(strclass);
        if(item!=classmap.end())
        {
            return (item->second);
        }
        else
            return 0;
    }
    void* CxParserRuntime::GetClassObj(const string & strclass,const string & strobj)
    {
        mu::classbasemap_type classmap = m_parser.GetClassMap();
        classbase *pclass;
        //if(!classmap.size())
        //	return NULL;
        /*
        OrgStorage_type::iterator item = m_ObjectStorage.find(a_str);
        if (item!=m_ObjectStorage.end())
        return item->second;
        else
        return 0;

        return pclass->getvarpoint(pclass);
        */
        classbasemap_type::const_iterator item = classmap.find(strclass);
        if(item!=classmap.end())
        {
            pclass=(item->second);
            return pclass->getvarpoint(strobj);
        }
        else
        return 0;
        //classbasemap_type::const_iterator item = classmap.begin();
        //for (; item!=classmap.end(); ++item)
        //{
        //	if(strclass==item->first)
        //	{
        //		pclass=(item->second);
        //		for(int i=0;i<pclass->size();i++)
        //		{
        //			if(strobj == pclass->getvar(i))
        //				return pclass->getvarpoint(i);
        //		}
        //	}
        //}
    }
    void* CxParserRuntime::GetClassObj(const string & strclass,const int &iobjnum)
    {
        mu::classbasemap_type classmap = m_parser.GetClassMap();
        classbase *pclass;
        if(!classmap.size())
            return 0;

        classbasemap_type::const_iterator item = classmap.begin();
        for (; item!=classmap.end(); ++item)
        {
            if(strclass==item->first)
            {
                pclass=(item->second);

                if(iobjnum>=pclass->size())
                    return 0;
                //for(int i=0;i<pclass->size();i++)
                {
                     *m_stream <<pclass->getvar(iobjnum);
                     *m_stream <<"\r\n";
                    return pclass->getvarpoint(iobjnum);
                }
            }
        }
        return NULL;
    }


    void* CxParserRuntime::GetDoubleValue(const string & strname)
    {
        if(m_pdoubleclass!=0)
            return m_pdoubleclass->getvarpoint(strname);
        return 0;
    }
    int CxParserRuntime::GetClassObjSum(const string & strclass)
    {
        mu::classbasemap_type classmap = m_parser.GetClassMap();
        classbase *pclass;
        if(!classmap.size())
            return 0;

        classbasemap_type::const_iterator item = classmap.begin();
        for (; item!=classmap.end(); ++item)
        {
            if(strclass==item->first)
            {
                pclass=(item->second);

                return pclass->size();
            }
        }
        return 0;
    }
   
    void CxParserRuntime::SetExpr(const string & str)
    {

        m_parser.SetExpr(str);
    }
    value_type CxParserRuntime::Eval()
    {
        return m_parser.Eval();
    }
    double CxParserRuntime::GetResult()
    {
        return m_parser.Eval();
    }
    void CxParserRuntime::DefineVar(const string & str,double *dvalue)
    {
        m_parser.DefineVar(str,dvalue);
    }
    //need to do
    void CxParserRuntime::SetVarFactory()
    {
        //CXListCtrl *pGrid=	reinterpret_cast<CXListCtrl *>(pView);
        m_parser.SetVarFactory((mu::facfun_type)&CxParserRuntime::AddVariable,this);
    }

    void CxParserRuntime::FunTest(void *func)
    {
        m_Func=func;
        //*func;
    }

    double* CxParserRuntime::AddVariable(const char *a_szName,void *pClass)
    {
        // I don't want dynamic allocation here, so i used this static buffer
        // If you want dynamic allocation you must allocate all variables dynamically
        // in order to delete them later on. Or you find other ways to keep track of
        // variables that have been created implicitely.
        CxParserRuntime*me= reinterpret_cast<CxParserRuntime*>(pClass);
        // static double afValBuf[100];
        // static int iVal = 0;

        *(me->m_stream) << "Generating new variable \""
            << a_szName << "\" (slots left: "
            << 99-me->m_iVal << ")" << "\r\n";

        me->m_afValBuf[me->m_iVal] = 0;
        if (me->m_iVal>=99)
            throw mu::ParserError("Variable buffer overflow.");

        return &(me->m_afValBuf[me->m_iVal++]);
    }

    ParserByteCode::storage_type CxParserRuntime::GetByteCode()
    {
        return m_parser.GetStorageBase();
    }
    double CxParserRuntime::RunByteCode(ParserByteCode::storage_type Base)
    {
        m_parser.m_vByteCodeCollection.SetStorageBase(Base);
        double dresult= m_parser.RunCollectionCmdCode();
        m_parser.ClearCollection();
        return dresult;
    }
    double CxParserRuntime::RunOptCode()
    {
        return m_parser.RunCollectionOpt();
    }
    void CxParserRuntime::RunFastCode()
    {
        return m_parser.RunCode();
    }
    void CxParserRuntime::CopyRunOpt(int inum)
    {
        m_parser.CopyRUNOpt(inum);
    }
    void CxParserRuntime::RunOptNum(int inum)
        {
        //////////////////////////////////////////////////////////////////////////
        try
        {
            m_parser.RunOpt(inum);
        }
        catch(mu::Parser::exception_type &)
        {

            *m_stream  << "\n RunOptNum RunTime Error exception_type ";
        }
    }

    void CxParserRuntime::SetRunOpt(const string &strname)
    {
        m_parser.SetOptStack(strname);
    }


    void CxParserRuntime::RunOptString(const char *a_szName)
    {
     
        __try {
            m_parser.RunOptString(a_szName);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            *m_stream  << a_szName ;
            *m_stream  << "\n RunOptString RunTime Error \n";
           
        }

    }
    void CxParserRuntime::ClearOptMap()
    {
        m_parser.ClearOptStack();
    }
    void CxParserRuntime::RunOptNum_TimeLimit(int inum)
    {
        (void)inum;
        //m_parser.RunOptTimeLimit(inum);
    }
    void CxParserRuntime::SetOptCollect(bool Open_Close)
    {
        m_parser.SetOptCollect(Open_Close);
    }
    void CxParserRuntime::SetByteCollection(bool btruefalse)
    {
        m_parser.SetColllection(btruefalse);
    }
    void CxParserRuntime::ClearByteCollection()
    {
        m_parser.ClearCollection();
    }
    //////////////////////////////////////////////////////////////////////////
    bool CxParserRuntime::CommandLine(const string & astr)
    {
        std::string sLine=astr;
        if(sLine==">>quit"||sLine==">>quit\r\n")
        {
            exit(0);
        }
        else if(sLine==">>help"||sLine==">>help\r\n")
        {
            ShowHelp();
            return true;
        }
        else if (sLine==">>list var"||sLine==">>list var\r\n")
        {
            ListVar(&m_parser);
            return true;
        }
        else if (sLine==">>list const"||sLine==">>list const\r\n")
        {
            ListConst(&m_parser);
            return true;
        }
        else if (sLine==">>list exprvar"||sLine==">>list exprvar\r\n")
        {
            ListExprVar(&m_parser);
            return true;
        }
        else if (sLine==">>list const"||sLine==">>list const\r\n")
        {
            ListConst(&m_parser);
            return true;
        }
        else if(sLine==">>list func"||sLine==">>list func\r\n")
        {
            ListFunction(&m_parser);
            return true;
        }
        else if(sLine==">>list class"||sLine==">>list class\r\n")
        {
            ListClass( m_parser);
            return true;
        }
        else if(sLine==">>list only name")
        {
            ListClassONLYName();
            ListClassONLYFUNCTION();
            return true;
        }
        else if(sLine ==">>run"||sLine ==">>run\r\n")
        {
            m_parser.RunCode();
            *m_stream <<"create code :"<<"\r\n";
            *m_stream <<m_parser.m_StrCollection;
            return true;
        }
        else if(sLine ==">>run"||sLine ==">>run\r\n")
        {
            m_parser.RunCollectionOpt();
            return true;
        }
        else if(sLine==">>open collec"||sLine==">>open collec\r\n")
        {
            m_parser.SetColllection(true);
            return true;
        }
        else if(sLine==">>close collec"||sLine==">>close collec\r\n")
        {
            m_parser.SetColllection(false);
            return true;
        }
        else if(sLine==">>clear collec"||sLine==">>clear collec\r\n")
        {
            m_parser.ClearCollection();
            return true;
        }
        else if(sLine==">>clear"||sLine==">>clear\r\n")
        {
            m_stream->clear();
            return true;
        }
        else if(sLine==">>clear var"||sLine==">>clear var\r\n")
        {
            m_parser.ClearVar();
            return true;
        }
        else if(sLine==">>clear all"||sLine==">>clear all\r\n")
        {
            m_parser.ClearClassObj();
            m_parser.ClearVar();
            return true;
        }
        else if(sLine==">>test"||sLine==">>test\r\n")
        {
            SelfTest();
            return true;
        }
        else if(sLine==">>Set VarFact"||sLine==">>Set VarFact\r\n")
        {
            SetVarFactory();
            return true;
        }
        else if(sLine==">>open classdef"||sLine==">>open classdef\r\n")
        {
            m_parser.UsingClass(true);
            return true;
        }
        else if(sLine==">>close classdef"||sLine==">>close classdef\r\n")
        {
            m_parser.UsingClass(false);
            return true;
        }
        else
        {
            //
            //if(std::strncmp(sLine.c_str(),">>list", 5))
            //{
            //	FindClassObject(m_parser,sLine.c_str()+4);
            //}
        }
        return false;
    }
    void CxParserRuntime::ShowHelp()
    {
        *m_stream<<"!-----------------help------------------- "<<"\r\n";
        *m_stream << "Commands:\r\n";
        *m_stream << "  list var     - list parser variables\r\n";
        *m_stream << "  list exprvar - list expression variables\r\n";
        *m_stream << "  list const   - list all numeric parser constants\r\n";
        *m_stream << "  exit         - exits the parser\r\n";

        *m_stream << "  list func    - list parser express function"<<"\r\n";
        *m_stream << "  list class   - list parser class define and class var"<<"\r\n";


        *m_stream << "  open collec  - open a vector to collection expression"<<"\r\n";
        *m_stream << "  close collec - close vector collection"<<"\r\n";
        *m_stream << "  clear collec - clear expression vector"<<"\r\n";
        *m_stream << "  run          - run collection expression"<<"\r\n";

        *m_stream << "  clear       -clear output screen\r\n"<<"\r\n";
        *m_stream << "  clear var   -clear variables"<<"\r\n";

        *m_stream << "  open classdef  - make parser to recognize class define"<<"\r\n";
        *m_stream << "  close classdef - close recognize class define"<<"\r\n";


        *m_stream << "  test        -self test "<<"\r\n";

        *m_stream << "Constants:\r\n";
        *m_stream << "  \"_e\"   2.718281828459045235360287\r\n";
        *m_stream << "  \"_pi\"  3.141592653589793238462643\r\n";
        *m_stream << "---------------------------------------\r\n";
        *m_stream << "Enter a formula or a command:\r\n";
    }
    //---------------------------------------------------------------------------

    //---------------------------------------------------------------------------
    void  CxParserRuntime::SelfTest()
    {
        mu::Test::ParserTester pt;
        pt.Run();
    }

    //---------------------------------------------------------------------------
    void CxParserRuntime::ClearAll()
    {
        m_parser.ClearClassObj();
        m_parser.ClearVar();
    }
    //---------------------------------------------------------------------------
    void  CxParserRuntime::ListVar(const mu::Parser  *Pparser)
    {
        //std::ostringstream os;
        // Query the used variables (must be done after calc)
        mu::varmap_type variables = Pparser->GetVar();
        if (!variables.size())
            return;
        *m_stream << "\nParser variables:\r\n";
        *m_stream <<   "-----------------\r\n";
        *m_stream << "Number: " << (int)variables.size() << "\r\n";
        varmap_type::const_iterator item = variables.begin();
        for (; item!=variables.end(); ++item)
        {
            *m_stream << "Name: " << item->first << "   Address: [0x" << item->second << "]  ";
            m_parser.SetExpr(item->first);
            *m_stream << "Result: "<<Pparser->Eval()<<"\r\n";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    void CxParserRuntime::ListVar()
    {
        ListVar(&m_parser);
    }
    //---------------------------------------------------------------------------
    void  CxParserRuntime::ListConst(const mu::Parser * Pparser)
    {
        //std::ostringstream os;
        *m_stream << "\nParser constants:\r\n";
        *m_stream <<   "-----------------\r\n";
        mu::valmap_type cmap = Pparser->GetConst();
        if (!cmap.size())
        {
            *m_stream << "Expression does not contain constants\r\n";
        }
        else
        {
            valmap_type::const_iterator item = cmap.begin();
            for (; item!=cmap.end(); ++item)
                *m_stream << "  " << item->first << " =  " << item->second << "\r\n";
        }
    }

    //---------------------------------------------------------------------------
    void  CxParserRuntime::ListExprVar(const mu::Parser * Pparser)
    {
        //std::ostringstream os;
        std::string sExpr = Pparser->GetExpr();
        if (sExpr.length()==0)
        {
            *m_stream << "Expression string is empty\r\n";
            return;
        }
        // Query the used variables (must be done after calc)
        *m_stream << "\nExpression variables:\r\n";
        *m_stream <<   "---------------------\r\n";
        *m_stream << "Expression: " <<Pparser->GetExpr() << "\r\n";
        varmap_type variables =Pparser->GetUsedVar();
        if (!variables.size())
        {
            *m_stream << "Expression does not contain variables\r\n";
        }
        else
        {
            *m_stream << "Number: " << (int)variables.size() << "\r\n";
            mu::varmap_type::const_iterator item = variables.begin();
            for (; item!=variables.end(); ++item)
                *m_stream << "Name: " << item->first << "   Address: [0x" << item->second << "]\r\n";
        }
    }
    //---------------------------------------------------------------------------
    void  CxParserRuntime::ListFunction(const mu::Parser  *pParser)
    {
        // Make the type of the map containing function prototypes visible
        using mu::funmap_type;
        funmap_type funmap = pParser->GetFunDef();
        funmap_type::const_iterator item;
        *m_stream << "\nFunctions available:\r\n";
        // iterate over all function definitions
        for (item=funmap.begin(); item!=funmap.end(); ++item)
        {
            *m_stream << "  " << item->first;   // This is the Function Name
            *m_stream << "(";
            // Deal with different argument numbers
            int iArgc = item->second.GetArgc();
            if (iArgc>=0)
            {
                // Indicate number of input variables
                for (int i=0; i<iArgc; ++i)
                {
                    char cVar[] = "val ";
                    cVar[3] = '1' + (char)i;
                    *m_stream << cVar;
                    if (i!=iArgc-1)
                        *m_stream << ",";
                }
            }
            else
            {
                *m_stream << "...";  // Multiargument function
            }
            *m_stream << ")\r\n";
        }

    }

    //----------------------------------------------------------------------------
    void CxParserRuntime::ListClass(mu::Parser &Pparser)
    {
        mu::classbasemap_type classmap = Pparser.GetClassMap();
        if(!classmap.size())
            return;
        *m_stream << "\nParser Class:\r\n";
        *m_stream <<   "-----------------\r\n";
        *m_stream << "Number: " << (int)classmap.size() << "\r\n";
        classbasemap_type::const_iterator item = classmap.begin();
        for (; item!=classmap.end(); ++item)
        {
            *m_stream << "Class Name: " << item->first << "   Address: [0x" << item->second << "]  "<< "\r\n";
            classbase *pclass=(item->second);

            *m_stream << "      Object Number: " <<  (int)pclass->size() << "\r\n";
            *m_stream << "      member Function Number: " <<  (int)pclass->funcsize() << "\r\n";

            for(int i=0;i<pclass->size();i++)
            {
                *m_stream << "            Object Name: " << pclass->getvar(i) <<"   Address: [0x"
                    << pclass->getvarpoint(i)<< "]  "<< "\r\n";
            }
            for(int i=0;i<pclass->funcsize();i++)
            {
                *m_stream << "             member Function: " << pclass->getfuncname(i) <<"   Type: ["
                    << pclass->getfunctype(i)<< "]  "<< "\r\n";
            }
        }
        *m_stream <<"\r\n";
    }

    void CxParserRuntime::ListClassONLYName()
    {
        mu::classbasemap_type classmap = m_parser.GetClassMap();
        if(!classmap.size())
            return;

        classbasemap_type::const_iterator item = classmap.begin();
        for (; item!=classmap.end(); ++item)
        {
            *m_stream << item->first << "\r\n";
        }
        *m_stream <<"\r\n";
    }

    void CxParserRuntime::ListClassONLYFUNCTION()
    {
        mu::classbasemap_type classmap = m_parser.GetClassMap();
        if(!classmap.size())
            return;

        classbasemap_type::const_iterator item = classmap.begin();
        for (; item!=classmap.end(); ++item)
        {
            classbase *pclass=(item->second);
            for(int i=0;i<pclass->funcsize();i++)
            {
                *m_stream << pclass->getfuncname(i)<< "\r\n";
            }
        }
        *m_stream <<"\r\n";
    }


    //////////////////////////////////////////////////////////////////////////
    void CxParserRuntime::ListClass()
    {
        ListClass(m_parser);
    }
    //----------------------------------------------------------------------------
    void CxParserRuntime::FindClassObject(mu::Parser &Pparser,const char *a_szClass)
    {
        string astrclass(a_szClass);

        mu::classbasemap_type classmap = Pparser.GetClassMap();
        if(!classmap.size())
            return;
        classbasemap_type::const_iterator item = classmap.begin();
        for (; item!=classmap.end(); ++item)
        {
            classbase *pclass=(item->second);
            if(item->first == astrclass)
            {
                for(int i=0;i<pclass->size();i++)
                {
                    *m_stream << "            Object Name: " << pclass->getvar(i) <<"   Address: [0x"
                        << pclass->getvarpoint(i)<< "]  "<< "\r\n";
                }
                for(int i=0;i<pclass->funcsize();i++)
                {
                    *m_stream << "             member Function: " << pclass->getfuncname(i) <<"   Type: ["
                        << pclass->getfunctype(i)<< "]  "<< "\r\n";
                }
            }
        }
        *m_stream <<"\r\n";
    }
    //----------------------------------------------------------------------------
    bool CxParserRuntime::IsObject(mu::Parser &Pparser,const char *a_szClassObject)
    {
        string astrclassobj(a_szClassObject);
        mu::classbasemap_type classmap = Pparser.GetClassMap();
        if(!classmap.size())
            return false;
        classbasemap_type::const_iterator item = classmap.begin();
        for (; item!=classmap.end(); ++item)
        {
            classbase *pclass=(item->second);
            for(int i=0;i<pclass->size();i++)
            {
                if(astrclassobj == pclass->getvar(i))
                    return true;
            }
        }
        return false;
    }
    //----------------------------------------------------------------------------
    bool CxParserRuntime::IsObjectVar(const char *a_sz)
    {
        return IsObject(m_parser,a_sz);
    }
    //----------------------------------------------------------------------------
    void CxParserRuntime::ListFormula(mu::Parser &Pparser)
    {
        mu::string_type astring=Pparser.GetFormula();
        *m_stream << " Formula: " <<  astring << "\r\n";
    }
    //---------------------------------------------------------------------------
    void CxParserRuntime::SetStream(std::ostream *a_stream)
    {
        assert(a_stream);
        m_stream = a_stream;
    }
    //---------------------------------------------------------------------------
    void CxParserRuntime::SetCreateCodeStream(std::ostream *a_stream)
    {
        assert(a_stream);
        m_createstream = a_stream;
    }
    //---------------------------------------------------------------------------
    bool CxParserRuntime::Compile(const char *a_szLine)
    { 
        try
        {
            if (CommandLine(a_szLine))
            {
                m_iget=0;
                return 1;
            }
            //else
            {
                SetExpr(a_szLine);
                *m_stream<<"Result:"<<Eval()<<"\r\n";

                *m_stream<<"================Build: 1 OK , 0 Fail ============"<<"\r\n";
            }
        }
        catch(mu::Parser::exception_type &e)
        {

            *m_stream  << "\nError: ";
            //*m_stream  << "------\r\n";
            *m_stream  << " Message:  " << e.GetMsg() << "\r\n";
            //*m_stream  << " Formula:  " << e.GetExpr() << "\r\n";
            *m_stream  << " Token: " << e.GetToken() ;// << "\r\n";
            *m_stream  << " Position: " << (int)e.GetPos() << "\r\n";
            *m_stream  << " Errc: " << e.GetCode()  << "\r\n";
            *m_stream  <<"================Build: 0 OK , 1 Fail ============"<<"\r\n";
            return 0;
        }
        return 1;
    }
     
    void CxParserRuntime::ResetRun()
    {
        mu::Parser &Pparser=m_parser;
        mu::classbasemap_type classmap = Pparser.GetClassMap();
        if(!classmap.size())
            return;
        classbasemap_type::const_iterator item = classmap.end();
         
        for (; item!=classmap.begin(); )
        {
            --item;
            if("GDIimage32"==item->first)
            {
                return;
            }
        }
    }
    //---------------------------------------------------------------------------
    void CxParserRuntime::DragImageParserElement(int ipointx,int ipointy)
    {
        (void)ipointx;
        (void)ipointy;
        mu::Parser &Pparser=m_parser;
        mu::classbasemap_type classmap = Pparser.GetClassMap();
        if(!classmap.size())
            return;
        classbasemap_type::const_iterator item = classmap.end();
         
    }
    //---------------------------------------------------------------------------
    void CxParserRuntime::HitTestImageParserElement(int ipointx,int ipointy)
    {
        (void)ipointx;
        (void)ipointy;
        mu::Parser &Pparser=m_parser;
        mu::classbasemap_type classmap = Pparser.GetClassMap();
        if(!classmap.size())
            return;
        classbasemap_type::const_iterator item = classmap.end();

        for (; item!=classmap.begin(); )
        {
            --item;
            if("CFindPoint"==item->first)
            {
#ifdef USE_CROIMeasurePoint
#endif
            }

        } 
    }
    //---------------------------------------------------------------------------
    void CxParserRuntime::MouseDownParserElement(int PointX,int PointY)
    {
        (void)PointX;
        (void)PointY;
        mu::Parser &Pparser=m_parser;
        mu::classbasemap_type classmap = Pparser.GetClassMap();
        if(!classmap.size())
            return;
        classbasemap_type::const_iterator item = classmap.end();

        for (; item!=classmap.begin(); )
        {
            --item;
            if("CFastMatch"==item->first)
            {
#ifdef USE_CFastMatch

#endif
            }

        }

    }
    void CxParserRuntime::MouseUpParserElement(int PointX,int PointY)
    {
        (void)PointX;
        (void)PointY;
        mu::Parser &Pparser=m_parser;
        mu::classbasemap_type classmap = Pparser.GetClassMap();
        if(!classmap.size())
            return;
        classbasemap_type::const_iterator item = classmap.end();

        for (; item!=classmap.begin(); )
        {
            --item;
            if("CFastMatch"==item->first)
            {
#ifdef USE_CFastMatch

#endif
            }

        }

    }


    bool CxParserRuntime::MouseRDownParserElement()
    {

        mu::Parser &Pparser=m_parser;
        mu::classbasemap_type classmap = Pparser.GetClassMap();
        if(!classmap.size())
            return false;
        classbasemap_type::const_iterator item = classmap.end();

        for (; item!=classmap.begin(); )
        {
            --item;
            if("CPolygonShape"==item->first)
            {

            }
        }

        return false;
    }

    //----------------------------------------------------------------------------
    void CxParserRuntime::StopRun()
    {
        m_parser.RunStop();
    }
    //----------------------------------------------------------------------------
    void CxParserRuntime::SetRunOk()
    {
        m_parser.RunOk();
    }
    //----------------------------------------------------------------------------
    void CxParserRuntime::GetImageObjectAtt()//
    {
        mu::Parser &Pparser=m_parser;
        mu::classbasemap_type classmap = Pparser.GetClassMap();
        if(!classmap.size())
            return;
        classbasemap_type::const_iterator item = classmap.end();

        for (; item!=classmap.begin(); )
        {
            --item;
            classbase *pclass=(item->second);
            if("double"==item->first)
            {
                double *adouble=0;
                for(int i=0;i<pclass->size();i++)
                {
                    adouble=(double *)pclass->getvarpoint(i);
                    if(adouble)//if(_finite(*adouble))
                    {
                        *m_createstream << pclass->getvar(i) <<"="<<*adouble<<";\r\n";
                    }
                }
            }
        }
    }
    //----------------------------------------------------------------------------
    void CxParserRuntime::GetImageObjectAutoSave()//
    {
        mu::Parser &Pparser=m_parser;
        mu::classbasemap_type classmap = Pparser.GetClassMap();
        if(!classmap.size())
            return;
        classbasemap_type::const_iterator item = classmap.end();

        for (; item!=classmap.begin(); )
        {
            --item;
            classbase *pclass=(item->second);
            if("double"==item->first)
            {
                double *adouble=0;
                for(int i=0;i<pclass->size();i++)
                {
                    adouble=(double *)pclass->getvarpoint(i);
                    if(adouble)
                    {
                        *m_createstream << pclass->getvar(i) <<"="<<*adouble<<";\r\n";
                    }
                }
            }
            else if("Shape"==item->first)
            {
                Shape*ashape=0;
                for(int i=0;i<pclass->size();i++)
                {
                    ashape=(Shape*)pclass->getvarpoint(i);
                    if(ashape)
                    {
                        *m_createstream << pclass->getvar(i)
                                        <<".setrect("\
                                        <<ashape->rect().TopLeft().X()<<","
                                        <<ashape->rect().TopLeft().Y()<<","
                                        <<ashape->rect().Width()<<","
                                        <<ashape->rect().Height()<<")"
                                        <<";\r\n";
                    }
                }
            }
            else if("ShapeBase"==item->first){}
            else if("LineShape"==item->first){}
            else if("Findline"==item->first)
            {
                Findline*ashape=0;
                for(int i=0;i<pclass->size();i++)
                {
                    ashape=(Findline*)pclass->getvarpoint(i);
                    if(ashape)
                    {
                        *m_createstream << pclass->getvar(i) \
                                        <<".setrect("\
                            << ashape->rect().TopLeft().X() << ","
                            << ashape->rect().TopLeft().Y() << ","
                            << ashape->rect().Width() << ","
                            << ashape->rect().Height() << ")"
                                        <<";\r\n";
                    }
                }
            } 
        }
    }


    //--------------------------------------------------------------
    void CxParserRuntime::CreateClassDef(const char *pclassname,											const char *pclassdef)
    {
        m_parser.DefineCreateClass(pclassname,pclassdef);
    }
    void CxParserRuntime::CreateClassFunc(const char *pclassname,
                                            const char *pclassfucname,
                                            const char *pclassfucdef)
    {
        m_parser.DefineCreateClasFun(pclassname,pclassfucname,pclassfucdef);
    }
    void CxParserRuntime::ListCreateClassDef(mu::Parser &Pparser
        ,const char *pclassname)
    {
        string_type astr(pclassname);
        string_type strclass;
        CreateClass *paclass;
        int iclassmemsum = 0;
        mu::classbasemap_type classmap = Pparser.GetClassMap();
        if(!classmap.size())
            return;

        classbasemap_type::const_iterator item = classmap.begin();
        for (; item!=classmap.end(); ++item)
        {
            classbase *pclass=(item->second);
            strclass= pclass->getclass();
            if(astr == strclass)
            if(pclass->Iscreateclass())
            {
                paclass = (CreateClass*)pclass;
                iclassmemsum = paclass->GetClassMemberNum();
                for(int i=0;i<iclassmemsum;i++)
                {
                    *m_stream<<paclass->GetClassDefName(i)<<" "<<paclass->GetClassMemberName(i) <<";\r\n";
                }
            }
        }
        *m_stream <<"\r\n";
    }
    void CxParserRuntime::ListCreateClassFunDef(mu::Parser &Pparser
       ,const char *pclassname
       ,const char *pclassfuncname)
    {
        string_type astr(pclassname);
        string_type strclass;
        CreateClass *paclass;
        int iclassmemsum=0;
        mu::classbasemap_type classmap = Pparser.GetClassMap();
        if(!classmap.size())
            return;

        classbasemap_type::const_iterator item = classmap.begin();
        for (; item!=classmap.end(); ++item)
        {
            classbase *pclass=(item->second);
            strclass= pclass->getclass();
            if(astr == strclass)
                if(pclass->Iscreateclass())
                {
                    paclass = (CreateClass*)pclass;
                    *m_stream<< paclass->GetFuncDef(pclassfuncname)<<"\r\n";
                }
        }
        *m_stream <<"\r\n";
    }
    void CxParserRuntime::ListCreateClassDef(const char *pclassname)
    {
        ListCreateClassDef(m_parser,pclassname);
    }
    void CxParserRuntime::ListCreateClassFunDef(const char *pclassname
        ,const char *pclassfuncname)
    {
        ListCreateClassFunDef(m_parser,pclassname,pclassfuncname);
    }
}

