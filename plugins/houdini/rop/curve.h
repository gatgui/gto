#ifndef __ROP_GtoExport_curve_h__
#define __ROP_GtoExport_curve_h__

#include <UT/UT_Version.h>
#include <UT/UT_Vector3.h>
#include <GU/GU_Detail.h>
#if UT_MAJOR_VERSION_INT >= 12
#  include <GA/GA_Group.h>
#else
#  include <GB/GB_Group.h>
#endif
#include "object.h"

class Transform;
class HouGtoWriter;

class Curve : public Object
{
public:

  Curve(Transform *p=0);
  virtual ~Curve();
  
#if UT_MAJOR_VERSION_INT >= 12
  virtual bool declare(HouGtoWriter *writer, GU_Detail *gdp=0, const std::vector<GA_Offset> &prims=NoPrimitives);
#else
  virtual bool declare(HouGtoWriter *writer, GU_Detail *gdp=0, const std::vector<unsigned int> &prims=NoPrimitives);
#endif
  virtual void write(HouGtoWriter *writer);
  
  virtual bool isValid();
  virtual void invalidate();
  virtual void update();
  virtual void mergeBounds(const UT_Matrix4 &space, UT_BoundingBox &box);
  
  
  void setName(const std::string &name);
  const std::string& getName() const;
  
  void setParent(Transform *t);
  Transform* getParent() const;
  
  void cleanup();

protected:

  Transform *mParent;
  //const GU_Detail *mGeo;
  //GB_PrimitiveGroup *mGroup;
  std::string mName;
  
  unsigned int mNumPoints;
  
  float mBBox[6];
  
  std::vector<float> mPositions;
  std::vector<float> mWeights;
  std::vector<float> mKnots;
  int mDegree;
  int mForm;
  
  bool mDirty;
};

#endif

