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

#include "GeometrySplitter"
#include <osg/Notify>
#include <osg/NodeVisitor>
#include <osg/Texture2D>
#include <osg/StateSet>
#include <osg/Geode>
#include <osg/Array>
#include <osg/TriangleFunctor>
#include <osgUtil/MeshOptimizers>
#include <string>
#include <osgDB/ReadFile>
#include <osgDB/ReaderWriter>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/Registry>


struct FunctionCopy {
    void operator()(osg::Array* src, unsigned int index, osg::Array* dst) {
        osg::Vec3Array* vec3Array= dynamic_cast<osg::Vec3Array*>(src);
        if (vec3Array) {
            osg::Vec3Array* vec3ArrayDst = dynamic_cast<osg::Vec3Array*>(dst);
            vec3ArrayDst->push_back((*vec3Array)[index]);
            return;
        }

        osg::Vec2Array* vec2Array= dynamic_cast<osg::Vec2Array*>(src);
        if (vec2Array) {
            osg::Vec2Array* vec2ArrayDst = dynamic_cast<osg::Vec2Array*>(dst);
            vec2ArrayDst->push_back((*vec2Array)[index]);
            return;
        }

        osg::Vec4Array* vec4Array= dynamic_cast<osg::Vec4Array*>(src);
        if (vec2Array) {
            osg::Vec4Array* vec4ArrayDst = dynamic_cast<osg::Vec4Array*>(dst);
            vec4ArrayDst->push_back((*vec4Array)[index]);
            return;
        }
    }
};

struct ConvertToBindPerVertex {
    template <class T> void convert(T& array, osg::Geometry::AttributeBinding fromBinding, osg::Geometry::PrimitiveSetList& primitives, unsigned int size)
    {
        osg::ref_ptr<T> result = new T();
        for (unsigned int p = 0; p < primitives.size(); p++) {
            switch ( primitives[p]->getMode() ) {
            case osg::PrimitiveSet::POINTS:
                break;
            case osg::PrimitiveSet::LINES:
                switch(fromBinding) {
                case osg::Geometry::BIND_OFF:
                case osg::Geometry::BIND_PER_VERTEX:
                    break;
                case osg::Geometry::BIND_OVERALL:
                {
                    for (unsigned int i = 0; i < primitives[p]->getNumIndices(); i++)
                        result->push_back(array[0]);
                }
                break;
                case osg::Geometry::BIND_PER_PRIMITIVE_SET:
                {
                    unsigned int nb = primitives[p]->getNumIndices();
                    for (unsigned int i = 0; i < nb; i++)
                        result->push_back(array[p]);
                }
                break;
                }
                break;

            case osg::PrimitiveSet::TRIANGLES:
                switch(fromBinding) {
                case osg::Geometry::BIND_OFF:
                case osg::Geometry::BIND_PER_VERTEX:
                    break;
                case osg::Geometry::BIND_OVERALL:
                {
                    for (unsigned int i = 0; i < primitives[p]->getNumIndices(); i++)
                        result->push_back(array[0]);
                }
                break;
                case osg::Geometry::BIND_PER_PRIMITIVE_SET:
                {
                    unsigned int nb = primitives[p]->getNumIndices();
                    for (unsigned int i = 0; i < nb; i++)
                        result->push_back(array[p]);
                }
                break;
                }
                break;

            case osg::PrimitiveSet::TRIANGLE_STRIP:
                switch(fromBinding) {
                case osg::Geometry::BIND_OFF:
                case osg::Geometry::BIND_PER_VERTEX:
                    break;
                case osg::Geometry::BIND_OVERALL:
                {
                    for (unsigned int i = 0; i < primitives[p]->getNumIndices(); i++)
                        result->push_back(array[0]);
                }
                break;
                case osg::Geometry::BIND_PER_PRIMITIVE_SET:
                {
                    osg::notify(osg::FATAL) << "Can't convert Array from BIND_PER_PRIMITIVE_SET to BIND_PER_VERTEX, for TRIANGLE_STRIP" << std::endl;
                }
                break;
                }
                break;

            case osg::PrimitiveSet::TRIANGLE_FAN:
                switch(fromBinding) {
                case osg::Geometry::BIND_OFF:
                case osg::Geometry::BIND_PER_VERTEX:
                    break;
                case osg::Geometry::BIND_OVERALL:
                {
                    for (unsigned int i = 0; i < primitives[p]->getNumIndices(); i++)
                        result->push_back(array[0]);
                }
                break;
                case osg::Geometry::BIND_PER_PRIMITIVE_SET:
                {
                    osg::notify(osg::FATAL) << "Can't convert Array from BIND_PER_PRIMITIVE_SET to BIND_PER_VERTEX, for TRIANGLE_FAN" << std::endl;
                }
                break;
                }
                break;

            case osg::PrimitiveSet::QUADS:
                switch(fromBinding) {
                case osg::Geometry::BIND_OFF:
                case osg::Geometry::BIND_PER_VERTEX:
                    break;
                case osg::Geometry::BIND_OVERALL:
                {
                    for (unsigned int i = 0; i < primitives[p]->getNumIndices(); i++)
                        result->push_back(array[0]);
                }
                break;
                case osg::Geometry::BIND_PER_PRIMITIVE_SET:
                {
                    osg::notify(osg::FATAL) << "Can't convert Array from BIND_PER_PRIMITIVE_SET to BIND_PER_VERTEX, for QUADS" << std::endl;
                }
                break;
                }
                break;

            case osg::PrimitiveSet::QUAD_STRIP:
                switch(fromBinding) {
                case osg::Geometry::BIND_OFF:
                case osg::Geometry::BIND_PER_VERTEX:
                    break;
                case osg::Geometry::BIND_OVERALL:
                {
                    for (unsigned int i = 0; i < primitives[p]->getNumIndices(); i++)
                        result->push_back(array[0]);
                }
                break;
                case osg::Geometry::BIND_PER_PRIMITIVE_SET:
                {
                    osg::notify(osg::FATAL) << "Can't convert Array from BIND_PER_PRIMITIVE_SET to BIND_PER_VERTEX, for QUAD_STRIP" << std::endl;
                }
                break;
                }
                break;
            }
        }
        array = *result;
    }

    void operator()(osg::Array* src, osg::Geometry::AttributeBinding fromBinding, osg::Geometry::PrimitiveSetList& primitives, unsigned int size) {
        osg::Vec3Array* vec3Array= dynamic_cast<osg::Vec3Array*>(src);
        if (vec3Array) {
            convert(*vec3Array, fromBinding, primitives, size);
            return;
        }

        osg::Vec2Array* vec2Array= dynamic_cast<osg::Vec2Array*>(src);
        if (vec2Array) {
            convert(*vec2Array, fromBinding, primitives, size);
            return;
        }

        osg::Vec4Array* vec4Array= dynamic_cast<osg::Vec4Array*>(src);
        if (vec4Array) {
            convert(*vec4Array, fromBinding, primitives, size);
            return;
        }

        osg::Vec4ubArray* vec4ubArray= dynamic_cast<osg::Vec4ubArray*>(src);
        if (vec4ubArray) {
            convert(*vec4ubArray, fromBinding, primitives, size);
            return;
        }

    }
};


struct ArrayList {

    osg::ref_ptr<osg::Array> _vertexes;
    osg::ref_ptr<osg::Array> _normals;
    osg::ref_ptr<osg::Array> _colors;
    osg::ref_ptr<osg::Array> _secondaryColors;
    osg::ref_ptr<osg::Array> _fogCoords;
    std::vector<osg::ref_ptr<osg::Array> > _texCoordArrays;

    ArrayList() {}
    ArrayList(osg::Geometry& geometry) {
        _vertexes = geometry.getVertexArray();
        _normals = geometry.getNormalArray();
        _colors = geometry.getColorArray();
        _secondaryColors = geometry.getSecondaryColorArray();
        _fogCoords = geometry.getFogCoordArray();

        _texCoordArrays.resize(geometry.getNumTexCoordArrays());
        for(unsigned int i=0;i<geometry.getNumTexCoordArrays();++i)
            _texCoordArrays[i] = geometry.getTexCoordArray(i);
    }

    unsigned int append(unsigned int index, ArrayList& dst) {
        if (_vertexes.valid()) {
            FunctionCopy()(_vertexes.get(), index, dst._vertexes.get());
        }
        if (_normals.valid()) {
            FunctionCopy()(_normals.get(), index, dst._normals.get());
        }
        if (_colors.valid()) {
            FunctionCopy()(_colors.get(), index, dst._colors.get());
        }
        if (_secondaryColors.valid()) {
            FunctionCopy()(_secondaryColors.get(), index, dst._secondaryColors.get());
        }
        if (_fogCoords.valid()) {
            FunctionCopy()(_fogCoords.get(), index, dst._fogCoords.get());
        }

        for (unsigned int i = 0; i < _texCoordArrays.size(); i++)
            if (_texCoordArrays[i].valid()) {
                FunctionCopy()(_texCoordArrays[i].get(), index, dst._texCoordArrays[i].get());
            }

        return dst._vertexes->getNumElements()-1;
    }

    ArrayList clone() const {
        ArrayList array;
        if (_vertexes.valid())
            array._vertexes = dynamic_cast<osg::Array*>(_vertexes->cloneType());

        if (_normals.valid())
            array._normals = dynamic_cast<osg::Array*>(_normals->cloneType());

        if (_colors.valid())
            array._colors = dynamic_cast<osg::Array*>(_colors->cloneType());

        if (_secondaryColors.valid())
            array._secondaryColors = dynamic_cast<osg::Array*>(_secondaryColors->cloneType());

        if (_fogCoords.valid())
            array._fogCoords = dynamic_cast<osg::Array*>(_fogCoords->cloneType());

        array._texCoordArrays.resize(_texCoordArrays.size());
        for (unsigned int i = 0; i < _texCoordArrays.size(); i++) {
            if (_texCoordArrays[i].valid())
                array._texCoordArrays[i] = dynamic_cast<osg::Array*>(_texCoordArrays[i]->cloneType());
        }
        return array;
    }

    void setToGeometry(osg::Geometry& geom) {
        if (_vertexes.valid())
            geom.setVertexArray(_vertexes.get());
        if (_normals.valid()) {
            geom.setNormalArray(_normals.get());
            geom.setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
        }

        if (_colors.valid()) {
            geom.setColorArray(_colors.get());
            geom.setColorBinding(osg::Geometry::BIND_PER_VERTEX);
        }

        if (_secondaryColors.valid()) {
            geom.setSecondaryColorArray(_secondaryColors.get());
            geom.setSecondaryColorBinding(osg::Geometry::BIND_PER_VERTEX);
        }

        if (_fogCoords.valid()) {
            geom.setFogCoordArray(_fogCoords.get());
            geom.setFogCoordBinding(osg::Geometry::BIND_PER_VERTEX);
        }

        for (unsigned int i = 0; i < _texCoordArrays.size(); ++i) {
            if (_texCoordArrays[i].valid())
                geom.setTexCoordArray(i, _texCoordArrays[i].get());
        }
    }
};

struct GeometrySplitter
{
    GeometrySplitter(osg::Geometry& geometry ) : _geometry(geometry) {
        _arraySrc = ArrayList(geometry);
    }

    void split(unsigned int maxElements)
    {
        osg::Geometry::PrimitiveSetList primitives = _geometry.getPrimitiveSetList();
        for (unsigned int i = 0; i < primitives.size(); i++) {
            osg::DrawElements* currentPrimitives = dynamic_cast<osg::DrawElements*>(primitives[i].get());

            unsigned int currentPrimitiveIndex = 0;
            while (currentPrimitiveIndex < currentPrimitives->getNumIndices()) {

                unsigned int nbElements = currentPrimitives->getNumIndices() - currentPrimitiveIndex;
                if (nbElements > maxElements) {
                    nbElements = maxElements;
                }

                ArrayList arrayDst = _arraySrc.clone();
                osg::DrawElementsUShort* primitives = new osg::DrawElementsUShort(currentPrimitives->getMode());
                std::map<unsigned int,unsigned int> global2local;
                for (unsigned int index = 0; index < nbElements; index++) {
                    unsigned int idx = currentPrimitives->index(currentPrimitiveIndex+index);
                    unsigned int localidx;
                    if (global2local.find(idx) == global2local.end()) {
                        unsigned int newindex = _arraySrc.append(idx, arrayDst);
                        global2local[idx] = newindex;
                    }
                    localidx = global2local[idx];
                    primitives->push_back(localidx);
                }
                currentPrimitiveIndex += nbElements;
                osg::Geometry* geom = new osg::Geometry;
                geom->getPrimitiveSetList().push_back(primitives);
                geom->setStateSet(_geometry.getStateSet());
                arrayDst.setToGeometry(*geom);
                _geometryList.push_back(geom);
            }
        }
    }
    
    ArrayList _arraySrc;
    osg::Geometry& _geometry;
    GeometryList _geometryList;
};


struct TriangleConvertorVertexes
{
    osg::ref_ptr<osg::Vec3Array> _vertexes;

    void operator()(const osg::Vec3& v1,const osg::Vec3& v2,const osg::Vec3& v3, bool treatVertexDataAsTemporary) {
        _vertexes->push_back(v1);
        _vertexes->push_back(v2);
        _vertexes->push_back(v3);
    }
};

struct SetupTriangleConvertorVertexes : public osg::TriangleFunctor<TriangleConvertorVertexes>
{
    SetupTriangleConvertorVertexes() {
        _vertexes = new osg::Vec3Array; 
    }
};


void SplitGeometryVisitor::convertToBindPerVertex(osg::Geometry& srcGeom) 
{
    unsigned int size = srcGeom.getVertexArray()->getNumElements();
    if (srcGeom.getNormalArray() && srcGeom.getNormalBinding() != osg::Geometry::BIND_PER_VERTEX) {
        ConvertToBindPerVertex()(srcGeom.getNormalArray(), srcGeom.getNormalBinding(), srcGeom.getPrimitiveSetList(), size);
        srcGeom.setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
    }

    if (srcGeom.getColorArray() && srcGeom.getColorBinding() != osg::Geometry::BIND_PER_VERTEX) {
        ConvertToBindPerVertex()(srcGeom.getColorArray(), srcGeom.getColorBinding(), srcGeom.getPrimitiveSetList(), size);
        srcGeom.setColorBinding(osg::Geometry::BIND_PER_VERTEX);
    }

    if (srcGeom.getSecondaryColorArray() && srcGeom.getSecondaryColorBinding() != osg::Geometry::BIND_PER_VERTEX) {
        ConvertToBindPerVertex()(srcGeom.getSecondaryColorArray(), srcGeom.getSecondaryColorBinding(), srcGeom.getPrimitiveSetList(), size);
        srcGeom.setSecondaryColorBinding(osg::Geometry::BIND_PER_VERTEX);
    }

    if (srcGeom.getFogCoordArray() && srcGeom.getFogCoordBinding() != osg::Geometry::BIND_PER_VERTEX) {
        ConvertToBindPerVertex()(srcGeom.getFogCoordArray(), srcGeom.getFogCoordBinding(), srcGeom.getPrimitiveSetList(), size);
        srcGeom.setFogCoordBinding(osg::Geometry::BIND_PER_VERTEX);
    }
}

unsigned int SplitGeometryVisitor::splitByVertexArray(osg::Geometry& srcGeom, GeometryList& list)
{
    //convertToBindPerVertex(srcGeom);

    osgUtil::IndexMeshVisitor opt;
    opt.makeMesh(srcGeom);


    GeometrySplitter splitter(srcGeom);
    splitter.split(_maxVertexes);
        
    for (unsigned int i = 0; i < splitter._geometryList.size(); i++) {
        list.push_back(splitter._geometryList[i].get());
    }

    return splitter._geometryList.size();
}

void SplitGeometryVisitor::apply(osg::Geode& node) 
{
    GeometryList listGeometry;
    bool touched = false;
    for (unsigned int i = 0; i < node.getNumDrawables(); ++i) {
        osg::Geometry* geom = dynamic_cast<osg::Geometry*>(node.getDrawable(i));
        bool processed = false;
        if (geom) {
            convertToBindPerVertex(*geom);

            osg::Array* array = geom->getVertexArray();
            if (array->getNumElements() > _maxVertexes) {
                int nb = splitByVertexArray(*geom, listGeometry);
                osg::notify(osg::NOTICE) << "geometry " << geom << " " << geom->getName() << " splitted to " <<  nb << " models because vertexes size " << array->getNumElements() << " is more than " << _maxVertexes << std::endl;
                processed = true;
                touched = true;
            }
        }
        if (!processed) {
            listGeometry.push_back(geom);
        }
    }
    if (touched) {
        node.removeDrawables(0,node.getNumDrawables());
        for (unsigned int d = 0; d < listGeometry.size(); d++) {
            node.addDrawable(listGeometry[d].get());
        }
    }
}
