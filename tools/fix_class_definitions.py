import os
import re

header_files = [
    (r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\FindLine.h', 'Findline', 'FindLine'),
    (r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\FindCircle.h', 'Findcircle', 'FindCircle'),
    (r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\FindEllipse.h', 'Findellipse', 'FindEllipse'),
]

for file_path, old_name, new_name in header_files:
    if not os.path.exists(file_path):
        print(f"File not found: {file_path}")
        continue
    
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original_content = content
    
    content = re.sub(r'class\s+' + re.escape(old_name) + r'\s*:', f'class {new_name}:', content)
    
    if content != original_content:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Fixed class definition in: {file_path}")
    else:
        print(f"No changes: {file_path}")