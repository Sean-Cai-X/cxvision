import os

header_files_to_fix = [
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\FindLine.h',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\FindCircle.h',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\FindEllipse.h',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\FastMatch.h',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\FindObject.h',
]

replacements = [
    ('#include "Findline.h"', '#include "FindLine.h"'),
    ('#include "Findcircle.h"', '#include "FindCircle.h"'),
    ('#include "Findellipse.h"', '#include "FindEllipse.h"'),
    ('#include "fastmatch.h"', '#include "FastMatch.h"'),
    ('#include "findobject.h"', '#include "FindObject.h"'),
]

for file_path in header_files_to_fix:
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
        print(f"Fixed includes in: {file_path}")
    else:
        print(f"No changes: {file_path}")