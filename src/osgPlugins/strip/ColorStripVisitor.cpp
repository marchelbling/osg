#include "ColorStripVisitor.h"

#include <cstdlib> // rand
#include <ctime>
#include <osg/Material>

void ColorStripVisitor::apply(osg::Geode& node)
{
  for (int i = 0 ; i < node.getNumDrawables(); i++) {
    if (node.getDrawable(i)) {
      osg::Geometry* geom = node.getDrawable(i)->asGeometry();

      if (geom)
      {
        if(_geometryStrips.find(geom) == _geometryStrips.end())
          _geometryStrips.insert(std::pair<osg::Geometry*,
                                           GeometryList>(geom,
                                                         GeometryList()));
      }
    }
  }
}

osg::Vec4 ColorStripVisitor::randomColor()
{
  static bool randSeeded = false;
  if(!randSeeded)
  {
    srand (time(NULL));
    randSeeded = true;
  }

  return osg::Vec4(rand() * 1.f / RAND_MAX,
                   rand() * 1.f / RAND_MAX,
                   rand() * 1.f / RAND_MAX,
                   0.2f);
}

void ColorStripVisitor::splitStrips()
{
  for(std::map<osg::Geometry*, GeometryList>::iterator it_geom = _geometryStrips.begin() ;
      it_geom != _geometryStrips.end() ;
      ++ it_geom)
  {
    osg::Geometry* geom = it_geom->first;
    GeometryList& clones = it_geom->second;

    // 1. clone geometry for each strip
    for(size_t ii = 0 ; ii < geom->getNumPrimitiveSets() ; ++ ii)
    {
      if(geom->getPrimitiveSet(ii)->getMode() != osg::PrimitiveSet::TRIANGLE_STRIP)
        continue;

      osg::Geometry* clone = dynamic_cast<osg::Geometry *>(geom->clone(osg::CopyOp::SHALLOW_COPY));
      osg::PrimitiveSet const* strip = clone->getPrimitiveSet(ii);
      clone->addPrimitiveSet(const_cast<osg::PrimitiveSet*>(strip));
      // Just keep last added strip
      clone->removePrimitiveSet(0 , clone->getNumPrimitiveSets() - 1);

      // Set strip color
      osg::ref_ptr<osg::StateSet> stateset = clone->getOrCreateStateSet();

      // Creating a random color for clone
      osg::ref_ptr<osg::Material> mat(new osg::Material);
      mat->setShininess(osg::Material::FRONT, 96.f);
      mat->setDiffuse(osg::Material::FRONT, randomColor());
      stateset->setAttribute(mat.get());

      clones.push_back(clone);
    }

    // 2. removes original geometry and replaces it with clones
    for(std::map<osg::Geometry*, GeometryList>::const_iterator it_geom = _geometryStrips.begin() ;
        it_geom != _geometryStrips.end() ;
        ++ it_geom)
    {
      osg::Geometry* geom = it_geom->first;
      GeometryList clones = it_geom->second;
      if(clones.size())
      {
        std::vector<osg::Node*> parents = geom->getParents();

        for (unsigned int j = 0; j < parents.size(); j++)
        {
          osg::Geode* parent = parents[j]->asGeode();
          if(parent)
          {
            for(GeometryList::const_iterator clone = clones.begin() ; clone != clones.end() ; ++ clone)
              parent->addDrawable(*clone);
            parent->removeDrawable(geom);
          }
        }
      }
    }
  }
}
