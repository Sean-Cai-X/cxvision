import os

files_to_fix = [
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\CxScriptRuntimeResultCapture.h',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\CxScriptRuntimeResultCapture.cpp',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\ViewController.h',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\ViewController.cpp',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\CxCoreBoundary.h',
    r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\CxCoreBoundary.cpp'
]

replacements = [
    ('class Findline;', 'class FindLine;'),
    ('class Findcircle;', 'class FindCircle;'),
    ('class Findellipse;', 'class FindEllipse;'),
    ('class fastmatch;', 'class FastMatch;'),
    ('Findline*', 'FindLine*'),
    ('Findcircle*', 'FindCircle*'),
    ('Findellipse*', 'FindEllipse*'),
    ('fastmatch*', 'FastMatch*'),
    ('fastmatch&', 'FastMatch&'),
    ('Findline&', 'FindLine&'),
    ('Findcircle&', 'FindCircle&'),
    ('Findellipse&', 'FindEllipse&'),
    ('fastmatch::', 'FastMatch::'),
    ('Findline::', 'FindLine::'),
    ('Findcircle::', 'FindCircle::'),
    ('Findellipse::', 'FindEllipse::'),
    ('CaptureFindlineResult', 'CaptureFindLineResult'),
    ('CaptureFindcircleResult', 'CaptureFindCircleResult'),
    ('CaptureFindellipseResult', 'CaptureFindEllipseResult'),
    ('CaptureFastMatchResult', 'CaptureFastMatchResult'),
    ('CxScriptTypeTraits<Findline>', 'CxScriptTypeTraits<FindLine>'),
    ('CxScriptTypeTraits<Findcircle>', 'CxScriptTypeTraits<FindCircle>'),
    ('CxScriptTypeTraits<Findellipse>', 'CxScriptTypeTraits<FindEllipse>'),
    ('CxScriptTypeTraits<fastmatch>', 'CxScriptTypeTraits<FastMatch>'),
    ('ExportLineMeasurement(Findline&', 'ExportLineMeasurement(FindLine&'),
    ('ExportCircleMeasurement(Findcircle&', 'ExportCircleMeasurement(FindCircle&'),
    ('ExportEllipseMeasurement(Findellipse&', 'ExportEllipseMeasurement(FindEllipse&'),
    ('ExportMatchResult(fastmatch&', 'ExportMatchResult(FastMatch&'),
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
        print(f"Fixed: {file_path}")
    else:
        print(f"No changes: {file_path}")