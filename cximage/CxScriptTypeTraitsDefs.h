#pragma once

#include "../cxparser/CxScriptTypeRegistry.h"

class Image;
class Findline;
class Findcircle;
class Findellipse;
class FindRect;
class findobject;
class FindSegmentation;
class fastmatch;
class FastMatchDiagnostic;
class CircleRingGauge;

CXSCRIPT_TYPE_TRAITS(Image, Image)
CXSCRIPT_TYPE_TRAITS(Findline, FindLine)
CXSCRIPT_TYPE_TRAITS(Findcircle, FindCircle)
CXSCRIPT_TYPE_TRAITS(Findellipse, FindEllipse)
CXSCRIPT_TYPE_TRAITS(FindRect, FindRect)
CXSCRIPT_TYPE_TRAITS(findobject, FindObject)
CXSCRIPT_TYPE_TRAITS(FindSegmentation, FindSegmentation)
CXSCRIPT_TYPE_TRAITS(fastmatch, FastMatch)
CXSCRIPT_TYPE_TRAITS(FastMatchDiagnostic, FastMatchDiagnostic)
CXSCRIPT_TYPE_TRAITS(CircleRingGauge, CircleRingGauge)