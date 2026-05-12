/*
  File: muParserStrClassMap.cpp
  Role: Support utilities used by the cxparser runtime.
*/

#include <cmath>
#include <string>
#include <iostream>
#include <map>
#include <memory>
#include "muParserDef.h"
#include "muParserStack.h"
#include "muParserTokenReader.h"

#include "muParserBytecode.h"
#include "muParserError.h"

#include "muParserStrClassMap.h"

namespace mu
{

	/*
	  Role: Initialize the string-backed class definition store.
	*/
	muParserStrClassMap::muParserStrClassMap():
	m_pclassgroup(NULL),
	m_iclassnum(0)
	{
	}

	/*
	  Role: Release the owned string-backed class group state.
	*/
	muParserStrClassMap::~muParserStrClassMap()
	{
		if(NULL!=m_pclassgroup)
			delete m_pclassgroup;
		m_iclassnum=0;
	}

}
