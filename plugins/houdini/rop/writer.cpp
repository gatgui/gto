#include "writer.h"
#include <Gto/Protocols.h>
#include <OP/OP_Director.h>
#include <GU/GU_Detail.h>
#if UT_MAJOR_VERSION_INT >= 12
#  include <GA/GA_Group.h>
#else
#  include <GB/GB_Group.h>
#endif
#include <OBJ/OBJ_Node.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_PrimPoly.h>
#include <GEO/GEO_PrimPart.h>
#include <GU/GU_PrimPart.h>
#include <GU/GU_PrimNURBCurve.h>
#include <SOP/SOP_Node.h>
#include <sstream>
#include <fstream>
#include <map>
#include <deque>
#include "object.h"
#include "mesh.h"
#include "transform.h"
#include "curve.h"
#include "particles.h"


HouGtoWriter::HouGtoWriter(float time, bool asDiff, float scale, bool ignorePrimGroups)
  : Gto::Writer(), mIsDiff(asDiff), mCtx(time), mErrStr(""), mGlobalScale(scale), mIgnorePrimitiveGroups(ignorePrimGroups)
{
}

HouGtoWriter::~HouGtoWriter()
{
  cleanup();
}

OP_Context& HouGtoWriter::getContext()
{
  return mCtx;
}

bool HouGtoWriter::isDiff() const
{
  return mIsDiff;
}

void HouGtoWriter::cleanup()
{
  for (size_t i=0; i<mObjects.size(); ++i)
  {
    delete mObjects[i];
  }
  mObjects.clear();
}

std::string HouGtoWriter::getUniqueName(const std::string &name, int *offset) const
{
  // first strip any trailing number out of name
  std::string base = name;
  while (base.length() > 0)
  {
    char lastChar = base[base.length()-1];
    if (lastChar >= '0' && lastChar <= '9')
    {
      base.erase(base.length()-1, 1);
    }
    else
    {
      break;
    }
  }

  std::string rv = name;
  unsigned int index = (offset ? *offset : 1);
  while (mObjectMap.find(rv) != mObjectMap.end())
  {
    std::ostringstream oss;
    oss << base << index++;
    rv = oss.str();
  }
  return rv;
}

bool HouGtoWriter::declareShapes(const GU_Detail *gdp, const std::string &name, Transform *parent)
{
  // only one mesh per detail (can have many details for same obj of primitive groups are created)
  
  std::deque<ShapeInfo> shapes;
  ShapeInfo *meshShape = 0;
  
#if UT_MAJOR_VERSION_INT >= 12
  const GA_Range &prr = gdp->getPrimitiveRange();
  
  for (GA_Iterator pit=prr.begin(); !pit.atEnd(); ++pit)
  {
    const GEO_Primitive *prim = gdp->getGEOPrimitive(pit.getOffset());
    
    const GEO_PrimPoly *poly = dynamic_cast<const GEO_PrimPoly*>(prim);
    if (poly != 0)
    {
      if (meshShape == 0)
      {
        shapes.push_back(ShapeInfo());
        meshShape = &(shapes.back());
        meshShape->name = name;
        meshShape->type = ShapeInfo::MESH;
        meshShape->prims.reserve(prr.getEntries());
      }
      
      meshShape->prims.push_back(pit.getOffset());
      continue;
    }
    
    const GU_PrimNURBCurve *crv = dynamic_cast<const GU_PrimNURBCurve*>(prim);
    if (crv != 0)
    {
      shapes.push_back(ShapeInfo());
      ShapeInfo &shape = shapes.back();
      shape.name = name;
      shape.type = ShapeInfo::CURVE;
      shape.prims.push_back(pit.getOffset());
      continue;
    }
    
    //const GEO_PrimParticle *parts = dynamic_cast<const GEO_PrimParticle*>(prim);
    const GU_PrimParticle *parts = dynamic_cast<const GU_PrimParticle*>(prim);
    if (parts != 0)
    {
      shapes.push_back(ShapeInfo());
      ShapeInfo &shape = shapes.back();
      shape.name = name;
      shape.type = ShapeInfo::PARTICLES;
      shape.prims.push_back(pit.getOffset());
      continue;
    }
  }
#else
  const GEO_PrimList &prl = gdp->primitives();
  
  for (unsigned int pi=0; pi<prl.entries(); ++pi)
  {
    const GEO_Primitive *prim = prl.entry(pi);
    
    const GEO_PrimPoly *poly = dynamic_cast<const GEO_PrimPoly*>(prim);
    if (poly != 0)
    {
      if (meshShape == 0)
      {
        shapes.push_back(ShapeInfo());
        meshShape = &(shapes.back());
        meshShape->name = name;
        meshShape->type = ShapeInfo::MESH;
        meshShape->prims.reserve(prl.entries());
      }
      
      meshShape->prims.push_back(pi);
      continue;
    }
    
    const GU_PrimNURBCurve *crv = dynamic_cast<const GU_PrimNURBCurve*>(prim);
    if (crv != 0)
    {
      shapes.push_back(ShapeInfo());
      ShapeInfo &shape = shapes.back();
      shape.name = name;
      shape.type = ShapeInfo::CURVE;
      shape.prims.push_back(pi);
      continue;
    }
    
    //const GEO_PrimParticle *parts = dynamic_cast<const GEO_PrimParticle*>(prim);
    const GU_PrimParticle *parts = dynamic_cast<const GU_PrimParticle*>(prim);
    if (parts != 0)
    {
      shapes.push_back(ShapeInfo());
      ShapeInfo &shape = shapes.back();
      shape.name = name;
      shape.type = ShapeInfo::PARTICLES;
      shape.prims.push_back(pi);
      continue;
    }
  }
#endif
  
  int shapeIndex = 1;
  
  for (size_t i=0; i<shapes.size(); ++i)
  {
    // Adjust name if needed
    shapes[i].name = getUniqueName(shapes[i].name, &shapeIndex);

    Transform *pobj = parent;

    if (shapes.size() > 1)
    {
      // also create transform
      std::string pname = shapes[i].name + "_xform";
      // shapes[i].name is unique
      
      pobj = new Transform(NULL, parent);
      pobj->setName(pname);
      
      if (!pobj->declare(this))
      {
        std::ostringstream oss;
        oss << "ROP_GtoExport: Could not declare transform \"" << pname << "\"";
        mErrStr = oss.str();
        delete pobj;
        cleanup();
        return false;
      }
      
      mObjects.push_back(pobj);
      mObjectMap[pname] = pobj;
    }
        
    Object *o = 0;
    
    if (shapes[i].type == ShapeInfo::MESH)
    {
      Mesh *m = new Mesh(pobj);
      m->setName(shapes[i].name);
      o = m;
    }
    else if (shapes[i].type == ShapeInfo::CURVE)
    {
      Curve *c = new Curve(pobj);
      c->setName(shapes[i].name);
      o = c;
    }
    else if (shapes[i].type == ShapeInfo::PARTICLES)
    {
      Particles *p = new Particles(pobj);
      p->setName(shapes[i].name);
      o = p;
    }
    else
    {
      continue;
    }
    
    if (!o->declare(this, (GU_Detail*)gdp, shapes[i].prims))
    {
      std::ostringstream oss;
      oss << "ROP_GtoExport: Could not declare shape \"" << name << "\"";
      mErrStr = oss.str();
      delete o;
      cleanup();
      return false;
    }
    
    mObjects.push_back(o);
    mObjectMap[shapes[i].name] = o;
  }
  
  return true;
}

bool HouGtoWriter::addSOP(const std::string &sopPath)
{
  mErrStr = "";
  
  OP_Director *director = OPgetDirector();
  
  if (!director)
  {
    mErrStr = "ROP_GtoExport: Could not get director";
    return false;
  }
  
  SOP_Node *node = director->getSOPNode(sopPath.c_str());
  
  if (!node)
  {
    std::ostringstream oss;
    oss << "ROP_GtoExport: Could not get SOP node \"" << sopPath << "\"";
    mErrStr = oss.str();
    return false;
  }
  
  const GU_Detail *gdp = node->getCookedGeo(mCtx);
  
  OP_ERROR errLvl = node->error();
  if (!gdp || errLvl >= UT_ERROR_ABORT)
  {
    std::ostringstream oss;
    oss << "ROP_GtoExport: SOP failed to cook";
    if (errLvl >= UT_ERROR_ABORT)
    {
      UT_String err;
      node->getErrorMessages(err);
      if (err.buffer() != 0)
      {
        oss << " -> \"" << err.toStdString() << "\"";
      }
    }
    mErrStr = oss.str();
    return false;
  }
  
  // Get details attribute holding hierarchy and transformations data
  
#if UT_MAJOR_VERSION_INT >= 12
  const GA_Attribute *attr;
#else
  const GB_Attribute *attr;
#endif
  
  if (!mIgnorePrimitiveGroups)
  {
    attr = GetStringAttrib(gdp, GetTransformsInfoKey(mCtx.getFloatFrame()));
    if (attr)
    {
      GetTransformsInfo(attr, mTInfo);
    }
    else
    {
      mTInfo.clear();
    }
    
    attr = GetStringAttrib(gdp, GetParentsInfoKey());
    if (attr)
    {
      GetParentsInfo(attr, mPInfo);
    }
    else
    {
      mPInfo.clear();
    }
  }
  else
  {
    mTInfo.clear();
    mPInfo.clear();
  }
  
#if UT_MAJOR_VERSION_INT >= 12
  const GA_ElementGroupTable &primGroups = gdp->primitiveGroups();
#else
  const GB_GroupList &primGroups = gdp->primitiveGroups();
#endif
  // only process non-internal groups
  
  cleanup();
  
  // Should not be the sop bu the obj node
  OBJ_Node *p = dynamic_cast<OBJ_Node*>(node->getParent());
  
  // Always create root transform for obj
  Transform *root = new Transform(p);;
  if (!root->declare(this))
  {
    std::ostringstream oss;
    oss << "ROP_GtoExport: Could not declare root node \"" << root->getName() << "\"";
    mErrStr = oss.str();
    delete root;
    return false;
  }
  mObjects.push_back(root);
  mObjectMap[root->getName()] = root;
  
  ParentsInfo::const_iterator pit = mPInfo.begin();
  
  while (pit != mPInfo.end())
  {
    declareTransforms(gdp, p, root, pit->first);
    ++pit;
  }

  /*
  if (mObjects.size() == 0)
  {
    root = new Transform(p);
    if (!root->declare(this))
    {
      std::ostringstream oss;
      oss << "ROP_GtoExport: Could not declare root node \"" << root->getName() << "\"";
      mErrStr = oss.str();
      delete root;
      return false;
    }
    mObjects.push_back(root);
    mObjectMap[root->getName()] = root;
  }
  else
  {
    root = (Transform*) mObjects[0];
  }
  */
  
  size_t numPrimitiveGroups=0;
#if UT_MAJOR_VERSION_INT >= 12
  GA_Group *grp;
  for (GA_GroupTable::iterator<GA_ElementGroup> it=primGroups.beginTraverse(); !it.atEnd(); ++it)
  {
    if (!it.group()->getInternal())
    {
      ++numPrimitiveGroups;
    }
  }
#else
  GB_Group *grp = primGroups.head();
  while (grp)
  {
    if (!grp->getInternal())
    {
      ++numPrimitiveGroups;
    }
    grp = grp->next();
  }
#endif
  if (numPrimitiveGroups == 0 || mIgnorePrimitiveGroups)
  {
    std::string name = node->getName().nonNullBuffer();
    
    if (!declareShapes(gdp, name, root))
    {
      return false;
    }
  }
  else
  {
    std::string name, pname;
    std::map<std::string, Object*>::iterator oit;
    Transform *pobj = 0;
    Transform *ntpobj = root; // parent of the new Transform node if we create any

#if UT_MAJOR_VERSION_INT >= 12
    for (GA_GroupTable::iterator<GA_ElementGroup> it=primGroups.beginTraverse(); !it.atEnd(); ++it)
    {
      grp = it.group();
#else
    grp = primGroups.head();
    while (grp)
    {
#endif
      pobj = 0;
      if (grp->getInternal())
      {
#if UT_MAJOR_VERSION_INT < 12
        grp = grp->next();
#endif
        continue;         
      }
      name = grp->getName();
      
      // houdini creates group for internal use, they usually start with _, skip them
      if (name.length() >= 1 && name[0] == '_')
      {
#if UT_MAJOR_VERSION_INT < 12
        grp = grp->next();
#endif
        continue;
      }
      
      pit = mPInfo.find(name);
      
      if (pit != mPInfo.end())
      {
        const std::set<std::string> &parents = pit->second;
        
        if (parents.size() > 0)
        {
          pname = *(parents.begin());
          
          oit = mObjectMap.find(pname);
          
          if (oit != mObjectMap.end())
          {
            pobj = dynamic_cast<Transform*>(oit->second);
          }
        }
      }
      
      if (!pobj)
      {
        // we don't have a parent
        // shall we create a transform
#if UT_MAJOR_VERSION_INT >= 12
        if (primGroups.entries() > 1)
#else
        if (primGroups.length() > 1)
#endif
        {
          // create a buffer transform
          pname = name;
          
          size_t p = pname.find("Shape");
          
          if (p != std::string::npos)
          {
            pname.replace(p, 5, "");
            pname = getUniqueName(pname);
          }
          else
          {
            //name += "Shape";
            pname += "_xform";
            pname = getUniqueName(pname);
          }
          
          pobj = new Transform(NULL, ntpobj);
          pobj->setName(pname);
          
          if (!pobj->declare(this))
          {
            std::ostringstream oss;
            oss << "ROP_GtoExport: Could not declare transform \"" << pname << "\"";
            mErrStr = oss.str();
            delete pobj;
            cleanup();
            return false;
          }
          
          mObjects.push_back(pobj);
          mObjectMap[pname] = pobj;
        }
        else
        {
          // only one primitive group, add directly under root
          pobj = ntpobj;
        }
      }
      
#if UT_MAJOR_VERSION_INT >= 12
      GU_Detail *sgdp = new GU_Detail((GU_Detail*)gdp, (GA_PrimitiveGroup*)grp);
#else
      GU_Detail *sgdp = new GU_Detail((GU_Detail*)gdp, (GB_PrimitiveGroup*)grp);
#endif
       
      bool rv = declareShapes(sgdp, name, pobj);
      
      delete sgdp;
      
      if (!rv)
      {
        return false;
      }
      
#if UT_MAJOR_VERSION_INT < 12
      grp = grp->next();
#endif
    }
  }
  
  return true;
}

Object* HouGtoWriter::declareTransforms(const GU_Detail *gdp, OBJ_Node *obj, Transform *root, const std::string &nname)
{
  std::map<std::string, Object*>::iterator oit = mObjectMap.find(nname);
  
  if (oit == mObjectMap.end())
  {
    UT_Matrix4 lmat;
    
    TransformsInfo::const_iterator tit;
    
    std::set<std::string>::const_iterator nit;
    
    const std::set<std::string> &parents = mPInfo[nname];
    
    Transform *parent = 0;
    
    
    // Object not yet declared
    
    // Check if it should be exported as a transform or not
    
#if UT_MAJOR_VERSION_INT >= 12
    const GA_PrimitiveGroup *grp = gdp->findPrimitiveGroup(nname.c_str());
#else
    const GB_PrimitiveGroup *grp = gdp->findPrimitiveGroup(nname.c_str());
#endif
    
    if (grp != 0)
    {
      // This should be a shape
      // -> Declare its parent transforms
      nit = parents.begin();
      while (nit != parents.end())
      {
        oit = mObjectMap.find(*nit);
        if (oit == mObjectMap.end())
        {
          parent = (Transform*) declareTransforms(gdp, obj, root, *nit);
        }
        else
        {
          parent = (Transform*) oit->second;
        }

        break; // No support for instancing yet
      }

      return NULL;
    }
    else
    {
      // it might also be the case here that a group has been deleted from original gto
      // in which case we don't want to create a transform...
      // this is not really a problem

#if UT_MAJOR_VERSION_INT >= 12
      if (gdp->primitiveGroups().entries() == 0)
#else
      if (gdp->primitiveGroups().length() == 0)
#endif
      {
        // there are no primitive groups... [all geometry in one shape]
        // this actually will not happen if we have the mTInfo and mPInfo data
        // as those come from the SOP_GtoImport which set primitiveGroups
      }
      else
      {
        // not matching primitive groups for node name, consider it a transform
      }
    }
    
    // Get local matrix
    
    tit = mTInfo.find(nname);
    
    if (tit == mTInfo.end())
    {
      lmat.identity();
    }
    else
    {
      lmat = tit->second;
    }

    // Apply global scale if necessary
    if (mGlobalScale != 1.0f)
    {
      UT_Vector3 T;
      lmat.getTranslates(T);
      T.x() *= mGlobalScale;
      T.y() *= mGlobalScale;
      T.z() *= mGlobalScale;
      lmat.setTranslates(T);
    }
    
    // Create parent(s) before
    
    nit = parents.begin();
    
    while (nit != parents.end())
    {
      oit = mObjectMap.find(*nit);
      if (oit == mObjectMap.end())
      {
        parent = (Transform*) declareTransforms(gdp, obj, root, *nit);
      }
      else
      {
        parent = (Transform*) oit->second;
      }
      
      // no instance support for now
      break;
    }
    
    //Transform *t = new Transform(parent ? NULL : obj, parent);
    Transform *t = new Transform(NULL, (parent ? parent : root));
    
    t->setName(nname);
    t->setLocal(lmat);
    
    // don't need to update yet [only declaring]
    
    if (!t->declare(this))
    {
      delete t;
      return NULL;
    }
    
    mObjects.push_back(t);
    mObjectMap[nname] = t;
    
    return t;
  }
  else
  {
    return oit->second;
  }
}

void HouGtoWriter::write()
{
  // NOTE: in the mObjects array, parents appear before their children
  //       so update will be done in the right order
  //       unnecessary update won't occur because of the dirty flags in Mesh and Transform
  //       update should not be called in write as the write call sequence is unknown at
  //       that point
  
  beginData();
  for (size_t i=0; i<mObjects.size(); ++i)
  {
    mObjects[i]->update();
    mObjects[i]->write(this);
  }
  endData();
}

