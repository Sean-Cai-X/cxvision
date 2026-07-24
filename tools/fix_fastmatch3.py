import subprocess
import os

result = subprocess.run(['git', 'show', '164a858:cximage/fastmatch.h'], capture_output=True, text=True)

if result.returncode != 0:
    print(f"git show failed: {result.stderr}")
    exit(1)

content = result.stdout

content = content.replace('#include "Findline.h"', '#include "FindLine.h"')
content = content.replace('class fastmatch :public Findline', 'class FastMatch :public FindLine')
content = content.replace('fastmatch();', 'FastMatch();')
content = content.replace('~fastmatch();', '~FastMatch();')
content = content.replace('fastmatch* m_prelationmatch', 'FastMatch* m_prelationmatch')

with open(r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\FastMatch_new2.h', 'w', encoding='utf-8') as f:
    f.write(content)

print('Fixed file created successfully')
print(f"File length: {len(content)} bytes")
print(f"First 200 chars: {content[:200]}")