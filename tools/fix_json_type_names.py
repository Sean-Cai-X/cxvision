import os
import json

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

FIXES = {
    '"Findline"': '"FindLine"',
    '"Findcircle"': '"FindCircle"',
}

def fix_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original = content
    for old, new in FIXES.items():
        content = content.replace(old, new)
    
    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Fixed: {filepath}")

def fix_json_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    changed = False
    
    def fix_dict(d):
        nonlocal changed
        for key, value in d.items():
            if isinstance(value, str):
                if value == "Findline":
                    d[key] = "FindLine"
                    changed = True
                elif value == "Findcircle":
                    d[key] = "FindCircle"
                    changed = True
            elif isinstance(value, dict):
                fix_dict(value)
            elif isinstance(value, list):
                for item in value:
                    if isinstance(item, dict):
                        fix_dict(item)
    
    fix_dict(data)
    
    if changed:
        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2)
        print(f"Fixed JSON: {filepath}")

def main():
    cxscript_dir = os.path.join(REPO_ROOT, 'cxparser', 'cxscript')
    
    for root, dirs, files in os.walk(cxscript_dir):
        for filename in files:
            filepath = os.path.join(root, filename)
            
            if filename.endswith('.json'):
                fix_json_file(filepath)
            elif filename.endswith('.cxsc'):
                fix_file(filepath)
    
    print("Done!")

if __name__ == '__main__':
    main()
