import os

files_to_fix = [
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\CxScriptRuntimeResultCapture.h',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\CxScriptRuntimeResultCapture.cpp',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\ViewController.h',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\ViewController.cpp'
]

for file_path in files_to_fix:
    if not os.path.exists(file_path):
        print(f"File not found: {file_path}")
        continue
    
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    content = content.replace('class Findline;', 'class FindLine;')
    content = content.replace('class Findcircle;', 'class FindCircle;')
    content = content.replace('class Findellipse;', 'class FindEllipse;')
    content = content.replace('class fastmatch;', 'class FastMatch;')
    content = content.replace('Findline*', 'FindLine*')
    content = content.replace('Findcircle*', 'FindCircle*')
    content = content.replace('Findellipse*', 'FindEllipse*')
    content = content.replace('fastmatch*', 'FastMatch*')
    content = content.replace('fastmatch::', 'FastMatch::')
    content = content.replace('Findline::', 'FindLine::')
    content = content.replace('Findcircle::', 'FindCircle::')
    content = content.replace('Findellipse::', 'FindEllipse::')
    content = content.replace('CxScriptTypeTraits<Findline>', 'CxScriptTypeTraits<FindLine>')
    content = content.replace('CxScriptTypeTraits<Findcircle>', 'CxScriptTypeTraits<FindCircle>')
    content = content.replace('CxScriptTypeTraits<Findellipse>', 'CxScriptTypeTraits<FindEllipse>')
    content = content.replace('CxScriptTypeTraits<fastmatch>', 'CxScriptTypeTraits<FastMatch>')
    
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print(f"Fixed: {file_path}")