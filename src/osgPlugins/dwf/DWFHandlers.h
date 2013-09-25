/*
*
* DWF (Design Web Format) File Loader for OpenSceneGraph.
*
* Copyright (c) 2012 Andrey Dankevich <dankevicha@gmail.com>
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
*/

#include <osg/Material>

#include "dwf/w3dtk/BOpcodeHandler.h"

class DWFOpenSegmentHandler : public TK_Open_Segment
{
private:
	string includeLibraryName;

	friend class CloseSegmentHandler; 

public:
	TK_Status Execute ( BStreamFileToolkit& pTk );
};

class DWFIncludeSegmentHandler : public TK_Referenced_Segment
{
private:
	string includeLibraryName;

public:
	DWFIncludeSegmentHandler ( ) : TK_Referenced_Segment ( TKE_Include_Segment ) {;}

	TK_Status Execute( BStreamFileToolkit& pTk );
};

class DWFCloseSegmentHandler : public TK_Close_Segment
{
public:
	TK_Status Execute( BStreamFileToolkit& pTk );
};

class DWFModellingMatrixHandler : public TK_Matrix
{
public:
	DWFModellingMatrixHandler ( ) : TK_Matrix ( TKE_Modelling_Matrix ){;}

	TK_Status Execute( BStreamFileToolkit& pTk );
};


class DWFCommentHandler : public TK_Comment
{
public:
	TK_Status Execute( BStreamFileToolkit& parser );
};

class DWFColorHandler : public TK_Color
{
private:
	osg::ref_ptr<osg::Material> _material;

public:
	TK_Status Execute( BStreamFileToolkit& parser );

	inline osg::Material* getOrCreateMaterial ()
	{
		if ( !_material )
			_material =  new osg::Material;
		return _material;
	}

	bool isChannelValid ( int channel) const
	{
		return ( GetChannels() & ( 1 << channel ));
	}
};

class DWFShellHandler : public TK_Shell
{
public:
	TK_Status Execute( BStreamFileToolkit& parser );
};
