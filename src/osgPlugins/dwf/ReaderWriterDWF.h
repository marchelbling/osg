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

using namespace osg;
using namespace osgDB;

///////////////////////////////////////////////////////////////////////////////
//!
//! \class ReaderWriterDWF
//! \brief This is the Reader for the dwf file format
//!
//////////////////////////////////////////////////////////////////////////////
class ReaderWriterDWF : public ReaderWriter
{
public:
	ReaderWriterDWF()
	{
		supportsExtension("dwf","Design Web Format");
	}

	virtual const char* className() { return "ReaderWriterDWF"; }
	virtual ReadResult readNode(const std::string& fileName, const osgDB::ReaderWriter::Options*) const;

private:
	osg::Node* readDWF ( const std::string& filePath ) const;
};