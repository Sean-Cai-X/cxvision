import os

files_to_fix = [
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\CxScriptRuntimeResultCapture.cpp',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\ViewController.cpp',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\ManualConsoleGauge.cpp',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\ManualConsoleRuntimeView.cpp',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\ManualConsoleCxScriptDebug.cpp',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\ManualConsoleEvidenceChain.cpp',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\ManualConsoleParamRegressionPanel.cpp',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\CxScriptCasePackageWriter.cpp',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\CxScriptHeadlessRunner.cpp',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\Run.cpp',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\GuiMain.cpp',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\ParserClass.cpp',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\CxCoreBoundary.cpp'
]

replacements = [
    ('#include "Findline.h"', '#include "FindLine.h"'),
    ('#include "Findcircle.h"', '#include "FindCircle.h"'),
    ('#include "Findellipse.h"', '#include "FindEllipse.h"'),
    ('#include "fastmatch.h"', '#include "FastMatch.h"'),
    ('#include "findobject.h"', '#include "FindObject.h"')
]

for file_path in files_to_fix:
    if not os.path.exists(file_path):
        print(f"File not found: {file_path}")
        continue
    
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original_content = content
    
    for old, new in replacements:
        content = content.replace(old, new)
    
    if content != original_content:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Fixed includes: {file_path}")
    else:
        print(f"No changes: {file_path}")