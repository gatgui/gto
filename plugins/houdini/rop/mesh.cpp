#include "writer.h"
#include "mesh.h"
#include "transform.h"
#include "log.h"
#include <Gto/Protocols.h>
#include <GEO/GEO_Point.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_PrimPoly.h>
#include <GEO/GEO_AttributeHandle.h>
#if UT_MAJOR_VERSION_INT >= 12
#  include <GA/GA_AttributeDict.h>
#else
#  include <GEO/GEO_PrimList.h>
#  include <GEO/GEO_PointList.h>
#  include <GEO/GEO_AttribDict.h>
#  include <GB/GB_AttributeTable.h>
#endif
#include <algorithm>
#include <iostream>
#include <set>

#define GTO_TRANSFORM_VERSION 2
#define GTO_POLYGON_VERSION 2
#define GTO_CATMULL_CLARK_VERSION 2
//#define GTO_NURBS_VERSION 1

// ---

#ifdef _DEBUG

static void PrintDict(const std::map<std::string, std::string> &dict)
{
  std::map<std::string, std::string>::const_iterator it = dict.begin();
  while (it != dict.end())
  {
    std::cout << it->first << " -> " << it->second << std::endl;
    ++it;
  }
}

static void PrintListDict(const std::map<std::string, std::vector<std::string> > &dict)
{
  std::map<std::string, std::vector<std::string> >::const_iterator it = dict.begin();
  while (it != dict.end())
  {
    std::cout << it->first << " -> [";
    if (it->second.size() > 0)
    {
      std::cout << it->second[0];
      for (size_t i=1; i<it->second.size(); ++i)
      {
        std::cout << ", " << it->second[i];
      }
    }
    std::cout << "]" << std::endl;
    ++it;
  }
}

#endif

// ---

Mesh::Mesh(Transform *p)
  : Object()
  , mParent(p)
  , mName("")
  , mNumFaceVertex(0)
  , mNumVertex(0)
  , mDirty(true)
{
  if (mParent)
  {
    mParent->addChild(this);
  }
}

Mesh::~Mesh()
{
  cleanup();
}

void Mesh::setName(const std::string &name)
{
  mName = name;
}

const std::string& Mesh::getName() const
{
  return mName;
}

void Mesh::setParent(Transform *t)
{
  mParent = t;
}

Transform* Mesh::getParent() const
{
  return mParent;
}

bool Mesh::isValid()
{
  return !mDirty;
}

void Mesh::invalidate()
{
  // NOOP?
}

void Mesh::update()
{
  if (!mDirty)
  {
    return;
  }
  
  // beware that houdini object transform shall not be included
  // in the parent world matrix at this time
  // (though it will appear added to the root transform in the gto file)
  
  UT_Matrix4 mat;
  UT_Vector4 v;
  UT_BoundingBox box;

  if (getParent() == 0)
  {
    mat.identity();
  }
  else
  {
    mat = getParent()->getWorld();
    mat.invert();
  }

  // NOTE: changes are irreversible... declare must be call again if there's any
  //       need to call several times update for the same object
  //       this shall not be an issue ATM
  
  // Transform points and compute bounding box

  box.makeInvalid();

  for (unsigned int i=0, j=0; i<mNumVertex; ++i, j+=3)
  {
    v.assign(mPositions[j], mPositions[j+1], mPositions[j+2], 1.0f);
    v = v * mat;
    
    box.enlargeBounds(v);
    
    mPositions[j] = v.x();
    mPositions[j+1] = v.y();
    mPositions[j+2] = v.z();
  }

  // Update bounding box

  mBBox[0] = box.xmin();
  mBBox[1] = box.ymin();
  mBBox[2] = box.zmin();
  mBBox[3] = box.xmax();
  mBBox[4] = box.ymax();
  mBBox[5] = box.zmax();
  
  // Also transform attributes

  mNormals.transformBy(mat, Attrib::TransformAsNormal);
  mVertexAttrib.transformBy(mat); // Attrib::TransformAsVector?
  mPointAttrib.transformBy(mat); // Attrib::TransformAsVector?
  mPrimAttrib.transformBy(mat); // Attrib::TransformAsVector?
  mDetailAttrib.transformBy(mat); // Attrib::TransformAsVector?

  mDirty = false;
}

void Mesh::mergeBounds(const UT_Matrix4 &space, UT_BoundingBox &box)
{
  UT_BoundingBox thisBox(mBBox[0], mBBox[1], mBBox[2], mBBox[3], mBBox[4], mBBox[5]);
  
  thisBox.transform(space);
  
  box.enlargeBounds(thisBox);
}

void Mesh::cleanup()
{
  mPositions.clear();
  mPolyVertexCount.clear();
  mPolyVertexIndices.clear();
  mVertexIndicesMap.clear();
  
  mColourSetNamesMap.clear();
  mUVSetNamesMap.clear();
  mUsedUVSets.clear();
  mUsedColourSets.clear();
  
  mVertexAttrib.clear();
  mPointAttrib.clear();
  mPrimAttrib.clear();
  mDetailAttrib.clear();
  mNormals.reset();
  mHardEdges.reset();

  mNumFaceVertex = 0;
  
  mNumVertex = 0;
}

void Mesh::findUVAndColourSetNames(GU_Detail *gdp)
{
  std::vector<std::string> elems0, elems1;
  
#if UT_MAJOR_VERSION_INT >= 12
  GA_ROAttributeRef rattr;
  
  GA_Attribute *attr;
  
  rattr = gdp->findGlobalAttribute("uvmap");
  if (rattr.isValid() && rattr.isString() && rattr.getTupleSize() == 1)
  {
    const char *tmp = rattr.getString(0, 0);
    std::string uvmap = (tmp ? tmp : "");
#else
  const GB_AttributeTable &gla = gdp->attribs();
  
  GB_Attribute *attr;  
  
  attr = gla.find("uvmap", GB_ATTRIB_INDEX);
  if (attr != 0 && attr->getIndex(0) != NULL)
  {
    std::string uvmap = attr->getIndex(0);
#endif
    Split(uvmap, ',', elems0);
    for (size_t i=0; i<elems0.size(); ++i)
    {
      Split(elems0[i], '=', elems1);
      if (elems1.size() == 2)
      {
        if (elems1[0] != "st")
        {
          mUVSetNamesMap[elems1[1]] = std::string(GTO_PROPERTY_UV_SET) + "_" + elems1[0];
        }
        else
        {
          mUVSetNamesMap[elems1[1]] = "st";
        }
      }
    }
  }
  else
  {
    std::string setn;
    int num;
    
#if UT_MAJOR_VERSION_INT >= 12
    for (GA_AttributeDict::iterator it=gdp->getAttributes().begin(GA_ATTRIB_VERTEX, GA_SCOPE_PUBLIC); !it.atEnd(); ++it)
    {
      attr = it.attrib();
#else
    GEO_VertexAttribDict &va = gdp->vertexAttribs();
    
    attr = va.getHead();    
    while (attr)
    {
#endif
      setn = attr->getName();
      if (setn == "uv")
      {
        mUVSetNamesMap[setn] = "st";
      }
      else if (sscanf(setn.c_str(), "uv%d", &num) == 1)
      {
        mUVSetNamesMap[setn] = std::string(GTO_PROPERTY_UV_SET) + "_" + setn;
      }
#if UT_MAJOR_VERSION_INT < 12
      attr = (GB_Attribute*) attr->next();
#endif
    }
  }
  
#if UT_MAJOR_VERSION_INT >= 12
  rattr = gdp->findGlobalAttribute("colmap");
  if (rattr.isValid() && rattr.isString() && rattr.getTupleSize() == 1)
  {
    const char *tmp = rattr.getString(0, 0);
    std::string colmap = (tmp ? tmp : "");
#else
  attr = gla.find("colmap", GB_ATTRIB_INDEX);
  if (attr != 0 && attr->getIndex(0) != NULL)
  {
    std::string colmap = attr->getIndex(0);
#endif
    Split(colmap, ',', elems0);
    for (size_t i=0; i<elems0.size(); ++i)
    {
      Split(elems0[i], '=', elems1);
      if (elems1.size() == 2)
      {
        if (elems1[0] != "colour")
        {
          mColourSetNamesMap[elems1[1]] = std::string(GTO_PROPERTY_COLOUR_SET) + "_" + elems1[0];
        }
        else
        {
          mColourSetNamesMap[elems1[1]] = "colour";
        }
      }
    }
  }
  else
  {
    int num;
    std::string setn;

#if UT_MAJOR_VERSION_INT >= 12
    for (GA_AttributeDict::iterator it=gdp->getAttributes().begin(GA_ATTRIB_VERTEX, GA_SCOPE_PUBLIC); !it.atEnd(); ++it)
    {
      attr = it.attrib();
#else
    GEO_VertexAttribDict &va = gdp->vertexAttribs();
    
    attr = va.getHead();    
    while (attr)
    {
#endif
      setn = attr->getName();
      if (setn == "col")
      {
        mColourSetNamesMap[setn] = "colour";
      }
      else if (sscanf(setn.c_str(), "col%d", &num) == 1)
      {
        mColourSetNamesMap[setn] = std::string(GTO_PROPERTY_COLOUR_SET) + "_" + setn;
      }
#if UT_MAJOR_VERSION_INT < 12
      attr = (GB_Attribute*) attr->next();
#endif
    }
  }
  
#if UT_MAJOR_VERSION_INT >= 12
  rattr = gdp->findGlobalAttribute("uvusage");
  if (rattr.isValid() && rattr.isString() && rattr.getTupleSize() == 1)
  {
    const char *tmp = rattr.getString(0, 0);
    std::string uvusage = (tmp ? tmp : "");
#else
  attr = gla.find("uvusage", GB_ATTRIB_INDEX);
  if (attr != 0 && attr->getIndex(0) != NULL)
  {
    std::string uvusage = attr->getIndex(0);
#endif
    Split(uvusage, ',', elems0);
    for (size_t i=0; i<elems0.size(); ++i)
    {
      Split(elems0[i], ' ', elems1);
      if (elems1.size() >= 2)
      {
        mUsedUVSets[elems1[0]].resize(elems1.size()-1);
        for (size_t j=1; j<elems1.size(); ++j)
        {
          mUsedUVSets[elems1[0]][j-1] = elems1[j];
        }
      }
    }
  }
  
#if UT_MAJOR_VERSION_INT >= 12
  rattr = gdp->findGlobalAttribute("colusage");
  if (rattr.isValid() && rattr.isString() && rattr.getTupleSize() == 1)
  {
    const char *tmp = rattr.getString(0, 0);
    std::string colusage = (tmp ? tmp : "");
#else
  attr = gla.find("colusage", GB_ATTRIB_INDEX);
  if (attr != 0 && attr->getIndex(0) != NULL)
    {
    std::string colusage = attr->getIndex(0);
#endif
    Split(colusage, ',', elems0);
    for (size_t i=0; i<elems0.size(); ++i)
    {
      Split(elems0[i], ' ', elems1);
      if (elems1.size() >= 2)
      {
        mUsedColourSets[elems1[0]].resize(elems1.size()-1);
        for (size_t j=1; j<elems1.size(); ++j)
        {
          mUsedColourSets[elems1[0]][j-1] = elems1[j];
        }
        }
      }
  }
  
  if (mUsedUVSets.find(mName) == mUsedUVSets.end())
  {
    std::vector<std::string> &names = mUsedUVSets[mName];
    names.resize(mUVSetNamesMap.size());
    
    std::map<std::string, std::string>::iterator it = mUVSetNamesMap.begin();
    size_t i = 0;
    
    while (it != mUVSetNamesMap.end())
    {
      names[i] = it->first;
      ++it;
      ++i;
    }
  }
    
  if (mUsedColourSets.find(mName) == mUsedColourSets.end())
  {
    std::vector<std::string> &names = mUsedColourSets[mName];
    names.resize(mColourSetNamesMap.size());

    std::map<std::string, std::string>::iterator it = mColourSetNamesMap.begin();
    size_t i = 0;

    while (it != mColourSetNamesMap.end())
    {
      names[i] = it->first;
      ++it;
      ++i;
    }
  }

#ifdef _DEBUG
  PrintDict(mUVSetNamesMap);
  PrintListDict(mUsedUVSets);

  PrintDict(mColourSetNamesMap);
  PrintListDict(mUsedColourSets);
#endif
}

#if UT_MAJOR_VERSION_INT >= 12
bool Mesh::declare(HouGtoWriter *writer, GU_Detail *gdp, const std::vector<GA_Offset> &validPrimitives)
#else
bool Mesh::declare(HouGtoWriter *writer, GU_Detail *gdp, const std::vector<unsigned int> &validPrimitives)
#endif
{
  if (!gdp)
  {
    return false;
  }
  
  cleanup();

  float globalScale = writer->getGlobalScale();
  
#if UT_MAJOR_VERSION_INT >= 12
  
  std::vector<GA_Offset> polyVertexOffsets;
  
  mPolyVertexCount.resize(validPrimitives.size());
  polyVertexOffsets.reserve(validPrimitives.size() * 4); // rough estimate to avoid too many re-allocations
  
  for (size_t i=0; i<validPrimitives.size(); ++i)
  {
    const GEO_Primitive *poly = gdp->getGEOPrimitive(validPrimitives[i]);
#else  
  const GEO_PointList &ptl = gdp->points();
  const GEO_PrimList &prl = gdp->primitives();
  
  mPolyVertexCount.resize(validPrimitives.size());
  mPolyVertexIndices.reserve(validPrimitives.size() * 4); // rough estimate to avoid too many re-allocations
  
  for (size_t i=0; i<validPrimitives.size(); ++i)
  {
    const GEO_PrimPoly *poly = (GEO_PrimPoly*) prl.entry(validPrimitives[i]);
#endif
    
    unsigned int nv = poly->getVertexCount();
    mPolyVertexCount[i] = nv;
    mNumFaceVertex += nv;
    
    for (unsigned int j=0; j<nv; ++j)
    {
#if UT_MAJOR_VERSION_INT >= 12
      GA_Offset poff = poly->getPointOffset(nv-1-j);
      
      // is there a way to max this size?
      polyVertexOffsets.push_back(poff);
#else
      const GEO_Point *pt = poly->getVertex(nv-1-j).getPt();
      
      unsigned int idx = pt->getNum();
      
      if (idx >= ptl.entries())
      {
        std::cout << "*** Invalid index" << std::endl;
      }
      
      // is there a way to max this size?
      mPolyVertexIndices.push_back((int)idx);
#endif
    }
  }
  
  if (mPolyVertexCount.size() > 0)
  {
#if UT_MAJOR_VERSION_INT >= 12
    std::vector<GA_Offset> validPoints;
    std::vector<GA_Offset> sortedIndices(polyVertexOffsets);
    GA_Offset index, prevIndex;
    prevIndex = GA_INVALID_OFFSET;
#else
    std::vector<unsigned int> validPoints;
    std::vector<int> sortedIndices(mPolyVertexIndices);
    int index, prevIndex;
    prevIndex = -1;
#endif
    int numPositions;
    UT_Vector4 pos;
    
    // will this work with T=GA_Offset?
    std::sort(sortedIndices.begin(), sortedIndices.end());
    
    numPositions = 0;
    
    // maximum expected size
    validPoints.reserve(sortedIndices.size());
    mPositions.reserve(3 * sortedIndices.size());
    
    for (size_t i=0; i<sortedIndices.size(); ++i)
    {
      index = sortedIndices[i];
      
      if (index == prevIndex)
      {
        continue;
      }
      
      validPoints.push_back(index);
      
#if UT_MAJOR_VERSION_INT >= 12
      pos = gdp->getPos4(index);
#else
      pos = ptl[index]->getPos();
#endif
      
      mVertexIndicesMap[index] = numPositions;
      
      ++numPositions;

      mPositions.push_back(pos.x() * globalScale);
      mPositions.push_back(pos.y() * globalScale);
      mPositions.push_back(pos.z() * globalScale);
      
      prevIndex = index;
    }
    
    // Now reorder indices
#if UT_MAJOR_VERSION_INT >= 12
    mPolyVertexIndices.resize(polyVertexOffsets.size());
    for (size_t i=0; i<polyVertexOffsets.size(); ++i)
    {
      index = polyVertexOffsets[i];
      mPolyVertexIndices[i] = mVertexIndicesMap[index];
    }
    polyVertexOffsets.clear();
#else
    for (size_t i=0; i<mPolyVertexIndices.size(); ++i)
    {
      index = mPolyVertexIndices[i];
      mPolyVertexIndices[i] = mVertexIndicesMap[index];
    }
#endif
    
    mNumVertex = mVertexIndicesMap.size();
    
    findUVAndColourSetNames(gdp);

#ifdef _DEBUG
    Log::Print("Build hard edges list...");
#endif
      
    // Hard edges
    
      
#if UT_MAJOR_VERSION_INT >= 12
    GA_RWAttributeRef hedge = gdp->findVertexAttribute("hardEdge");
    if (hedge.isValid())
#else
    GEO_AttributeHandle hedge;
    hedge = gdp->getAttribute(GEO_VERTEX_DICT, "hardEdge");
    if (hedge.isAttributeValid())
#endif
    {
      std::set<std::pair<int, int> > edges;
      std::pair<int, int> edge;
    
      for (size_t i=0; i<validPrimitives.size(); ++i)
      {
#if UT_MAJOR_VERSION_INT >= 12
        GEO_Primitive *prim = gdp->getGEOPrimitive(validPrimitives[i]);
#else
        GEO_Primitive *prim = gdp->primitives()[validPrimitives[i]];
#endif     
        unsigned int nv = prim->getVertexCount();
        
        for (unsigned int v=0; v<nv; ++v)
        {
          unsigned int iv1 = (v + 1) % nv;
#if UT_MAJOR_VERSION_INT >= 12
          int e0 = 0;
          GA_Offset v0 = prim->getVertexOffset(v);
          //GA_Offset v1 = prim->getVertexOffset(iv1);
          hedge.getAIFTuple()->get(hedge.getAttribute(), v0, e0, 0);
          if (e0 != 0)
          {
            GA_Offset ip0 = prim->getPointOffset(v);
            GA_Offset ip1 = prim->getPointOffset(iv1);
#else
          const GEO_Vertex &v0 = prim->getVertex(v);
          const GEO_Vertex &v1 = prim->getVertex(iv1);
          hedge.setElement(&v0);
          if (hedge.getI(0) != 0)
          {
            int ip0 = v0.getPt()->getNum();
            int ip1 = v1.getPt()->getNum();
#endif
            // get remapped point indices
            int i0 = mVertexIndicesMap[ip0];
            int i1 = mVertexIndicesMap[ip1];
            
            edge.first = (i0 < i1 ? i0 : i1);
            edge.second = (i0 < i1 ? i1 : i0);
            
            if (edges.find(edge) == edges.end())
            {
              edges.insert(edge);
            }
          }
        }
      }
      
      if (edges.size() > 0)
      {
        mHardEdges.init(Gto::Int, 2);
        mHardEdges.resize(edges.size());
        int *values = (int*) mHardEdges.data();
        std::set<std::pair<int, int> >::iterator it = edges.begin();
        size_t e = 0;
        while (it != edges.end())
        {
          values[e] = it->first;
          values[e+1] = it->second;
          e += 2;
          ++it;
        }
      }
    }

    AttribDict::iterator ait;

#ifdef _DEBUG
    Log::Print("Collecting vertex attributes...");
#endif
    mVertexAttrib.addIgnore("hardEdge");
    mVertexAttrib.collectVertices(gdp, validPrimitives);
    // Normals
    ait = mVertexAttrib.find("N");
    if (ait != mVertexAttrib.end())
    {
#ifdef _DEBUG
      Log::Print("  Found N");
      Log::Print("    %s", ait->second.second.info().c_str());
#endif
      std::swap(ait->second.second, mNormals);
      if (mNormals.getType() == Gto::Float && mNormals.getDim() == 3)
      {
        mNormals.buildSparseMapping<Gto::Float, 3>(mPolyVertexCount);
      }
      else
      {
        mNormals.buildDenseMapping(mPolyVertexCount);
      }
      mVertexAttrib.erase(ait);
    }
    // UV sets
    std::vector<std::string> &uvSetNames = mUsedUVSets[mName];
    for (size_t uvi=0; uvi<uvSetNames.size(); ++uvi)
    {
      ait = mVertexAttrib.find(uvSetNames[uvi]);
      if (ait == mVertexAttrib.end())
      {
        continue;
      }
#ifdef _DEBUG
      Log::Print("  Found UV set %s", uvSetNames[uvi].c_str());
#endif
      Attrib &attr = ait->second.second;
      if ((attr.getDim() == 2 || attr.getDim() == 3) && attr.getType() == Gto::Float)
      {
        if (attr.getDim() == 3)
        {
          /*
#ifdef _DEBUG
          Log::Print("    Convert dim from 3 to 2");
#endif
          int nuv = attr.getSize();
          std::vector<unsigned char> uvs(nuv * 2 * sizeof(float));
          float *sptr = (float*) attr.data();
          float *dptr = (float*) &uvs[0];
          for (int i=0, j=0, k=0; i<nuv; ++i, j+=2, k+=3)
          {
            dptr[j] = sptr[k];
            dptr[j+1] = sptr[k+1];
          }
          attr.swapData(uvs, nuv, 2);
          */
          attr.setDim(2);
        }
#ifdef _DEBUG
        Log::Print("    Build sparse mappings");
#endif
        attr.buildSparseMapping<Gto::Float, 2>(mPolyVertexCount);
        std::map<std::string, std::string>::iterator nit =  mUVSetNamesMap.find(uvSetNames[uvi]);
        std::string uvSetName = (nit != mUVSetNamesMap.end() ? nit->second : uvSetNames[uvi]);
        std::swap(ait->second, mUVSets[uvSetName]);
      }
      mVertexAttrib.erase(ait);
    }
    // Colour sets
    std::vector<std::string> &colSetNames = mUsedColourSets[mName];
    for (size_t csi=0; csi<colSetNames.size(); ++csi)
    {
      ait = mVertexAttrib.find(colSetNames[csi]);
      if (ait == mVertexAttrib.end())
      {
        continue;
      }
#ifdef _DEBUG
      Log::Print("  Found color set %s", colSetNames[csi].c_str());
#endif
      Attrib &attr = ait->second.second;
      if (attr.getDim() == 3 && attr.getType() == Gto::Float)
      {
#ifdef _DEBUG
        Log::Print("    Build dense mappings");
#endif
        attr.buildDenseMapping(mPolyVertexCount);
        std::map<std::string, std::string>::iterator nit =  mColourSetNamesMap.find(colSetNames[csi]);
        std::string colSetName = (nit != mColourSetNamesMap.end() ? nit->second : colSetNames[csi]);
        std::swap(ait->second, mColourSets[colSetName]);
      }
      mVertexAttrib.erase(ait);
    }
    if (globalScale != 1.0f)
    {
      mVertexAttrib.scaleBy(globalScale);
    }

#ifdef _DEBUG
    Log::Print("Collect poiny attributes...");
#endif
    mPointAttrib.addIgnore("P");
    if (mNormals.isValid())
    {
#ifdef _DEBUG
      Log::Print("  Ignore N as it was found in vertex attributes");
#endif
      mPointAttrib.addIgnore("N");
    }
    mPointAttrib.collectPoints(gdp, validPoints, false);
    ait = mPointAttrib.find("N");
    if (ait != mPointAttrib.end())
    {
#ifdef _DEBUG
      Log::Print("  Found N");
      Log::Print("    %s", ait->second.second.info().c_str());
#endif
      std::swap(ait->second.second, mNormals);
      // copy points mapping
#ifdef _DEBUG
      Log::Print("  Copy vertex position mappings");
#endif
      mNormals.mapping().assign(mPolyVertexIndices.begin(), mPolyVertexIndices.end());
      //mNormals.buildDenseMapping(mPolyVertexCount);
      mPointAttrib.erase(ait);
    }
    if (globalScale != 1.0f)
    {
      mPointAttrib.scaleBy(globalScale);
    }
    // Compute velocity min/max/mag
    mExportVelocityInfo = false;
    mMinVelocity[0] = std::numeric_limits<float>::max();
    mMinVelocity[1] = mMinVelocity[0];
    mMinVelocity[2] = mMinVelocity[0];
    mMaxVelocity[0] = -std::numeric_limits<float>::max();
    mMaxVelocity[1] = mMaxVelocity[0];
    mMaxVelocity[2] = mMaxVelocity[0];
    mMaxVelocityMag = 0.0f;
    ait = mPointAttrib.find(GTO_PROPERTY_VELOCITY);
    if (ait == mPointAttrib.end())
    {
      ait = mPointAttrib.find("v");
    }
    if (ait != mPointAttrib.end())
    {
      Attrib &va = ait->second.second;
      
      if (va.getDim() == 3)
      {
        if (va.getType() == Gto::Float)
        {
          float *vv = (float*) va.data();
          for (int i=0, j=0; i<va.getSize(); ++i, j+=3)
          {
            float vx = vv[j];
            float vy = vv[j+1];
            float vz = vv[j+2];
            float mag = float(sqrt(vx*vx + vy*vy + vz*vz));
            if (vx < mMinVelocity[0]) mMinVelocity[0] = vx;
            if (vy < mMinVelocity[1]) mMinVelocity[1] = vy;
            if (vz < mMinVelocity[2]) mMinVelocity[2] = vz;
            if (vx > mMaxVelocity[0]) mMaxVelocity[0] = vx;
            if (vy > mMaxVelocity[1]) mMaxVelocity[1] = vy;
            if (vz > mMaxVelocity[2]) mMaxVelocity[2] = vz;
            if (mag > mMaxVelocityMag) mMaxVelocityMag = mag;
          }
          mExportVelocityInfo = true;
        }
        else if (va.getType() == Gto::Double)
        {
          double *vv = (double*) va.data();
          for (int i=0, j=0; i<va.getSize(); ++i, j+=3)
          {
            float vx = float(vv[j]);
            float vy = float(vv[j+1]);
            float vz = float(vv[j+2]);
            float mag = float(sqrt(vx*vx + vy*vy + vz*vz));
            if (vx < mMinVelocity[0]) mMinVelocity[0] = vx;
            if (vy < mMinVelocity[1]) mMinVelocity[1] = vy;
            if (vz < mMinVelocity[2]) mMinVelocity[2] = vz;
            if (vx > mMaxVelocity[0]) mMaxVelocity[0] = vx;
            if (vy > mMaxVelocity[1]) mMaxVelocity[1] = vy;
            if (vz > mMaxVelocity[2]) mMaxVelocity[2] = vz;
            if (mag > mMaxVelocityMag) mMaxVelocityMag = mag;
          }
          mExportVelocityInfo = true;
        }
      }
    }

    // no specifics
#ifdef _DEBUG
    Log::Print("Collect primitive attributes...");
#endif
    mPrimAttrib.collectPrimitives(gdp, validPrimitives);
    if (globalScale != 1.0f)
    {
      mPrimAttrib.scaleBy(globalScale);
    }
    // apply global scale if necessary (data read by collectPrimitives)
    
#ifdef _DEBUG
    Log::Print("Collect detail attributes...");
#endif
    mDetailAttrib.addIgnore("varmap");
    mDetailAttrib.addIgnore("colmap");
    mDetailAttrib.addIgnore("uvmap");
    mDetailAttrib.addIgnore("uvusage");
    mDetailAttrib.addIgnore("colusage");
    mDetailAttrib.addIgnore("parenting");
    mDetailAttrib.collectGlobals(gdp);
    ait = mDetailAttrib.begin();
    while (ait != mDetailAttrib.end())
    {
      int fframe, sframe;
      if (sscanf(ait->first.c_str(), "transforms_%d_%d", &fframe, &sframe) == 2)
      {
#ifdef _DEBUG
        Log::Print("  Filter out attribute %s", ait->first.c_str());
#endif
        mDetailAttrib.erase(ait);
        ait = mDetailAttrib.begin();
      }
      else
      {
        ++ait;
      }
    }
    if (globalScale != 1.0f)
    {
      mDetailAttrib.scaleBy(globalScale);
    }

    // if no normals found, use internal ones
    //if (!mNormals.isValid() || mNormals.isEmpty())
    if (!mNormals.isValid())
    {
#ifdef _DEBUG
      Log::Print("No normals found, use Houdini internal (per point) normals");
      Log::Print("  mNormals: %s", mNormals.info().c_str());
#endif
      UT_Vector3 nrm;
      
      mNormals.init(Gto::Float, 3, "vector");
      mNormals.resize(numPositions);
      float *nattr = (float*) mNormals.data();
      
      for (size_t i=0, off=0; i<validPoints.size(); ++i, off+=3)
      {
#if UT_MAJOR_VERSION_INT >= 12
        gdp->computeNormalInternal(validPoints[i], nrm);
#else
        gdp->computeNormalInternal(*(ptl[validPoints[i]]), nrm);
#endif
        nattr[off+0] = nrm.x();
        nattr[off+1] = nrm.y();
        nattr[off+2] = nrm.z();
      }
      mNormals.mapping().assign(mPolyVertexIndices.begin(), mPolyVertexIndices.end());
    }

    writer->intern(mName.c_str());
    
    writer->beginObject(mName.c_str(), GTO_PROTOCOL_POLYGON, GTO_POLYGON_VERSION);
    
    writer->beginComponent(GTO_COMPONENT_POINTS);
    writer->property(GTO_PROPERTY_POSITION, Gto::Float, numPositions, 3);
    writer->endComponent();
    
    if (mNormals.isValid())
    {
      writer->beginComponent(GTO_COMPONENT_NORMALS);
      mNormals.declareGto(writer, GTO_PROPERTY_NORMAL);
      writer->endComponent(); 
    }
    if (!writer->isDiff() && mHardEdges.isValid())
    {
      writer->beginComponent(GTO_COMPONENT_FLAGS);
      mHardEdges.declareGto(writer, GTO_PROPERTY_HARDEDGE);
      writer->endComponent(); 
    }
    
    if (!writer->isDiff())
    {
      writer->beginComponent(GTO_COMPONENT_ELEMENTS);
      writer->property(GTO_PROPERTY_SIZE, Gto::Short, mPolyVertexCount.size(), 1);
      writer->endComponent();
      
      if (mUVSets.size() > 0)
      {
        writer->beginComponent(GTO_COMPONENT_MAPPINGS);
        mUVSets.declareGto(writer);
        writer->endComponent();
      }
      
      if (mColourSets.size() > 0)
      {
        writer->beginComponent(GTO_COMPONENT_COLOURS);
        mColourSets.declareGto(writer);
        writer->endComponent();
      }
    }

    if (!writer->isDiff() || mNormals.isValid())
    {
      writer->beginComponent(GTO_COMPONENT_INDICES);
      if (!writer->isDiff())
      {
        writer->property(GTO_PROPERTY_VERTEX, Gto::Int, mPolyVertexIndices.size(), 1);
        mUVSets.declareGtoMapping(writer);
        mColourSets.declareGtoMapping(writer);
      }
      if (mNormals.isValid())
      {
        mNormals.declareGtoMapping(writer, GTO_PROPERTY_NORMAL);
      }
      writer->endComponent();
    }
      
    //if (!writer->isDiff())
    {
      writer->beginComponent(GTO_COMPONENT_VERTEX_ATTRIBS);
      mVertexAttrib.declareGto(writer);
      writer->endComponent();

      writer->beginComponent(GTO_COMPONENT_POINT_ATTRIBS);
      mPointAttrib.declareGto(writer);
      writer->endComponent();

      writer->beginComponent(GTO_COMPONENT_PRIMITIVE_ATTRIBS);
      mPrimAttrib.declareGto(writer);
      writer->endComponent();

      writer->beginComponent(GTO_COMPONENT_OBJECT_ATTRIBS);
      mDetailAttrib.declareGto(writer);
      if (mExportVelocityInfo)
      {
        writer->property("minVelocity", Gto::Float, 1, 3);
        writer->property("maxVelocity", Gto::Float, 1, 3);
        writer->property("maxVelocityMag", Gto::Float, 1, 1);
      }
      writer->endComponent();
    }
    
    writer->beginComponent(GTO_COMPONENT_OBJECT);
    writer->property(GTO_PROPERTY_BOUNDINGBOX, Gto::Float, 1, 6);
    if (writer->isDiff())
    {
      writer->intern(GTO_PROTOCOL_DIFFERENCE);
      writer->property(GTO_PROPERTY_PROTOCOL, Gto::String, 1, 1);
    }
    else
    {
      // What about instances?
      if (mParent != 0)
      {
        writer->property(GTO_PROPERTY_PARENT, Gto::String, 1, 1);
      }
    }
    writer->endComponent();
    
    writer->endObject();
  }
  
  return true;
}

void Mesh::write(HouGtoWriter *writer)
{
  if (mPolyVertexCount.size() > 0)
  {
    writer->propertyData(&mPositions[0]);
    
    if (mNormals.isValid())
    {
      mNormals.outputGto(writer);
    }
    if (!writer->isDiff() && mHardEdges.isValid())
    {
      mHardEdges.outputGto(writer);
    }
        
    // mappings
    if (!writer->isDiff())
    {
      writer->propertyData(&mPolyVertexCount[0]);
      mUVSets.outputGto(writer);
      mColourSets.outputGto(writer);
    }
    
    // indices
    if (!writer->isDiff() || mNormals.isValid())
    {
      if (!writer->isDiff())
      {
        writer->propertyData(&mPolyVertexIndices[0]);
        mUVSets.outputGtoMapping(writer);
        mColourSets.outputGtoMapping(writer);
      }
      if (mNormals.isValid())
      {
        mNormals.outputGtoMapping(writer);
      }
    }
      
    //if (!writer->isDiff())
    {
      mVertexAttrib.outputGto(writer);
      mPointAttrib.outputGto(writer);
      mPrimAttrib.outputGto(writer);
      mDetailAttrib.outputGto(writer);
      if (mExportVelocityInfo)
      {
        writer->propertyData(mMinVelocity);
        writer->propertyData(mMaxVelocity);
        writer->propertyData(&mMaxVelocityMag);
      }
    }
    
    writer->propertyData(mBBox);
    
    if (writer->isDiff())
    {
      int protocol = writer->lookup(GTO_PROTOCOL_DIFFERENCE);
      writer->propertyData(&protocol);
    }
    else
    {
      if (mParent != 0)
      {
        int name = writer->lookup(mParent->getName());
        writer->propertyData(&name);
      }
    }
  }
}

