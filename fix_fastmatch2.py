import re

with open(r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\FastMatch_original.h', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace('#include "Findline.h"', '#include "FindLine.h"')
content = content.replace('class fastmatch :public Findline', 'class FastMatch :public FindLine')
content = content.replace('fastmatch();', 'FastMatch();')
content = content.replace('~fastmatch();', '~FastMatch();')
content = content.replace('fastmatch* m_prelationmatch', 'FastMatch* m_prelationmatch')

with open(r'D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cximage\FastMatch_new.h', 'w', encoding='utf-8') as f:
    f.write(content)

print('Fixed file created successfully')