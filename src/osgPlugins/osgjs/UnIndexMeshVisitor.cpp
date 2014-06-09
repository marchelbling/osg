/* -*-c++-*- OpenSceneGraph - Copyright (C) Cedric Pinson 
*
* This application is open source and may be redistributed and/or modified   
* freely and without restriction, both in commercial and non commercial
* applications, as long as this copyright notice is maintained.
* 
* This application is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*
*/

#include <osg/Geometry>
#include <osg/TriangleIndexFunctor>
#include <osg/PrimitiveSet>
#include "LineIndexFunctor"
#include "PointIndexFunctor"
#include "UnIndexMeshVisitor"
#include "GeometryArray"


typedef std::vector<unsigned int> IndexList;
// this help works only for indexed primitive to unindex it


struct ExtractIndex
{
    IndexList _indexes;

    // for points
    inline void operator() ( const unsigned int& i) {
        _indexes.push_back(i);
    }

    // for lines
    inline void operator() ( const unsigned int& i, const unsigned int& i1) {

        _indexes.push_back(i);
        _indexes.push_back(i1);
    }

    // for triangles
    inline void operator() ( const unsigned int& i, const unsigned int& i1 , const unsigned int &i2 ) {

        _indexes.push_back(i);
        _indexes.push_back(i1);
        _indexes.push_back(i2);
    }

};


typedef osg::TriangleIndexFunctor<ExtractIndex> UnIndexedTriangleFunctor;
typedef LineIndexFunctor<ExtractIndex> UnIndexedLineFunctor;
typedef PointIndexFunctor<ExtractIndex> UnIndexedPointFunctor;

void UnIndexMeshVisitor::apply(osg::Drawable& drw) 
{
    osg::Geometry* geom = drw.asGeometry();
    if (!geom) {
        return;
    }
    apply(*geom);
}


void UnIndexMeshVisitor::apply(osg::Geometry& geom) 
{

    // no point optimizing if we don't have enough vertices.
    if (!geom.getVertexArray()) return;


    // check for the existence of surface primitives
    unsigned int numIndexedPrimitives = 0;
    osg::Geometry::PrimitiveSetList& primitives = geom.getPrimitiveSetList();
    osg::Geometry::PrimitiveSetList::iterator itr;
    for(itr=primitives.begin();
        itr!=primitives.end();
        ++itr)
    {
        osg::PrimitiveSet::Type type = (*itr)->getType();
        if ((type == osg::PrimitiveSet::DrawElementsUBytePrimitiveType
             || type == osg::PrimitiveSet::DrawElementsUShortPrimitiveType
             || type == osg::PrimitiveSet::DrawElementsUIntPrimitiveType))
            numIndexedPrimitives++;
    }
    
    // no polygons or no indexed primitive, nothing to do
    if (!numIndexedPrimitives) {
        return;
    }

    // we dont manage lines
    
    GeometryArrayList arraySrc(geom);
    GeometryArrayList arrayList = arraySrc.cloneType();

    osg::Geometry::PrimitiveSetList newPrimitives;

    for(itr=primitives.begin();
        itr!=primitives.end();
        ++itr)
    {
        osg::PrimitiveSet::Mode mode = (osg::PrimitiveSet::Mode)(*itr)->getMode();

        switch(mode) {

            // manage triangles
        case(osg::PrimitiveSet::TRIANGLES):
        case(osg::PrimitiveSet::TRIANGLE_STRIP):
        case(osg::PrimitiveSet::TRIANGLE_FAN):
        case(osg::PrimitiveSet::QUADS):
        case(osg::PrimitiveSet::QUAD_STRIP):
        case(osg::PrimitiveSet::POLYGON):
        {
            // for each geometry list indexes of vertexes
            // to makes triangles
            UnIndexedTriangleFunctor triangleIndexList;
            (*itr)->accept(triangleIndexList);
            
            unsigned int index = arrayList.size();

            // now copy each vertex to new array, like a draw array
            arraySrc.append(triangleIndexList._indexes, arrayList);

            newPrimitives.push_back(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, index, triangleIndexList._indexes.size()));
        }
        break;

        // manage lines
        case(osg::PrimitiveSet::LINES):
        case(osg::PrimitiveSet::LINE_STRIP):
        case(osg::PrimitiveSet::LINE_LOOP):
        {
            UnIndexedLineFunctor linesIndexList;
            (*itr)->accept(linesIndexList);

            unsigned int index = arrayList.size();

            // now copy each vertex to new array, like a draw array
            arraySrc.append(linesIndexList._indexes, arrayList);

            newPrimitives.push_back(new osg::DrawArrays(osg::PrimitiveSet::LINES, index, linesIndexList._indexes.size()));
        }
        break;
        case(osg::PrimitiveSet::POINTS):
        {
            UnIndexedPointFunctor pointsIndexList;
            (*itr)->accept(pointsIndexList);

            unsigned int index = arrayList.size();

            // now copy each vertex to new array, like a draw array
            arraySrc.append(pointsIndexList._indexes, arrayList);
            newPrimitives.push_back(new osg::DrawArrays(osg::PrimitiveSet::POINTS, index, pointsIndexList._indexes.size()));
        }
        break;
        default:
            break;
        }
    }

    arrayList.setToGeometry(geom);
    geom.setPrimitiveSetList(newPrimitives);
}
