import os
import re

TYPE_MIGRATIONS = {
    '"Findline"': '"FindLine"',
    "'Findline'": "'FindLine'",
    '"findline"': '"FindLine"',
    "'findline'": "'FindLine'",
    '"Match"': '"FastMatch"',
    "'Match'": "'FastMatch'",
    '"fastmatch"': '"FastMatch"',
    "'fastmatch'": "'FastMatch'",
    '"CFastMatch"': '"FastMatch"',
    "'CFastMatch'": "'FastMatch'",
    '"FindCircle"': '"FindCircle"',
    '"FindCircle"': '"FindCircle"',
    '"FindEllipse"': '"FindEllipse"',
    '"FindRect"': '"FindRect"',
}

TOOL_ID_EXCEPTIONS = ["tool", "tool_id", "owner_type", "suite", "catalog", "--tool", "tool_name"]

def should_skip_line(line):
    lower_line = line.lower()
    for exception in TOOL_ID_EXCEPTIONS:
        if exception in lower_line:
            return True
    return False

def migrate_file(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        original_content = content
        changed = False
        
        for old, new in TYPE_MIGRATIONS.items():
            if old in content:
                content = content.replace(old, new)
                changed = True
        
        if changed:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            print(f"[UPDATED] {filepath}")
        return changed
    except Exception as e:
        print(f"[ERROR] {filepath}: {e}")
        return False

def find_files(root_dir, extensions):
    for dirpath, dirnames, filenames in os.walk(root_dir):
        for filename in filenames:
            if any(filename.endswith(ext) for ext in extensions):
                yield os.path.join(dirpath, filename)

def main():
    root = r'd:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo'
    
    cpp_files = list(find_files(os.path.join(root, 'cximage'), ['.h', '.cpp']))
    cpp_files += list(find_files(os.path.join(root, 'cxparser'), ['.h', '.cpp']))
    cpp_files += list(find_files(os.path.join(root, 'cxvision'), ['.h', '.cpp']))
    
    print(f"Found {len(cpp_files)} C++ files")
    
    updated_count = 0
    for filepath in cpp_files:
        if migrate_file(filepath):
            updated_count += 1
    
    print(f"\nUpdated {updated_count} C++ files")
    
    cxsc_files = list(find_files(os.path.join(root, 'cxparser', 'cxscript'), ['.cxsc', '.cxs']))
    print(f"\nFound {len(cxsc_files)} CxScript files")
    
    cxsc_updated = 0
    for filepath in cxsc_files:
        if migrate_file(filepath):
            cxsc_updated += 1
    
    print(f"\nUpdated {cxsc_updated} CxScript files")
    print(f"\nTotal updated: {updated_count + cxsc_updated}")

if __name__ == '__main__':
    main()
