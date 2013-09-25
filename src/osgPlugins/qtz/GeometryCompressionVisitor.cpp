#include <osg/Geometry>
#include <osg/Geode>
#include <osg/ValueObject>
#include <iostream>
#include <cfloat> // FLT_MIN, FLT_MAX

#include "GeometryCompressionVisitor.h"


GeometryCompressionVisitor::GeometryCompressionVisitor(bool compress,
                                                       int  attr,
                                                       int  bytes,
                                                       int  mode) :
    _attributes(attr), _bytes(bytes), _compress(compress), _mode(mode),
    osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
{}


void GeometryCompressionVisitor::setVertexBoundingBox(const osg::Geode& node)
{
  osg::BoundingSphere bs = node.getBound();
  osg::BoundingBox bb;
  bb.expandBy(bs);

  _vertexBoundingBox = bb;
}


void GeometryCompressionVisitor::setUvBoundingBox(const osg::Geode& node)
{
  // set uvs bounding box
  float umin = FLT_MAX, vmin = FLT_MAX, umax = FLT_MIN, vmax = FLT_MIN;
  for (int i = 0 ; i < node.getNumDrawables(); i++)
  {
    if (node.getDrawable(i))
    {
      osg::Geometry const* geom = node.getDrawable(i)->asGeometry();
      for(size_t uv_id = 0 ; uv_id < geom->getNumTexCoordArrays() ; ++ uv_id)
      {
        osg::Vec2Array const* uvs = dynamic_cast<osg::Vec2Array const*>(geom->getTexCoordArray(uv_id));
        for(osg::Vec2Array::const_iterator it = uvs->begin() ; it != uvs->end() ; ++ it)
        {
          umin = std::min(umin, it->x());
          vmin = std::min(vmin, it->y());

          umax = std::max(umax, it->x());
          vmax = std::max(vmax, it->y());
        }
      }
    }
  }

  _uvBoundingBox = osg::BoundingBox(umin, vmin, 0.f, umax, vmax, 0.f);
}

void GeometryCompressionVisitor::apply(osg::Geometry& geom)
{
  if ( !geom.getName().empty() )
    osg::notify(osg::NOTICE) << "Geometry: " << geom.getName() << std::endl;
  else
    osg::notify(osg::NOTICE) << "Geometry: " << &geom << std::endl;

  handleCompressionParameters(geom);


  std::vector< std::vector<size_t> > strips = getStripsVector(geom);

  if(handleVertices())
  {
    CVertexArray ca(_vertexBoundingBox.corner(0), _vertexBoundingBox.corner(7), _bytes, _mode);
    osg::ref_ptr<osg::Array> vertices = geom.getVertexArray();
    if(_compress)
      ca.compress(dynamic_cast<osg::Vec3Array const*>(vertices.get()), strips);
    else
      ca.decompress(dynamic_cast<osg::Vec3Array const*>(vertices.get()), strips);
    geom.setVertexArray(ca.getData());
  }

  if(handleNormals())
  {
    CNormalArray ca(osg::Vec3(-1., -1., -1.), osg::Vec3(1., 1., 1.), _bytes, _mode);
    osg::ref_ptr<osg::Array> normals = geom.getNormalArray();
    if(_compress)
      ca.compress(normals.get());
    else
      ca.decompress(normals.get());
    geom.setNormalArray(ca.getData());
  }

  if(handleUvs())
  {
    //FIXME: UV bounding box should be computed
    CUVArray ca(osg::Vec2(-1., -1.), osg::Vec2(1., 1.), _bytes, _mode);
    for(size_t uv_id = 0 ; uv_id < geom.getNumTexCoordArrays() ; ++ uv_id)
    {
      osg::ref_ptr<osg::Array> uvs = geom.getTexCoordArray(uv_id);
      if(_compress)
        ca.compress(dynamic_cast<osg::Vec2Array const*>(uvs.get()), strips);
      else
        ca.decompress(dynamic_cast<osg::Vec2Array const*>(uvs.get()), strips);
      geom.setTexCoordArray(uv_id, ca.getData());
    }
  }
}


void GeometryCompressionVisitor::apply(osg::Geode& node)
{
  // set attributes bounding box
  setVertexBoundingBox(node);
  setUvBoundingBox(node);

  for (int i = 0 ; i < node.getNumDrawables(); i++) {
    if (node.getDrawable(i)) {
      osg::Geometry* geom = node.getDrawable(i)->asGeometry();

      if (geom)
        apply(*geom);
    }
  }
}


void GeometryCompressionVisitor::handleCompressionParameters(osg::Geometry& geom)
{
  if(_compress)
  {
    //set compression parameters in user values
    geom.setUserValue("bbl_x", _vertexBoundingBox.corner(0).x());
    geom.setUserValue("bbl_y", _vertexBoundingBox.corner(0).y());
    geom.setUserValue("bbl_z", _vertexBoundingBox.corner(0).z());
    geom.setUserValue("ufr_x", _vertexBoundingBox.corner(7).x());
    geom.setUserValue("ufr_y", _vertexBoundingBox.corner(7).y());
    geom.setUserValue("ufr_z", _vertexBoundingBox.corner(7).z());
    geom.setUserValue("attributes", _attributes);
    geom.setUserValue("mode", _mode);
    geom.setUserValue("bytes", _bytes);
  }
  else
  {
    //get compression parameters from user values
    float bx, by, bz, ux, uy, uz;
    if(geom.getUserValue("bbl_x", bx) &&
       geom.getUserValue("bbl_y", by) &&
       geom.getUserValue("bbl_z", bz) &&
       geom.getUserValue("ufr_x", ux) &&
       geom.getUserValue("ufr_y", uy) &&
       geom.getUserValue("ufr_z", uz))
      _vertexBoundingBox = osg::BoundingBox(bx, by, bz, ux, uy, uz);
    else
      return;

    if(!geom.getUserValue("attributes", _attributes)) return;
    if(!geom.getUserValue("mode", _mode)) return;
    if(!geom.getUserValue("bytes", _bytes)) return;
  }
}


std::vector< std::vector<size_t> > GeometryCompressionVisitor::getStripsVector(osg::Geometry const& geom) const
{
  std::vector< std::vector<size_t> > strips;

  // suppose that the geometry already stores topology as strips
  for(size_t ii = 0 ; ii < geom.getNumPrimitiveSets() ; ++ ii)
  {
    osg::PrimitiveSet const* strip = geom.getPrimitiveSet(ii);
    if(strip)
    {
      if(strip->getMode() == osg::PrimitiveSet::TRIANGLE_STRIP)
      {
        std::vector<size_t> current_strip;
        for(size_t jj = 0 ; jj < strip->getNumIndices() ; ++ jj)
          current_strip.push_back(strip->index(jj));
        strips.push_back(current_strip);
      }
      else if(strip->getMode() == osg::PrimitiveSet::TRIANGLES)
      {
        size_t nb_triangles = strip->getNumIndices() / 3;
        for(size_t jj = 0 ; jj < nb_triangles ; ++ jj)
        {
          std::vector<size_t> triangle;
          triangle.push_back(strip->index(3 * jj));
          triangle.push_back(strip->index(3 * jj + 1));
          triangle.push_back(strip->index(3 * jj + 2));
          strips.push_back(triangle);
        }
      }
    }
  }

  return strips;
}
