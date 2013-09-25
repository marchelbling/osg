#ifndef COLORSTRIPVISITOR_H
#define COLORSTRIPVISITOR_H

#include <osg/NodeVisitor>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Vec4>

#include<map>
#include<vector>

class ColorStripVisitor : public osg::NodeVisitor
{
  public:
    typedef std::vector<osg::ref_ptr<osg::Geometry> > GeometryList;
 
    ColorStripVisitor() : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
    {}

    void apply(osg::Geode& node);
    void splitStrips();

  private:
    osg::Vec4 randomColor();
    std::map< osg::Geometry*, GeometryList > _geometryStrips;


};

#endif
