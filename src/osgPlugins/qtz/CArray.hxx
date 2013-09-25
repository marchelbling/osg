#ifndef CARRAY_HXX
#define CARRAY_HXX

#include <cfloat>

template <typename C>
CBaseArray<C>::CBaseArray(T const& bbl, T const& ufr, int bytes, int mode) :
  _bbl(bbl), _ufr(ufr), _bytes(bytes), _mode(mode), _data(NULL)
{
}


template <typename C>
CBaseArray<C>::~CBaseArray()
{
}


template<typename C>
void CBaseArray<C>::compress(C const* input,
                             std::vector< std::vector<size_t> >const& strips)
{
  osg::ref_ptr<C> predictions = NULL;

  if(usePrediction() && !strips.empty())
  {
    predictions = new C(input->getNumElements());
    predictParallelogram(predictions, input, strips);
    input = predictions.get();
  }

  if(useQuantization())
  {
    _data = new C;
    quantizeArray(dynamic_cast<C*>(_data.get()), input, _bbl, _ufr , strips);
  }
  else
    _data = new C(input->begin(), input->end());
}


template<typename C>
void CBaseArray<C>::decompress(C const* input,
                               std::vector< std::vector<size_t> >const& strips)
{
  osg::ref_ptr<C> positions = NULL;

  if(useQuantization())
  {
    _data = new C;
    unquantizeArray(dynamic_cast<C*>(_data.get()), input, _bbl, _ufr, strips);
    input = dynamic_cast<C*>(_data.get());
  }

  if(usePrediction() && !strips.empty())
  {
    positions = new C(input->getNumElements());
    unpredictParallelogram(positions, input, strips);
    _data = positions.get();
  }
}


template<typename C>
void CBaseArray<C>::predictParallelogram(C* predictions,
                                         C const* positions,
                                         std::vector< std::vector<size_t> > const& strips)
{
  std::vector<bool> processed(positions->getNumElements(), false);
  T vertex;

  // reset bounding box vectors
  setMaxVector(_bbl);
  setMinVector(_ufr);

  for(std::vector< std::vector<size_t> >::const_iterator it_strip = strips.begin() ;
      it_strip != strips.end() ; ++ it_strip)
  {
    std::deque<T> strip_window;
    for(std::vector<size_t>::const_iterator it = it_strip->begin() ;
        it != it_strip->end() ; ++ it)
    {
      vertex = (*positions)[*it];
      if(strip_window.size() != 3)
        (*predictions)[*it] = vertex;
      else
      {
        if(!processed[*it])
        {
          (*predictions)[*it] = vertex - parallelogramPrediction<T>(strip_window);
          _bbl = vectorMin(_bbl, (*predictions)[*it]);
          _ufr = vectorMax(_ufr, (*predictions)[*it]);
        }
        strip_window.pop_front();
      }
      processed[*it] = true;
      strip_window.push_back(vertex);
    }
  }
}


template<typename C>
void CBaseArray<C>::unpredictParallelogram(C* positions,
                                           C const* predictions,
                                           std::vector< std::vector<size_t> > const& strips)
{
  std::vector<bool> processed(predictions->getNumElements(), false);
  T prediction;

  // first keep strip starting vertices invariant
  for(std::vector< std::vector<size_t> >::const_iterator it_strip = strips.begin() ;
      it_strip != strips.end() ; ++ it_strip)
  {
    for(std::vector<size_t>::const_iterator it = it_strip->begin() ;
        it != it_strip->begin() + 3 ; ++ it)
    {
      (*positions)[*it] = (*predictions)[*it];
      processed[*it] = true;
    }
  }

  for(std::vector< std::vector<size_t> >::const_iterator it_strip = strips.begin() ;
      it_strip != strips.end() ; ++ it_strip)
  {
    std::deque<T> strip_window;
    for(std::vector<size_t>::const_iterator it = it_strip->begin() ;
        it != it_strip->end() ; ++ it)
    {
      prediction = (*predictions)[*it];
      if(strip_window.size() == 3)
      {
        if(!processed[*it])
          (*positions)[*it] = prediction + parallelogramPrediction<T>(strip_window);
        strip_window.pop_front();
      }
      else if(!processed[*it])
        (*positions)[*it] = prediction;

      processed[*it] = true;
      strip_window.push_back((*positions)[*it]);
    }
  }
}


template<typename C>
osg::Vec3 CBaseArray<C>::quantize(osg::Vec3 const& v, osg::Vec3 const& h, osg::Vec3 const& bbl) const
{
  return osg::Vec3(static_cast<int>(.5f + (v.x() - bbl.x()) / h.x()),
                   static_cast<int>(.5f + (v.y() - bbl.y()) / h.y()),
                   static_cast<int>(.5f + (v.z() - bbl.z()) / h.z()));
}


template<typename C>
osg::Vec2 CBaseArray<C>::quantize(osg::Vec2 const& v, osg::Vec2 const& h, osg::Vec2 const& bbl) const
{
  return osg::Vec2(static_cast<int>(.5f + (v.x() - bbl.x()) / h.x()),
                   static_cast<int>(.5f + (v.y() - bbl.y()) / h.y()));
}


template<typename C>
osg::Vec3 CBaseArray<C>::unquantize(osg::Vec3 const& v, osg::Vec3 const& h, osg::Vec3 const& bbl) const
{
  return bbl + osg::Vec3(v.x() * h.x(),
                         v.y() * h.y(),
                         v.z() * h.z());
}


template<typename C>
osg::Vec2 CBaseArray<C>::unquantize(osg::Vec2 const& v, osg::Vec2 const& h, osg::Vec2 const& bbl) const
{
  return bbl + osg::Vec2(v.x() * h.x(),
                         v.y() * h.y());
}


template<typename C>
osg::Vec3 CBaseArray<C>::vectorMin(osg::Vec3 const& a, osg::Vec3 const&b) const
{
  return osg::Vec3(std::min(a.x(), b.x()),
                   std::min(a.y(), b.y()),
                   std::min(a.z(), b.z()));
}

template<typename C>
osg::Vec2 CBaseArray<C>::vectorMin(osg::Vec2 const& a, osg::Vec2 const&b) const
{
  return osg::Vec2(std::min(a.x(), b.x()),
                   std::min(a.y(), b.y()));
}

template<typename C>
osg::Vec3 CBaseArray<C>::vectorMax(osg::Vec3 const& a, osg::Vec3 const&b) const
{
  return osg::Vec3(std::max(a.x(), b.x()),
                   std::max(a.y(), b.y()),
                   std::max(a.z(), b.z()));
}

template<typename C>
osg::Vec2 CBaseArray<C>::vectorMax(osg::Vec2 const& a, osg::Vec2 const&b) const
{
  return osg::Vec2(std::max(a.x(), b.x()),
                   std::max(a.y(), b.y()));
}

template<typename C>
void CBaseArray<C>::setMinVector(osg::Vec3& v) const
{ v = osg::Vec3(FLT_MIN, FLT_MIN, FLT_MIN); }

template<typename C>
void CBaseArray<C>::setMinVector(osg::Vec2& v) const
{ v = osg::Vec2(FLT_MIN, FLT_MIN); }

template<typename C>
void CBaseArray<C>::setMaxVector(osg::Vec3& v) const
{ v = osg::Vec3(FLT_MAX, FLT_MAX, FLT_MAX); }

template<typename C>
void CBaseArray<C>::setMaxVector(osg::Vec2& v) const
{ v = osg::Vec2(FLT_MAX, FLT_MAX); }


template<typename C>
osg::BoundingBox CBaseArray<C>::makeBoundingBox(osg::Vec3 const& bbl, osg::Vec3 const& ufr) const
{  return osg::BoundingBox(bbl, ufr); }

template<typename C>
osg::BoundingBox CBaseArray<C>::makeBoundingBox(osg::Vec2 const& bbl, osg::Vec2 const& ufr) const
{  return osg::BoundingBox(bbl.x(), bbl.y(), 0.f, ufr.x(), ufr.y(), 0.f); }

#endif
