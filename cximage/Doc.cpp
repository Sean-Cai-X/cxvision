 #include "Doc.h"

 

// Doc 构造/析构
Doc::Doc() 
{
	Handle(Aspect_DisplayConnection) aDisplayConnection;
	m_GraphicDriver = new OpenGl_GraphicDriver(aDisplayConnection);
 
	Handle(Graphic3d_GraphicDriver) theGraphicDriver = GetGraphicDriver();

	auto myViewer_OCC = new V3d_Viewer(theGraphicDriver);
	myViewer_OCC->SetDefaultLights();
	myViewer_OCC->SetLightOn();
	auto myViewer_Sub = new V3d_Viewer(theGraphicDriver);
	myViewer_Sub->SetDefaultLights();
	myViewer_Sub->SetLightOn();
	auto myViewer_Solut = new V3d_Viewer(theGraphicDriver);
	myViewer_Solut->SetDefaultLights();
	myViewer_OCC->SetLightOn();

	Handle(AIS_InteractiveContext) myAISContext_OCC = new AIS_InteractiveContext(myViewer_OCC);
	Handle(AIS_InteractiveContext) myAISContext_Sub = new AIS_InteractiveContext(myViewer_Sub);
	Handle(AIS_InteractiveContext) myAISContext_Solut = new AIS_InteractiveContext(myViewer_Solut);
	//BoundBox();
	//myAISContext->SetDisplayMode(AIS_Shaded, true);   //实体显示模式
	InitShowStyle(myAISContext_OCC,0);
	InitShowStyle(myAISContext_Sub,1);
	InitShowStyle(myAISContext_Solut,2);
	myAISContextMap.insert({ "m_pOCCView", myAISContext_OCC });
	myAISContextMap.insert({ "m_pOCCViewSub", myAISContext_Sub });
	myAISContextMap.insert({ "m_pOCCViewSolut", myAISContext_Solut });
	myViewerMap.insert({ "m_pOCCView", myViewer_OCC });
	myViewerMap.insert({ "m_pOCCViewSub", myViewer_Sub });
	myViewerMap.insert({ "m_pOCCViewSolut", myViewer_Solut });
}

void Doc::InitShowStyle(Handle(AIS_InteractiveContext)& myAISContext,int index)
{
	myAISContext->SetDisplayMode(AIS_WireFrame, true);  //线框显示模式
	myAISContext->SetAutomaticHilight(Standard_True);


	//设置高亮模型
	Handle(Prs3d_Drawer) aHighlightStyle = myAISContext->HighlightStyle(); // 获取高亮风格
	Handle(Prs3d_LineAspect) highlightStyle = aHighlightStyle->LineAspect();
	highlightStyle->SetWidth(3.0);
	aHighlightStyle->SetMethod(Aspect_TOHM_COLOR);                      // 颜色显示方式              
	aHighlightStyle->SetColor(Quantity_NOC_DEEPPINK);         // 设置高亮颜色  
	aHighlightStyle->SetDisplayMode(1); // 1整体高亮 2包围盒
	myAISContext->SetHighlightStyle(aHighlightStyle);
	//选择时高亮模型
	Handle(Prs3d_Drawer) aSelectionStyle = myAISContext->SelectionStyle();  // 获取选择风格
	Handle(Prs3d_LineAspect) lineStyle = aSelectionStyle->LineAspect();
	lineStyle->SetWidth(3.0);
	aSelectionStyle->SetMethod(Aspect_TOHM_COLOR);  // 颜色显示方式
	if(index == 2)
		aSelectionStyle->SetColor(Quantity_NOC_SLATEBLUE);
	else
		aSelectionStyle->SetColor(Quantity_NOC_WHITE);   // 设置选择后颜色
	aSelectionStyle->SetDisplayMode(1); // 整体高亮
	myAISContext->SetSelectionStyle(aSelectionStyle);

}

Doc::~Doc()
{
}

 

Handle(V3d_Viewer) Doc::GetViewer(std::string name)
{
	return myViewerMap[name];
}

// Doc 构造/析构
void Doc::ShowShape(const Handle(AIS_InteractiveObject)& AShapes, std::string name, Standard_Boolean isShow)
{
	myAISContextMap[name]->Display(AShapes, isShow);
}

void Doc::UpDateViewer(std::string name)
{
	myAISContextMap[name]->UpdateCurrentViewer();
}

void Doc::RemoveShape(string name, const Handle(AIS_InteractiveObject)& AShape)
{
	myAISContextMap[name]->Erase(AShape, Standard_False);
}

 
 
