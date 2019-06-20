#include "userattr.h"
#include "log.h"
#include <UT/UT_Vector3.h>
#include <UT/UT_Vector4.h>

const unsigned char GtoDataType2Type<Gto::Byte>::epsilon = 0;
const unsigned short GtoDataType2Type<Gto::Short>::epsilon = 0;
const int GtoDataType2Type<Gto::Int>::epsilon = 0;
const float GtoDataType2Type<Gto::Float>::epsilon = 0.0f;
const double GtoDataType2Type<Gto::Double>::epsilon = 0.0;

template <typename ST, typename DT>
void Convert(int d, int sz, const std::vector<unsigned char> &src, std::vector<unsigned char> &dst)
{
  const ST *sptr = (const ST*) &(src[0]);
  
  dst.resize(sz * d * sizeof(DT));
  DT *dptr = (DT*) &(dst[0]);
  
  for (int i=0; i<sz; ++i)
  {
    for (int j=0; j<d; ++j, ++sptr, ++dptr)
    {
      *dptr = (DT) *sptr;
    }
  }
}

// ---

Attrib::Attrib()
  : mType(Gto::ErrorType)
  , mDim(0)
  , mSize(0)
{
}

Attrib::Attrib(Gto::DataType t, int d, const std::string &interp)
  : mType(Gto::ErrorType)
  , mDim(0)
  , mSize(0)
{
  init(t, d, interp);
}

Attrib::Attrib(const Attrib &rhs)
  : mType(rhs.mType)
  , mDim(rhs.mDim)
  , mSize(rhs.mSize)
  , mData(rhs.mData)
  , mStrings(rhs.mStrings)
  , mInterp(rhs.mInterp)
  , mMapping(rhs.mMapping)
{
}

Attrib::~Attrib()
{
  clear();
}

Attrib& Attrib::operator=(const Attrib &rhs)
{
  if (this != &rhs)
  {
    mType = rhs.mType;
    mDim = rhs.mDim;
    mSize = rhs.mSize;
    mData = rhs.mData;
    mStrings = rhs.mStrings;
    mMapping = rhs.mMapping;
  }
  return *this;
}

bool Attrib::init(Gto::DataType t, int d, const std::string &interp)
{
  switch (t)
  {
  case Gto::Byte:
  case Gto::Short:
  case Gto::Int:
  case Gto::Float:
  case Gto::Double:
  case Gto::String:
    break;
  default:
    // Gto::Half, Gto::Boolean
    return false;
  }
  mType = t;
  mDim = d;
  mSize = 0;
  mInterp = interp;
  mData.clear();
  mStrings.clear();
  mMapping.clear();
  return true;
}

#if UT_MAJOR_VERSION_INT >= 12

bool Attrib::init(const GA_Attribute *attrib)
{
  Gto::DataType type = Gto::ErrorType;
  int dim = attrib->getTupleSize();
  std::string interp = "";

  switch (attrib->getStorageClass())
  {
  case GA_STORECLASS_INT:
    type = Gto::Int;
    break;
  case GA_STORECLASS_FLOAT:
    type = Gto::Float;
    break;
  case GA_STORECLASS_STRING:
    type = Gto::String;
    break;
  default:
    break;
  }
  
  switch (attrib->getTypeInfo())
  {
  case GA_TYPE_POINT:
  case GA_TYPE_HPOINT:
    interp = "point";
    break;
  case GA_TYPE_VECTOR:
  case GA_TYPE_NORMAL:
    interp = "vector";
    break;
  case GA_TYPE_COLOR:
    interp = "rgb";
    break;
  default:
    break;
  }

  return init(type, dim, interp);
}

GA_Attribute* Attrib::create(GU_Detail *gdp, GA_AttributeOwner klass, const std::string &name)
{
  if (!isValid())
  {
    return NULL;
  }

  GA_RWAttributeRef ref;

  switch (mType)
  {
  case Gto::Byte:
  case Gto::Short:
  case Gto::Int:
    ref = gdp->addIntTuple(klass, GA_SCOPE_PUBLIC, name.c_str(), mDim);
    break;
  case Gto::Float:
  case Gto::Double:
    ref = gdp->addFloatTuple(klass, GA_SCOPE_PUBLIC, name.c_str(), mDim);
    break;
  case Gto::String:
    ref = gdp->addStringTuple(klass, GA_SCOPE_PUBLIC, name.c_str(), mDim);
    break;
  default:
    break;
  }

  if (ref.isValid())
  {
    GA_Attribute *attrib = ref.getAttribute();
    if (mInterp == "point" || mInterp == "hpoint")
    {
      attrib->setTypeInfo(mDim == 4 ? GA_TYPE_HPOINT : GA_TYPE_POINT);
    }
    else if (mInterp == "vector")
    {
      attrib->setTypeInfo(GA_TYPE_VECTOR);
    }
    else if (mInterp == "rgb")
    {
      attrib->setTypeInfo(GA_TYPE_COLOR);
    }
    return attrib;
  }
  else
  {
    return NULL;
  }
}

#else

bool Attrib::init(const GB_Attribute *attrib)
{
  Gto::DataType type = Gto::ErrorType;
  int dim = 0;
  std::string interp = "";

  switch (attrib->getType())
  {
  case GB_ATTRIB_INT:
    type = Gto::Int;
    dim = attrib->getSize() / sizeof(int);
    break;
  case GB_ATTRIB_FLOAT:
    type = Gto::Float;
    dim = attrib->getSize() / sizeof(float);
    break;
  case GB_ATTRIB_VECTOR:
    type = Gto::Float;
    dim = 3;
    interp = "vector";
    break;
  case GB_ATTRIB_INDEX:
    type = Gto::String;
    dim = attrib->getSize() / sizeof(int);
    break;
  default:
    break;
  }
  
  return init(type, dim, interp);
}

GEO_AttributeHandle Attrib::create(GU_Detail *gdp, GEO_AttributeOwner klass, const std::string &name)
{
  if (!isValid())
  {
    return GEO_AttributeHandle();
  }

  GB_AttributeRef ref;

  switch (mType)
  {
  case Gto::Byte:
  case Gto::Short:
  case Gto::Int:
    ref = gdp->addAttribute(name.c_str(), mDim*sizeof(int), GB_ATTRIB_INT, NULL, klass);
    break;
  case Gto::Float:
  case Gto::Double:
    if (mInterp == "vector" && mDim == 3)
    {
      ref = gdp->addAttribute(name.c_str(), mDim*sizeof(float), GB_ATTRIB_VECTOR, NULL, klass);
    }
    else
    {
      ref = gdp->addAttribute(name.c_str(), mDim*sizeof(float), GB_ATTRIB_FLOAT, NULL, klass);
    }
    break;
  case Gto::String:
    ref = gdp->addAttribute(name.c_str(), mDim*sizeof(int), GB_ATTRIB_INDEX, NULL, klass);
  default:
    break;
  }

  return (ref.isValid() ? gdp->getAttribute(klass, name.c_str()) : GEO_AttributeHandle());
}

#endif

void Attrib::clear()
{
  mData.clear();
  mStrings.clear();
  mMapping.clear();
  mSize = 0;
}

void Attrib::reset()
{
  mData.clear();
  mStrings.clear();
  mMapping.clear();
  mSize = 0;
  mDim = 0;
  mInterp = "";
  mType = Gto::ErrorType;
}

void Attrib::swap(Attrib &rhs)
{
  using std::swap;
  
#ifdef _DEBUG
  Log::Print("Swap attribtues");
#endif
  swap(mType, rhs.mType);
  swap(mDim, rhs.mDim);
  swap(mSize, rhs.mSize);
  swap(mData, rhs.mData);
  swap(mStrings, rhs.mStrings);
  swap(mInterp, rhs.mInterp);
  swap(mMapping, rhs.mMapping);
}

void Attrib::setType(Gto::DataType t)
{
  if (!isValid())
  {
    mType = t;
    return;
  }
  
  std::vector<unsigned char> newData;
  
  switch (mType)
  {
  case Gto::Byte:
    switch (t)
    {
    case Gto::Short:
      Convert<unsigned char, unsigned short>(mDim, mSize, mData, newData);
      break;
    case Gto::Int:
      Convert<unsigned char, int>(mDim, mSize, mData, newData);
      break;
    case Gto::Float:
      Convert<unsigned char, float>(mDim, mSize, mData, newData);
      break;
    case Gto::Double:
      Convert<unsigned char, double>(mDim, mSize, mData, newData);
      break;
    default:
      break;
    }
    break;
  case Gto::Short:
    switch (t)
    {
    case Gto::Byte:
      Convert<unsigned short, unsigned char>(mDim, mSize, mData, newData);
      break;
    case Gto::Int:
      Convert<unsigned short, int>(mDim, mSize, mData, newData);
      break;
    case Gto::Float:
      Convert<unsigned short, float>(mDim, mSize, mData, newData);
      break;
    case Gto::Double:
      Convert<unsigned short, double>(mDim, mSize, mData, newData);
      break;
    default:
      break;
    }
    break;
  case Gto::Int:
    switch (t)
    {
    case Gto::Byte:
      Convert<int, unsigned char>(mDim, mSize, mData, newData);
      break;
    case Gto::Short:
      Convert<int, unsigned short>(mDim, mSize, mData, newData);
      break;
    case Gto::Float:
      Convert<int, float>(mDim, mSize, mData, newData);
      break;
    case Gto::Double:
      Convert<int, double>(mDim, mSize, mData, newData);
      break;
    default:
      break;
    }
    break;
  case Gto::Float:
    switch (t)
    {
    case Gto::Byte:
      Convert<float, unsigned char>(mDim, mSize, mData, newData);
      break;
    case Gto::Short:
      Convert<float, unsigned short>(mDim, mSize, mData, newData);
      break;
    case Gto::Int:
      Convert<float, int>(mDim, mSize, mData, newData);
      break;
    case Gto::Double:
      Convert<float, double>(mDim, mSize, mData, newData);
      break;
    default:
      break;
    }
    break;
  case Gto::Double:
    switch (t)
    {
    case Gto::Byte:
      Convert<double, unsigned char>(mDim, mSize, mData, newData);
      break;
    case Gto::Short:
      Convert<double, unsigned short>(mDim, mSize, mData, newData);
      break;
    case Gto::Int:
      Convert<double, int>(mDim, mSize, mData, newData);
      break;
    case Gto::Float:
      Convert<double, float>(mDim, mSize, mData, newData);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
  
  if (newData.size() > 0)
  {
    std::swap(mData, newData);
    mType = t;
  }
  else if (mSize == 0)
  {
    // originally empty, just change type
    mType = t;
  }
}

void Attrib::append(const Attrib &rhs, int i)
{
  if (mDim == rhs.getDim() && mType == rhs.getType())
  {
    int j = i;
    if (rhs.isMapped())
    {
      j = rhs.mMapping[i];
    }

    size_t bSize = mDim * ByteSize(mType);

    mData.resize((mSize + 1) * bSize);

    // should check rhs pointer
    unsigned char *dst = ((unsigned char*) data()) + (mSize * bSize);
    unsigned char *src = ((unsigned char*) rhs.data()) + (j * bSize);
    for (size_t k=0; k<bSize; ++k)
    {
      *dst++ = *src++;
    }

    if (mType == Gto::String)
    {
      mStrings.push_back(rhs.mStrings[j]);
    }

    if (isMapped())
    {
      mMapping.push_back(mSize);  
    }

    ++mSize;
  }
}

void Attrib::setDim(int d)
{
  if (!isValid())
  {
    mDim = d;
    return;
  }
  
  if (d <= 0 || d == mDim)
  {
    return;
  }
  
  if (mSize > 0)
  {
    size_t bs = ByteSize(mType);
    size_t sstride = mDim * bs;
    size_t dstride = d * bs;
    
    std::vector<unsigned char> newData(mSize * dstride);
    
    unsigned char *src = (unsigned char*) &mData[0];
    unsigned char *dst = (unsigned char*) &newData[0];
    
    if (d > mDim)
    {
      size_t zerolen = dstride - sstride;
      for (int i=0; i<mSize; ++i, src+=sstride, dst+=dstride)
      {
        memcpy(dst, src, sstride);
        memset(dst+sstride, 0, zerolen);
      }
    }
    else
    {
      for (int i=0; i<mSize; ++i, src+=sstride, dst+=dstride)
      {
        memcpy(dst, src, dstride);
      }
    }
    
    std::swap(mData, newData);
    
    if (mType == Gto::String)
    {
      std::vector<std::string> newStrings(mSize * d);
      
      if (d > mDim)
      {
        // fill with empty strings
        for (int i=0, ss=0, ds=0; i<mSize; ++i, ss+=mDim, ds+=d)
        {
          for (int j=0; j<mDim; ++j)
          {
            newStrings[ds+j] = mStrings[ss+j];
          }
          for (int j=mDim; j<d; ++j)
          {
            newStrings[ds+j] = "";
          }
        }
      }
      else
      {
        for (int i=0, ss=0, ds=0; i<mSize; ++i, ss+=mDim, ds+=d)
        {
          for (int j=0; j<d; ++j)
          {
            newStrings[ds+j] = mStrings[ss+j];
          }
        }
      }
      
      std::swap(mStrings, newStrings);
    }
  }
  // would this work if done before reading?
  mDim = d;
}

void Attrib::resize(int sz)
{
  if (!isValid())
  {
    return;
  }
  if (sz <= 0)
  {
    clear();
    mSize = 0;
  }
  else
  {
    size_t bs = sz * mDim * ByteSize(mType);
    mData.resize(bs);
    if (mType == Gto::String)
    {
      mStrings.resize(sz * mDim);
    }
    mSize = sz;
    mMapping.clear();
  }
}

bool Attrib::swapData(std::vector<unsigned char> &newData, int sz, int dim)
{
  if (!isValid() || mType == Gto::String)
  {
    return false;
  }
  if (sz * dim * ByteSize(mType) != newData.size())
  {
    return false;
  }
  std::swap(mData, newData);
  mSize = sz;
  mDim = dim;
  mMapping.clear();
  return true;
}

void Attrib::buildDenseMapping(const std::vector<unsigned short> &polyVertexCount)
{
  mMapping.resize(mSize);
  int offset = 0;
  for (size_t i=0; i<polyVertexCount.size(); ++i)
  {
    int nv = int(polyVertexCount[i]);
    for (int j=0, k=offset; j<nv; ++j, ++k)
    {
      // CW -> CCW
      mMapping[k] = offset + nv - 1 - j;
    }
    offset += int(nv);
  }
}

void Attrib::scaleBy(float scl, Attrib::TransformType tt)
{
  if (!isValid() || isEmpty())
  {
    return;
  }
  if (mDim < 3 || mDim > 4)
  {
    return;
  }

  bool doScale = false;

  if (tt == TransformUseInterp)
  {
    doScale = (mInterp == "point" || mInterp == "hpoint" || mInterp == "vector");
  }
  else if (tt == TransformAsPoint || TransformAsVector)
  {
    doScale = true;
  }

  if (doScale)
  {
    if (mType == Gto::Float)
    {
      float *ptr = (float*) data();
      for (int i=0, j=0; i<mSize; ++i, j+=mDim)
      {
        ptr[j] *= scl;
        ptr[j+1] *= scl;
        ptr[j+2] *= scl;
      }
    }
    else if (mType == Gto::Double)
    {
      double *ptr = (double*) data();
      for (int i=0, j=0; i<mSize; ++i, j+=mDim)
      {
        ptr[j] *= scl;
        ptr[j+1] *= scl;
        ptr[j+2] *= scl;
      }
    }
  }
}

void Attrib::transformBy(const UT_Matrix4 &m, Attrib::TransformType tt)
{
  if (!isValid() || isEmpty())
  {
    return;
  }

  if (mDim < 3 || mDim > 4)
  {
    return;
  }

  bool asPoint = false;
  bool normalize = false;

  if (tt == TransformUseInterp)
  {
    asPoint = (mInterp == "point" || mInterp == "hpoint");
    normalize = (mInterp == "normal");
  }
  else if (tt == TransformAsPoint)
  {
    asPoint = true;
    normalize = false;
  }
  else if (tt == TransformAsVector)
  {
    asPoint = false;
    normalize = false;
  }
  else if (tt == TransformAsNormal)
  {
    asPoint = false;
    normalize = true;
  }

  UT_Vector4 v;

  if (mType == Gto::Float)
  {
    float *ptr = (float*) data();
    
    if (mDim == 3)
    {
      if (asPoint)
      {
        for (int i=0, j=0; i<mSize; ++i, j+=3)
        {
          v.assign(ptr[j], ptr[j+1], ptr[j+2], 1.0f);
          v = v * m;
          ptr[j] = v.x();
          ptr[j+1] = v.y();
          ptr[j+2] = v.z();
        }
      }
      else
      {
        for (int i=0, j=0; i<mSize; ++i, j+=3)
        {
          v.assign(ptr[j], ptr[j+1], ptr[j+2], 0.0f);
          v.rowVecMult3(m);
          if (normalize) v.normalize();
          ptr[j] = v.x();
          ptr[j+1] = v.y();
          ptr[j+2] = v.z();
        }
      }
    }
    else
    {
      for (int i=0, j=0; i<mSize; ++i, j+=4)
      {
        v.assign(ptr[j], ptr[j+1], ptr[j+2], ptr[j+3]);
        v = v * m;
        if (normalize) v.normalize();
        ptr[j] = v.x();
        ptr[j+1] = v.y();
        ptr[j+2] = v.z();
        ptr[j+3] = v.w();
      }
    }
  }
  else if (mType == Gto::Double)
  {
    double *ptr = (double*) data();

    if (mDim == 3)
    {
      if (asPoint)
      {
        for (int i=0, j=0; i<mSize; ++i, j+=3)
        {
          v.assign(ptr[j], ptr[j+1], ptr[j+2], 1.0f);
          v = v * m;
          ptr[j] = v.x();
          ptr[j+1] = v.y();
          ptr[j+2] = v.z();
        }
      }
      else
      {
        for (int i=0, j=0; i<mSize; ++i, j+=3)
        {
          v.assign(ptr[j], ptr[j+1], ptr[j+2], 0.0f);
          v.rowVecMult3(m);
          if (normalize) v.normalize();
          ptr[j] = v.x();
          ptr[j+1] = v.y();
          ptr[j+2] = v.z();
        }
      }
    }
    else
    {
      for (int i=0, j=0; i<mSize; ++i, j+=4)
      {
        v.assign(ptr[j], ptr[j+1], ptr[j+2], ptr[j+3]);
        v = v * m;
        if (normalize) v.normalize();
        ptr[j] = v.x();
        ptr[j+1] = v.y();
        ptr[j+2] = v.z();
        ptr[j+3] = v.w();
      }
    }
  }
}

//bool Attrib::fromHoudini(Gto::Writer *w, HAttrib &src)
bool Attrib::fromHoudini(HAttrib &src)
{
#if UT_MAJOR_VERSION_INT >= 12
  
  if (!src.attrib)
  {
    return false;
  }
  // want at least mDim element in tuple, if there's more, ignore them
  if (src.attrib->getTupleSize() < mDim)
  {
    return false;
  }
  switch (mType)
  {
  case Gto::Byte:
    {
      if (src.attrib->getStorageClass() != GA_STORECLASS_INT)
      {
        return false;
      }
      const GA_AIFTuple *tpl = src.attrib->getAIFTuple();
      if (!tpl)
      {
        return false;
      }
      resize(src.elems.size());
      unsigned char *ptr = (unsigned char*) data();
      int32 tmp;
      for (size_t i=0; i<src.elems.size(); ++i)
      {
        for (int j=0; j<mDim; ++j, ++ptr)
        {
          tpl->get(src.attrib, src.elems[i], tmp, j);
          *ptr = (unsigned char) tmp;
        }
      }
    }
    break;
  case Gto::Short:
    {
      if (src.attrib->getStorageClass() != GA_STORECLASS_INT)
      {
        return false;
      }
      const GA_AIFTuple *tpl = src.attrib->getAIFTuple();
      if (!tpl)
      {
        return false;
      }
      resize(src.elems.size());
      unsigned short *ptr = (unsigned short*) data();
      int32 tmp;
      for (size_t i=0; i<src.elems.size(); ++i)
      {
        for (int j=0; j<mDim; ++j, ++ptr)
        {
          tpl->get(src.attrib, src.elems[i], tmp, j);
          *ptr = (unsigned short) tmp;
        }
      }
    }
    break;
  case Gto::Int:
    {
      if (src.attrib->getStorageClass() != GA_STORECLASS_INT)
      {
        return false;
      }
      const GA_AIFTuple *tpl = src.attrib->getAIFTuple();
      if (!tpl)
      {
        return false;
      }
      resize(src.elems.size());
      int *ptr = (int*) data();
      int32 tmp;
      for (size_t i=0; i<src.elems.size(); ++i)
      {
        for (int j=0; j<mDim; ++j, ++ptr)
        {
          tpl->get(src.attrib, src.elems[i], tmp, j);
          *ptr = (int) tmp;
        }
      }
    }
    break;
  case Gto::Float:
    {
      if (src.attrib->getStorageClass() != GA_STORECLASS_FLOAT)
      {
        return false;
      }
      const GA_AIFTuple *tpl = src.attrib->getAIFTuple();
      if (!tpl)
      {
        return false;
      }
      resize(src.elems.size());
      float *ptr = (float*) data();
      fpreal32 tmp;
      for (size_t i=0; i<src.elems.size(); ++i)
      {
        for (int j=0; j<mDim; ++j, ++ptr)
        {
          tpl->get(src.attrib, src.elems[i], tmp, j);
          *ptr = (float) tmp;
        }
      }
    }
    break;
  case Gto::Double:
    {
      if (src.attrib->getStorageClass() != GA_STORECLASS_FLOAT)
      {
        return false;
      }
      const GA_AIFTuple *tpl = src.attrib->getAIFTuple();
      if (!tpl)
      {
        return false;
      }
      resize(src.elems.size());
      double *ptr = (double*) data();
      fpreal64 tmp;
      for (size_t i=0; i<src.elems.size(); ++i)
      {
        for (int j=0; j<mDim; ++j, ++ptr)
        {
          tpl->get(src.attrib, src.elems[i], tmp, j);
          *ptr = (double) tmp;
        }
      }
    }
    break;
  case Gto::String:
    {
      if (src.attrib->getStorageClass() != GA_STORECLASS_STRING)
      {
        return false;
      }
      const GA_AIFStringTuple *tpl = src.attrib->getAIFSharedStringTuple();
      if (!tpl)
      {
        tpl = src.attrib->getAIFStringTuple();
        if (!tpl)
        {
          return false;
        }
      }
      resize(src.elems.size());
      int k=0;
      const char *tmp; 
      for (size_t i=0; i<src.elems.size(); ++i)
      {
        for (int j=0; j<mDim; ++j, ++k)
        {
          tmp = tpl->getString(src.attrib, src.elems[i], j);
          if (tmp)
          {
            mStrings[k] = tmp;
          }
          else
          {
            mStrings[k] = "";
          }
        }
      }
    }
    break;
  default:
    return false;
  }
  
#else
  
  if (!src.attrib.isAttributeValid())
  {
    return false;
  }
  
  const GB_Attribute *attrib = src.attrib.getAttribute();
  
  int dim = 0;
  
  switch (mType)
  {
  case Gto::Byte:
    {
      if (attrib->getType() != GB_ATTRIB_INT)
      {
        return false;
      }
      dim = attrib->getSize() / sizeof(int);
      if (dim < mDim)
      {
        return false;
      }
      resize(src.klass == GEO_DETAIL_DICT ? 1 : src.elems.size());
      unsigned char *ptr = (unsigned char*) data();
      switch (src.klass)
      {
      case GEO_VERTEX_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++ptr)
          {
            src.attrib.setElement((GEO_Vertex*)src.elems[i]);
            *ptr = (unsigned char) src.attrib.getI(j);
          }
        }
        break;
      case GEO_POINT_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++ptr)
          {
            src.attrib.setElement((GEO_Point*)src.elems[i]);
            *ptr = (unsigned char) src.attrib.getI(j);
          }
        }
        break;
      case GEO_PRIMITIVE_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++ptr)
          {
            src.attrib.setElement((GEO_Primitive*)src.elems[i]);
            *ptr = (unsigned char) src.attrib.getI(j);
          }
        }
        break;
      case GEO_DETAIL_DICT:
        src.attrib.setElement(src.detail);
        for (int j=0; j<mDim; ++j, ++ptr)
        {
          *ptr = (unsigned char) src.attrib.getI(j);
        }
        break;
      default:
        break;
      }
    }
    break;
  case Gto::Short:
    {
      if (attrib->getType() != GB_ATTRIB_INT)
      {
        return false;
      }
      dim = attrib->getSize() / sizeof(int);
      if (dim < mDim)
      {
        return false;
      }
      resize(src.klass == GEO_DETAIL_DICT ? 1 : src.elems.size());
      unsigned short *ptr = (unsigned short*) data();
      switch (src.klass)
      {
      case GEO_VERTEX_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++ptr)
          {
            src.attrib.setElement((GEO_Vertex*)src.elems[i]);
            *ptr = (unsigned short) src.attrib.getI(j);
          }
        }
        break;
      case GEO_POINT_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++ptr)
          {
            src.attrib.setElement((GEO_Point*)src.elems[i]);
            *ptr = (unsigned short) src.attrib.getI(j);
          }
        }
        break;
      case GEO_PRIMITIVE_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++ptr)
          {
            src.attrib.setElement((GEO_Primitive*)src.elems[i]);
            *ptr = (unsigned short) src.attrib.getI(j);
          }
        }
        break;
      case GEO_DETAIL_DICT:
        src.attrib.setElement(src.detail);
        for (int j=0; j<mDim; ++j, ++ptr)
        {
          *ptr = (unsigned short) src.attrib.getI(j);
        }
        break;
      default:
        break;
      }
    }
    break;
  case Gto::Int:
    {
      if (attrib->getType() != GB_ATTRIB_INT)
      {
        return false;
      }
      dim = attrib->getSize() / sizeof(int);
      if (dim < mDim)
      {
        return false;
      }
      resize(src.klass == GEO_DETAIL_DICT ? 1 : src.elems.size());
      int *ptr = (int*) data();
      switch (src.klass)
      {
      case GEO_VERTEX_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++ptr)
          {
            src.attrib.setElement((GEO_Vertex*)src.elems[i]);
            *ptr = src.attrib.getI(j);
          }
        }
        break;
      case GEO_POINT_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++ptr)
          {
            src.attrib.setElement((GEO_Point*)src.elems[i]);
            *ptr = src.attrib.getI(j);
          }
        }
        break;
      case GEO_PRIMITIVE_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++ptr)
          {
            src.attrib.setElement((GEO_Primitive*)src.elems[i]);
            *ptr = src.attrib.getI(j);
          }
        }
        break;
      case GEO_DETAIL_DICT:
        src.attrib.setElement(src.detail);
        for (int j=0; j<mDim; ++j, ++ptr)
        {
          *ptr = src.attrib.getI(j);
        }
        break;
      default:
        break;
      }
    }
    break;
  case Gto::Float:
    {
      if (attrib->getType() == GB_ATTRIB_FLOAT)
      {
        dim = attrib->getSize() / sizeof(float);
        if (dim < mDim)
        {
          return false;
        }
      }
      else if (attrib->getType() == GB_ATTRIB_VECTOR)
      {
        // dim = 3;
        if (3 != mDim)
        {
          return false;
        }
      }
      else
      {
        return false;
      }
      resize(src.klass == GEO_DETAIL_DICT ? 1 : src.elems.size());
      float *ptr = (float*) data();
      switch (src.klass)
      {
      case GEO_VERTEX_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++ptr)
          {
            src.attrib.setElement((GEO_Vertex*)src.elems[i]);
            *ptr = src.attrib.getF(j);
          }
        }
        break;
      case GEO_POINT_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++ptr)
          {
            src.attrib.setElement((GEO_Point*)src.elems[i]);
            *ptr = src.attrib.getF(j);
          }
        }
        break;
      case GEO_PRIMITIVE_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++ptr)
          {
            src.attrib.setElement((GEO_Primitive*)src.elems[i]);
            *ptr = src.attrib.getF(j);
          }
        }
        break;
      case GEO_DETAIL_DICT:
        src.attrib.setElement(src.detail);
        for (int j=0; j<mDim; ++j, ++ptr)
        {
          *ptr = src.attrib.getF(j);
        }
        break;
      default:
        break;
      }
    }
    break;
  case Gto::Double:
    {
      if (attrib->getType() == GB_ATTRIB_FLOAT)
      {
        dim = attrib->getSize() / sizeof(float);
        if (dim < mDim)
        {
          return false;
        }
      }
      else if (attrib->getType() == GB_ATTRIB_VECTOR)
      {
        // dim = 3;
        if (3 != mDim)
        {
          return false;
        }
      }
      else
      {
        return false;
      }
      resize(src.klass == GEO_DETAIL_DICT ? 1 : src.elems.size());
      double *ptr = (double*) data();
      switch (src.klass)
      {
      case GEO_VERTEX_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++ptr)
          {
            src.attrib.setElement((GEO_Vertex*)src.elems[i]);
            *ptr = (double) src.attrib.getF(j);
          }
        }
        break;
      case GEO_POINT_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++ptr)
          {
            src.attrib.setElement((GEO_Point*)src.elems[i]);
            *ptr = (double) src.attrib.getF(j);
          }
        }
        break;
      case GEO_PRIMITIVE_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++ptr)
          {
            src.attrib.setElement((GEO_Primitive*)src.elems[i]);
            *ptr = (double) src.attrib.getF(j);
          }
        }
        break;
      case GEO_DETAIL_DICT:
        src.attrib.setElement(src.detail);
        for (int j=0; j<mDim; ++j, ++ptr)
        {
          *ptr = (double) src.attrib.getF(j);
        }
        break;
      default:
        break;
      }
    }
    break;
  case Gto::String:
    {
      if (attrib->getType() != GB_ATTRIB_INDEX)
      {
        return false;
      }
      dim = attrib->getSize() / sizeof(int);
      if (dim < mDim)
      {
        return false;
      }
      UT_String sval;
      int k=0;
      resize(src.klass == GEO_DETAIL_DICT ? 1 : src.elems.size());
      // cannot set mData yet
      // we still don't know the final index at that time
      // (w->intern(mStrings[k].c_str()) doesn't return one, and w->lookup() will fail until string table is complete)
      switch (src.klass)
      {
      case GEO_VERTEX_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++k)
          {
            src.attrib.setElement((GEO_Vertex*)src.elems[i]);
            mStrings[k] = ((src.attrib.getString(sval, j) && sval.nonNullBuffer()) ? sval.nonNullBuffer() : "");
          }
        }
        break;
      case GEO_POINT_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++k)
          {
            src.attrib.setElement((GEO_Point*)src.elems[i]);
            mStrings[k] = ((src.attrib.getString(sval, j) && sval.nonNullBuffer()) ? sval.nonNullBuffer() : "");
          }
        }
        break;
      case GEO_PRIMITIVE_DICT:
        for (size_t i=0; i<src.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++k)
          {
            src.attrib.setElement((GEO_Primitive*)src.elems[i]);
            mStrings[k] = ((src.attrib.getString(sval, j) && sval.nonNullBuffer()) ? sval.nonNullBuffer() : "");
          }
        }
        break;
      case GEO_DETAIL_DICT:
        src.attrib.setElement(src.detail);
        for (int j=0; j<mDim; ++j, ++k)
        {
          mStrings[k] = ((src.attrib.getString(sval, j) && sval.nonNullBuffer()) ? sval.nonNullBuffer() : "");
        }
        break;
      default:
        break;
      }
    }
    break;
  default:
    return false;
  }
  
#endif
  
  return true;
}

bool Attrib::toHoudini(Gto::Reader *r, HAttrib &dst)
{
#if UT_MAJOR_VERSION_INT >= 12
  
  if (!dst.attrib)
  {
    return false;
  }
  
  // handle mapped array here too (don't need in fromHoudini, we built
  // mappings after as houdini does not have mapped elements)
  
  // T *ptr = (T*) &mData[0]
  // for (int i=0, k=0; i<mSize; ++i)
  //   mapped: k = mDim * mMapping[i]
  //   for (int j=0; j<mDim; ++j, ++k)
  //     non-mapped: ptr[k]
  //     mapped    : ptr[k]
  //
  // check sized agains mMapping size for mapped elements
  
  if (isMapped())
  {
    if (dst.elems.size() != mMapping.size())
    {
      return false;
    }
  }
  else
  {
    if (dst.elems.size() != size_t(mSize))
    {
      return false;
    }
  }
  //if (dst.elems.size() != size_t(mSize))
  //{
  //  return false;
  //}
  
  //if (dst.attrib->getTupleSize() != mDim)
  // requires at least mDim elements in tuple
  if (dst.attrib->getTupleSize() < mDim)
  {
    return false;
  }
  
  switch (mType)
  {
  case Gto::Byte:
    {
      if (dst.attrib->getStorageClass() != GA_STORECLASS_INT)
      {
        return false;
      }
      const GA_AIFTuple *tpl = dst.attrib->getAIFTuple();
      if (!tpl)
      {
        return false;
      }
      unsigned char *ptr = (unsigned char*) data();
      if (isMapped())
      {
        for (size_t i=0, k=0; i<dst.elems.size(); ++i)
        {
          k = mDim * mMapping[i];
          for (int j=0; j<mDim; ++j, ++k)
          {
            tpl->set(dst.attrib, dst.elems[i], int32(ptr[k]), j);
          }
        }
      }
      else
      {
        for (size_t i=0, k=0; i<dst.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++k)
          {
            tpl->set(dst.attrib, dst.elems[i], int32(ptr[k]), j);
          }
        }
      }
    }
    break;
  case Gto::Short:
    {
      if (dst.attrib->getStorageClass() != GA_STORECLASS_INT)
      {
        return false;
      }
      const GA_AIFTuple *tpl = dst.attrib->getAIFTuple();
      if (!tpl)
      {
        return false;
      }
      unsigned short *ptr = (unsigned short*) data();
      if (isMapped())
      {
        for (size_t i=0, k=0; i<dst.elems.size(); ++i)
        {
          k = mDim * mMapping[i];
          for (int j=0; j<mDim; ++j, ++k)
          {
            tpl->set(dst.attrib, dst.elems[i], int32(ptr[k]), j);
          }
        }
      }
      else
      {
        for (size_t i=0, k=0; i<dst.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++k)
          {
            tpl->set(dst.attrib, dst.elems[i], int32(ptr[k]), j);
          }
        }
      }
    }
    break;
  case Gto::Int:
    {
      if (dst.attrib->getStorageClass() != GA_STORECLASS_INT)
      {
        return false;
      }
      const GA_AIFTuple *tpl = dst.attrib->getAIFTuple();
      if (!tpl)
      {
        return false;
      }
      int *ptr = (int*) data();
      if (isMapped())
      {
        for (size_t i=0, k=0; i<dst.elems.size(); ++i)
        {
          k = mDim * mMapping[i];
          for (int j=0; j<mDim; ++j, ++k)
          {
            tpl->set(dst.attrib, dst.elems[i], int32(ptr[k]), j);
          }
        }
      }
      else
      {
        for (size_t i=0, k=0; i<dst.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++k)
          {
            tpl->set(dst.attrib, dst.elems[i], int32(ptr[k]), j);
          }
        }
      }
    }
    break;
  case Gto::Float:
    {
      if (dst.attrib->getStorageClass() != GA_STORECLASS_FLOAT)
      {
        return false;
      }
      const GA_AIFTuple *tpl = dst.attrib->getAIFTuple();
      if (!tpl)
      {
        return false;
      }
      float *ptr = (float*) data();
      if (isMapped())
      {
        for (size_t i=0, k=0; i<dst.elems.size(); ++i)
        {
          k = mDim * mMapping[i];
          for (int j=0; j<mDim; ++j, ++k)
          {
            tpl->set(dst.attrib, dst.elems[i], fpreal32(ptr[k]), j);
          }
        }
      }
      else
      {
        for (size_t i=0, k=0; i<dst.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++k)
          {
            tpl->set(dst.attrib, dst.elems[i], fpreal32(ptr[k]), j);
          }
        }
      }
    }
    break;
  case Gto::Double:
    {
      if (dst.attrib->getStorageClass() != GA_STORECLASS_FLOAT)
      {
        return false;
      }
      const GA_AIFTuple *tpl = dst.attrib->getAIFTuple();
      if (!tpl)
      {
        return false;
      }
      double *ptr = (double*) data();
      if (isMapped())
      {
        for (size_t i=0, k=0; i<dst.elems.size(); ++i)
        {
          k = mDim * mMapping[i];
          for (int j=0; j<mDim; ++j, ++k)
          {
            tpl->set(dst.attrib, dst.elems[i], fpreal64(ptr[k]), j);
          }
        }
      }
      else
      {
        for (size_t i=0, k=0; i<dst.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++k)
          {
            tpl->set(dst.attrib, dst.elems[i], fpreal64(ptr[k]), j);
          }
        }
      }
    }
    break;
  case Gto::String:
    {
      if (dst.attrib->getStorageClass() != GA_STORECLASS_STRING)
      {
        return false;
      }
      const GA_AIFStringTuple *tpl = dst.attrib->getAIFSharedStringTuple();
      if (!tpl)
      {
        tpl = dst.attrib->getAIFStringTuple();
        if (!tpl)
        {
          return false;
        }
      }
      int *ptr = (int*) data();
      if (isMapped())
      {
        for (size_t i=0, k=0; i<dst.elems.size(); ++i)
        {
          k = mDim * mMapping[i];
          for (int j=0; j<mDim; ++j, ++k)
          {
            mStrings[k] = r->stringFromId(ptr[k]);
            tpl->setString(dst.attrib, dst.elems[i], mStrings[k].c_str(), j);
          }
        }
      }
      else
      {
        for (size_t i=0, k=0; i<dst.elems.size(); ++i)
        {
          for (int j=0; j<mDim; ++j, ++k)
          {
            mStrings[k] = r->stringFromId(ptr[k]);
            tpl->setString(dst.attrib, dst.elems[i], mStrings[k].c_str(), j);
          }
        }
      }
    }
    break;
  default:
    return false;
  }
  
#else
  
  if (!dst.attrib.isAttributeValid())
  {
    return false;
  }
  
  GB_Attribute *attrib = dst.attrib.getAttribute();
  
  if (dst.klass == GEO_DETAIL_DICT)
  {
    if (isMapped())
    {
      if (mMapping.size() != 1)
      {
        return false;
      }
    }
    else
    {
      if (mSize != 1)
      {
        return false;
      }
    }
  }
  else
  {
    if (isMapped())
    {
      if (dst.elems.size() != mMapping.size())
      {
        return false;
      }
    }
    else
    {
      if (dst.elems.size() != size_t(mSize))
      {
        return false;
      }
    }
  }
  
  int dim;
  
  switch (mType)
  {
  case Gto::Byte:
    {
      if (attrib->getType() != GB_ATTRIB_INT)
      {
        return false;
      }
      dim = attrib->getSize() / sizeof(int);
      if (dim < mDim)
      {
        return false;
      }
      unsigned char *ptr = (unsigned char*) data();
      switch (dst.klass)
      {
      case GEO_VERTEX_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            dst.attrib.setElement((GEO_Vertex*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Vertex*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        break;
      case GEO_POINT_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            dst.attrib.setElement((GEO_Point*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Point*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        break;
      case GEO_PRIMITIVE_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            dst.attrib.setElement((GEO_Primitive*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Primitive*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        break;
      case GEO_DETAIL_DICT:
        dst.attrib.setElement(dst.detail);
        if (isMapped())
        {
          for (int j=0, k=mDim*mMapping[0]; j<mDim; ++j, ++k)
          {
            dst.attrib.setI(ptr[k], j);
          }
        }
        else
        {
          for (int j=0; j<mDim; ++j)
          {
            dst.attrib.setI(ptr[j], j);
          }
        }
        break;
      default:
        break;
      }
    }
    break;
  case Gto::Short:
    {
      if (attrib->getType() != GB_ATTRIB_INT)
      {
        return false;
      }
      dim = attrib->getSize() / sizeof(int);
      if (dim < mDim)
      {
        return false;
      }
      unsigned short *ptr = (unsigned short*) data();
      switch (dst.klass)
      {
      case GEO_VERTEX_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setElement((GEO_Vertex*)dst.elems[i]);
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setElement((GEO_Vertex*)dst.elems[i]);
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        break;
      case GEO_POINT_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            dst.attrib.setElement((GEO_Point*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Point*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        break;
      case GEO_PRIMITIVE_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            dst.attrib.setElement((GEO_Primitive*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Primitive*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        break;
      case GEO_DETAIL_DICT:
        dst.attrib.setElement(dst.detail);
        if (isMapped())
        {
          for (int j=0, k=mDim*mMapping[0]; j<mDim; ++j, ++k)
          {
            dst.attrib.setI(ptr[k], j);
          }
        }
        else
        {
          for (int j=0; j<mDim; ++j)
          {
            dst.attrib.setI(ptr[j], j);
          }
        }
        break;
      default:
        break;
      }
    }
    break;
  case Gto::Int:
    {
      if (attrib->getType() != GB_ATTRIB_INT)
      {
        return false;
      }
      dim = attrib->getSize() / sizeof(int);
      if (dim < mDim)
      {
        return false;
      }
      int *ptr = (int*) data();
      switch (dst.klass)
      {
      case GEO_VERTEX_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            dst.attrib.setElement((GEO_Vertex*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Vertex*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        break;
      case GEO_POINT_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            dst.attrib.setElement((GEO_Point*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Point*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        break;
      case GEO_PRIMITIVE_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            dst.attrib.setElement((GEO_Primitive*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Primitive*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setI(ptr[k], j);
            }
          }
        }
        break;
      case GEO_DETAIL_DICT:
        dst.attrib.setElement(dst.detail);
        if (isMapped())
        {
          for (int j=0, k=mDim*mMapping[0]; j<mDim; ++j, ++k)
          {
            dst.attrib.setI(ptr[k], j);
          }
        }
        else
        {
          for (int j=0; j<mDim; ++j)
          {
            dst.attrib.setI(ptr[j], j);
          }
        }
        break;
      default:
        break;
      }
    }
    break;
  case Gto::Float:
    {
      if (attrib->getType() == GB_ATTRIB_FLOAT)
      {
        dim = attrib->getSize() / sizeof(float);
        if (dim < mDim)
        {
          return false;
        }
      }
      else if (attrib->getType() == GB_ATTRIB_VECTOR)
      {
        // dim = 3;
        if (3 != mDim)
        {
          return false;
        }
      }
      else
      {
        return false;
      }
      float *ptr = (float*) data();
      switch (dst.klass)
      {
      case GEO_VERTEX_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            dst.attrib.setElement((GEO_Vertex*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setF(ptr[k], j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Vertex*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setF(ptr[k], j);
            }
          }
        }
        break;
      case GEO_POINT_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            dst.attrib.setElement((GEO_Point*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setF(ptr[k], j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Point*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setF(ptr[k], j);
            }
          }
        }
        break;
      case GEO_PRIMITIVE_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[k];
            dst.attrib.setElement((GEO_Primitive*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setF(ptr[k], j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Primitive*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setF(ptr[k], j);
            }
          }
        }
        break;
      case GEO_DETAIL_DICT:
        dst.attrib.setElement(dst.detail);
        if (isMapped())
        {
          for (int j=0, k=mDim*mMapping[0]; j<mDim; ++j, ++k)
          {
            dst.attrib.setF(ptr[k], j);
          }
        }
        else
        {
          for (int j=0; j<mDim; ++j)
          {
            dst.attrib.setF(ptr[j], j);
          }
        }
        break;
      default:
        break;
      }
    }
    break;
  case Gto::Double:
    {
      if (attrib->getType() == GB_ATTRIB_FLOAT)
      {
        dim = attrib->getSize() / sizeof(float);
        if (dim < mDim)
        {
          return false;
        }
      }
      else if (attrib->getType() == GB_ATTRIB_VECTOR)
      {
        // dim = 3;
        if (3 != mDim)
        {
          return false;
        }
      }
      else
      {
        return false;
      }
      double *ptr = (double*) data();
      switch (dst.klass)
      {
      case GEO_VERTEX_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            dst.attrib.setElement((GEO_Vertex*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setF(ptr[k], j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Vertex*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setF(ptr[k], j);
            }
          }
        }
        break;
      case GEO_POINT_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            dst.attrib.setElement((GEO_Point*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setF(ptr[k], j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Point*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setF(ptr[k], j);
            }
          }
        }
        break;
      case GEO_PRIMITIVE_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            dst.attrib.setElement((GEO_Primitive*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setF(ptr[k], j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Primitive*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              dst.attrib.setF(ptr[k], j);
            }
          }
        }
        break;
      case GEO_DETAIL_DICT:
        dst.attrib.setElement(dst.detail);
        if (isMapped())
        {
          for (int j=0, k=mDim*mMapping[0]; j<mDim; ++j, ++k)
          {
            dst.attrib.setF(ptr[k], j);
          }
        }
        else
        {
          for (int j=0; j<mDim; ++j)
          {
            dst.attrib.setF(ptr[j], j);
          }
        }
        break;
      default:
        break;
      }
    }
    break;
  case Gto::String:
    {
      if (attrib->getType() != GB_ATTRIB_INDEX)
      {
        return false;
      }
      dim = attrib->getSize() / sizeof(int);
      if (dim < mDim)
      {
        return false;
      }
      int *ptr = (int*) data();
      switch (dst.klass)
      {
      case GEO_VERTEX_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            dst.attrib.setElement((GEO_Vertex*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              mStrings[k] = r->stringFromId(ptr[k]);
              dst.attrib.setI(dst.attrib.addDefinedString(mStrings[k].c_str()), j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Vertex*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              mStrings[k] = r->stringFromId(ptr[k]);
              dst.attrib.setI(dst.attrib.addDefinedString(mStrings[k].c_str()), j);
            }
          }
        }
        break;
      case GEO_POINT_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            dst.attrib.setElement((GEO_Point*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              mStrings[k] = r->stringFromId(ptr[k]);
              dst.attrib.setI(dst.attrib.addDefinedString(mStrings[k].c_str()), j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Point*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              mStrings[k] = r->stringFromId(ptr[k]);
              dst.attrib.setI(dst.attrib.addDefinedString(mStrings[k].c_str()), j);
            }
          }
        }
        break;
      case GEO_PRIMITIVE_DICT:
        if (isMapped())
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            k = mDim * mMapping[i];
            dst.attrib.setElement((GEO_Primitive*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              mStrings[k] = r->stringFromId(ptr[k]);
              dst.attrib.setI(dst.attrib.addDefinedString(mStrings[k].c_str()), j);
            }
          }
        }
        else
        {
          for (size_t i=0, k=0; i<dst.elems.size(); ++i)
          {
            dst.attrib.setElement((GEO_Primitive*)dst.elems[i]);
            for (int j=0; j<mDim; ++j, ++k)
            {
              mStrings[k] = r->stringFromId(ptr[k]);
              dst.attrib.setI(dst.attrib.addDefinedString(mStrings[k].c_str()), j);
            }
          }
        }
        break;
      case GEO_DETAIL_DICT:
        dst.attrib.setElement(dst.detail);
        if (isMapped())
        {
          for (int j=0, k=mDim*mMapping[0]; j<mDim; ++j, ++k)
          {
            mStrings[k] = r->stringFromId(ptr[k]);
            dst.attrib.setI(dst.attrib.addDefinedString(mStrings[k].c_str()), j);
          }
        }
        else
        {
          for (int j=0; j<mDim; ++j)
          {
            mStrings[j] = r->stringFromId(ptr[j]);
            dst.attrib.setI(dst.attrib.addDefinedString(mStrings[j].c_str()), j);
          }
        }
        break;
      default:
        break;
      }
    }
    break;
  default:
    return false;
  }
  
#endif
  
  return true;
}

void Attrib::declareGtoMapping(Gto::Writer *w, const std::string &name)
{
  if (!isValid() || isEmpty() || !isMapped())
  {
    return;
  }
  w->property(name.c_str(), Gto::Int, mMapping.size(), 1);
}

void Attrib::outputGtoMapping(Gto::Writer *w)
{
  if (!isValid() || isEmpty() || !isMapped())
  {
    return;
  }
  w->propertyData((void*) &mMapping[0]);
}

void Attrib::declareGto(Gto::Writer *w, const std::string &name)
{
  if (!isValid() || isEmpty())
  {
    return;
  }
  if (mType == Gto::String)
  {
    for (size_t i=0; i<mStrings.size(); ++i)
    {
      w->intern(mStrings[i].c_str());
    }
  }
  w->property(name.c_str(), mType, mSize, mDim, (mInterp.length() > 0 ? mInterp.c_str() : 0));
}

void Attrib::outputGto(Gto::Writer *w)
{
  if (!isValid() || isEmpty())
  {
    return;
  }
  if (mType == Gto::String)
  {
    int *ptr = (int*) data();
    for (size_t i=0; i<mStrings.size(); ++i)
    {
      ptr[i] = w->lookup(mStrings[i].c_str());
    }
  }
  w->propertyData(data());
}

// ---

AttribDict::AttribDict()
  : BaseAttribDict()
{
}

AttribDict::AttribDict(const AttribDict &rhs)
  : BaseAttribDict(rhs)
  , mHA(rhs.mHA)
  , mIgnores(rhs.mIgnores)
{
}

AttribDict::~AttribDict()
{
}

AttribDict& AttribDict::operator=(const AttribDict &rhs)
{
  BaseAttribDict::operator=(rhs);
  if (this != &rhs)
  {
    mHA = rhs.mHA;
    mIgnores = rhs.mIgnores;
  }
  return *this;
}

void AttribDict::clearIgnores()
{
  mIgnores.clear();
}

void AttribDict::addIgnore(const std::string &n)
{
  mIgnores.insert(n);
}

void AttribDict::addIgnores(const AttribDict &ad, bool addSourceIgnores)
{
  const_iterator it = ad.begin();
  while (it != ad.end())
  {
    addIgnore(it->first);
    ++it;
  }
  if (addSourceIgnores)
  {
    std::set<std::string>::const_iterator iit = ad.mIgnores.begin();
    while (iit != ad.mIgnores.end())
    {
      addIgnore(*iit);
      ++iit;
    }
  }
}

void AttribDict::removeIgnore(const std::string &n)
{
  std::set<std::string>::iterator it = mIgnores.find(n);
  if (it != mIgnores.end())
  {
    mIgnores.erase(it);
  }
}

bool AttribDict::ignore(const std::string &n) const
{
  return (mIgnores.find(n) != mIgnores.end());
}

#if UT_MAJOR_VERSION_INT >= 12

void AttribDict::collect(GU_Detail *gdp, GA_AttributeOwner klass, const std::vector<GA_Offset> &elems, bool readData)
{
  mHA.detail = gdp;
  mHA.klass = klass;
  mHA.elems = elems;
  
  clear();

  const GA_AttributeDict *attribs = 0;

  switch (klass)
  {
  case GA_ATTRIB_VERTEX:
    attribs = &(gdp->vertexAttribs());
    break;
  case GA_ATTRIB_POINT:
    attribs = &(gdp->pointAttribs());
    break;
  case GA_ATTRIB_PRIMITIVE:
    attribs = &(gdp->primitiveAttribs());
    break;
  case GA_ATTRIB_GLOBAL:
    attribs = &(gdp->attribs());
    break;
  default:
    break;
  }

  if (!attribs)
  {
    return;
  }

  for (GA_AttributeDict::iterator it=attribs->begin(GA_SCOPE_PUBLIC); !it.atEnd(); ++it)
  {
    std::string name = it.name();
    if (ignore(name))
    {
#ifdef _DEBUG
      Log::Print("Ignore attribute %s", name.c_str());
#endif
      continue;
    }
    GA_Attribute *attrib = it.attrib();
    std::pair<GA_Attribute*, Attrib> &entry = (*this)[name];
    if (!entry.second.init(attrib))
    {
#ifdef _DEBUG
      Log::Print("Ignore attribute %s (unsupported)", name.c_str());
#endif
      erase(find(name));
    }
    else
    {
      if (readData)
      {
        mHA.attrib = attrib;
        if (!entry.second.fromHoudini(mHA))
        {
#ifdef _DEBUG
          Log::Print("Ignore attribute %s (could not read data)", name.c_str());
#endif
          erase(find(name));
          continue;
        }
      }
#ifdef _DEBUG
      Log::Print("Add attribute %s", name.c_str());
#endif
      entry.first = attrib;
    }
  }
}

void AttribDict::collectPoints(GU_Detail *gdp, const std::vector<GA_Offset> &elems, bool elemsAsPrimitives, bool readData)
{
  std::vector<GA_Offset> pelems;

  if (elems.size() == 0)
  {
    pelems.resize(gdp->getNumPoints());
    int i=0;
    for (GA_Iterator it(gdp->getPointRange()); !it.atEnd(); ++it, ++i)
    {
      pelems[i] = it.getOffset();
    }
  }
  else if (!elemsAsPrimitives)
  {
    return collect(gdp, GA_ATTRIB_POINT, elems, readData);
  }
  else
  {
    std::vector<GA_Offset> primPoints;
    int j=0;
    for (size_t i=0; i<elems.size(); ++i)
    {
      const GEO_Primitive *prim = gdp->getGEOPrimitive(elems[i]);
      GA_Range pr = prim->getPointRange();
      primPoints.resize(primPoints.size() + pr.getEntries());
      for (GA_Iterator it(pr); !it.atEnd(); ++it, ++j)
      {
        primPoints[j] = it.getOffset();
      }
    }

    std::sort(primPoints.begin(), primPoints.end());

    pelems.reserve(primPoints.size());

    GA_Offset index, prevIndex = GA_INVALID_OFFSET;

    for (size_t i=0; i<primPoints.size(); ++i)
    {
      index = primPoints[i];
      if (index == prevIndex)
      {
        continue;
      }
      pelems.push_back(index);
      prevIndex = index;
    }
  }

  collect(gdp, GA_ATTRIB_POINT, pelems, readData);
}

void AttribDict::collectVertices(GU_Detail *gdp, const std::vector<GA_Offset> &prims, bool readData)
{
  std::vector<GA_Offset> elems;
  if (prims.size() == 0)
  {
    elems.resize(gdp->getNumVertices());
    int i=0;
    for (GA_Iterator it(gdp->getVertexRange()); !it.atEnd(); ++it, ++i)
    {
      elems[i] = it.getOffset();
    }
  }
  else if (prims.size() == 1)
  {
    const GEO_Primitive *prim = gdp->getGEOPrimitive(prims[0]);
    elems.resize(prim->getVertexCount());
    int i=0;
    for (GA_Iterator it(prim->getVertexRange()); !it.atEnd(); ++it, ++i)
    {
      elems[i] = it.getOffset();
    }
  }
  else
  {
    for (size_t i=0; i<prims.size(); ++i)
    {
      const GEO_Primitive *prim = gdp->getGEOPrimitive(prims[i]);
      for (GA_Iterator it(prim->getVertexRange()); !it.atEnd(); ++it)
      {
        elems.push_back(it.getOffset());
      }
    }
  }
  collect(gdp, GA_ATTRIB_VERTEX, elems, readData);
}

void AttribDict::collectPrimitives(GU_Detail *gdp, const std::vector<GA_Offset> &prims, bool readData)
{

  if (prims.size() == 0)
  {
    std::vector<GA_Offset> elems(gdp->getNumPrimitives());
    int i=0;
    for (GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it, ++i)
    {
      elems[i] = it.getOffset();
    }
    collect(gdp, GA_ATTRIB_PRIMITIVE, prims, readData);
  }
  else
  {
    collect(gdp, GA_ATTRIB_PRIMITIVE, prims, readData);
  }
}

void AttribDict::collectGlobals(GU_Detail *gdp, bool readData)
{
  std::vector<GA_Offset> elems(1);
  elems[0] = 0;
  collect(gdp, GA_ATTRIB_GLOBAL, elems, readData);
}

void AttribDict::create(GU_Detail *gdp, GA_AttributeOwner klass, const std::vector<GA_Offset> &elems)
{
  mHA.detail = gdp;
  mHA.klass = klass;
  mHA.elems = elems;

  iterator it = begin();
  while (it != end())
  {
    GA_RWAttributeRef ref = gdp->findAttribute(klass, GA_SCOPE_PUBLIC, it->first.c_str());
    if (!ref.isValid())
    {
      it->second.first = it->second.second.create(gdp, klass, it->first);
    }
    else
    {
      it->second.first = ref.getAttribute();
    }
    ++it;
  }
}

#else

void AttribDict::collect(GU_Detail *gdp, GEO_AttributeOwner klass, const std::vector<GB_AttributeElem*> &elems, bool readData)
{
  mHA.detail = gdp;
  mHA.klass = klass;
  mHA.elems = elems;

  clear();

  const GB_AttributeDict *attribs = 0;

  switch (klass)
  {
  case GEO_VERTEX_DICT:
    attribs = &(gdp->vertexAttribs());
    break;
  case GEO_POINT_DICT:
    attribs = &(gdp->pointAttribs());
    break;
  case GEO_PRIMITIVE_DICT:
    attribs = &(gdp->primitiveAttribs());
    break;
  case GEO_DETAIL_DICT:
    attribs = &(gdp->attribs());
  default:
    break;
  }

  if (!attribs)
  {
    return;
  }

  GB_Attribute *attrib = attribs->getHead();
  while (attrib)
  {
    std::string name = attrib->getName();
    if (ignore(name))
    {
#ifdef _DEBUG
      Log::Print("Ignore attribute %s", name.c_str());
#endif
      attrib = (GB_Attribute*) attrib->next();
      continue;
    }
    std::pair<GEO_AttributeHandle, Attrib> &entry = (*this)[name];
    if (!entry.second.init(attrib))
    {
#ifdef _DEBUG
      Log::Print("Ignore attribute %s (unsupported)", name.c_str());
#endif
      erase(find(name));
    }
    else
    {
      if (readData)
      {
        mHA.attrib = gdp->getAttribute(klass, name.c_str());
        if (!entry.second.fromHoudini(mHA))
        {
#ifdef _DEBUG
          Log::Print("Ignore attribute %s (could not read data)", name.c_str());
#endif
          erase(find(name));
          attrib = (GB_Attribute*) attrib->next();
          continue;
        }
      }
#ifdef _DEBUG
      Log::Print("Add attribute %s", name.c_str());
#endif
      entry.first = gdp->getAttribute(klass, name.c_str());
    }
    attrib = (GB_Attribute*) attrib->next();
  }
}

void AttribDict::collect(GU_Detail *gdp, GEO_AttributeOwner klass, unsigned int offset, unsigned int count, bool readData)
{
  mHA.detail = gdp;
  mHA.klass = klass;
  mHA.elems.clear();

  clear();

  const GB_AttributeDict *attribs = 0;

  switch (klass)
  {
  case GEO_VERTEX_DICT:
    {
      attribs = &(gdp->vertexAttribs());
      GEO_PrimList &prims = gdp->primitives();
      for (unsigned int i=0, j=offset; i<count; ++i, ++j)
      {
        GEO_Primitive *prim = prims[j];
        for (unsigned int k=0; k<prim->getVertexCount(); ++k)
        {
          mHA.elems.push_back(&(prim->getVertex(k)));
        }
      }
    }
    break;
  case GEO_POINT_DICT:
    {
      attribs = &(gdp->pointAttribs());
      GEO_PointList &points = gdp->points();
      mHA.elems.resize(count);
      for (unsigned int i=0, j=offset; i<count; ++i, ++j)
      {
        mHA.elems[i] = points[j];
      }
    }
    break;
  case GEO_PRIMITIVE_DICT:
    {
      attribs = &(gdp->primitiveAttribs());
      GEO_PrimList &prims = gdp->primitives();
      mHA.elems.resize(count);
      for (unsigned int i=0, j=offset; i<count; ++i, ++j)
      {
        mHA.elems[i] = prims[j];
      }
    }
    break;
  case GEO_DETAIL_DICT:
    attribs = &(gdp->attribs());
  default:
    break;
  }

  if (!attribs)
  {
    return;
  }

  GB_Attribute *attrib = attribs->getHead();
  while (attrib)
  {
    std::string name = attrib->getName();
    if (ignore(name))
    {
#ifdef _DEBUG
      Log::Print("Ignore attribute %s", name.c_str());
#endif
      attrib = (GB_Attribute*) attrib->next();
      continue;
    }
    std::pair<GEO_AttributeHandle, Attrib> &entry = (*this)[name];
    if (!entry.second.init(attrib))
    {
#ifdef _DEBUG
      Log::Print("Ignore attribute %s (unsupported)", name.c_str());
#endif
      erase(find(name));
    }
    else
    {
      if (readData)
      {
        mHA.attrib = gdp->getAttribute(klass, name.c_str());
        if (!entry.second.fromHoudini(mHA))
        {
#ifdef _DEBUG
          Log::Print("Ignore attribute %s (could not read data)", name.c_str());
#endif
          erase(find(name));
          attrib = (GB_Attribute*) attrib->next();
          continue;
        }
      }
#ifdef _DEBUG
      Log::Print("Add attribute %s", name.c_str());
#endif
      entry.first = gdp->getAttribute(klass, name.c_str());
    }
    attrib = (GB_Attribute*) attrib->next();
  }
}

void AttribDict::collectPoints(GU_Detail *gdp, const std::vector<unsigned int> &elems, bool elemsAsPrimitives, bool readData)
{
  GEO_PointList &points = gdp->points();

  std::vector<GB_AttributeElem*> pelems;

  if (elems.size() == 0)
  {
    collect(gdp, GEO_POINT_DICT, 0, points.entries(), readData);
  }
  else if (!elemsAsPrimitives)
  {
    pelems.resize(elems.size());
    for (size_t i=0; i<elems.size(); ++i)
    {
      pelems[i] = points[i];
    }
    collect(gdp, GEO_POINT_DICT, pelems, readData);
  }
  else
  {
    std::map<GEO_Point*, size_t> ptIndices;
    for (size_t i=0; i<points.entries(); ++i)
    {
      ptIndices[points[i]] = i;
    }
    
    GEO_PrimList &primitives = gdp->primitives();

    std::vector<size_t> primPoints;
    size_t k=0;
    for (size_t i=0; i<elems.size(); ++i)
    {
      GEO_Primitive *prim = primitives[elems[i]];
      primPoints.resize(primPoints.size() + prim->getVertexCount());
      for (size_t j=0; j<prim->getVertexCount(); ++j, ++k)
      {
        GEO_Vertex &v = prim->getVertex(j);
        primPoints[k] = ptIndices[v.getPt()];
      }
    }
    
    pelems.reserve(primPoints.size());

    std::sort(primPoints.begin(), primPoints.end());

    size_t index, prevIndex = size_t(-1);

    for (size_t i=0; i<primPoints.size(); ++i)
    {
      index = primPoints[i];
      if (index == prevIndex)
      {
        continue;
      }
      pelems.push_back(points[index]);
      prevIndex = index;
    }

    collect(gdp, GEO_POINT_DICT, pelems, readData);
  }
}

void AttribDict::collectVertices(GU_Detail *gdp, const std::vector<unsigned int> &prims, bool readData)
{
  GEO_PrimList &primitives = gdp->primitives();

  std::vector<GB_AttributeElem*> elems;

  if (prims.size() == 0)
  {
    for (size_t i=0; i<primitives.entries(); ++i)
    {
      GEO_Primitive *prim = primitives[i];
      for (size_t j=0; j<prim->getVertexCount(); ++j)
      {
        elems.push_back(&(prim->getVertex(j)));
      }
    }
  }
  else if (prims.size() == 1)
  {
    GEO_Primitive *prim = primitives[prims[0]];
    elems.resize(prim->getVertexCount());
    for (size_t i=0; i<elems.size(); ++i)
    {
      elems[i] = &(prim->getVertex(i));
    }
  }
  else
  {
    for (size_t i=0; i<prims.size(); ++i)
    {
      GEO_Primitive *prim = primitives[prims[i]];
      for (size_t j=0; j<prim->getVertexCount(); ++j)
      {
        elems.push_back(&(prim->getVertex(j)));
      }
    }
  }

  collect(gdp, GEO_VERTEX_DICT, elems, readData);
}

void AttribDict::collectPrimitives(GU_Detail *gdp, const std::vector<unsigned int> &prims, bool readData)
{
  GEO_PrimList &primitives = gdp->primitives();

  if (prims.size() == 0)
  {
    collect(gdp, GEO_PRIMITIVE_DICT, 0, primitives.entries(), readData);
  }
  else
  {
    std::vector<GB_AttributeElem*> elems(prims.size());
    for (size_t i=0; i<prims.size(); ++i)
    {
      elems[i] = primitives[prims[i]];
    }
    collect(gdp, GEO_PRIMITIVE_DICT, elems, readData);
  }
}

void AttribDict::collectGlobals(GU_Detail *gdp, bool readData)
{
  collect(gdp, GEO_DETAIL_DICT, 0, 1, readData);
}

void AttribDict::create(GU_Detail *gdp, GEO_AttributeOwner klass, unsigned int offset, unsigned int count)
{
  mHA.detail = gdp;
  mHA.klass = klass;
  mHA.elems.clear();

  switch (klass)
  {
  case GEO_VERTEX_DICT:
    {
      GEO_PrimList &prims = gdp->primitives();
      for (unsigned int i=0, j=offset; i<count; ++i, ++j)
      {
        GEO_Primitive *prim = prims[j];
        for (unsigned int k=0; k<prim->getVertexCount(); ++k)
        {
          mHA.elems.push_back(&(prim->getVertex(k)));
        }
      }
    }
    break;
  case GEO_POINT_DICT:
    {
      GEO_PointList &points = gdp->points();
      mHA.elems.resize(count);
      for (unsigned int i=0, j=offset; i<count; ++i, ++j)
      {
        mHA.elems[i] = points[j];
      }
    }
    break;
  case GEO_PRIMITIVE_DICT:
    {
      GEO_PrimList &prims = gdp->primitives();
      mHA.elems.resize(count);
      for (unsigned int i=0, j=offset; i<count; ++i, ++j)
      {
        mHA.elems[i] = prims[j];
      }
    }
    break;
  default:
    break;
  }

  iterator it = begin();
  while (it != end())
  {
    GEO_AttributeHandle hdl = gdp->getAttribute(klass, it->first.c_str());
    if (!hdl.isAttributeValid())
    {
      it->second.first = it->second.second.create(gdp, klass, it->first);
    }
    else
    {
      it->second.first = hdl;
    }
    ++it;
  }
}

#endif

void AttribDict::declareGtoMapping(Gto::Writer *w, const std::string &prefix, const std::string &suffix)
{
  iterator it = begin();
  while (it != end())
  {
    it->second.second.declareGtoMapping(w, prefix+it->first+suffix);
    ++it;
  }
}

void AttribDict::outputGtoMapping(Gto::Writer *w)
{
  iterator it = begin();
  while (it != end())
  {
    it->second.second.outputGtoMapping(w);
    ++it;
  }
}

void AttribDict::declareGto(Gto::Writer *w)
{
  iterator it = begin();
  while (it != end())
  {
    mHA.attrib = it->second.first;
    //if (it->second.second.fromHoudini(w, mHA))
    if (!it->second.second.isEmpty() || it->second.second.fromHoudini(mHA))
    {
      it->second.second.declareGto(w, it->first);
    }
    ++it;
  }
}

void AttribDict::outputGto(Gto::Writer *w)
{
  iterator it = begin();
  while (it != end())
  {
    it->second.second.outputGto(w);
    ++it;
  }
}

bool AttribDict::fromHoudini(const char *name)
{
  iterator it;
  
  if (name)
  {
    it = find(name);
    if (it != end())
    {
      mHA.attrib = it->second.first;
      return it->second.second.fromHoudini(mHA);
    }
    else
    {
      return false;
    }
  }
  else
  {
    bool rv = true;
    it = begin();
    while (it != end())
    {
      mHA.attrib = it->second.first;
      rv = (rv && it->second.second.fromHoudini(mHA));
      ++it;
    }
    return rv;
  }
}

void AttribDict::toHoudini(Gto::Reader *r)
{
  iterator it = begin();
  while (it != end())
  {
    mHA.attrib = it->second.first;
    it->second.second.toHoudini(r, mHA);
    ++it;
  }
}
