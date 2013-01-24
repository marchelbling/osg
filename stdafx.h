// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//


#ifndef _W3D_STREAM_PROCESSOR_H
#define _W3D_STREAM_PROCESSOR_H

//
// We need to try and include stl stuff before the max and min 
// macros get defined an mess up the use of std::max and std::min
// (problem on linux and maybe other platforms).
//
#include "dwfcore/STL.h"

#include <iostream>
#include <stack>
#include <map>
#include <string>
#include <assert.h>

#include "dwfcore/File.h"
#include "dwfcore/String.h"

#include "dwf/package/Constants.h"
#include "dwf/package/Manifest.h"
#include "dwf/package/EModelSection.h"
#include "dwf/package/reader/PackageReader.h"

#include "dwf/w3dtk/BStream.h"
#include "dwf/w3dtk/BOpcodeShell.h"

#include <osgDB/Registry>
#include <osgDB/ReadFile>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>
#include <osg/Geode>

using namespace std;
using namespace DWFCore;
using namespace DWFToolkit;

typedef map<string, osg::Node*> MapIncludeNodes;
typedef pair<string, osg::Node*> MapIncludeNodesValue;

#endif // _W3D_STREAM_PROCESSOR_H
