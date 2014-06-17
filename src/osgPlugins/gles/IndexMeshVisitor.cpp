#include <vector>
#include <algorithm> // sort
#include <limits> // numeric_limits

#include <osg/Geometry>
#include <osg/PrimitiveSet>
#include <osg/Array>
#include <osg/ValueObject>

#include "IndexMeshVisitor"
#include "PrimitiveIndexors"


namespace {
// A helper class that gathers up all the attribute arrays of an
// osg::Geometry object that are BIND_PER_VERTEX and runs an
// ArrayVisitor on them.
    struct GeometryArrayGatherer
    {
        typedef std::vector<osg::Array*> ArrayList;

        GeometryArrayGatherer(osg::Geometry& geometry) {
            add(geometry.getVertexArray());
            add(geometry.getNormalArray());
            add(geometry.getColorArray());
            add(geometry.getSecondaryColorArray());
            add(geometry.getFogCoordArray());
            for(unsigned int i = 0 ; i < geometry.getNumTexCoordArrays() ; ++ i) {
                add(geometry.getTexCoordArray(i));
            }
            for(unsigned int i = 0 ; i < geometry.getNumVertexAttribArrays() ; ++ i) {
                add(geometry.getVertexAttribArray(i));
            }
        }

        void add(osg::Array* array) {
            if (array) {
                _arrayList.push_back(array);
            }
        }

        void accept(osg::ArrayVisitor& av) {
            for(ArrayList::iterator itr = _arrayList.begin() ; itr != _arrayList.end(); ++ itr) {
                (*itr)->accept(av);
            }
        }

        ArrayList _arrayList;
    };


// Compact the vertex attribute arrays. Also stolen from TriStripVisitor
    class RemapArray : public osg::ArrayVisitor
    {
    public:
        RemapArray(const IndexList& remapping) : _remapping(remapping)
        {}

        const IndexList& _remapping;

        template<class T>
        inline void remap(T& array) {
            for(unsigned int i = 0 ; i < _remapping.size() ; ++ i) {
                if(i != _remapping[i]) {
                    array[i] = array[_remapping[i]];
                }
            }
            array.erase(array.begin() + _remapping.size(),
                        array.end());
        }

        virtual void apply(osg::Array&) {}
        virtual void apply(osg::ByteArray& array)   { remap(array); }
        virtual void apply(osg::ShortArray& array)  { remap(array); }
        virtual void apply(osg::IntArray& array)    { remap(array); }
        virtual void apply(osg::UByteArray& array)  { remap(array); }
        virtual void apply(osg::UShortArray& array) { remap(array); }
        virtual void apply(osg::UIntArray& array)   { remap(array); }
        virtual void apply(osg::FloatArray& array)  { remap(array); }
        virtual void apply(osg::DoubleArray& array) { remap(array); }

        virtual void apply(osg::Vec2dArray& array) { remap(array); }
        virtual void apply(osg::Vec3dArray& array) { remap(array); }
        virtual void apply(osg::Vec4dArray& array) { remap(array); }

        virtual void apply(osg::Vec2Array& array) { remap(array); }
        virtual void apply(osg::Vec3Array& array) { remap(array); }
        virtual void apply(osg::Vec4Array& array) { remap(array); }

        virtual void apply(osg::Vec2iArray& array) { remap(array); }
        virtual void apply(osg::Vec3iArray& array) { remap(array); }
        virtual void apply(osg::Vec4iArray& array) { remap(array); }

        virtual void apply(osg::Vec2uiArray& array) { remap(array); }
        virtual void apply(osg::Vec3uiArray& array) { remap(array); }
        virtual void apply(osg::Vec4uiArray& array) { remap(array); }

        virtual void apply(osg::Vec2sArray& array) { remap(array); }
        virtual void apply(osg::Vec3sArray& array) { remap(array); }
        virtual void apply(osg::Vec4sArray& array) { remap(array); }

        virtual void apply(osg::Vec2usArray& array) { remap(array); }
        virtual void apply(osg::Vec3usArray& array) { remap(array); }
        virtual void apply(osg::Vec4usArray& array) { remap(array); }

        virtual void apply(osg::Vec2bArray& array) { remap(array); }
        virtual void apply(osg::Vec3bArray& array) { remap(array); }
        virtual void apply(osg::Vec4bArray& array) { remap(array); }

        virtual void apply(osg::Vec4ubArray& array) { remap(array); }
        virtual void apply(osg::Vec3ubArray& array) { remap(array); }
        virtual void apply(osg::Vec2ubArray& array) { remap(array); }

        virtual void apply(osg::MatrixfArray& array) { remap(array); }

    protected:
        RemapArray& operator= (const RemapArray&) { return *this; }
    };


// Compare vertices in a mesh using all their attributes. The vertices
// are identified by their index. Extracted from TriStripVisitor.cpp
    struct VertexAttribComparitor : public GeometryArrayGatherer
    {
        VertexAttribComparitor(osg::Geometry& geometry) : GeometryArrayGatherer(geometry)
        {}

        bool operator() (unsigned int lhs, unsigned int rhs) const {
            for(ArrayList::const_iterator itr = _arrayList.begin(); itr != _arrayList.end(); ++ itr) {
                int compare = (*itr)->compare(lhs, rhs);
                if (compare == -1) { return true; }
                if (compare == 1)  { return false; }
            }
            return false;
        }

        int compare(unsigned int lhs, unsigned int rhs) {
            for(ArrayList::iterator itr = _arrayList.begin(); itr != _arrayList.end(); ++ itr) {
                int compare = (*itr)->compare(lhs, rhs);
                if (compare == -1) { return -1; }
                if (compare == 1)  { return 1; }
            }
            return 0;
        }

        protected:
            VertexAttribComparitor& operator= (const VertexAttribComparitor&) { return *this; }
    };
}

void IndexMeshVisitor::apply(osg::Geometry& geom) {
    // TODO: this is deprecated
    if (geom.getNormalBinding() == osg::Geometry::BIND_PER_PRIMITIVE_SET) return;
    if (geom.getColorBinding() == osg::Geometry::BIND_PER_PRIMITIVE_SET) return;
    if (geom.getSecondaryColorBinding() == osg::Geometry::BIND_PER_PRIMITIVE_SET) return;
    if (geom.getFogCoordBinding() == osg::Geometry::BIND_PER_PRIMITIVE_SET) return;

    // no point optimizing if we don't have enough vertices.
    if (!geom.getVertexArray() || geom.getVertexArray()->getNumElements() < 3) return;


    // duplicate shared arrays as it isn't safe to rearrange vertices when arrays are shared.
    if (geom.containsSharedArrays()) {
        geom.duplicateSharedArrays();
    }

    osg::Geometry::PrimitiveSetList& primitives = geom.getPrimitiveSetList();
    osg::Geometry::PrimitiveSetList::iterator itr;

    osg::Geometry::PrimitiveSetList new_primitives;
    new_primitives.reserve(primitives.size());

    // compute duplicate vertices
    typedef std::vector<unsigned int> IndexList;
    unsigned int numVertices = geom.getVertexArray()->getNumElements();
    IndexList indices(numVertices);
    unsigned int i, j;
    for(i = 0 ; i < numVertices ; ++ i) {
        indices[i] = i;
    }

    VertexAttribComparitor arrayComparitor(geom);
    std::sort(indices.begin(), indices.end(), arrayComparitor);

    unsigned int lastUnique = 0;
    unsigned int numUnique = 1;
    for(i = 1 ; i < numVertices ; ++ i) {
        if (arrayComparitor.compare(indices[lastUnique], indices[i]) != 0) {
            lastUnique = i;
            ++ numUnique;
        }
    }

    IndexList remapDuplicatesToOrignals(numVertices);
    lastUnique = 0;
    for(i = 1 ; i < numVertices ; ++ i) {
        if (arrayComparitor.compare(indices[lastUnique],indices[i]) != 0) {
            // found a new vertex entry, so previous run of duplicates needs
            // to be put together.
            unsigned int min_index = indices[lastUnique];
            for(j = lastUnique + 1 ; j < i ; ++ j) {
                min_index = osg::minimum(min_index, indices[j]);
            }
            for(j = lastUnique ; j < i ; ++ j) {
                remapDuplicatesToOrignals[indices[j]] = min_index;
            }
            lastUnique = i;
        }
    }

    unsigned int min_index = indices[lastUnique];
    for(j = lastUnique + 1 ; j < i ; ++ j) {
        min_index = osg::minimum(min_index, indices[j]);
    }
    for(j = lastUnique ; j < i ; ++ j) {
        remapDuplicatesToOrignals[indices[j]] = min_index;
    }

    // copy the arrays.
    IndexList finalMapping(numVertices);
    IndexList copyMapping;
    copyMapping.reserve(numUnique);
    unsigned int currentIndex = 0;
    for(i = 0 ; i < numVertices ; ++ i) {
        if (remapDuplicatesToOrignals[i] == i) {
            finalMapping[i] = currentIndex;
            copyMapping.push_back(i);
            currentIndex++;
        }
        else {
            finalMapping[i] = finalMapping[remapDuplicatesToOrignals[i]];
        }
    }

    // remap any shared vertex attributes
    RemapArray ra(copyMapping);
    arrayComparitor.accept(ra);

    // triangulate faces
    {
        TriangleIndexor ti;
        ti._maxIndex = numVertices;
        ti._remapping = finalMapping;

        for(itr = primitives.begin() ; itr != primitives.end() ; ++ itr) {
            (*itr)->accept(ti);
        }

        addDrawElements(ti._indices, osg::PrimitiveSet::TRIANGLES, new_primitives);
    }

    // line-ify line-type primitives
    {
        LineIndexor li, wi; // lines and wireframes
        li._maxIndex = numVertices;
        wi._maxIndex = numVertices;
        li._remapping = finalMapping;
        wi._remapping = finalMapping;

        for(itr = primitives.begin() ; itr != primitives.end() ; ++ itr) {
            bool isWireframe = false;
            if((*itr)->getUserValue("wireframe", isWireframe) && isWireframe) {
                (*itr)->accept(wi);
            }
            else {
                (*itr)->accept(li);
            }
        }
        addDrawElements(li._indices, osg::PrimitiveSet::LINES, new_primitives);
        addDrawElements(wi._indices, osg::PrimitiveSet::LINES, new_primitives, "wireframe");
    }

    // adds points primitives
    {
        IndexList points;
        for(itr = primitives.begin() ; itr != primitives.end() ; ++ itr) {
            if((*itr) && (*itr)->getMode() == osg::PrimitiveSet::POINTS) {
                for(unsigned int k = 0 ; k < (*itr)->getNumIndices() ; ++ k) {
                    points.push_back(finalMapping[(*itr)->index(k)]);
                }
            }
        }
        addDrawElements(points, osg::PrimitiveSet::POINTS, new_primitives);
    }

    geom.setPrimitiveSetList(new_primitives);
    setProcessed(geom);
}


void IndexMeshVisitor::addDrawElements(IndexList& data,
                                       osg::PrimitiveSet::Mode mode,
                                       osg::Geometry::PrimitiveSetList& primitives,
                                       std::string userValue) {
    if(!data.empty()) {
        osg::DrawElementsUInt* elements = new osg::DrawElementsUInt(mode,
                                                                    data.begin(),
                                                                    data.end());
        if(!userValue.empty()) {
            elements->setUserValue(userValue, true);
        }
        primitives.push_back(elements);
    }
}
