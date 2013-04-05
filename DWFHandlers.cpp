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
#include "DWFHandlers.h"

#include <osgUtil/SmoothingVisitor>

using namespace std;
using namespace DWFCore;
using namespace DWFToolkit;

stack<osg::Node*> topNodes;
MapIncludeNodes includeNodes;

const string IncludeLibraryAliasFormat ( "?Include Library/%s" );

TK_Status DWFOpenSegmentHandler::Execute ( BStreamFileToolkit& pTk )
{
	TK_Status status = TK_Open_Segment::Execute(pTk);
	if(status == TK_Normal )
	{
		osg::Group* osg_group = new osg::Group;

		includeLibraryName.reserve ( strlen ( GetSegment () ) + 1 );

		if ( 1 == sscanf ( GetSegment (), IncludeLibraryAliasFormat.data (), includeLibraryName.data () ) )
		{
			// included library is being processed...
			includeNodes [ includeLibraryName.data () ] = osg_group;
		}
		else
		{
			assert ( topNodes.top() != 0 && topNodes.top()->asGroup() != 0 );
			topNodes.top()->asGroup()->addChild ( osg_group );
		}

		topNodes.push ( osg_group );
	}
	return status;
}

TK_Status DWFIncludeSegmentHandler::Execute( BStreamFileToolkit& pTk )
{
	TK_Status status = TK_Referenced_Segment::Execute(pTk);
	if(status == TK_Normal )
	{
		includeLibraryName.reserve ( strlen ( GetSegment () ) + 1 );

		if ( 1 == sscanf ( GetSegment (), IncludeLibraryAliasFormat.data (), includeLibraryName.data () ) )
		{
			osg::Group* osg_group = dynamic_cast<osg::Group*>( includeNodes [ includeLibraryName.data () ] );
			assert ( osg_group != 0 );
			topNodes.top()->asGroup()->addChild ( osg_group );
		}
	}
	return status;
}

TK_Status DWFCloseSegmentHandler::Execute( BStreamFileToolkit& pTk )
{
	TK_Status status = TK_Close_Segment::Execute(pTk);
	if(status == TK_Normal )
	{
		topNodes.pop();
	}
	return status;
}

TK_Status DWFModellingMatrixHandler::Execute( BStreamFileToolkit& pTk )
{
	TK_Status status = TK_Matrix::Execute(pTk);
	if(status == TK_Normal )
	{
		osg::MatrixTransform* transform = new osg::MatrixTransform;
		transform->setMatrix( osg::Matrix ( GetMatrix() ) );

		assert ( topNodes.top() != 0 && topNodes.top()->asGroup() != 0 );
		topNodes.top()->asGroup()->addChild ( transform );

		topNodes.push ( transform );
	}
	return status;
}

TK_Status DWFCommentHandler::Execute( BStreamFileToolkit& parser )
{
	TK_Status status = TK_Comment::Execute(parser);
	if (status == TK_Normal)
	{
		OSG_DEBUG << GetComment ( ) << endl;
	}
	return status;
};

TK_Status DWFColorHandler::Execute( BStreamFileToolkit& parser )
{
	TK_Status status = TK_Color::Execute(parser);
	if (status == TK_Normal)
	{
		_material.release ();

		float alpha = 1.0f;

		if ( GetDiffuse() )
			getOrCreateMaterial()->setDiffuse ( osg::Material::FRONT_AND_BACK, osg::Vec4 ( GetDiffuse()[0], GetDiffuse()[1], GetDiffuse()[2], alpha) );

		if ( GetSpecular() )
			getOrCreateMaterial()->setSpecular ( osg::Material::FRONT_AND_BACK, osg::Vec4 ( GetSpecular()[0], GetSpecular()[1], GetSpecular()[2], alpha) );

		if ( isChannelValid((int)TKO_Channel_Gloss) )
			getOrCreateMaterial()->setShininess(osg::Material::FRONT_AND_BACK, GetGloss() );

		if ( _material.valid () )
			topNodes.top()->getOrCreateStateSet()->setAttributeAndModes( _material.get() );
	}

	return status;
};

TK_Status DWFShellHandler::Execute( BStreamFileToolkit& parser )
{
	TK_Status status = TK_Shell::Execute ( parser );
	if ( status == TK_Normal )
	{
		const int n_face_list = GetFacesLength ();
		const int* const face_list = GetFaces ();

		const int n_points = GetPointCount ();
		const float* const points = GetPoints ();

		osg::Geode* mGeode = new osg::Geode;

		// ind_face - order number of face
		// ind_face_list - index in GetFaces() array, see help for TK_Shell::SetFaces()
		for ( int ind_face_list = 0, ind_face = 0; ind_face_list																												 < n_face_list; ind_face++ )
		{
			// The first integer is the number
            // of vertices that should be connected to form the first n_verts_in_face
			const int n_verts_in_face = face_list [ ind_face_list++ ];

			// POLYGON
			osg::Vec3Array* vertices = new osg::Vec3Array;
			for ( int vert_ind = 0; vert_ind < n_verts_in_face; ++vert_ind, ++ind_face_list )
			{
				vertices->push_back (
		                     osg::Vec3 (
		                                points [ 3 * face_list [ ind_face_list ] + 0 ],
		                                points [ 3 * face_list [ ind_face_list ] + 1 ],
		                                points [ 3 * face_list [ ind_face_list ] + 2 ] ) );
			}

			osg::Geometry* geom = new osg::Geometry;

			// pass the created vertex array to the points geometry object.
			geom->setVertexArray ( vertices );
			geom->addPrimitiveSet( new osg::DrawArrays( osg::PrimitiveSet::POLYGON, 0, vertices->size() ) );

			if ( HasFaceColors () )
			{
				osg::Material* material =  new osg::Material;
				float alpha = 1.0f;
				material->setDiffuse ( osg::Material::FRONT_AND_BACK, osg::Vec4 ( GetFaceColors()[3*ind_face+0], GetFaceColors()[3*ind_face+1], GetFaceColors()[3*ind_face+2], alpha) );
				geom->getOrCreateStateSet()->setAttributeAndModes( material );
			}

			mGeode->addDrawable(geom);

			// TODO: make this conditional
			osgUtil::SmoothingVisitor sv;
			sv.smooth ( *geom );
		}

		assert ( topNodes.top() != 0 && topNodes.top()->asGroup() != 0 );
		topNodes.top()->asGroup()->addChild ( mGeode );
	}
	return status;
}
