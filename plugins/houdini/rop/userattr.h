#ifndef __ROP_GtoExport___userattr_h__
#define __ROP_GtoExport___userattr_h__

#include "log.h"
#include <UT/UT_Version.h>
#include <UT/UT_Matrix4.h>
#include <GU/GU_Detail.h>
#include <Gto/Reader.h>
#include <Gto/Writer.h>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <set>

#if UT_MAJOR_VERSION_INT >= 12
# include <GA/GA_Attribute.h>
# include <GA/GA_AIFTuple.h>
# include <GA/GA_AIFStringTuple.h>
# include <GA/GA_AIFSharedStringTuple.h>

struct HAttrib
{
  GU_Detail *detail;
  GA_AttributeOwner klass;
  GA_Attribute *attrib;
  std::vector<GA_Offset> elems; // for detail attrib, add a single offset 0
};

#else
# include <GEO/GEO_AttributeOwner.h>
# include <GEO/GEO_AttributeHandle.h>
# include <GB/GB_AttributeElem.h>

struct HAttrib
{
  GU_Detail *detail;
  GEO_AttributeOwner klass;
  GEO_AttributeHandle attrib;
  std::vector<GB_AttributeElem*> elems; // empty for detail attrib
};

#endif

template <Gto::DataType GT>
struct GtoDataType2Type
{
  typedef void* T;
};

template <> struct GtoDataType2Type<Gto::Byte> { typedef unsigned char T; static const unsigned char epsilon; };
template <> struct GtoDataType2Type<Gto::Short> { typedef unsigned short T; static const unsigned short epsilon; };
template <> struct GtoDataType2Type<Gto::Int> { typedef int T; static const int epsilon; };
template <> struct GtoDataType2Type<Gto::Float> { typedef float T; static const float epsilon; };
template <> struct GtoDataType2Type<Gto::Double> { typedef double T; static const double epsilon; };

class Attrib
{
public:

  enum TransformType
  {
    TransformAsPoint = 0,
    TransformAsVector,
    TransformAsNormal,
    TransformUseInterp
  };

  template <Gto::DataType GT, int D>
  struct MappedElement
  {
    typename GtoDataType2Type<GT>::T val[D];

    MappedElement();
    MappedElement(const MappedElement<GT, D> &rhs);
    ~MappedElement();
    MappedElement<GT, D>& operator=(const MappedElement<GT, D> &rhs);
    const typename GtoDataType2Type<GT>::T& operator[](int i) const;
    typename GtoDataType2Type<GT>::T& operator[](int i);
    bool operator<(const MappedElement<GT, D> &rhs) const;
  };
  
  static size_t ByteSize(Gto::DataType t);
  
public:
  
  Attrib();
  Attrib(Gto::DataType t, int d, const std::string &interp="");
  Attrib(const Attrib &rhs);
  ~Attrib();
  
  Attrib& operator=(const Attrib &rhs);
  
  Gto::DataType getType() const;
  int getDim() const;
  int getSize() const;
  
  bool isValid() const;
  bool isEmpty() const;
  bool isMapped() const;
  
  bool init(Gto::DataType t, int d, const std::string &interp="");
#if UT_MAJOR_VERSION_INT >= 12
  bool init(const GA_Attribute *attrib);
  GA_Attribute* create(GU_Detail *gdp, GA_AttributeOwner klass, const std::string &name);
#else
  bool init(const GB_Attribute *attrib);
  GEO_AttributeHandle create(GU_Detail *gdp, GEO_AttributeOwner klass, const std::string &name);
#endif
  void clear();
  void reset();

  void swap(Attrib &rhs);

  void setType(Gto::DataType t);
  void setDim(int d);
  void resize(int sz);

  void *data() const;
  bool swapData(std::vector<unsigned char> &newData, int sz, int dim);
  
  void append(const Attrib &rhs, int i);

  std::vector<int>& mapping();
  const std::vector<int>& mapping() const;
  template <Gto::DataType GT, int D> void buildSparseMapping(const std::vector<unsigned short> &polyVertexCount);
  void buildDenseMapping(const std::vector<unsigned short> &polyVertexCount);

  void transformBy(const UT_Matrix4 &m, TransformType tt=TransformUseInterp);
  void scaleBy(float scl, TransformType tt=TransformUseInterp);

  //bool fromHoudini(Gto::Writer *w, HAttrib &src);
  bool fromHoudini(HAttrib &src);
  bool toHoudini(Gto::Reader *r, HAttrib &dst);
  
  void declareGto(Gto::Writer *w, const std::string &name);
  void outputGto(Gto::Writer *w);

  void declareGtoMapping(Gto::Writer *w, const std::string &name);
  void outputGtoMapping(Gto::Writer *w);
  
  inline std::string info() const
  {
    std::ostringstream oss;
    oss << "type=" << mType
        << ", dim=" << mDim
        << ", size=" << mSize
        << ", interp=" << mInterp
        << ", dataSize=" << mData.size()
        << ", mapped? " << (isMapped() ? "true" : "false");
    return oss.str();
  }
  
private:
  
  Gto::DataType mType;
  int mDim;
  int mSize;
  std::vector<unsigned char> mData;
  std::vector<std::string> mStrings;
  std::string mInterp;
  std::vector<int> mMapping;
};

#if UT_MAJOR_VERSION_INT >= 12
typedef std::pair<GA_Attribute*, Attrib> AttribDictEntry;
#else
typedef std::pair<GEO_AttributeHandle, Attrib> AttribDictEntry;
#endif

namespace std
{
  template<>
  inline void swap<Attrib>(Attrib &lhs, Attrib &rhs)
  {
    lhs.swap(rhs);
  }
  /*
  template<> void swap<AttribDictEntry>(AttribDictEntry &lhs, AttribDictEntry &rhs)
  {
    AttribDictEntry::first_type tmp = lhs.first;
    lhs.first = rhs.first;
    rhs.first = tmp;
    
    using std::swap;
    
    swap(lhs.second, rhs.second);
  }
  */
}

typedef std::map<std::string, AttribDictEntry> BaseAttribDict;

class AttribDict : public BaseAttribDict
{
public:

  AttribDict();
  AttribDict(const AttribDict &);
  virtual ~AttribDict();
  
  AttribDict& operator=(const AttribDict &);

  void clearIgnores();
  void addIgnore(const std::string &n);
  // add to ignore list, all names that appears in ad
  // also add ad ignores
  void addIgnores(const AttribDict &ad, bool addSourceIgnores=true);
  void removeIgnore(const std::string &n);
  bool ignore(const std::string &n) const;

#if UT_MAJOR_VERSION_INT >= 12
  
  // elems are vertices/points/primitives/details offsets
  void collect(GU_Detail *gdp, GA_AttributeOwner klass, const std::vector<GA_Offset> &elems, bool readData=true);
  void collectPoints(GU_Detail *gdp, const std::vector<GA_Offset> &elems, bool elemsAsPrimitives=true, bool readData=true);
  void collectVertices(GU_Detail *gdp, const std::vector<GA_Offset> &prims, bool readData=true);
  void collectPrimitives(GU_Detail *gdp, const std::vector<GA_Offset> &prims, bool readData=true);

  void create(GU_Detail *gdp, GA_AttributeOwner klass, const std::vector<GA_Offset> &elems);

#else
  
  // Beware: (offset, count) are for primitives if klass is GEO_VERTEX_DICT
  void collect(GU_Detail *gdp, GEO_AttributeOwner klass, unsigned int offset, unsigned int count, bool readData=true);
  void collect(GU_Detail *gdp, GEO_AttributeOwner klass, const std::vector<GB_AttributeElem*> &elems, bool readData=true);
  void collectPoints(GU_Detail *gdp, const std::vector<unsigned int> &elems, bool elemsAsPrimitives=true, bool readData=true);
  void collectVertices(GU_Detail *gdp, const std::vector<unsigned int> &prims, bool readData=true);
  void collectPrimitives(GU_Detail *gdp, const std::vector<unsigned int> &prims, bool readData=true);
  
  void create(GU_Detail *gdp, GEO_AttributeOwner klass, unsigned int offset, unsigned int count);

#endif

  void collectGlobals(GU_Detail *gdp, bool readData=true);
  

  void transformBy(const UT_Matrix4 &m, Attrib::TransformType tt=Attrib::TransformUseInterp);
  void scaleBy(float scl, Attrib::TransformType tt=Attrib::TransformUseInterp);

  void declareGto(Gto::Writer *w);
  void outputGto(Gto::Writer *w);

  void declareGtoMapping(Gto::Writer *w, const std::string &prefix="", const std::string &suffix="");
  void outputGtoMapping(Gto::Writer *w);

  bool fromHoudini(const char *name=0);
  void toHoudini(Gto::Reader *r);

private:
  
  HAttrib mHA;
  std::set<std::string> mIgnores;
};

// ---

template <Gto::DataType GT, int D>
Attrib::MappedElement<GT, D>::MappedElement()
{
}

template <Gto::DataType GT, int D>
Attrib::MappedElement<GT, D>::MappedElement(const MappedElement<GT, D> &rhs)
{
  for (int i=0; i<D; ++i) val[i] = rhs.val[i];
}

template <Gto::DataType GT, int D>
Attrib::MappedElement<GT, D>::~MappedElement()
{
}

template <Gto::DataType GT, int D>
Attrib::MappedElement<GT, D>& Attrib::MappedElement<GT, D>::operator=(const MappedElement<GT, D> &rhs)
{
  for (int i=0; i<D; ++i) val[i] = rhs.val[i];
  return *this;
}

template <Gto::DataType GT, int D>
const typename GtoDataType2Type<GT>::T& Attrib::MappedElement<GT, D>::operator[](int i) const
{
  return val[i];
}

template <Gto::DataType GT, int D>
typename GtoDataType2Type<GT>::T& Attrib::MappedElement<GT, D>::operator[](int i)
{
  return val[i];
}

template <Gto::DataType GT, int D>
bool Attrib::MappedElement<GT, D>::operator<(const Attrib::MappedElement<GT, D> &rhs) const
{
  typename GtoDataType2Type<GT>::T tmp;
  const typename GtoDataType2Type<GT>::T eps = GtoDataType2Type<GT>::epsilon;

  for (int i=0; i<D; ++i)
  {
    tmp = val[i] - rhs.val[i];
    if (-eps <= tmp && tmp <= eps)
    {
      continue;
    }
    else if (val[i] < rhs.val[i])
    {
      return true;
    }
    else
    {
      return false;
    }
  }
  return false;
}

// ---

inline size_t Attrib::ByteSize(Gto::DataType t)
{
  switch (t)
  {
  case Gto::Byte:
    return sizeof(unsigned char);
  case Gto::Short:
    return sizeof(unsigned short);
  case Gto::Int:
    return sizeof(int);
  case Gto::Float:
    return sizeof(float);
  case Gto::Double:
    return sizeof(double);
  case Gto::String:
    return sizeof(int);
  default:
    return size_t(-1);
  }
}

inline Gto::DataType Attrib::getType() const
{
  return mType;
}

inline int Attrib::getDim() const
{
  return mDim;
}

inline int Attrib::getSize() const
{
  return mSize;
}

inline bool Attrib::isValid() const
{
  return (mType != Gto::ErrorType && mDim > 0);
}

inline bool Attrib::isEmpty() const
{
  return (mSize == 0);
}

inline bool Attrib::isMapped() const
{
  return (mMapping.size() > 0);
}

inline std::vector<int>& Attrib::mapping()
{
  return mMapping;
}

inline const std::vector<int>& Attrib::mapping() const
{
  return mMapping;
}

inline void* Attrib::data() const
{
  return (isEmpty() ? 0 : (void*) &(mData[0]));
}

template <Gto::DataType GT, int D>
void Attrib::buildSparseMapping(const std::vector<unsigned short> &polyVertexCount)
{
  if (!isValid() || isEmpty() || GT == Gto::String)
  {
    return;
  }

  std::map<MappedElement<GT, D>, int> indexMap;
  typename std::map<MappedElement<GT, D>, int>::iterator iit;

  std::vector<unsigned char> newValues;

  size_t elemSize = D * ByteSize(GT);
  newValues.reserve(mSize * elemSize);

  MappedElement<GT, D> tmp;
  
  int idx;
  int nvalues = 0;
  
  mMapping.resize(mSize);

  typename GtoDataType2Type<GT>::T *src = (typename GtoDataType2Type<GT>::T *) data();
  //typename GtoDataType2Type<GT>::T *dst = (typename GtoDataType2Type<GT>::T *) &(newValues[0]);
  typename GtoDataType2Type<GT>::T *dst;
  
  // NOTE: don't forget houdini's CW winding

  int offset = 0;
  int j = 0;
  int k = 0;
  for (size_t i=0; i<polyVertexCount.size(); ++i)
  {
    int nv = int(polyVertexCount[i]);

    for (int v=0; v<nv; ++v, j+=D)
    {
      for (int d=0; d<D; ++d) tmp[d] = src[j+d];

      iit = indexMap.find(tmp);

      if (iit == indexMap.end())
      {
        newValues.resize((nvalues + 1) * elemSize);
        // resize should not trigger a reallocation because we have reserved enough space
        // reset though dst pointer just in case (some allocation scheme might still want to re-allocate, who knows...)
        dst = (typename GtoDataType2Type<GT>::T *) &(newValues[0]);
        for (int d=0; d<D; ++d) dst[k+d] = tmp[d];
        idx = nvalues++;
        k += D;
        indexMap[tmp] = idx;
      }
      else
      {
        idx = iit->second;
      }

      // CCW winding
      mMapping[offset+nv-1-v] = idx;
    }

    offset += nv;
  }

  // swap data
  mSize = nvalues;
  std::swap(mData, newValues);
}

// ---

inline void AttribDict::transformBy(const UT_Matrix4 &m, Attrib::TransformType tt)
{
  for (iterator it=begin(); it!=end(); ++it)
  {
    it->second.second.transformBy(m, tt);
  }
}

inline void AttribDict::scaleBy(float scl, Attrib::TransformType tt)
{
  for (iterator it=begin(); it!=end(); ++it)
  {
    it->second.second.scaleBy(scl, tt);
  }
}

#endif
