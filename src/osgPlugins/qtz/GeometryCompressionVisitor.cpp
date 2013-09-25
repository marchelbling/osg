#include <osg/Geometry>
#include <osg/Geode>
#include <osg/ValueObject>
#include <iostream>

#include "GeometryCompressionVisitor.h"


GeometryCompressionVisitor::GeometryCompressionVisitor(bool compress,
                                                       int  attr,
                                                       int  bytes,
                                                       int  mode) :
    _attributes(attr), _bytes(bytes), _compress(compress), _mode(mode),
    osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
{}


void GeometryCompressionVisitor::apply(osg::Geometry& geom)
{
  if ( !geom.getName().empty() )
    osg::notify(osg::NOTICE) << "Geometry: " << geom.getName() << std::endl;
  else
    osg::notify(osg::NOTICE) << "Geometry: " << &geom << std::endl;

  if(_compress)
  {
    //set compression parameter in userdata
    geom.setUserValue("bbl_x", _boundingBox.corner(0).x()); 
    geom.setUserValue("bbl_y", _boundingBox.corner(0).y()); 
    geom.setUserValue("bbl_z", _boundingBox.corner(0).z()); 
    geom.setUserValue("ufr_x", _boundingBox.corner(7).x()); 
    geom.setUserValue("ufr_y", _boundingBox.corner(7).y()); 
    geom.setUserValue("ufr_z", _boundingBox.corner(7).z()); 
    geom.setUserValue("attributes", _attributes);
    geom.setUserValue("mode", _mode);
    geom.setUserValue("bytes", _bytes);
  }
  else
  {
    float bx, by, bz, ux, uy, uz;
    if(geom.getUserValue("bbl_x", bx) &&
       geom.getUserValue("bbl_y", by) &&
       geom.getUserValue("bbl_z", bz) &&
       geom.getUserValue("ufr_x", ux) &&
       geom.getUserValue("ufr_y", uy) &&
       geom.getUserValue("ufr_z", uz))
      _boundingBox = osg::BoundingBox(bx, by, bz, ux, uy, uz);
    else
      return;

    if(!geom.getUserValue("attributes", _attributes)) return;
    if(!geom.getUserValue("mode", _mode)) return;
    if(!geom.getUserValue("bytes", _bytes)) return;
  }

  CArray ca(_boundingBox, _bytes, _mode);

  //// suppose that the geometry already stores topology as strips
  std::vector< std::vector<size_t> > strips;

  for(size_t ii = 0 ; ii < geom.getNumPrimitiveSets() ; ++ ii)
  {
    osg::PrimitiveSet* strip = geom.getPrimitiveSet(ii);
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

  if(handleVertices())
  {
    osg::ref_ptr<osg::Array> vertices = geom.getVertexArray();
    if(_compress)
      ca.compressVertices(vertices.get(), strips);
    else
      ca.decompressVertices(vertices.get(), strips);
    geom.setVertexArray(ca.getData());
  }

  if(handleNormals())
  {
    osg::ref_ptr<osg::Array> normals = geom.getNormalArray();
    if(_compress)
      ca.compressNormals(normals.get());
    else
      ca.decompressNormals(normals.get());
    geom.setNormalArray(ca.getData());
  }

  if(handleUvs())
  {
    for(size_t uv_id = 0 ; uv_id < geom.getNumTexCoordArrays() ; ++ uv_id)
    {
      osg::ref_ptr<osg::Array> uvs = geom.getTexCoordArray(uv_id);
      if(_compress)
        ca.compressUvs(uvs.get(), strips);
      else
        ca.decompressUvs(uvs.get(), strips);
      geom.setTexCoordArray(uv_id, ca.getData());
    }
  }

  if(handleColors())
  {
    osg::ref_ptr<osg::Array> colors = geom.getColorArray();
    if(_compress) 
      ca.compressColors(colors.get());
    else
      ca.decompressColors(colors.get());
    geom.setColorArray(ca.getData());
  }
}


void GeometryCompressionVisitor::apply(osg::Geode& node)
{
  for (int i = 0 ; i < node.getNumDrawables(); i++) {
    if (node.getDrawable(i)) {
      osg::Geometry* geom = node.getDrawable(i)->asGeometry();

      if (geom)
        apply(*geom);
    }
  }
}
