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

#include "stdafx.h"
#include "ReaderWriterDWF.h"
#include "DWFHandlers.h"

using namespace osg;
using namespace osgDB;

extern stack<osg::Node*> topNodes;

// register with Registry to instantiate the above reader/writer.
REGISTER_OSGPLUGIN(dwf, ReaderWriterDWF);

///////////////////////////////////////////////////////////////////////////////
//!
//! \brief Function which is called when any dwf file is requested to load in
//! \osgDB. Load read dwf file and if it successes return the osg::Node
//!
///////////////////////////////////////////////////////////////////////////////
osgDB::ReaderWriter::ReadResult ReaderWriterDWF::readNode(const std::string& fileName, const osgDB::ReaderWriter::Options* options) const
{
	OSG_DEBUG << "Starting DWF Reader for " << fileName.c_str() << endl;

	std::string ext = osgDB::getLowerCaseFileExtension(fileName);
	if ( !acceptsExtension(ext) )
		return ReadResult::FILE_NOT_HANDLED;

	//
	//  Open each source file and grab the sections and contents and add them to the destination
	//
	DWFString inFile ( fileName.c_str() );
	DWFFile oInfile( *inFile );

	std::string filePath = osgDB::findDataFile( fileName, options );
	if ( filePath.empty() )
		return ReadResult::FILE_NOT_FOUND;


	osg::Group* root = new osg::Group;
	root->addChild ( readDWF ( filePath ) );

	return root;
}

osg::Node* ReaderWriterDWF::readDWF ( const std::string& filePath ) const
{
	try
	{
		DWFFile oDWF( filePath.c_str() );
		DWFToolkit::DWFPackageReader oReader( oDWF );

		DWFPackageReader::tPackageInfo tInfo;
		oReader.getPackageInfo( tInfo );
		OSG_DEBUG <<  "DWF Package version: " << tInfo.nVersion << endl;

		wchar_t zBuffer[256] = {0};

		if (tInfo.eType != DWFPackageReader::eDWFPackage)

		{
			_DWFCORE_SWPRINTF( zBuffer, 256, L"File is not a DWF package [%s]",
				(tInfo.eType == DWFPackageReader::eW2DStream) ? L"W2D Stream" :
				(tInfo.eType == DWFPackageReader::eDWFStream) ? L"DWF Stream (<6.0)" :
				(tInfo.eType == DWFPackageReader::eZIPFile) ? L"ZIP Archive" : L"Unknown" );

			OSG_FATAL << zBuffer << endl;
			return 0;
		}
		else if (tInfo.nVersion < _DWF_FORMAT_VERSION_INTRO_3D)
		{
			OSG_FATAL << L"DWF package specified is not a 3D DWF" << endl;
			return 0;
		}

		OSG_DEBUG << L"DWF package specified is a 3D DWF" << endl;

		// read and parse the manifest
		DWFToolkit::DWFManifest& rManifest = oReader.getManifest();

		// obtain the emodel section
		DWFToolkit::DWFManifest::SectionIterator* piSections = rManifest.findSectionsByType( _DWF_FORMAT_EMODEL_TYPE_WIDE_STRING );

		// make sure we got a section
		if ((piSections == NULL) || (piSections->valid() == false))
		{
			if (piSections)
			{
				DWFCORE_FREE_OBJECT( piSections );
			}

			OSG_FATAL << L"Malformed or unexpected 3D DWF, cannot continue without an EModel section" << endl;
			return 0;
		}

		// get the EModel Section
		DWFToolkit::DWFEModelSection* pSection = dynamic_cast<DWFEModelSection*>(piSections->get());

		// done with the iterator
		DWFCORE_FREE_OBJECT( piSections );

		if (pSection == NULL)
		{
			OSG_FATAL << L"Type mismatch - not an EModel Section" << endl;
			return 0;
		}

		// read the descriptor to get all the details
		pSection->readDescriptor();

		// get the graphics stream
		DWFToolkit::DWFResourceContainer::ResourceIterator* piResources = pSection->findResourcesByRole( DWFXML::kzRole_Graphics3d );

		if ((piResources == NULL) || (piResources->valid() == false))
		{
			if (piResources)
			{
				DWFCORE_FREE_OBJECT( piResources );
			}

			OSG_FATAL << "Illegal EModel section - no graphics" << endl;
			return 0;
		}

		// get the w3d resource
		DWFToolkit::DWFGraphicResource* pW3D = dynamic_cast<DWFGraphicResource*>(piResources->get());

		// done with the iterator
		DWFCORE_FREE_OBJECT( piResources );

		if (pW3D == NULL)
		{
			OSG_FATAL << L"Type mismatch - not a W3D resource" << endl;
			return 0;
		}

		// get the data stream
		DWFCore::DWFInputStream* pW3DStream = pW3D->getInputStream();

		// Create the HSF toolkit object that does the stream I/O
		BStreamFileToolkit oW3DStreamParser;

		// create a Node to contain all our osg::Geometry objects.
		osg::Node* osg_group = new osg::Group;
		topNodes.push ( osg_group );
		//osg_group->addChild ( drawDebug ( ) );

		// Note that the parser object will delete this object on it's own destruction
		oW3DStreamParser.SetOpcodeHandler ( TKE_Comment, new DWFCommentHandler );
		oW3DStreamParser.SetOpcodeHandler ( TKE_Shell, new DWFShellHandler );
		oW3DStreamParser.SetOpcodeHandler ( TKE_Open_Segment, new DWFOpenSegmentHandler );
		oW3DStreamParser.SetOpcodeHandler ( TKE_Close_Segment, new DWFCloseSegmentHandler );
		oW3DStreamParser.SetOpcodeHandler ( TKE_Include_Segment, new DWFIncludeSegmentHandler );
		oW3DStreamParser.SetOpcodeHandler ( TKE_Modelling_Matrix, new DWFModellingMatrixHandler );
		oW3DStreamParser.SetOpcodeHandler ( TKE_Color, new DWFColorHandler );

		// Attach the stream to the parser
		oW3DStreamParser.OpenStream( *pW3DStream );

		const size_t readBlockSize = 2 << 13;
		size_t nBytesRead = 0;
		char aBuffer [ readBlockSize ] = {0};

		// read and process the stream
		while (pW3DStream->available() > 0)
		{
			// read from the stream ourselves, we could also use ReadBuffer()
			// but it basically just performs this same action.
			nBytesRead = pW3DStream->read( aBuffer, readBlockSize );

			// use the parser to process the buffer
			if (oW3DStreamParser.ParseBuffer(aBuffer, nBytesRead, TK_Normal) == TK_Error)
			{
				OSG_FATAL << L"Error occurred parsing buffer" << endl;
				break;
			}
		}

		// Done with the stream, we must delete it
		oW3DStreamParser.CloseStream();
		DWFCORE_FREE_OBJECT( pW3DStream );

		return osg_group;
	}
	catch (DWFException& ex)
	{
		OSG_FATAL << ex.type() << endl;
		OSG_FATAL << ex.message() << endl;
		OSG_FATAL << ex.function() << endl;
		OSG_FATAL << ex.file() << endl;
		OSG_FATAL << ex.line() << endl;
	}

	return NULL;
}