
#ifndef _MainFrm_Header
#define _MainFrm_Header

#include "../ElementProcess/ElementFactory.h"
#include "../ImageProcess/IImageProcess.h"

class MainFrm
{
	MainFrm();
	~MainFrm();


	static list<CElementBase*> list_Elements;//元素列表
	static CElementBase* focusEle;// 当前选中的元素
	static list<CElementBase*> selectElements;//参与构造元素点集
	static list<CElementBase*> fullElements;//所有的元素集(包含扫描)
	static vector<CElementBase*> mySolutionList; //解法设定列表
	static bool DrawManualPoint();							//取点
	static bool DrawRectPoint(const cv::Mat& matInput, float fRatio);//矩形框取点							//方形框取点
	static bool DrawCirclePoint(Mat matInput, float fRatio);	//圆形框取点
	static bool DrawLine(Mat matInput, float fRatio);       //线段扫描
	static bool DrawCircle(Mat matInput, float fRatio);     //圆扫描框，三点取圆
	static bool DrawArc(Mat matInput, float fRatio);        //圆弧扫描框，三点确定圆弧
	static bool DrawRect(Mat matInput, float fRatio);       //矩形扫描框，两个点去顶矩形
	static bool DrawEllipse(Mat matInput, float fRatio);    //椭圆扫描框
	static bool DrawORing(Mat matInput, float fRatio);      //O-Ring扫描框
	static bool DrawContourCircle(Mat matInput, float fRatio);//轮廓圆扫描框
	static bool DrawClosedContour(Mat matInput, float fRatio);//封闭轮廓扫描框
	static bool GetNearestPoint(Mat matInput, float fRatio); //获取最近点
	static bool GetFarthestPoint(Mat matInput, float fRatio);//获取最远点
	//static System::Numerics::Vector3 GetCloseFollower(VMLib::ViewModels::OnlineImgInfo^ OnlineImg);//闭合型元素扫描
	//static System::Numerics::Vector3 GetOpenFollower(VMLib::ViewModels::OnlineImgInfo^ OnlineImg);//开放型元素扫描


	//图像处理部分
	IImageProcess* m_pProc;

	//static Point ProcessManualPoint(const string strImagePath, Point ptInput, Point& ptOutput);

	//圆形框，获取离圆心最近的边缘点
	static vector<Point2f> GetPointFromCircleArea(const Mat& imgInput, cv::Point ptCenter, float fRadius, cv::Point& ptOutput);

	//矩形框取点，
	static vector<cv::Point2f> GetPointFromRectArea(const Mat& imgInput, const vector<cv::Point2f>& ptIn, float nThickness, cv::Point2f& ptOutput);

	//圆形框
	static vector<Point2f> ProcessLineSegment(const Mat& imgInput, double scale, POINT pt1, POINT pt2,
		POINT& ptOutStart, POINT& ptOutEnd, bool& bIsBlackToWhite);

	//返回拟合后圆心、半径和外接正方形
	static vector<Point2f> ProcessCircle(const Mat& imgInput, Point2f ptArr[], int nThickness, Point2f& ptCenter,
		double& dRadius, Rect2f& rcBoundingCircle, bool& bIsBlackToWhite);

	//圆弧扫描框
	static vector<Point2f> ProcessArc(const Mat& imgInput, cv::Point ptArr[], int nThickness, cv::Point& ptCenter,
		double& dRadius, cv::Point& ptStart, cv::Point& ptEnd, cv::Point& ptArcCen, bool& bIsBlackToWhite);

	//矩形扫描框
	static vector<Point2f> ProcessRectRing(const Mat& imgInput, const vector<cv::Point>& ptIn, int nThickness,
		double& angle, Point2f& ptOutStart, Point2f& ptOutEnd, bool& IsFromBlackToWhite);

	//离散化
	static vector<Point2f> ProcRectPoints(vector<Point2f> vecPoints, int nNum, Point2f ptStart, Point2f ptEnd, void* pProcess);

	//椭圆扫描框
	static vector<Point2f> ProcessEllipseRing(const Mat& imgInput, cv::Point ptArr[], int nThickness, RotatedRect& rotRect, bool& bIsBlackToWhite);
	//Point& ptCenter, double& radiusL, double& radiusS, double& dAngle);

//O-ring扫描框
	static vector<Point2f> ProcessORing(const Mat& imgInput, cv::Point ptInput, double dRadius, cv::Point& ptCenter1,
		double& dRadius1, cv::Point& ptCenter2, double& dRadius2, Rect& rcBoundingCircle);

	//轮廓圆扫描框
	static vector<Point2f> ProcessContourCircle(const Mat& imgInput, Point2f ptInput, double dRadius,
		Point2f& ptCenterOutput, double& dRadiusOutput, Rect2f& rcBoundingCircle);

	//闭合轮廓扫描框
	static vector<Point2f> ProcessClosedContour(const Mat& imgInput, Point2f ptInput, double dRadius);

	//最近点:距离提取框指定一侧框线的最近边缘点
	static vector<Point2f> GetNearestPointInROI(const Mat& imgInput, cv::Point ptInput[], int nThickness, Point2f& ptOutput);

	//最远点:距离提取框指定一侧框线的最近点所在轮廓中的最远边缘点
	static vector<Point2f> GetFarthestPointInROI(const Mat& imgInput, cv::Point ptInput[], int nThickness, Point2f& ptOutput);

	//开放云线扫描
	static int ProcessOpenFollower0(vector<Point2f>& ptMcs, vector<Point2f>& ptClick, double distThresh);
	static int ProcessOpenFollowerN(vector<Point2f>& ptAll, vector<Point2f>& ptMcs,
		const vector<Point2f>& ptClick, double distThresh);

	static int AutoFocus();//自动对焦
	static UINT FocusThreadProc(LPVOID pParam);

	int AutoFucosForOpticalCali();
	// 重写


	static void SolutionOkCallBack(const std::string& uid);
	static void SeletShapesCallBack(vector<std::string> uids);
	static void ZoomViewCallBack(bool IsUp);
	static void ShowSolutionDialog(vector<CElementBase*> solutionList);
	//static void BtnSetEle(VMLib::Models::EnumTypes::ElementType btnName, ShapeRecognition rec, tuple<string, string, string> tuples, bool isUpdate = false);
	//static void BtnSetExtool(VMLib::Models::EnumTypes::ToolType btnName, bool isMulPointInput);
	static void AddElement(ElementType typeName, ShapeRecognition rec, tuple<string, string, string> tuples, bool isUpdate = false);
	//static void EleChangeFunc(VMLib::Models::Elements::ElementBase^ curentEle);
	static CElementBase* InitElement(ElementType typeName, ShapeRecognition rec, tuple<string, string, string> tuples);
	static bool SetupView(int i);
	static bool ZoomSubView(int value);
	static bool ZoomView(int i);
	//static void SetFocusEleLable(System::String^ name, bool IsShow);
	static void EraseALL();
	//选中偏差图对应元素形状
	//static void FocusSubElement(System::String^ eleUid);
	//pcs视图设置
	//static void SetPcsView(System::String^ eleCoorUid);
	static void CloseWindow(int nCode);
	//将元素视图导出到报告
	//static System::Windows::Media::Imaging::BitmapSource^ ExportElemntView();
	//static bool CreateEleFunc(VMLib::Models::Elements::ElementBase^ elementBase, bool isUpdate);
	//static void StructuralEleFunc(VMLib::Models::Elements::ElementBase^ curElement, VMLib::Models::Elements::ElementBase^ selectElement, VMLib::Models::EnumTypes::ToolType);
	//static void StructuralEleFunc_Multiple(VMLib::Models::Elements::ElementBase^ curElement, Dictionary<VMLib::Models::EnumTypes::ToolType, List<VMLib::Models::Elements::ElementBase^>^>^ selectElements);
	//static void DeleteEleFunc(VMLib::Models::Elements::ElementBase^ elementBase);
	//static void ResetElementFunc(VMLib::Models::Elements::ElementBase^ elementBase);
	//static void ResetElementInfo(CElementBase* element, VMLib::Models::Elements::ElementBase^ wpfElement);
	//元素偏移函数
	//static void ElementOffsetFunc(VMLib::Models::Elements::ElementBase^ elementBase);
	//元素偏移函数
	//static bool ElementUpdateFunc(VMLib::Models::Elements::ElementBase^ UpdateEle);
	//查找上一个坐标系
	static CElementBase* FindPrevCoordSys(CElementBase* curElement);
	//添加构造
	static void AddToolType(CElementBase* element, ToolType toolType, ElementType selectEleType, bool isMulPointInput);
	//执行构造函数
	static void HandleTools(CElementBase*& lastElement, list<CElementBase*> selectEles, list<CElementBase*> elements, list<CElementBase*> fullElments, bool isMulPoint);
	//获取集合匹配id的元素
	static CElementBase* GetCurElement(list<CElementBase*> elements, string curUid);
	//添加到元素列表
	static void TryAddToListElement(list<CElementBase*>& elements, CElementBase* element);
	//构造元素
	static void StructuralElement(CElementBase* element);
	//转换元素类型
	//static void ChangeElementType(ElementType curEleType, VMLib::Models::EnumTypes::ElementType& reslutEleType);
	//static void ChangeElementToolType(ToolType curToolType, VMLib::Models::EnumTypes::ToolType& reslutToolType);
	//设置元素信息
	//static void SetElementInfo(CElementBase* element, VMLib::Models::Elements::ElementBase^ elementBase);
//	static void AddComponentEle(CElementBase*& lastElement, list<CElementBase*> selectEles, list<CElementBase*> elements, list<CElementBase*> fullElments,
//		VMLib::Models::Elements::ElementBase^ elementBase);
	//添加toolType
	//static void AddToolType(CElementBase* element, VMLib::Models::Elements::ElementBase^ elementBase);
	//导入解析元素
	static void ImportElements(vector<CElementBase*> newElements);

//	static double GetToler(VMLib::Models::Elements::ElementBase^ element, int tolerType, int elementType);
//	static double GetToler_Position(VMLib::Models::Elements::ElementBase^ element, int tolerType, int elementType, double tolerance, tuple<vector<double>, int, int, bool> tupleDatum, bool isDatum);
//	static bool GetElementToler(VMLib::Models::Elements::ElementBase^ element, VMLib::Models::ToleranceEntity^ eleToler);
//	static int GetElementType(VMLib::Models::EnumTypes::ElementType elementType);
//	static int GetEleTolerType(VMLib::Models::EnumTypes::ElementTolerance tolerType);
	static void OCCViewEraseAll();

	static void doOpticalCali();
	static void doPlatFormCali();
	static void doVerticalCali();
	static void doLightCali();
	static void OpenFileNew();
	static void OpenFileOld();
	static void FileSave();
	static void FileSaveAs();
	static void FileImportIWP();
	static void OnEleSave();

};
#endif