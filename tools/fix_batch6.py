import os

files_to_fix = [
    (r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\ManualStateTestConsole.cpp', [
        ('FastMatch_', 'fastmatch_'),
    ]),
]

for file_path, replacements in files_to_fix:
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
        print(f"Fixed: {file_path}")
    else:
        print(f"No changes: {file_path}")