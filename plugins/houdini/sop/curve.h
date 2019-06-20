#ifndef __SOP_GtoImport_curve_h__
#define __SOP_GtoImport_curve_h__

#include "object.h"

class Transform;

class Curve : public Object
{
public:
  
  enum
  {
    // components
    READ_POINTS = OBJECT_COMP_MAX,
    READ_SURFACE,
    CURVE_COMP_MAX,
    // properties
    READ_POSITIONS = OBJECT_PROP_MAX,
    READ_WEIGHTS,
    READ_DEGREE,
    READ_KNOTS,
    READ_RANGE,
    READ_FORM,
    READ_BOUNDS,
    CURVE_PROP_MAX
  };
  
  Curve(const std::string &name, bool boundsOnly, bool asDiff=false);
  virtual ~Curve();
  
  virtual Gto::Reader::Request component(Reader *rdr,
                                         const std::string& name,
                                         const std::string& interp,
                                         const Gto::Reader::ComponentInfo &header);
  
  virtual Gto::Reader::Request property(Reader *rdr,
                                        const std::string& name,
                                        const std::string& interp,
                                        const Gto::Reader::PropertyInfo &header);
  
  virtual void* data(Reader *rdr, const Gto::Reader::PropertyInfo &header, size_t bytes);
  
  virtual void dataRead(Reader *rdr, const Gto::Reader::PropertyInfo &header);
  
  virtual bool toHoudini(Reader *reader,
                         OP_Context &ctx,
                         GU_Detail *gdp,
                         const ExpandOptions &opts,
                         Object *obj0=0,
                         Object *obj1=0,
                         float blend=0.0f,
                         float t0=0.0f,
                         float t1=0.0f);
  
  virtual void resolveParent(Reader *reader);
  
  
  Transform* getParent() const;
    
  void setParent(Transform *t);
  
  virtual bool switchParent(Reader *reader, size_t idx);
  
  void cleanup();
  
  inline UT_BoundingBoxF getBounds() const { return UT_BoundingBoxF(mBounds[0], mBounds[1], mBounds[2], mBounds[3], mBounds[4], mBounds[5]); }

protected:

  Transform *mParent;
  
  int mNumPoints;
  float *mPositions;
  float *mWeights;
  int mDegree;
  int mNumKnots;
  float *mKnots;
  float mRange[2];
  int mForm;

  bool mBoundsOnly;
  float mBounds[6];
};

#endif

