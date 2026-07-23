import os
import re

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CXIMAGE_DIR = os.path.join(REPO_ROOT, 'cximage')

TOOL_ID_KEYWORDS = [
    'tool', 'tool_id', 'owner_type', 'owner_binding', 'setownertool',
    'case_id', 'target_id', 'script_id', 'image_id', 'suite', 'catalog',
    '--tool', 'tool_name', 'findcircle_gauge'
]

def is_tool_id_context(line, col):
    before = line[:col].lower()
    for keyword in TOOL_ID_KEYWORDS:
        if keyword.lower() in before:
            return True
    return False

def fix_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original = content
    
    lines = content.split('\n')
    new_lines = []
    
    for i, line in enumerate(lines):
        new_line = line
        matches = list(re.finditer(r'"Findcircle"', line))
        
        for match in reversed(matches):
            start = match.start()
            end = match.end()
            
            if not is_tool_id_context(line, start):
                new_line = new_line[:start] + '"FindCircle"' + new_line[end:]
        
        new_lines.append(new_line)
    
    content = '\n'.join(new_lines)
    
    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Fixed: {filepath}")

def main():
    for root, dirs, files in os.walk(CXIMAGE_DIR):
        for filename in files:
            if not filename.endswith('.cpp') and not filename.endswith('.h'):
                continue
            
            filepath = os.path.join(root, filename)
            
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
            
            if '"Findcircle"' in content:
                fix_file(filepath)
    
    print("Done!")

if __name__ == '__main__':
    main()
