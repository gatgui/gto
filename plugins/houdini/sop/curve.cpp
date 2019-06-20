#include "curve.h"
#include "reader.h"
#include "transform.h"
#include "utils.h"
#include "bounds.h"
#include <sstream>
#include <GU/GU_Curve.h>
#include <GU/GU_PrimNURBCurve.h>
#include <GU/GU_PrimRBezCurve.h>
#if UT_MAJOR_VERSION_INT >= 12
# include <GA/GA_Basis.h>
# include <GA/GA_NUBBasis.h>
#else
# include <GB/GB_Basis.h>
# include <GB/GB_NUBBasis.h>
#endif
#include <Gto/Protocols.h>

Curve::Curve(const std::string &name, bool boundsOnly, bool asDiff)
  : Object(name, asDiff)
  , mParent(0)
  , mNumPoints(0)
  , mPositions(0)
  , mWeights(0)
  , mDegree(0)
  , mNumKnots(0)
  , mKnots(0)
  , mForm(0)
  , mBoundsOnly(boundsOnly)
{
}

Curve::~Curve()
{
  cleanup();
}
  
Gto::Reader::Request Curve::component(Reader *rdr,
                                      const std::string& name,
                                      const std::string& interp,
                                      const Gto::Reader::ComponentInfo &header)
{
  if (mBoundsOnly)
  {
    return Object::component(rdr, name, interp, header);
  }

  if (name == GTO_COMPONENT_POINTS)
  {
    return Gto::Reader::Request(true, (void*) READ_POINTS);
  }
  else if (name == GTO_COMPONENT_SURFACE)
  {
    return Gto::Reader::Request(!isDiff(), (void*) READ_SURFACE);
  }
  return Object::component(rdr, name, interp, header);
}

Gto::Reader::Request Curve::property(Reader *rdr,
                                     const std::string& name,
                                     const std::string& interp,
                                     const Gto::Reader::PropertyInfo &header)
{
  void *what = header.component->componentData;
  
  if (mBoundsOnly)
  {
    if (what == (void*) READ_OBJECT)
    {
      if (name == GTO_PROPERTY_BOUNDINGBOX)
      {
        return Gto::Reader::Request(true, (void*) READ_BOUNDS);
      }
      // as Object also read object components, do not early return
    }

    return Object::property(rdr, name, interp, header);
  }

  if (what == (void*) READ_POINTS)
  {
    if (name == GTO_PROPERTY_POSITION)
    {
      return Gto::Reader::Request(true, (void*) READ_POSITIONS);
    }
    else if (name == GTO_PROPERTY_WEIGHT)
    {
      return Gto::Reader::Request(true, (void*) READ_WEIGHTS);
    }
  }
  else if (what == (void*) READ_SURFACE)
  {
    if (name == GTO_PROPERTY_DEGREE)
    {
      return Gto::Reader::Request(true, (void*) READ_DEGREE);
    }
    else if (name == GTO_PROPERTY_VKNOTS)
    {
      return Gto::Reader::Request(true, (void*) READ_KNOTS);
    }
    else if (name == GTO_PROPERTY_VRANGE)
    {
      return Gto::Reader::Request(true, (void*) READ_RANGE);
    }
    else if (name == GTO_PROPERTY_VFORM)
    {
      return Gto::Reader::Request(true, (void*) READ_FORM);
    }
  }
  return Object::property(rdr, name, interp, header);
}

void* Curve::data(Reader *rdr, const Gto::Reader::PropertyInfo &header, size_t bytes)
{
  void *what = header.propertyData;
  
  if (mBoundsOnly)
  {
    if (what == (void*) READ_BOUNDS)
    {
      if (header.width != 6 || header.size != 1 || header.type != Gto::Float)
      {
        return NULL;
      }
      return mBounds;
    }

    return Object::data(rdr, header, bytes);
  }
  
  if (what == (void*) READ_POSITIONS)
  {
    if (header.width != 3 || header.type != Gto::Float)
    {
      return NULL;
    }
    if (mNumPoints == 0)
    {
      mNumPoints = header.size;
    }
    else if (mNumPoints != header.size)
    {
      return NULL;
    }
    mPositions = new float[mNumPoints*3];
    return mPositions;
  }
  else if (what == (void*) READ_WEIGHTS)
  {
    if (header.width != 1 || header.type != Gto::Float)
    {
      return NULL;
    }
    if (mNumPoints == 0)
    {
      mNumPoints = header.size;
    }
    else if (mNumPoints != header.size)
    {
      return NULL;
    }
    mWeights = new float[mNumPoints];
    return mWeights;
  }
  else if (what == (void*) READ_DEGREE)
  {
    if (header.width != 1 || header.size != 1 || header.type != Gto::Int)
    {
      return NULL;
    }
    return &mDegree;
  }
  else if (what == (void*) READ_KNOTS)
  {
    if (header.width != 1 || header.type != Gto::Float)
    {
      return NULL;
    }
    mNumKnots = header.size;
    mKnots = new float[mNumKnots];
    return mKnots;
  }
  else if (what == (void*) READ_RANGE)
  {
    if (header.width != 1 || header.size != 2 || header.type != Gto::Float)
    {
      return NULL;
    }
    return mRange;
  }
  else if (what == (void*) READ_FORM)
  {
    if (header.width != 1 || header.size != 1 || header.type != Gto::Int)
    {
      return NULL;
    }
    return &mForm;
  }

  return Object::data(rdr, header, bytes);
}

void Curve::dataRead(Reader *rdr, const Gto::Reader::PropertyInfo &header)
{
  Object::dataRead(rdr, header);
}

bool Curve::toHoudini(Reader *reader,
                      OP_Context &ctx,
                      GU_Detail *gdp,
                      const ExpandOptions &opts,
                      Object *obj0,
                      Object *obj1,
                      float blend,
                      float t0,
                      float t1)
{
  Object::toHoudini(reader, ctx, gdp, opts, obj0, obj1, blend, t0, t1);
  
  ++mInstanceNum;
  
  if (!opts.ignoreVisibility && !isVisible(-1))
  {
    return true;
  }
  
  if (mBoundsOnly)
  {
    return GenerateBox<Curve>(reader, ctx, gdp, opts, this, mInstanceNum, obj0, obj1, blend, t0, t1);
  }

  UT_Matrix4 world(1.0f);
  UT_Matrix4 pworld(1.0f), nworld(1.0f);
  
  if (mNumPoints <= 0)
  {
    return false;
  }
  
#if UT_MAJOR_VERSION_INT >= 12
  GA_PrimitiveGroup *pmgroup = 0;
#else
  GB_PrimitiveGroup *pmgroup = 0;
#endif
  
  if (!opts.computeInstance || !opts.separateInstance)
  {
    pmgroup = gdp->findPrimitiveGroup(getName().c_str());
  }
  if (!pmgroup)
  {
    if (opts.separateInstance && mInstanceNum > 0)
    {
      std::ostringstream oss;
      oss << getName() << "_" << mInstanceNum;
      pmgroup = gdp->newPrimitiveGroup(oss.str().c_str());
    }
    else
    {
      pmgroup = gdp->newPrimitiveGroup(getName().c_str());
    }
  }
  
#if UT_MAJOR_VERSION_INT >= 12
  std::vector<GA_Offset> poffsets(mNumPoints);
  //pmgroup->makeOrdered();
#else
  GEO_PointList &pl = gdp->points();
  size_t poffset = pl.entries();
#endif
  
  // compute interpolated positions if necessary
  float *positions = mPositions;
  float *weights = mWeights;
  bool interpolated = false;
  
  if (obj0 && ((Curve*)obj0)->mNumPoints == mNumPoints)
  {
    if (obj1 && ((Curve*)obj1)->mNumPoints == mNumPoints)
    {
      float a = 1.0f - blend;
      float b = blend;
      
      interpolated = true;
      
      UT_Matrix4 wm0(1.0f);
      UT_Matrix4 wm1(1.0f);
      
      if (mParent && getName() != opts.shapeName && !opts.ignoreTransforms)
      {
        wm0 = mParent->updateWorldMatrix(opts.globalScale, -1);
        wm1 = mParent->updateWorldMatrix(opts.globalScale,  1);
      }
      
      float *p0 = ((Curve*)obj0)->mPositions;
      float *p1 = ((Curve*)obj1)->mPositions;
      
      positions = new float[mNumPoints * 3];
      
      for (int i=0, j=0; i<mNumPoints; ++i, j+=3)
      {
        UT_Vector4 pp(p0[j] * opts.globalScale, p0[j+1] * opts.globalScale, p0[j+2] * opts.globalScale, 1.0f);
        pp = pp * wm0;
        
        UT_Vector4 np(p1[j] * opts.globalScale, p1[j+1] * opts.globalScale, p1[j+2] * opts.globalScale, 1.0f);
        np = np * wm1;
        
        positions[j] = a * pp.x() + b * np.x();
        positions[j+1] = a * pp.y() + b * np.y();
        positions[j+2] = a * pp.z() + b * np.z();
      }
      
      // also interpolate weights if present
      if (mWeights != 0)
      {
        float *w0 = ((Curve*)obj0)->mWeights;
        float *w1 = ((Curve*)obj1)->mWeights;
        
        weights = new float[mNumPoints];
        
        for (int i=0; i<mNumPoints; ++i)
        {
          weights[i] = a * w0[i] + b * w1[i];
        }
      }
    }
    else
    {
      positions = ((Curve*)obj0)->mPositions;
      weights = ((Curve*)obj0)->mWeights;
      
      if (mParent && getName() != opts.shapeName && !opts.ignoreTransforms)
      {
        world = mParent->updateWorldMatrix(opts.globalScale, -1);
      }
    }
  }
  else
  {
    if (mParent && getName() != opts.shapeName && !opts.ignoreTransforms)
    {
      if (blend <= 0.0f)
      {
        world = mParent->updateWorldMatrix(opts.globalScale, -1);
      }
      else
      {
        pworld = mParent->updateWorldMatrix(opts.globalScale, -1);
        nworld = mParent->updateWorldMatrix(opts.globalScale,  1);
      }
    }
  }
  
  // add points
  for (int i=0, j=0; i<mNumPoints; ++i, j+=3)
  {
    //UT_Vector4 p(positions[j], positions[j+1], positions[j+2], (weights ? weights[i] : 1.0f));
    UT_Vector4 p(positions[j], positions[j+1], positions[j+2], 1.0f);
    
    if (!interpolated)
    {
      p.x() *= opts.globalScale;
      p.y() *= opts.globalScale;
      p.z() *= opts.globalScale;
      
      if (blend <= 0.0f)
      {
        p = p * world;
      }
      else
      {
        p = (1.0f - blend) * (p * pworld) + blend * (p * nworld);
      }
    }
    
#if UT_MAJOR_VERSION_INT >= 12
    GA_Offset po = gdp->appendPointOffset();
    gdp->setPos4(po, p);
    poffsets[i] = po;
#else
    GEO_Point *pp = gdp->appendPoint();
    pp->setPos(p);
#endif
  }
  
  // now add primitive and set points
  bool clamped = true;
  
  for (int i=0; i<mDegree; ++i)
  {
    if (mKnots[i] != mKnots[i+1] ||
        mKnots[mNumKnots-i-1] != mKnots[mNumKnots-i-2])
    {
      clamped = false;
      break;
    }
  }
  
  GU_PrimNURBCurve *crv = GU_PrimNURBCurve::build(gdp, mNumPoints, mDegree+1, (mForm!=1 ? 1 : 0), (clamped ? 1 : 0), 0);
  
  for (int i=0; i<mNumPoints; ++i)
  {
#if UT_MAJOR_VERSION_INT >= 12
    crv->setVertexPoint(i, poffsets[i]);
#else
    crv->getVertex(i).setPt(pl.entry(poffset + i));
#endif
  }
  
  if (weights)
  {
    crv->weights(1);
    for (int i=0; i<mNumPoints; ++i)
    {
      crv->setWeight(i, weights[i]);
    }
  }
  else
  {
    crv->weights(0);
  }
  
#if UT_MAJOR_VERSION_INT >= 12
  GA_NUBBasis *basis = new GA_NUBBasis(mKnots[0], mKnots[mNumKnots-1], mNumKnots, mDegree+1, clamped);
  GA_KnotVector &knots = basis->getKnotVector();
  for (int i=0; i<mNumKnots; ++i)
  {
    knots(i) = mKnots[i];
  }
  crv->setBasis(basis);
#else
  crv->setBasis(new GB_NUBBasis(mKnots, mNumKnots, mDegree+1, clamped));
#endif
  
  // Add instance number primitive attribute
#if UT_MAJOR_VERSION_INT >= 12
  GA_RWAttributeRef aInstNum = gdp->findAttribute(GA_ATTRIB_PRIMITIVE, GA_SCOPE_PUBLIC, "instance_num");
  if (!aInstNum.isValid())
  {
    aInstNum = gdp->addIntTuple(GA_ATTRIB_PRIMITIVE, GA_SCOPE_PUBLIC, "instance_num", 1);
  }
  aInstNum.getAIFTuple()->set(aInstNum.get(), gdp->primitiveOffset(gdp->getNumPrimitives()-1), mInstanceNum, 0);
#else
  GEO_AttributeHandle aInstNum = gdp->getAttribute(GEO_PRIMITIVE_DICT, "instance_num");
  if (!aInstNum.isAttributeValid())
  {
    gdp->addAttribute("instance_num", sizeof(int), GB_ATTRIB_INT, NULL, GEO_PRIMITIVE_DICT);
    aInstNum = gdp->getAttribute(GEO_PRIMITIVE_DICT, "instance_num");
  }
  aInstNum.setElement(crv);
  aInstNum.setI(mInstanceNum, 0);
#endif

  if (interpolated)
  {
    delete[] positions;
    if (weights)
    {
      delete[] weights;
    }
  }
  
  // add primitive to group
  if (pmgroup)
  {
    pmgroup->add(crv);
  }
  
  // only first parent (don't support instance for now)
  if (getParent() != 0)
  {
    reader->setParent(getName(), getParent()->getName());
  }
  
  return true;
}

void Curve::resolveParent(Reader *reader)
{
  if (mNumParents >= 1)
  {
    Object *o = reader->findObject(mParentNames[0]);
    if (o != 0)
    {
      setParent(dynamic_cast<Transform*>(o));
    }
  }

  Object::resolveParent(reader);
}

Transform* Curve::getParent() const
{
  return mParent;
}

void Curve::setParent(Transform *t)
{
  mParent = t;
  setParentObject(t);
}

bool Curve::switchParent(Reader *reader, size_t idx)
{
  if (Object::switchParent(reader, idx))
  {
    mParent = dynamic_cast<Transform*>(mParentObject);
    return true;
  }
  return false;
}

void Curve::cleanup()
{
  Object::cleanup();

  if (mPositions != 0)
  {
    delete[] mPositions;
  }
  if (mWeights != 0)
  {
    delete[] mWeights;
  }
  if (mKnots != 0)
  {
    delete[] mKnots;
  }
  
  mParent = 0;
  mNumPoints = 0;
  mPositions = 0;
  mWeights = 0;
  mDegree = 0;
  mNumKnots = 0;
  mKnots = 0;
  mForm = 0;
}


