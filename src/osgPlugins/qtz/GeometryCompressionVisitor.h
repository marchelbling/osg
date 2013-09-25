#ifndef GEOMETRY_COMPRESSION_VISITOR_H
#define GEOEMTRY_COMPRESSION_VISITOR_H

#include <osg/NodeVisitor>
#include <osg/BoundingBox>

#include "CArray.h"

// visitor that will be executed for each geometry = awesome
class GeometryCompressionVisitor : public osg::NodeVisitor
{
  public:

    enum Attribute {
      vertex = 1 << 0,
      normal = 1 << 1,
      uv     = 1 << 2,
      color  = 1 << 3
    };

    GeometryCompressionVisitor(bool compress,
                               int  attr     = vertex,
                               int  bytes    = 3,
                               int  mode     = CArray::quantization);

    void setBoundingBox(const osg::BoundingBox& bb) { _boundingBox = bb;}
    void setAttributes(int attributes) { _attributes = attributes; }
    void setMode(int mode) { _mode = mode; }
    void setBytes(int bytes) { _bytes = std::min(3, std::max(1, bytes)); }

    void apply(osg::Geometry& geom);
    void apply(osg::Geode& node);


  private:
    bool handleVertices() const { return _attributes & vertex; }
    bool handleNormals()  const { return _attributes & normal; }
    bool handleUvs()      const { return _attributes & uv; }
    bool handleColors()   const { return _attributes & color; }

    osg::BoundingBox _boundingBox;
    int              _bytes;
    bool             _compress;
    int              _attributes;
    int              _mode;
};

#endif
