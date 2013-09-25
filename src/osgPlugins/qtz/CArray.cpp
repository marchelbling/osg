#include "CArray.h"

#include <cmath>

void CNormalArray::compress(osg::Array const* buffer)
{
  if(usePrediction())
  {
    osg::ref_ptr<osg::Vec2Array> projections = new osg::Vec2Array;

    project(projections.get(), dynamic_cast<osg::Vec3Array const*>(buffer));

    if(useQuantization())
    {
      _data = new osg::Vec2Array;
      quantizeArray<osg::Vec2Array>(dynamic_cast<osg::Vec2Array*>(_data.get()),
                                    projections.get(),
                                    osg::Vec2f(-1., -1.),
                                    osg::Vec2f(1., 1.));
    }
    else
      _data = projections;
  }
  else if(useQuantization())
  {
    _data = new osg::Vec3Array;
    quantizeArray<osg::Vec3Array>(dynamic_cast<osg::Vec3Array*>(_data.get()),
                                  dynamic_cast<osg::Vec3Array const*>(buffer),
                                  osg::Vec3f(-1., -1., -1.),
                                  osg::Vec3f(1., 1., 1.));
  }
}


void CNormalArray::decompress(osg::Array const* buffer)
{
  _data = new osg::Vec3Array;

  if(usePrediction())
  {
    osg::ref_ptr<osg::Vec2Array const> projections = NULL;

    if(useQuantization())
    {
      osg::ref_ptr<osg::Vec2Array> unquantized = new osg::Vec2Array;
      unquantizeArray<osg::Vec2Array>(unquantized.get(),
                                      dynamic_cast<osg::Vec2Array const*>(buffer),
                                      osg::Vec2f(-1., -1.),
                                      osg::Vec2f(1., 1.));
      projections = unquantized;
    }
    else
      projections = dynamic_cast<osg::Vec2Array const*>(buffer);

    unproject(dynamic_cast<osg::Vec3Array*>(_data.get()),
                      projections.get());
  }
  else if(useQuantization())
  {
    unquantizeArray<osg::Vec3Array>(dynamic_cast<osg::Vec3Array*>(_data.get()),
                                    dynamic_cast<osg::Vec3Array const*>(buffer),
                                    osg::Vec3f(-1., -1., -1.),
                                    osg::Vec3f(1., 1., 1.));
  }
}


void CNormalArray::projectAzimuth(osg::Vec2Array* projections, osg::Vec3Array const* normals)
{
  for(osg::Vec3Array::const_iterator it = normals->begin() ; it != normals->end() ; ++ it)
  {
    float alpha = std::sqrt(2.f / std::max<float>(0.0001f, 1.f - it->z()));
    projections->push_back(osg::Vec2(it->x() * alpha,
                                     it->y() * alpha));
  }
}


void CNormalArray::unprojectAzimuth(osg::Vec3Array* normals, osg::Vec2Array const* projections)
{
  for(osg::Vec2Array::const_iterator it = projections->begin() ; it != projections->end() ; ++ it)
  {
    float x2 = it->x() * it->x(),
          y2 = it->y() * it->y();
    float beta = std::sqrt(std::max(0.0001f, 1.f - (x2 + y2) / 4.f));
    normals->push_back(osg::Vec3(it->x() * beta,
                                 it->y() * beta,
                                 -1.f + (x2 + y2) / 2.f));
  }
}

