import os

header_files = [
    (r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\FindLine.h', 'Findline', 'FindLine'),
    (r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\FindCircle.h', 'Findcircle', 'FindCircle'),
    (r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\FindEllipse.h', 'Findellipse', 'FindEllipse'),
    (r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\FastMatch.h', 'fastmatch', 'FastMatch'),
    (r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\FindObject.h', 'findobject', 'FindObject'),
]

for file_path, old_name, new_name in header_files:
    if not os.path.exists(file_path):
        print(f"File not found: {file_path}")
        continue
    
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original_content = content
    
    content = content.replace(f'class {old_name};', f'class {new_name};')
    content = content.replace(f'class {old_name}:', f'class {new_name}:')
    content = content.replace(f'{old_name}();', f'{new_name}();')
    content = content.replace(f'~{old_name}();', f'~{new_name}();')
    content = content.replace(f'{old_name}::', f'{new_name}::')
    content = content.replace(f'{old_name}DisplaySnapshot', f'{new_name}DisplaySnapshot')
    content = content.replace(f'{old_name}MeasureGeometryRequest', f'{new_name}MeasureGeometryRequest')
    content = content.replace(f'{old_name}MeasureInputDebug', f'{new_name}MeasureInputDebug')
    content = content.replace(f'{old_name}MeasureResult', f'{new_name}MeasureResult')
    
    if content != original_content:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Fixed class names in: {file_path}")
    else:
        print(f"No changes: {file_path}")