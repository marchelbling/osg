#ifndef CARRAY_H
#define CARRAY_H

// STL
#include <vector>
#include <deque>
#include <fstream>
// OSG
#include <osg/Array>
#include <osg/BoundingBox>

class CArray
{
  public:

    enum Mode {
      quantization = 1 << 0,
      prediction   = 1 << 1
    };

    CArray(osg::BoundingBoxf const&, int, int);
    ~CArray();

    // compression methods
    void compressVertices(osg::Array const*,
                          std::vector< std::vector<size_t> >const& strips = std::vector< std::vector<size_t> >());
    void compressUvs(osg::Array const*,
                     std::vector< std::vector<size_t> >const& strips = std::vector< std::vector<size_t> >());
    void compressNormals(osg::Array const*);
    void compressColors(osg::Array const*) //TODO:
    {}

    // decompression methods
    void decompressVertices(osg::Array const*,
                            std::vector< std::vector<size_t> >const& strips = std::vector< std::vector<size_t> >());
    void decompressUvs(osg::Array const*,
                       std::vector< std::vector<size_t> >const& strips = std::vector< std::vector<size_t> >());
    void decompressNormals(osg::Array const*);
    void decompressColors(osg::Array const*) //TODO:
    {}

    osg::Array* getData() const
    {
      return _data.get();
    }

  private:
    bool useQuantization() const
    { return static_cast<bool>(_mode & quantization); }

    bool usePrediction() const
    { return static_cast<bool>(_mode & prediction); }

    template<typename C>
    void compress(C const*,
                  std::vector< std::vector<size_t> > const&,
                  typename C::ElementDataType const&,
                  typename C::ElementDataType const&);

    template<typename C>
    void decompress(C const*,
                    std::vector< std::vector<size_t> > const&,
                    typename C::ElementDataType const&,
                    typename C::ElementDataType const&);

    template<typename C>
    void predictParallelogram(C*, C const*, std::vector< std::vector<size_t> > const&);
    template<typename C>
    void unpredictParallelogram(C*, C const*, std::vector< std::vector<size_t> > const&);

    void projectAzimuth(osg::Vec2Array*, osg::Vec3Array const*);
    void unprojectAzimuth(osg::Vec3Array*, osg::Vec2Array const*);

    template<typename T>
    inline T parallelogramPrediction(std::deque<T> const& window) const
    {
      return window[0] + window[2] - window[1];
    }

    template<typename T>
    T quantizationPrecision(T const& bbl, T const& ufr)
    {
      int bits = (_bytes << 3) - 1; // need 1 bit for sign during prediction step
      float precision = (1. / ((1 << bits) - 1));
      return (ufr - bbl) * precision;
    }

    template<typename C>
    void quantizeArray(C* result,
                       C const* buffer,
                       typename C::ElementDataType const& bbl,
                       typename C::ElementDataType const& ufr)
    {
      typename C::ElementDataType h = quantizationPrecision<typename C::ElementDataType>(bbl, ufr);

      for(typename C::const_iterator it = buffer->begin() ; it != buffer->end() ; ++ it)
        result->push_back(quantize<typename C::ElementDataType>(*it, h, bbl));
    }

    template<typename C>
    void unquantizeArray(C* result,
                         C const* buffer,
                         typename C::ElementDataType const& bbl,
                         typename C::ElementDataType const& ufr)
    {
      typename C::ElementDataType h = quantizationPrecision<typename C::ElementDataType>(bbl, ufr);

      for(typename C::const_iterator it = buffer->begin() ; it != buffer->end() ; ++ it)
        result->push_back(unquantize<typename C::ElementDataType>(*it, h, bbl));
    }

    template<typename T>
    T quantize(T const&, T const&, T const&) const;

    template<typename T>
    T unquantize(T const&, T const&, T const&) const;

  private:
    int _mode;
    osg::ref_ptr<osg::Array> _data;
    osg::BoundingBoxf const _bbox; // Bounding box
    int _bytes; // quantization precision
};

#endif
