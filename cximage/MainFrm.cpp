#include "MainFrm.h"

//OCCElementView* MainFrm::m_pOCCView;
//OCCSubView* MainFrm::m_pOCCViewSub;
CElementBase* MainFrm::focusEle;
list<CElementBase*> MainFrm::list_Elements;//元素列表
list<CElementBase*> MainFrm::selectElements;//参与构造元素点集
list<CElementBase*> MainFrm::fullElements;//所有的元素集(包含扫描)
vector<CElementBase*> MainFrm::mySolutionList;
//MySynchrosTSK MainFrm::tcs;
MainFrm::MainFrm()
{
//	opticalDlgStep1 = NULL;
//	platDlgStep1 = NULL;
//	verticalDlgStep1 = NULL;
//	m_pDlgStep1 = NULL;
  
	m_pProc = NULL;
}
MainFrm::~MainFrm()
{ 
 
}
void MainFrm::AddElement(ElementType typeName, ShapeRecognition rec, tuple<string, string, string> tuples, bool isUpdate)
{
	CElementBase* ptr_Ele = InitElement(typeName, rec, tuples);
	if (nullptr == ptr_Ele)
	{
		return;
	}
	if (!rec.IsScan)
	{
		focusEle = ptr_Ele;
		if (focusEle->m_strElementUid == "" || focusEle->m_strElementUid == get<0>(tuples))
		{
			list_Elements.push_back(focusEle);//将元素添加到元素列表
		}
	}
	else
	{
		TryAddToListElement(fullElements, ptr_Ele);
	}

}
void MainFrm::ShowSolutionDialog(vector<CElementBase*> solutionList)
{
	string sulutionsCheck[4]{ "MIN","MID","MAX","SUPPER" };
	//CMainFrame* pMainFrame = (CMainFrame*)AfxGetMainWnd();  // 获取主窗口指针
	//if (pMainFrame)
	{
		//CMSolutionEx dlg(pMainFrame);
		//HWND pDesktopWnd = ::GetDesktopWindow();
		//ASSERT(pMainFrame == pDesktopWnd || ::IsChild(pDesktopWnd, pMainFrame)); // 调试检查
		//dlg.SetDocument(solutionView);  // 初始化文档对象
		//dlg.highLightUid = solutionList.at(0)->m_strElementUid;
		for each(CElementBase * obj in solutionList)
		{
			/*string uids = msclr::interop::marshal_as<std::string>(System::Guid::NewGuid().ToString("N"));
			obj->m_strElementUid = uids;*/
			auto myShape = obj->Draw();
			if (!myShape)
				continue;
			if (obj->Tag == Special)
			{
				myShape->SetSelect(true);
				if (obj->solution_Tag != "")
				{/*
					for each(VMLib::Models::SolutionEntity ^ solut in((List<VMLib::Models::SolutionEntity^>^)VMLib::Models::SolutionSetting::solutionList))
					{
						string solutType = msclr::interop::marshal_as<std::string>(solut->solutionType.ToString());
						if (obj->solution_Tag._Equal(solutType))
						{
							string solutSubType = msclr::interop::marshal_as<std::string>(solut->solutionSubType.ToString());
							if (solutSubType._Equal(sulutionsCheck[obj->solutionSubTag]))
							{
								dlg.highLightUid = obj->m_strElementUid;
								break;
							}
						}
					}
				*/}
			}

			myShape->SetName(obj->m_strElementUid);
			//dlg.myshape.push_back(myShape);
		}

		mySolutionList = solutionList;
		//dlg.SetOKCallback(SolutionOkCallBack);
		//dlg.DoModal();
	}
}
void MainFrm::TryAddToListElement(list<CElementBase*>& elements, CElementBase* element)
{
	string uid = element->m_strElementUid;
	auto findIdx = find_if(elements.begin(), elements.end(), [uid](const CElementBase* ele)
		{
			return ele->m_strElementUid == uid;
		});
	int i = -1;
	//返回索引
	if (findIdx != elements.end())
		i = distance(elements.begin(), findIdx);
	if (i != -1)
	{
		auto it = elements.begin();
		std::advance(it, i);
		// 修改值
		*it = element;
		//elements[i] = element;
	}
	else
		elements.push_back(element);
}

void MainFrm::OCCViewEraseAll()
{
	//m_pOCCView->RemoveAllShapes();
	//m_pOCCView->occ_Shapes.clear();
	//m_pOCCViewSub->RemoveAllShapes();
	//m_pOCCViewSub->occ_Shapes.clear();
	//m_pOCCView->ViewerUpDate();
	//m_pOCCViewSub->ViewerUpDate();
}

CElementBase* MainFrm::InitElement(ElementType typeName, ShapeRecognition rec, tuple<string, string, string> tuples)
{
	CElementBase* ptr_Element = ElementFactory::EleCreate(typeName);
	if (nullptr == ptr_Element)
	{
		return ptr_Element;
	}
	string uid = get<0>(tuples);
	string name = get<1>(tuples);
	string drawMode = get<2>(tuples);
	ptr_Element->EleInit(fullElements.size(), uid, rec);
	ptr_Element->SetSlutionCallBack(ShowSolutionDialog); //设置回调函数
	ptr_Element->m_strElementName = name;
	ptr_Element->m_bIsScan = rec.IsScan;
	ptr_Element->m_strDrawMode = drawMode;
	return ptr_Element;
	//list_Elements.push_back(ptr_Element.get());
}
void MainFrm::EraseALL()
{
	OCCViewEraseAll();
	list_Elements.clear();
	selectElements.clear();
	fullElements.clear();
}

