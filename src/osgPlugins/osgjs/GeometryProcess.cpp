#include <cassert>
#include <limits>

#include <algorithm>
#include <utility>
#include <vector>

#include <iostream>
#include <osg/Notify>
#include <osg/Geometry>
#include <osg/Math>
#include <osg/PrimitiveSet>
#include <osg/TriangleIndexFunctor>
#include <osg/Array>
#include "GeometryProcess"

typedef std::vector<unsigned int> IndexList;

namespace {
// A helper class that gathers up all the attribute arrays of an
// osg::Geometry object that are BIND_PER_VERTEX and runs an
// ArrayVisitor on them.
    struct GeometryArrayGatherer
    {
        typedef std::vector<osg::Array*> ArrayList;

        GeometryArrayGatherer(osg::Geometry& geometry)
            : _useDrawElements(true)
        {
            add(geometry.getVertexArray(),osg::Geometry::BIND_PER_VERTEX);
            add(geometry.getNormalArray(),geometry.getNormalBinding());
            add(geometry.getColorArray(),geometry.getColorBinding());
            add(geometry.getSecondaryColorArray(),geometry.getSecondaryColorBinding());
            add(geometry.getFogCoordArray(),geometry.getFogCoordBinding());
            unsigned int i;
            for(i=0;i<geometry.getNumTexCoordArrays();++i)
            {
                add(geometry.getTexCoordArray(i),osg::Geometry::BIND_PER_VERTEX);
            }
            for(i=0;i<geometry.getNumVertexAttribArrays();++i)
            {
                add(geometry.getVertexAttribArray(i),geometry.getVertexAttribBinding(i));
            }
        }

        void add(osg::Array* array, osg::Geometry::AttributeBinding binding)
        {
            if (array)
                _arrayList.push_back(array);
        }

        void accept(osg::ArrayVisitor& av)
        {
            for(ArrayList::iterator itr=_arrayList.begin();
                itr!=_arrayList.end();
                ++itr)
            {
                (*itr)->accept(av);
            }
        }

        ArrayList _arrayList;
        // True if geometry contains bindings that are compatible with
        // DrawElements.
        bool _useDrawElements;
    };



    class Remapper : public osg::ArrayVisitor
    {
    public:
        static const unsigned invalidIndex;
        Remapper(const std::vector<unsigned>& remapping)
            : _remapping(remapping), _newsize(0)
        {
            for (std::vector<unsigned>::const_iterator itr = _remapping.begin(),
                     end = _remapping.end();
                 itr != end;
                 ++itr)
                if (*itr != invalidIndex)
                    ++_newsize;
        }

        const std::vector<unsigned>& _remapping;
        size_t _newsize;

        template<class T>
        inline void remap(T& array)
        {
            osg::ref_ptr<T> newarray = new T(_newsize);
            T* newptr = newarray.get();
            for (size_t i = 0; i < array.size(); ++i)
                if (_remapping[i] != invalidIndex)
                    (*newptr)[_remapping[i]] = array[i];
            array.swap(*newptr);

        }

        virtual void apply(osg::Array&) {}
        virtual void apply(osg::ByteArray& array) { remap(array); }
        virtual void apply(osg::ShortArray& array) { remap(array); }
        virtual void apply(osg::IntArray& array) { remap(array); }
        virtual void apply(osg::UByteArray& array) { remap(array); }
        virtual void apply(osg::UShortArray& array) { remap(array); }
        virtual void apply(osg::UIntArray& array) { remap(array); }
        virtual void apply(osg::FloatArray& array) { remap(array); }
        virtual void apply(osg::DoubleArray& array) { remap(array); }

        virtual void apply(osg::Vec2Array& array) { remap(array); }
        virtual void apply(osg::Vec3Array& array) { remap(array); }
        virtual void apply(osg::Vec4Array& array) { remap(array); }

        virtual void apply(osg::Vec4ubArray& array) { remap(array); }

        virtual void apply(osg::Vec2bArray& array) { remap(array); }
        virtual void apply(osg::Vec3bArray& array) { remap(array); }
        virtual void apply(osg::Vec4bArray& array) { remap(array); }

        virtual void apply(osg::Vec2sArray& array) { remap(array); }
        virtual void apply(osg::Vec3sArray& array) { remap(array); }
        virtual void apply(osg::Vec4sArray& array) { remap(array); }

        virtual void apply(osg::Vec2dArray& array) { remap(array); }
        virtual void apply(osg::Vec3dArray& array) { remap(array); }
        virtual void apply(osg::Vec4dArray& array) { remap(array); }

        virtual void apply(osg::MatrixfArray& array) { remap(array); }
    };

    const unsigned Remapper::invalidIndex = std::numeric_limits<unsigned>::max();


// Compact the vertex attribute arrays. Also stolen from TriStripVisitor
    class RemapArray : public osg::ArrayVisitor
    {
    public:
        RemapArray(const IndexList& remapping):_remapping(remapping) {}

        const IndexList& _remapping;

        template<class T>
        inline void remap(T& array)
        {
            for(unsigned int i=0;i<_remapping.size();++i)
            {
                if (i!=_remapping[i])
                {
                    array[i] = array[_remapping[i]];
                }
            }
            array.erase(array.begin()+_remapping.size(),array.end());
        }

        virtual void apply(osg::Array&) {}
        virtual void apply(osg::ByteArray& array) { remap(array); }
        virtual void apply(osg::ShortArray& array) { remap(array); }
        virtual void apply(osg::IntArray& array) { remap(array); }
        virtual void apply(osg::UByteArray& array) { remap(array); }
        virtual void apply(osg::UShortArray& array) { remap(array); }
        virtual void apply(osg::UIntArray& array) { remap(array); }
        virtual void apply(osg::FloatArray& array) { remap(array); }
        virtual void apply(osg::DoubleArray& array) { remap(array); }

        virtual void apply(osg::Vec2Array& array) { remap(array); }
        virtual void apply(osg::Vec3Array& array) { remap(array); }
        virtual void apply(osg::Vec4Array& array) { remap(array); }

        virtual void apply(osg::Vec4ubArray& array) { remap(array); }

        virtual void apply(osg::Vec2bArray& array) { remap(array); }
        virtual void apply(osg::Vec3bArray& array) { remap(array); }
        virtual void apply(osg::Vec4bArray& array) { remap(array); }

        virtual void apply(osg::Vec2sArray& array) { remap(array); }
        virtual void apply(osg::Vec3sArray& array) { remap(array); }
        virtual void apply(osg::Vec4sArray& array) { remap(array); }

        virtual void apply(osg::Vec2dArray& array) { remap(array); }
        virtual void apply(osg::Vec3dArray& array) { remap(array); }
        virtual void apply(osg::Vec4dArray& array) { remap(array); }

        virtual void apply(osg::MatrixfArray& array) { remap(array); }
    protected:

        RemapArray& operator = (const RemapArray&) { return *this; }
    };


// Compare vertices in a mesh using all their attributes. The vertices
// are identified by their index. Extracted from TriStripVisitor.cpp
    struct VertexAttribComparitor : public GeometryArrayGatherer
    {
        VertexAttribComparitor(osg::Geometry& geometry)
            : GeometryArrayGatherer(geometry)
        {
        }

        bool operator() (unsigned int lhs, unsigned int rhs) const
        {
            for(ArrayList::const_iterator itr=_arrayList.begin();
                itr!=_arrayList.end();
                ++itr)
            {
                int compare = (*itr)->compare(lhs,rhs);
                if (compare==-1) return true;
                if (compare==1) return false;
            }
            return false;
        }

        int compare(unsigned int lhs, unsigned int rhs)
        {
            for(ArrayList::iterator itr=_arrayList.begin();
                itr!=_arrayList.end();
                ++itr)
            {
                int compare = (*itr)->compare(lhs,rhs);
                if (compare==-1) return -1;
                if (compare==1) return 1;
            }
            return 0;
        }

    protected:
        VertexAttribComparitor& operator = (const VertexAttribComparitor&) { return *this; }

    };


// Construct an index list of triangles for DrawElements for any input
// primitives.
    struct MyTriangleOperator
    {

        IndexList _remapIndices;
        IndexList _in_indices;

        inline void operator()(unsigned int p1, unsigned int p2, unsigned int p3)
        {
            if (_remapIndices.empty())
            {
                _in_indices.push_back(p1);
                _in_indices.push_back(p2);
                _in_indices.push_back(p3);
            }
            else
            {
                _in_indices.push_back(_remapIndices[p1]);
                _in_indices.push_back(_remapIndices[p2]);
                _in_indices.push_back(_remapIndices[p3]);
            }
        }

    };
    typedef osg::TriangleIndexFunctor<MyTriangleOperator> MyTriangleIndexFunctor;





    template<class T>
    class LineIndexFunctor : public osg::PrimitiveIndexFunctor, public T
    {
    public:


        virtual void setVertexArray(unsigned int,const osg::Vec2*)
        {
        }

        virtual void setVertexArray(unsigned int ,const osg::Vec3* )
        {
        }

        virtual void setVertexArray(unsigned int,const osg::Vec4* )
        {
        }

        virtual void setVertexArray(unsigned int,const osg::Vec2d*)
        {
        }

        virtual void setVertexArray(unsigned int ,const osg::Vec3d* )
        {
        }

        virtual void setVertexArray(unsigned int,const osg::Vec4d* )
        {
        }

        virtual void begin(GLenum mode)
        {
            _modeCache = mode;
            _indexCache.clear();
        }

        virtual void vertex(unsigned int vert)
        {
            _indexCache.push_back(vert);
        }

        virtual void end()
        {
            if (!_indexCache.empty())
            {
                drawElements(_modeCache,_indexCache.size(),&_indexCache.front());
            }
        }

        virtual void drawArrays(GLenum mode,GLint first,GLsizei count)
        {
            switch(mode)
            {
            case(GL_TRIANGLES):
            {
                unsigned int pos=first;
                for(GLsizei i=2;i<count;i+=3,pos+=3)
                {
                    this->operator()(pos,pos+1);
                    this->operator()(pos+1,pos+2);
                    this->operator()(pos+2,pos);
                }
                break;
            }
            case(GL_TRIANGLE_STRIP):
            {
                unsigned int pos=first;
                for(GLsizei i=2;i<count;++i,++pos)
                {
                    if ((i%2)) { 
                        this->operator()(pos,pos+2);
                        this->operator()(pos+2,pos+1);
                        this->operator()(pos+1,pos);
                    } else {
                        this->operator()(pos,pos+1);
                        this->operator()(pos+1,pos+2);
                        this->operator()(pos,pos+2);
                    }
                }
                break;
            }
            case(GL_QUADS):
            {
                unsigned int pos=first;
                for(GLsizei i=3;i<count;i+=4,pos+=4)
                {
                    this->operator()(pos,pos+1);
                    this->operator()(pos+1,pos+2);
                    this->operator()(pos+2,pos+3);
                    this->operator()(pos+3,pos);
                }
                break;
            }
            case(GL_QUAD_STRIP):
            {
                unsigned int pos=first;
                for(GLsizei i=3;i<count;i+=2,pos+=2)
                {
                    this->operator()(pos,pos+1);
                    this->operator()(pos+1,pos+3);
                    this->operator()(pos+2,pos+3);
                    this->operator()(pos+2,pos);
                }
                break;
            }
            case(GL_POLYGON): // treat polygons as GL_TRIANGLE_FAN
            case(GL_TRIANGLE_FAN):
            {
                unsigned int pos=first+1;
                for(GLsizei i=2;i<count;++i,++pos)
                {
                    this->operator()(first, pos);
                    this->operator()(first, pos+1);
                    this->operator()(pos, pos+1);
                }
                break;
            }
            case(GL_POINTS):
            case(GL_LINES):
            case(GL_LINE_STRIP):
            case(GL_LINE_LOOP):
            default:
                // can't be converted into to triangles.
                break;
            }
        }

        virtual void drawElements(GLenum mode,GLsizei count,const GLubyte* indices)
        {
            if (indices==0 || count==0) return;

            typedef GLubyte Index;
            typedef const Index* IndexPointer;

            switch(mode)
            {
            case(GL_TRIANGLES):
            {
                IndexPointer ilast = &indices[count];
                for(IndexPointer  iptr=indices;iptr<ilast;iptr+=3) {
                    this->operator()(*iptr,*(iptr+1));
                    this->operator()(*(iptr+1),*(iptr+2));
                    this->operator()(*iptr,*(iptr+2));
                }
                break;
            }
            case(GL_TRIANGLE_STRIP):
            {
                IndexPointer iptr = indices;
                for(GLsizei i=2;i<count;++i,++iptr)
                {
                    if ((i%2)) {
                        this->operator()(*(iptr),*(iptr+2));
                        this->operator()(*(iptr+2),*(iptr+1));
                        this->operator()(*(iptr),*(iptr+1));
                    } else {
                        this->operator()(*(iptr),*(iptr+1));
                        this->operator()(*(iptr+1),*(iptr+2));
                        this->operator()(*(iptr),*(iptr+2));
                    }
                }
                break;
            }
            case(GL_QUADS):
            {
                IndexPointer iptr = indices;
                for(GLsizei i=3;i<count;i+=4,iptr+=4)
                {

                    this->operator()(*(iptr),*(iptr+1));
                    this->operator()(*(iptr+1),*(iptr+2));
                    this->operator()(*(iptr+2), *(iptr+3) );
                    this->operator()(*(iptr), *(iptr+3) );
                }
                break;
            }
            case(GL_QUAD_STRIP):
            {
                IndexPointer iptr = indices;
                for(GLsizei i=3;i<count;i+=2,iptr+=2)
                {
                    this->operator()(*(iptr),*(iptr+1));
                    this->operator()(*(iptr+3),*(iptr+1));
                    this->operator()(*(iptr+2),*(iptr+3));
                    this->operator()(*(iptr),*(iptr+2));
                }
                break;
            }
            case(GL_POLYGON): // treat polygons as GL_TRIANGLE_FAN
            case(GL_TRIANGLE_FAN):
            {
                IndexPointer iptr = indices;
                Index first = *iptr;
                ++iptr;
                for(GLsizei i=2;i<count;++i,++iptr)
                {
                    this->operator()(first, *(iptr));
                    this->operator()(first, *(iptr+1));
                    this->operator()(*(iptr),*(iptr+1));
                }
                break;
            }
            case(GL_POINTS):
            case(GL_LINES):
            case(GL_LINE_STRIP):
            case(GL_LINE_LOOP):
            default:
                // can't be converted into to triangles.
                break;
            }
        }

        virtual void drawElements(GLenum mode,GLsizei count,const GLushort* indices)
        {
            if (indices==0 || count==0) return;

            typedef GLushort Index;
            typedef const Index* IndexPointer;

            switch(mode)
            {
            case(GL_TRIANGLES):
            {
                IndexPointer ilast = &indices[count];
                for(IndexPointer  iptr=indices;iptr<ilast;iptr+=3) {
                    this->operator()(*iptr,*(iptr+1));
                    this->operator()(*(iptr+1),*(iptr+2));
                    this->operator()(*iptr,*(iptr+2));
                }
                break;
            }
            case(GL_TRIANGLE_STRIP):
            {
                IndexPointer iptr = indices;
                for(GLsizei i=2;i<count;++i,++iptr)
                {
                    if ((i%2)) {
                        this->operator()(*(iptr),*(iptr+2));
                        this->operator()(*(iptr+2),*(iptr+1));
                        this->operator()(*(iptr),*(iptr+1));
                    } else {
                        this->operator()(*(iptr),*(iptr+1));
                        this->operator()(*(iptr+1),*(iptr+2));
                        this->operator()(*(iptr),*(iptr+2));
                    }
                }
                break;
            }
            case(GL_QUADS):
            {
                IndexPointer iptr = indices;
                for(GLsizei i=3;i<count;i+=4,iptr+=4)
                {

                    this->operator()(*(iptr),*(iptr+1));
                    this->operator()(*(iptr+1),*(iptr+2));
                    this->operator()(*(iptr+2), *(iptr+3) );
                    this->operator()(*(iptr), *(iptr+3) );
                }
                break;
            }
            case(GL_QUAD_STRIP):
            {
                IndexPointer iptr = indices;
                for(GLsizei i=3;i<count;i+=2,iptr+=2)
                {
                    this->operator()(*(iptr),*(iptr+1));
                    this->operator()(*(iptr+3),*(iptr+1));
                    this->operator()(*(iptr+2),*(iptr+3));
                    this->operator()(*(iptr),*(iptr+2));
                }
                break;
            }
            case(GL_POLYGON): // treat polygons as GL_TRIANGLE_FAN
            case(GL_TRIANGLE_FAN):
            {
                IndexPointer iptr = indices;
                Index first = *iptr;
                ++iptr;
                for(GLsizei i=2;i<count;++i,++iptr)
                {
                    this->operator()(first, *(iptr));
                    this->operator()(first, *(iptr+1));
                    this->operator()(*(iptr),*(iptr+1));
                }
                break;
            }
            case(GL_POINTS):
            case(GL_LINES):
            case(GL_LINE_STRIP):
            case(GL_LINE_LOOP):
            default:
                // can't be converted into to triangles.
                break;
            }
        }

        virtual void drawElements(GLenum mode,GLsizei count,const GLuint* indices)
        {
            if (indices==0 || count==0) return;

            typedef GLuint Index;
            typedef const Index* IndexPointer;

            switch(mode)
            {
            case(GL_TRIANGLES):
            {
                IndexPointer ilast = &indices[count];
                for(IndexPointer  iptr=indices;iptr<ilast;iptr+=3) {
                    this->operator()(*iptr,*(iptr+1));
                    this->operator()(*(iptr+1),*(iptr+2));
                    this->operator()(*iptr,*(iptr+2));
                }
                break;
            }
            case(GL_TRIANGLE_STRIP):
            {
                IndexPointer iptr = indices;
                for(GLsizei i=2;i<count;++i,++iptr)
                {
                    if ((i%2)) {
                        this->operator()(*(iptr),*(iptr+2));
                        this->operator()(*(iptr+2),*(iptr+1));
                        this->operator()(*(iptr),*(iptr+1));
                    } else {
                        this->operator()(*(iptr),*(iptr+1));
                        this->operator()(*(iptr+1),*(iptr+2));
                        this->operator()(*(iptr),*(iptr+2));
                    }
                }
                break;
            }
            case(GL_QUADS):
            {
                IndexPointer iptr = indices;
                for(GLsizei i=3;i<count;i+=4,iptr+=4)
                {

                    this->operator()(*(iptr),*(iptr+1));
                    this->operator()(*(iptr+1),*(iptr+2));
                    this->operator()(*(iptr+2), *(iptr+3) );
                    this->operator()(*(iptr), *(iptr+3) );
                }
                break;
            }
            case(GL_QUAD_STRIP):
            {
                IndexPointer iptr = indices;
                for(GLsizei i=3;i<count;i+=2,iptr+=2)
                {
                    this->operator()(*(iptr),*(iptr+1));
                    this->operator()(*(iptr+3),*(iptr+1));
                    this->operator()(*(iptr+2),*(iptr+3));
                    this->operator()(*(iptr),*(iptr+2));
                }
                break;
            }
            case(GL_POLYGON): // treat polygons as GL_TRIANGLE_FAN
            case(GL_TRIANGLE_FAN):
            {
                IndexPointer iptr = indices;
                Index first = *iptr;
                ++iptr;
                for(GLsizei i=2;i<count;++i,++iptr)
                {
                    this->operator()(first, *(iptr));
                    this->operator()(first, *(iptr+1));
                    this->operator()(*(iptr),*(iptr+1));
                }
                break;
            }
            case(GL_POINTS):
            case(GL_LINES):
            case(GL_LINE_STRIP):
            case(GL_LINE_LOOP):
            default:
                // can't be converted into to triangles.
                break;
            }
        }

        GLenum               _modeCache;
        std::vector<GLuint>  _indexCache;
    };

    struct MyLineOperator
    {

        IndexList _remapIndices;
        IndexList _in_indices;

        inline void operator()(unsigned int p1, unsigned int p2)
        {
            if (_remapIndices.empty())
            {
                _in_indices.push_back(p1);
                _in_indices.push_back(p2);
            }
            else
            {
                _in_indices.push_back(_remapIndices[p1]);
                _in_indices.push_back(_remapIndices[p2]);
            }
        }

    };
    typedef LineIndexFunctor<MyLineOperator> MyLineIndexFunctor;

}

void IndexShape::makeMesh(osg::Geometry& geom) {

    if (geom.getNormalBinding()==osg::Geometry::BIND_PER_PRIMITIVE_SET) return;

    if (geom.getColorBinding()==osg::Geometry::BIND_PER_PRIMITIVE_SET) return;

    if (geom.getSecondaryColorBinding()==osg::Geometry::BIND_PER_PRIMITIVE_SET) return;

    if (geom.getFogCoordBinding()==osg::Geometry::BIND_PER_PRIMITIVE_SET) return;

    // no point optimizing if we don't have enough vertices.
    if (!geom.getVertexArray() || geom.getVertexArray()->getNumElements()<3) return;

    // check for the existence of surface primitives
    unsigned int numSurfacePrimitives = 0;
    unsigned int numNonIndexedPrimitives = 0;
    osg::Geometry::PrimitiveSetList& primitives = geom.getPrimitiveSetList();
    osg::Geometry::PrimitiveSetList::iterator itr;
    for(itr=primitives.begin();
        itr!=primitives.end();
        ++itr)
    {
        switch((*itr)->getMode())
        {
        case(osg::PrimitiveSet::TRIANGLES):
        case(osg::PrimitiveSet::TRIANGLE_STRIP):
        case(osg::PrimitiveSet::TRIANGLE_FAN):
        case(osg::PrimitiveSet::QUADS):
        case(osg::PrimitiveSet::QUAD_STRIP):
        case(osg::PrimitiveSet::POLYGON):
            ++numSurfacePrimitives;
        break;
        default:
            // For now, only deal with polygons
            return;
        }
        osg::PrimitiveSet::Type type = (*itr)->getType();
        if (!(type == osg::PrimitiveSet::DrawElementsUBytePrimitiveType
              || type == osg::PrimitiveSet::DrawElementsUShortPrimitiveType
              || type == osg::PrimitiveSet::DrawElementsUIntPrimitiveType))
            numNonIndexedPrimitives++;
    }

    // nothing to index
    if (!numSurfacePrimitives) return;

    // compute duplicate vertices
    typedef std::vector<unsigned int> IndexList;
    unsigned int numVertices = geom.getVertexArray()->getNumElements();
    IndexList indices(numVertices);
    unsigned int i,j;
    for(i=0;i<numVertices;++i)
    {
        indices[i] = i;
    }

    VertexAttribComparitor arrayComparitor(geom);
    std::sort(indices.begin(),indices.end(),arrayComparitor);

    unsigned int lastUnique = 0;
    unsigned int numUnique = 1;
    for(i=1;i<numVertices;++i)
    {
        if (arrayComparitor.compare(indices[lastUnique],indices[i]) != 0)
        {
            lastUnique = i;
            ++numUnique;
        }

    }
    IndexList remapDuplicatesToOrignals(numVertices);
    lastUnique = 0;
    for(i=1;i<numVertices;++i)
    {
        if (arrayComparitor.compare(indices[lastUnique],indices[i])!=0)
        {
            // found a new vertex entry, so previous run of duplicates needs
            // to be put together.
            unsigned int min_index = indices[lastUnique];
            for(j=lastUnique+1;j<i;++j)
            {
                min_index = osg::minimum(min_index,indices[j]);
            }
            for(j=lastUnique;j<i;++j)
            {
                remapDuplicatesToOrignals[indices[j]]=min_index;
            }
            lastUnique = i;
        }

    }
    unsigned int min_index = indices[lastUnique];
    for(j=lastUnique+1;j<i;++j)
    {
        min_index = osg::minimum(min_index,indices[j]);
    }
    for(j=lastUnique;j<i;++j)
    {
        remapDuplicatesToOrignals[indices[j]]=min_index;
    }


    // copy the arrays.
    IndexList finalMapping(numVertices);
    IndexList copyMapping;
    copyMapping.reserve(numUnique);
    unsigned int currentIndex=0;
    for(i=0;i<numVertices;++i)
    {
        if (remapDuplicatesToOrignals[i]==i)
        {
            finalMapping[i] = currentIndex;
            copyMapping.push_back(i);
            currentIndex++;
        }
    }

    for(i=0;i<numVertices;++i)
    {
        if (remapDuplicatesToOrignals[i]!=i)
        {
            finalMapping[i] = finalMapping[remapDuplicatesToOrignals[i]];
        }
    }

    osg::Geometry::PrimitiveSetList new_primitives;
    new_primitives.reserve(primitives.size());

    if (_wireframe) {

        MyLineIndexFunctor taf;
        taf._remapIndices.swap(finalMapping);

        for(itr=primitives.begin();
            itr!=primitives.end();
            ++itr)
        {
            // For now we only have primitive sets that play nicely with
            // the TriangleIndexFunctor.
            (*itr)->accept(taf);
        }

        osg::PrimitiveSet::Mode primitive;
        primitive = osg::PrimitiveSet::LINES;

        // remap any shared vertex attributes
        RemapArray ra(copyMapping);
        arrayComparitor.accept(ra);

        if (taf._in_indices.size() < 65536)
        {
            osg::DrawElementsUShort* elements = new osg::DrawElementsUShort(primitive);
            for (IndexList::iterator itr = taf._in_indices.begin(),
                     end = taf._in_indices.end();
                 itr != end;
                 ++itr)
            {
                elements->push_back((GLushort)(*itr));
            }
            new_primitives.push_back(elements);
        }
        else
        {
            osg::DrawElementsUInt* elements
                = new osg::DrawElementsUInt(primitive, taf._in_indices.begin(),
                                            taf._in_indices.end());
            new_primitives.push_back(elements);
        }

    } else {

        MyTriangleIndexFunctor taf;
        taf._remapIndices.swap(finalMapping);

        for(itr=primitives.begin();
            itr!=primitives.end();
            ++itr)
        {
            // For now we only have primitive sets that play nicely with
            // the TriangleIndexFunctor.
            (*itr)->accept(taf);
        }


        osg::PrimitiveSet::Mode primitive;
        primitive = osg::PrimitiveSet::TRIANGLES;

        // remap any shared vertex attributes
        RemapArray ra(copyMapping);
        arrayComparitor.accept(ra);

        if (taf._in_indices.size() < 65536)
        {
            osg::DrawElementsUShort* elements = new osg::DrawElementsUShort(primitive);
            for (IndexList::iterator itr = taf._in_indices.begin(),
                     end = taf._in_indices.end();
                 itr != end;
                 ++itr)
            {
                elements->push_back((GLushort)(*itr));
            }
            new_primitives.push_back(elements);
        }
        else
        {
            osg::DrawElementsUInt* elements
                = new osg::DrawElementsUInt(primitive, taf._in_indices.begin(),
                                            taf._in_indices.end());
            new_primitives.push_back(elements);
        }

    }

    geom.setPrimitiveSetList(new_primitives);
}

