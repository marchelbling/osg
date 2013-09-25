#include "CArray.h"

#include <iostream>
#include <cmath>

CArray::CArray(osg::BoundingBoxf const& bbox, int bytes, int mode) :
  _bbox(bbox), _bytes(bytes), _mode(mode), _data(NULL)
{
}


CArray::~CArray()
{
}


template<typename C>
void CArray::compress(C const* input,
                      std::vector< std::vector<size_t> >const& strips,
                      typename C::ElementDataType const& bbl,
                      typename C::ElementDataType const& ufr)
{
  osg::ref_ptr<C> predictions = NULL;

  if(usePrediction() && !strips.empty())
  {
    predictions = new C(input->getNumElements());
    predictParallelogram<C>(predictions, input, strips);
    input = predictions.get();
  }

  if(useQuantization())
  {
    _data = new C;
    quantizeArray<C>(dynamic_cast<C*>(_data.get()), input, bbl, ufr);
  }
  else
    _data = new C(input->begin(), input->end());
}


template<typename C>
void CArray::decompress(C const* input,
                        std::vector< std::vector<size_t> >const& strips,
                        typename C::ElementDataType const& bbl,
                        typename C::ElementDataType const& ufr)
{
  osg::ref_ptr<C> positions = NULL;

  if(useQuantization())
  {
    _data = new C;
    unquantizeArray<C>(dynamic_cast<C*>(_data.get()), input, bbl, ufr);
    input = dynamic_cast<C*>(_data.get());
  }

  if(usePrediction() && !strips.empty())
  {
    positions = new C(input->getNumElements());
    unpredictParallelogram<C>(positions, input, strips);
    _data = positions.get();
  }
}


void CArray::compressVertices(osg::Array const* buffer,
                               std::vector< std::vector<size_t> >const& strips)
{
  compress<osg::Vec3Array>(dynamic_cast<osg::Vec3Array const*>(buffer),
                           strips,
                           _bbox.corner(0),
                           _bbox.corner(7));
}


void CArray::decompressVertices(osg::Array const* buffer,
                                 std::vector< std::vector<size_t> >const& strips)
{
  decompress<osg::Vec3Array>(dynamic_cast<osg::Vec3Array const*>(buffer),
                             strips,
                             _bbox.corner(0),
                             _bbox.corner(7));
}


void CArray::compressUvs(osg::Array const* buffer,
                          std::vector< std::vector<size_t> >const& strips)
{
  compress<osg::Vec2Array>(dynamic_cast<osg::Vec2Array const*>(buffer),
                           strips,
                           osg::Vec2(0., 0.),
                           osg::Vec2(1., 1.));
}


void CArray::decompressUvs(osg::Array const* buffer,
                            std::vector< std::vector<size_t> >const& strips)
{
  decompress<osg::Vec2Array>(dynamic_cast<osg::Vec2Array const*>(buffer),
                             strips,
                             osg::Vec2(0., 0.),
                             osg::Vec2(1., 1.));
}


template<typename C>
void CArray::predictParallelogram(C* predictions,
                                  C const* positions,
                                  std::vector< std::vector<size_t> > const& strips)
{
  std::vector<bool> processed(positions->getNumElements(), false);
  typename C::ElementDataType vertex;

  for(std::vector< std::vector<size_t> >::const_iterator it_strip = strips.begin() ;
      it_strip != strips.end() ; ++ it_strip)
  {
    std::deque<typename C::ElementDataType> strip_window;
    for(std::vector<size_t>::const_iterator it = it_strip->begin() ;
        it != it_strip->end() ; ++ it)
    {
      vertex = (*positions)[*it];
      if(strip_window.size() != 3)
      {
        if(!processed[*it])
          (*predictions)[*it] = vertex;
      }
      else
      {
        if(!processed[*it])
          (*predictions)[*it] = vertex - parallelogramPrediction<typename C::ElementDataType>(strip_window);
        strip_window.pop_front();
      }
      processed[*it] = true;
      strip_window.push_back(vertex);
    }
  }
}


template<typename C>
void CArray::unpredictParallelogram(C* positions,
                                     C const* predictions,
                                     std::vector< std::vector<size_t> > const& strips)
{
  std::vector<bool> processed(predictions->getNumElements(), false);
  typename C::ElementDataType prediction;

  for(std::vector< std::vector<size_t> >::const_iterator it_strip = strips.begin() ;
      it_strip != strips.end() ; ++ it_strip)
  {
    std::deque<typename C::ElementDataType> strip_window;
    for(std::vector<size_t>::const_iterator it = it_strip->begin() ;
        it != it_strip->end() ; ++ it)
    {
      prediction = (*predictions)[*it];
      if(strip_window.size() == 3)
      {
        if(!processed[*it])
          (*positions)[*it] = prediction + parallelogramPrediction<typename C::ElementDataType>(strip_window);
        strip_window.pop_front();
      }
      else if(!processed[*it])
        (*positions)[*it] = prediction;

      processed[*it] = true;
      strip_window.push_back((*positions)[*it]);
    }
  }
}


void CArray::compressNormals(osg::Array const* buffer)
{
  if(usePrediction())
  {
    osg::ref_ptr<osg::Vec2Array> projections = new osg::Vec2Array;

    projectAzimuth(projections.get(), dynamic_cast<osg::Vec3Array const*>(buffer));

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


void CArray::projectAzimuth(osg::Vec2Array* projections, osg::Vec3Array const* normals)
{
  for(osg::Vec3Array::const_iterator it = normals->begin() ; it != normals->end() ; ++ it)
  {
    float alpha = std::sqrt(2.f / std::max<float>(0.0001f, 1.f - it->z()));
    projections->push_back(osg::Vec2(it->x() * alpha,
                                     it->y() * alpha));
  }
}


void CArray::decompressNormals(osg::Array const* buffer)
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

    unprojectAzimuth(dynamic_cast<osg::Vec3Array*>(_data.get()),
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


void CArray::unprojectAzimuth(osg::Vec3Array* normals, osg::Vec2Array const* projections)
{
  for(osg::Vec2Array::const_iterator it = projections->begin() ; it != projections->end() ; ++ it)
  {
    float x2 = it->x() * it->x(),
          y2 = it->y() * it->y();
    float beta = std::sqrt(1.f - (x2 + y2) / 4.f);
    normals->push_back(osg::Vec3(it->x() * beta,
                                 it->y() * beta,
                                 -1.f + (x2 + y2) / 2.f));
  }
}


template<>
osg::Vec3 CArray::quantize<osg::Vec3>(osg::Vec3 const& v, osg::Vec3 const& h, osg::Vec3 const& bbl) const
{
  return osg::Vec3(static_cast<int>(.5f + (v.x() - bbl.x()) / h.x()),
                   static_cast<int>(.5f + (v.y() - bbl.y()) / h.y()),
                   static_cast<int>(.5f + (v.z() - bbl.z()) / h.z()));
}


template<>
osg::Vec2 CArray::quantize<osg::Vec2>(osg::Vec2 const& v, osg::Vec2 const& h, osg::Vec2 const& bbl) const
{
  return osg::Vec2(static_cast<int>(.5f + (v.x() - bbl.x()) / h.x()),
                   static_cast<int>(.5f + (v.y() - bbl.y()) / h.y()));
}


template<>
osg::Vec3 CArray::unquantize<osg::Vec3>(osg::Vec3 const& v, osg::Vec3 const& h, osg::Vec3 const& bbl) const
{
  return bbl + osg::Vec3(v.x() * h.x(),
                         v.y() * h.y(),
                         v.z() * h.z());
}


template<>
osg::Vec2 CArray::unquantize<osg::Vec2>(osg::Vec2 const& v, osg::Vec2 const& h, osg::Vec2 const& bbl) const
{
  return bbl + osg::Vec2(v.x() * h.x(),
                         v.y() * h.y());
}
