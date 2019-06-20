#include "mesh.h"
#include "reader.h"
#include "transform.h"
#include "utils.h"
#include "bounds.h"
#include <GU/GU_PrimPoly.h>
//#include <GEO/GEO_AttributeHandle.h>
#include <UT/UT_Version.h>
#include <CH/CH_Manager.h>
#include <OP/OP_Director.h>
#include <Gto/Protocols.h>
#include <algorithm>


Mesh::Mesh(const std::string &name, bool boundsOnly, bool asDiff)
  : Object(name, asDiff)
  , mParent(0)
  , mNumPolygons(0)
  , mNumPolyVertices(0)
  , mPolyVertexCount(0)
  , mNumVertices(0)
  , mPolyVertexIndices(0)
  , mPositions(0)
#ifdef SUPPORTS_NORMALS
  , mNumNormals(0)
  , mPolyNormalIndices(0)
  , mNormals(0)
#endif
#ifdef SUPPORTS_HARDEDGES
  , mNumHardEdges(0)
  , mHardEdges(0)
#endif
  , mBoundsOnly(boundsOnly)
  , mMaxVelocityMag(0.0f)
{
  mMirrorFlag = 1;
  mMinVelocity[0] = std::numeric_limits<float>::max();
  mMinVelocity[1] = mMinVelocity[0];
  mMinVelocity[2] = mMinVelocity[0];
  mMaxVelocity[0] = -std::numeric_limits<float>::max();
  mMaxVelocity[1] = mMaxVelocity[0];
  mMaxVelocity[2] = mMaxVelocity[0];
}


Mesh::~Mesh()
{
  cleanup();
}

Gto::Reader::Request Mesh::component(Reader *rdr,
                                     const std::string& name,
                                     const std::string& interp,
                                     const Gto::Reader::ComponentInfo &header)
{
  if (mBoundsOnly)
  {
    // also read object attribs for velocity info
    if (name == GTO_COMPONENT_OBJECT_ATTRIBS)
    {
      return Gto::Reader::Request(true, (void*) READ_DETAILATTRIBS);
    }
    return Object::component(rdr, name, interp, header);
  }

  if (name == GTO_COMPONENT_ELEMENTS)
  {
    return Gto::Reader::Request(!isDiff(), (void*) READ_ELEMENTS);
  }
  else if (name == GTO_COMPONENT_POINTS)
  {
    return Gto::Reader::Request(true, (void*) READ_POINTS);
  }
#ifdef SUPPORTS_NORMALS
  else if (name == GTO_COMPONENT_NORMALS)
  {
    return Gto::Reader::Request(true, (void*) READ_NORMALS);
  }
#endif
#ifdef SUPPORTS_UVS
  else if (name == GTO_COMPONENT_MAPPINGS)
  {
    return Gto::Reader::Request(!isDiff(), (void*) READ_MAPPINGS);
  }
#endif
#ifdef SUPPORTS_COLOURS
  else if (name == GTO_COMPONENT_COLOURS)
  {
    return Gto::Reader::Request(!isDiff(), (void*) READ_COLOURS);
  }
#endif
#ifdef SUPPORTS_CREASES
  else if (name == GTO_COMPONENT_CREASES)
  {
    return Gto::Reader::Request(!isDiff(), (void*) READ_CREASES);
  }
#endif
#ifdef SUPPORTS_HARDEDGES
  else if (name == GTO_COMPONENT_FLAGS)
  {
    return Gto::Reader::Request(!isDiff(), (void*) READ_FLAGS);
  }
#endif
  else if (name == GTO_COMPONENT_INDICES)
  {
    return Gto::Reader::Request(true, (void*) READ_INDICES);
  }
#ifdef SUPPORTS_ATTRIBS
  else if (name == GTO_COMPONENT_POINT_ATTRIBS)
  {
    //return Gto::Reader::Request(!isDiff(), (void*) READ_POINTATTRIBS);
    return Gto::Reader::Request(true, (void*) READ_POINTATTRIBS);
  }
  else if (name == GTO_COMPONENT_PRIMITIVE_ATTRIBS)
  {
    //return Gto::Reader::Request(!isDiff(), (void*) READ_PRIMITIVEATTRIBS);
    return Gto::Reader::Request(true, (void*) READ_PRIMITIVEATTRIBS);
  }
  else if (name == GTO_COMPONENT_VERTEX_ATTRIBS)
  {
    //return Gto::Reader::Request(!isDiff(), (void*) READ_VERTEXATTRIBS);
    return Gto::Reader::Request(true, (void*) READ_VERTEXATTRIBS);
  }
  else if (name == GTO_COMPONENT_OBJECT_ATTRIBS)
  {
    //return Gto::Reader::Request(!isDiff(), (void*) READ_DETAILATTRIBS);
    return Gto::Reader::Request(true, (void*) READ_DETAILATTRIBS);
  }
#endif
  
  return Object::component(rdr, name, interp, header);
}

Gto::Reader::Request Mesh::property(Reader *rdr,
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
    }
    else if (what == (void*) READ_DETAILATTRIBS)
    {
      if (name == "minVelocity" || name == "maxVelocity" || name == "maxVelocityMag")
      {
        return Gto::Reader::Request(true, (void*) name.c_str());
      }
    }
    
    return Object::property(rdr, name, interp, header);
  }

  if (what == (void*) READ_ELEMENTS)
  {
    return Gto::Reader::Request((name == GTO_PROPERTY_SIZE), (void*) READ_POLY_VERTEX_COUNT);
  }
  else if (what == (void*) READ_POINTS)
  {
    return Gto::Reader::Request((name == GTO_PROPERTY_POSITION), (void*) READ_VERTEX_VALUES);
  }
#ifdef SUPPORTS_NORMALS
  else if (what == (void*) READ_NORMALS)
  {
    return Gto::Reader::Request((name == GTO_PROPERTY_NORMAL), (void*) READ_NORMAL_VALUES);
  }
#endif
#ifdef SUPPORTS_UVS
  else if (what == (void*) READ_MAPPINGS)
  {
    std::string uvSetName = "";
    if (name == GTO_PROPERTY_ST)
    {
      uvSetName = "st";
    }
    else if (name.find(GTO_PROPERTY_UV_SET) != std::string::npos)
    {
      uvSetName = name.substr(strlen(GTO_PROPERTY_UV_SET));
    }
    long idx = getUVSetIndex(uvSetName);
    return Gto::Reader::Request(idx >= 0, (void*) (READ_UV_VALUES + idx));
  }
#endif
#ifdef SUPPORTS_COLOURS
  else if (what == (void*) READ_COLOURS)
  {
    std::string colourSetName = "";
    if (name == GTO_PROPERTY_COLOUR)
    {
      colourSetName = "colour";
    }
    else if (name.find(GTO_PROPERTY_COLOUR_SET) != std::string::npos)
    {
      colourSetName = name.substr(strlen(GTO_PROPERTY_COLOUR_SET));
    }
    long idx = getColourSetIndex(colourSetName);
    return Gto::Reader::Request(idx >= 0, (void*) (READ_COLOUR_VALUES + idx));
  }
#endif
#ifdef SUPPORTS_CREASES
  else if (what == (void*) READ_CREASES)
  {
    if (name == GTO_PROPERTY_VERTEX_CREASE)
    {
      return Gto::Reader::Request(true, (void*) READ_VCREASE_VALUES);
    }
    else if (name == GTO_PROPERTY_EDGE_CREASE)
    {
      return Gto::Reader::Request(true, (void*) READ_ECREASE_VALUES);
    }
  }
#endif
#ifdef SUPPORTS_HARDEDGES
  else if (what == (void*) READ_FLAGS)
  {
    return Gto::Reader::Request((name == GTO_PROPERTY_HARDEDGE), (void*) READ_HARDEDGE_VALUES);
  }
#endif
  else if (what == (void*) READ_INDICES)
  {
    if (isDiff())
    {
      // only read normal indices (if any) in diff mode
#ifdef SUPPORTS_NORMALS
      if (name == GTO_PROPERTY_NORMAL)
      {
        return Gto::Reader::Request(true, (void*) READ_NORMAL_INDICES);
      }
#endif
    }
    else
    {
      if (name == GTO_PROPERTY_VERTEX)
      {
        return Gto::Reader::Request(true, (void*) READ_VERTEX_INDICES);
      }
      else if (name == GTO_PROPERTY_NORMAL)
      {
        return Gto::Reader::Request(true, (void*) READ_NORMAL_INDICES);
      }
#ifdef SUPPORTS_UVS
      else if (name == GTO_PROPERTY_ST)
      {
        long idx = getUVSetIndex("st");
        return Gto::Reader::Request(idx >= 0, (void*) (READ_UV_INDICES + idx));
      }
      else if (name.find(GTO_PROPERTY_UV_SET) != std::string::npos)
      {
        long idx = getUVSetIndex(name.substr(strlen(GTO_PROPERTY_UV_SET)));
        return Gto::Reader::Request(idx >= 0, (void*) (READ_UV_INDICES + idx));
      }
#endif
#ifdef SUPPORTS_COLOURS
      else if (name == GTO_PROPERTY_COLOUR)
      {
        long idx = getColourSetIndex("colour");
        return Gto::Reader::Request(idx >= 0, (void*) (READ_COLOUR_INDICES + idx));
      }
      else if (name.find(GTO_PROPERTY_COLOUR_SET) != std::string::npos)
      {
        long idx = getColourSetIndex(name.substr(strlen(GTO_PROPERTY_COLOUR_SET)));
        return Gto::Reader::Request(idx >= 0, (void*) (READ_COLOUR_INDICES + idx));
      }
#endif
#ifdef SUPPORTS_CREASES
      else if (name == GTO_PROPERTY_VERTEX_CREASE)
      {
        return Gto::Reader::Request(true, (void*) READ_VCREASE_INDICES);
      }
      else if (name == GTO_PROPERTY_EDGE_CREASE)
      {
        return Gto::Reader::Request(true, (void*) READ_ECREASE_INDICES);
      }
#endif
    }
  }
#ifdef SUPPORTS_ATTRIBS
  else if (what == (void*) READ_POINTATTRIBS)
  {
    return Gto::Reader::Request(true, (void*) name.c_str());
  }
  else if (what == (void*) READ_PRIMITIVEATTRIBS)
  {
    return Gto::Reader::Request(true, (void*) name.c_str());
  }
  else if (what == (void*) READ_VERTEXATTRIBS)
  {
    return Gto::Reader::Request(true, (void*) name.c_str());
  }
  else if (what == (void*) READ_DETAILATTRIBS)
  {
    return Gto::Reader::Request(true, (void*) name.c_str());
  }
#endif
  
  return Object::property(rdr, name, interp, header);
}

void* Mesh::data(Reader *rdr, const Gto::Reader::PropertyInfo &header, size_t bytes)
{
  if (mBoundsOnly)
  {
    if (header.propertyData == (void*) READ_BOUNDS)
    {
      if (header.width != 6 || header.size != 1 || header.type != Gto::Float)
      {
        return NULL;
      }
      return mBounds;
    }
    else if (header.component->componentData == (void*) READ_DETAILATTRIBS)
    {
      std::string pname = (const char*) header.propertyData;
      if (pname == "minVelocity")
      {
        if (header.width == 3 && header.size == 1 && header.type == Gto::Float)
        {
          return mMinVelocity;
        }
      }
      else if (pname == "maxVelocity")
      {
        if (header.width == 3 && header.size == 1 && header.type == Gto::Float)
        {
          return mMaxVelocity;
        }
      }
      else if (pname == "maxVelocityMag")
      {
        if (header.width == 1 && header.size == 1 && header.type == Gto::Float)
        {
          return &mMaxVelocityMag;
        }
      }
    }

    return Object::data(rdr, header, bytes);
  }

  //bool asVector = false;
  
#ifdef SUPPORTS_ATTRIBS
  if (header.component->componentData == (void*) READ_POINTATTRIBS)
  {
    std::string pname = (const char*) header.propertyData;
    std::string interp = rdr->stringFromId(header.interpretation);
    AttribDictEntry &entry = mPointAttrib[pname];
    if (entry.second.init((Gto::DataType) header.type, header.width, interp))
    {
      entry.second.resize(header.size);
      return entry.second.data();
    }
    else
    {
      mPointAttrib.erase(mPointAttrib.find(pname));
      return NULL;
    }
  }
  else if (header.component->componentData == (void*) READ_PRIMITIVEATTRIBS)
  {
    std::string pname = (const char*) header.propertyData;
    std::string interp = rdr->stringFromId(header.interpretation);
    AttribDictEntry &entry = mPrimitiveAttrib[pname];
    if (entry.second.init((Gto::DataType) header.type, header.width, interp))
    {
      entry.second.resize(header.size);
      return entry.second.data();
    }
    else
    {
      mPrimitiveAttrib.erase(mPrimitiveAttrib.find(pname));
      return NULL;
    }
  }
  else if (header.component->componentData == (void*) READ_VERTEXATTRIBS)
  {
    std::string pname = (const char*) header.propertyData;
    std::string interp = rdr->stringFromId(header.interpretation);
    AttribDictEntry &entry = mVertexAttrib[pname];
    if (entry.second.init((Gto::DataType) header.type, header.width, interp))
    {
      entry.second.resize(header.size);
      return entry.second.data();
    }
    else
    {
      mVertexAttrib.erase(mVertexAttrib.find(pname));
      return NULL;
    }
  }
  else if (header.component->componentData == (void*) READ_DETAILATTRIBS)
  {
    std::string pname = (const char*) header.propertyData;
    if (pname == "minVelocity")
    {
      if (header.width == 3 && header.size == 1 && header.type == Gto::Float)
      {
        return mMinVelocity;
      }
    }
    else if (pname == "maxVelocity")
    {
      if (header.width == 3 && header.size == 1 && header.type == Gto::Float)
      {
        return mMaxVelocity;
      }
    }
    else if (pname == "maxVelocityMag")
    {
      if (header.width == 1 && header.size == 1 && header.type == Gto::Float)
      {
        return &mMaxVelocityMag;
      }
    }
    else
    {
      std::string interp = rdr->stringFromId(header.interpretation);
      AttribDictEntry &entry = mDetailAttrib[pname];
      if (entry.second.init((Gto::DataType) header.type, header.width, interp))
      {
        entry.second.resize(header.size);
        return entry.second.data();
      }
      else
      {
        mDetailAttrib.erase(mDetailAttrib.find(pname));
        return NULL;
      }
    }
  }
#endif
  else if (header.propertyData == (void*) READ_POLY_VERTEX_COUNT)
  {
    if (header.width != 1 || header.type != Gto::Short)
    {
      return NULL;
    }
    mNumPolygons = header.size;
    mPolyVertexCount = new unsigned short[mNumPolygons];
    return mPolyVertexCount;
  }
  else if (header.propertyData == (void*) READ_VERTEX_VALUES)
  {
    if (header.width != 3 || header.type != Gto::Float)
    {
      return NULL;
    }
    mNumVertices = header.size;
    mPositions = new float[3 * mNumVertices];
    return mPositions;
  }
  else if (header.propertyData == (void*) READ_VERTEX_INDICES)
  {
    if (header.width != 1 || header.type != Gto::Int)
    {
      return NULL;
    }
    // header.size should match mPolyVertexCount
    mPolyVertexIndices = new int[header.size];
    return mPolyVertexIndices;
  }
#ifdef SUPPORTS_NORMALS
  else if (header.propertyData == (void*) READ_NORMAL_VALUES)
  {
    if (header.width != 3 || header.type != Gto::Float)
    {
      return NULL;
    }
    mNumNormals = header.size;
    mNormals = new float[3 * mNumNormals];
    return mNormals;
  }
  else if (header.propertyData == (void*) READ_NORMAL_INDICES)
  {
    if (header.width != 1 || header.type != Gto::Int)
    {
      return NULL;
    }
    // header.size should match mPolyVertexCount
    mPolyNormalIndices = new int[header.size];
    return mPolyNormalIndices;
  }
#endif
#ifdef SUPPORTS_UVS
  else if (header.propertyData >= (void*) READ_UV_INDICES && 
           header.propertyData <  (void*) (READ_UV_INDICES + MAX_UVS))
  {
    if (header.width != 1 || header.type != Gto::Int)
    {
      return NULL;
    }
    long idx = long(header.propertyData) - READ_UV_INDICES;
    if (idx >= mUVSetNames.size())
    {
      return NULL;
    }
    // header.size should match mPolyVertexCount
    UVSet &uvSet = mUVSets[mUVSetNames[idx]];
    uvSet.indices = new int[header.size];
    return uvSet.indices;
  }
  else if (header.propertyData >= (void*) READ_UV_VALUES && 
           header.propertyData <  (void*) (READ_UV_VALUES + MAX_UVS))
  {
    if (header.width != 2 || header.type != Gto::Float)
    {
      return NULL;
    }
    long idx = long(header.propertyData) - READ_UV_VALUES;
    if (idx >= mUVSetNames.size())
    {
      return NULL;
    }
    UVSet &uvSet = mUVSets[mUVSetNames[idx]];
    uvSet.numUVs = header.size;
    uvSet.UVs = new float[2 * header.size];
    return uvSet.UVs;
  }
#endif
#ifdef SUPPORTS_COLOURS
  else if (header.propertyData >= (void*) READ_COLOUR_INDICES && 
           header.propertyData <  (void*) (READ_COLOUR_INDICES + MAX_COLOURS))
  {
    if (header.width != 1 || header.type != Gto::Int)
    {
      return NULL;
    }
    long idx = long(header.propertyData) - READ_COLOUR_INDICES;
    if (idx >= mColourSetNames.size())
    {
      return NULL;
    }
    // header.size should match mPolyVertexCount
    ColourSet &colourSet = mColourSets[mColourSetNames[idx]];
    colourSet.indices = new int[header.size];
    return colourSet.indices;
  }
  else if (header.propertyData >= (void*) READ_COLOUR_VALUES && 
           header.propertyData <  (void*) (READ_COLOUR_VALUES + MAX_COLOURS))
  {
    if ((header.width != 3 && header.width != 4) || header.type != Gto::Float)
    {
      return NULL;
    }
    long idx = long(header.propertyData) - READ_COLOUR_VALUES;
    if (idx >= mColourSetNames.size())
    {
      return NULL;
    }
    ColourSet &colourSet = mColourSets[mColourSetNames[idx]];
    colourSet.numColours = header.size;
    colourSet.numComponents = header.width;
    colourSet.colours = new float[header.width * header.size];
    return colourSet.colours;
  }
#endif
#ifdef SUPPORTS_CREASES
  else if (header.propertyData == (void*) READ_VCREASE_INDICES)
  {
    // TODO
  }
  else if (header.propertyData == (void*) READ_ECREASE_INDICES)
  {
    // TODO
  }
  else if (header.propertyData == (void*) READ_VCREASE_VALUES)
  {
    // TODO
  }
  else if (header.propertyData == (void*) READ_ECREASE_VALUES)
  {
    // TODO
  }
#endif
#ifdef SUPPORTS_HARDEDGES
  else if (header.propertyData == (void*) READ_HARDEDGE_VALUES)
  {
    if (header.width != 2 || header.type != Gto::Int)
    {
      return NULL;
    }
    mNumHardEdges = header.size;
    mHardEdges = new int[header.size * 2];
    return mHardEdges;
  }
#endif
  
  return Object::data(rdr, header, bytes);
}

void Mesh::dataRead(Reader *rdr, const Gto::Reader::PropertyInfo &header)
{
  if (!mBoundsOnly)
  {
    if (header.propertyData == (void*) READ_POLY_VERTEX_COUNT)
    {
      mNumPolyVertices = 0;
      for (int i=0; i<mNumPolygons; ++i)
      {
        mNumPolyVertices += mPolyVertexCount[i];
      }
    }
  }
  
  Object::dataRead(rdr, header);
}

bool Mesh::toHoudini(Reader *reader,
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
    // if obj0 and obj1 are NULL...
    // pass on minVelocity, maxVelocity, maxVelocityMag?
    float padding = 0.0f;
    
    if (!obj0 && !obj1 && reader->isFrame())
    {
      float fps = OPgetDirector()->getChannelManager()->getSamplesPerSec();
      // velocity is in units per seconds
      // do no apply global scale yet, GenerateBox will apply it on padded box
      // we divide by current fps to get padding in units for a full frame worth of motion blur
      padding = mMaxVelocityMag / fps;
    }
    
    return GenerateBox<Mesh>(reader, ctx, gdp, opts, this, mInstanceNum, obj0, obj1, blend, t0, t1, padding);
  }

  UT_Matrix4 world(1.0f);
  UT_Matrix4 pworld(1.0f), nworld(1.0f);
  
  if (mNumPolygons <= 0)
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
  std::vector<GA_Offset> poffsets(mNumVertices);
  std::vector<GA_Offset> pmoffsets(mNumPolygons);
  std::vector<GA_Offset> voffsets(mNumPolyVertices);
  std::vector<GEO_Primitive*> pml(mNumPolygons);
#else
  GEO_PointList &pl = gdp->points();
  GEO_PrimList &pml = gdp->primitives();
  size_t poffset = pl.entries();
  size_t pmoffset = pml.entries();
#endif
  
  // compute interpolated positions and normals if necessary
  float *positions = mPositions;
#ifdef SUPPORTS_NORMALS
  float *normals = (mNumNormals > 0 ? mNormals : 0);
#endif
  bool interpolated = false;
  
  if (obj0 && ((Mesh*)obj0)->mNumVertices == mNumVertices)
  {
    if (obj1 && ((Mesh*)obj1)->mNumVertices == mNumVertices)
    {
      float a = 1.0f - blend;
      float b = blend;
      
      interpolated = true;
      
      UT_Matrix4 w0(1.0f);
      UT_Matrix4 w1(1.0f);
      
      if (getName() != opts.shapeName && !opts.ignoreTransforms)
      {
        if (mParent)
        {
          w0 = mParent->updateWorldMatrix(opts.globalScale, -1);
          w1 = mParent->updateWorldMatrix(opts.globalScale,  1);
        }
      }
      float *p0 = ((Mesh*)obj0)->mPositions;
      float *p1 = ((Mesh*)obj1)->mPositions;
      
      positions = new float[mNumVertices * 3];
      
      for (int i=0, j=0; i<mNumVertices; ++i, j+=3)
      {
        UT_Vector4 pp(p0[j] * opts.globalScale, p0[j+1] * opts.globalScale, p0[j+2] * opts.globalScale, 1.0f);
        pp = pp * w0;
        
        UT_Vector4 np(p1[j] * opts.globalScale, p1[j+1] * opts.globalScale, p1[j+2] * opts.globalScale, 1.0f);
        np = np * w1;
        
        positions[j] = a * pp.x() + b * np.x();
        positions[j+1] = a * pp.y() + b * np.y();
        positions[j+2] = a * pp.z() + b * np.z();
      }
      
#ifdef SUPPORTS_NORMALS
      // only interpolate normals if we have matching count
      // -> we might different mappings from frame to frame (and different normals count then)
      // -> should we handle that also? interpolation is much less straightforward then
      if (mNumNormals > 0 &&
          ((Mesh*)obj0)->mNumNormals == mNumNormals &&
          ((Mesh*)obj1)->mNumNormals == mNumNormals)
      {
        float *n0 = ((Mesh*)obj0)->mNormals;
        float *n1 = ((Mesh*)obj1)->mNormals;
        
        normals = new float[mNumNormals * 3];
        
        for (int i=0, j=0; i<mNumNormals; ++i, j+=3)
        {
          UT_Vector4 pn(n0[j], n0[j+1], n0[j+2], 0.0f);
          //pn = pn * w0;
          pn.rowVecMult3(w0);
          
          UT_Vector4 nn(n1[j], n1[j+1], n1[j+2], 0.0f);
          //nn = nn * w1;
          nn.rowVecMult3(w1);
          
          normals[j] = a * pn.x() + b * nn.x();
          normals[j+1] = a * pn.y() + b * nn.y();
          normals[j+2] = a * pn.z() + b * nn.z();
        }
      }
#endif
    }
    else
    {
      positions = ((Mesh*)obj0)->mPositions;
#ifdef SUPPORTS_NORMALS
      normals = ((Mesh*)obj0)->mNormals;
#endif
      
      if (getName() != opts.shapeName && !opts.ignoreTransforms)
      {
        if (mParent)
        {
          world = mParent->updateWorldMatrix(opts.globalScale, -1);
        }
      }
    }
  }
  else
  {
    if (getName() != opts.shapeName && !opts.ignoreTransforms)
    {
      if (mParent)
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
  }


  //
  //invert normal if minus scale has matrix with instance
  UT_Matrix4 idsm(1);
  UT_Matrix4 pidsm(1);
  UT_Matrix4 nidsm(1);

  mMirrorFlag.x() = ((world[0][0] >= 0 ) ? 1 : -1 );
  mMirrorFlag.y() = ((world[1][1] >= 0 ) ? 1 : -1 );
  mMirrorFlag.z() = ((world[2][2] >= 0 ) ? 1 : -1 );
  int mMirrorAlign = 1;
  if (opts.computeMirror)
  {
    mMirrorAlign = mMirrorFlag.x() * mMirrorFlag.y() * mMirrorFlag.z();
  
    idsm[0][0] = ((world[0][0] >= 0 ) ? 1 : -1 );
    idsm[1][1] = ((world[1][1] >= 0 ) ? 1 : -1 );
    idsm[2][2] = ((world[2][2] >= 0 ) ? 1 : -1 );
  
    pidsm[0][0] = ((pworld[0][0] >= 0 ) ? 1 : -1 );
    pidsm[1][1] = ((pworld[1][1] >= 0 ) ? 1 : -1 );
    pidsm[2][2] = ((pworld[2][2] >= 0 ) ? 1 : -1 );
  
    nidsm[0][0] = ((nworld[0][0] >= 0 ) ? 1 : -1 );
    nidsm[1][1] = ((nworld[1][1] >= 0 ) ? 1 : -1 );
    nidsm[2][2] = ((nworld[2][2] >= 0 ) ? 1 : -1 );
  }  
  
  // add points
  for (int i=0, j=0; i<mNumVertices; ++i, j+=3)
  {
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
  
  if (interpolated)
  {
    delete[] positions;
  }

#ifdef SUPPORTS_HARDEDGES
  std::map<EdgeInfo, std::vector<int> > edgeFaces;
#endif
  
  // add polygons
  for (int i=0, j=0; i<mNumPolygons; ++i)
  {
    int nv = mPolyVertexCount[i];
    
    GEO_PrimPoly *pp = GU_PrimPoly::build(gdp, nv, GU_POLY_CLOSED, 0);
#if UT_MAJOR_VERSION_INT >= 12
    pml[i] = pp;
    pmoffsets[i] = gdp->primitiveOffset(gdp->getNumPrimitives()-1);
#endif

    // Houdini expects polygon vertices to be defined in clockwise order (WTF!!!????)
    if (mMirrorAlign >= 0)
    {
      for (int k=nv-1, l=0; k>=0; --k, ++l)
      {
        int v0 = mPolyVertexIndices[j+k];
        int v1 = mPolyVertexIndices[j+(k-1 < 0 ? nv-1 : k-1)];
        //int v1 = mPolyVertexIndices[j+(k+1)%nv];
        
#ifdef SUPPORTS_HARDEDGES
        EdgeInfo edge(v0, v1);
        edgeFaces[edge].push_back(i);
#endif
        
#if UT_MAJOR_VERSION_INT >= 12
        pp->setVertexPoint(l, poffsets[v0]);
        // voffsets will be used to transfer gto vertex data to houdini vertex data
        // do not use l but k (k is the gto poly vertex index, l is the houdini one)
        voffsets[j+k] = pp->getVertexOffset(l);
#else
        pp->getVertex(l).setPt(pl.entry(poffset + v0));
#endif
      }
    }
    else
    {
      for (int k=0, l=0; k<nv; ++k, ++l)
      {
        int v0 = mPolyVertexIndices[j+k];
        int v1 = mPolyVertexIndices[j+(k-1 < 0 ? nv-1 : k-1)];
        //int v1 = mPolyVertexIndices[j+(k+1)%nv];
        
#ifdef SUPPORTS_HARDEDGES
        EdgeInfo edge(v0, v1);
        edgeFaces[edge].push_back(i);
#endif
        
#if UT_MAJOR_VERSION_INT >= 12
        pp->setVertexPoint(l, poffsets[v0]);
        // voffsets will be used to transfer gto vertex data to houdini vertex data
        // do not use l but k (k is the gto poly vertex index, l is the houdini one)
        voffsets[j+k] = pp->getVertexOffset(l);
#else
        pp->getVertex(l).setPt(pl.entry(poffset + v0));
#endif
        }
    }
    
    if (pmgroup)
    {
      pmgroup->add(pp);
    }
    
    j += nv;
  }

#ifdef SUPPORTS_NORMALS
  // add normals [need to transform them]
  if (normals != 0)
  {
#if UT_MAJOR_VERSION_INT >= 12
    GA_RWAttributeRef nh = gdp->findVertexAttribute("N");
    if (!nh.isValid())
    {
      float ndef[3] = {0.0f, 0.0f, 0.0f};
      GA_Defaults defVal(ndef, 3);
      nh = gdp->addFloatTuple(GA_ATTRIB_VERTEX, GA_SCOPE_PUBLIC, "N", 3, defVal);
      nh.getAttribute()->setTypeInfo(GA_TYPE_NORMAL);
    }
#else
    GEO_AttributeHandle nh = gdp->getVertexAttribute("N");
    if (!nh.isAttributeValid())
    {
      float ndef[3] = {0.0f, 0.0f, 0.0f};
      gdp->addVertexAttrib("N", 3*sizeof(float), GB_ATTRIB_VECTOR, (void*)ndef);
      nh = gdp->getVertexAttribute("N");
    }
#endif
    
    for (int i=0, j=0; i<mNumPolygons; ++i)
    {
      int nv = mPolyVertexCount[i];
      
#if UT_MAJOR_VERSION_INT >= 12
      GEO_Primitive *prim = pml[i];
#else
      GEO_Primitive *prim = pml.entry(pmoffset + i);
#endif

      if (mMirrorAlign >= 0)
      {
        //for (int k=0; k<nv; ++k)
        for (int k=nv-1, l=0; k>=0; --k, ++l)
        {              
          // this are the reference indices, is this ok for interpolated normals?
          int bi = mPolyNormalIndices[j+k] * 3;
          
          UT_Vector4 n(normals[bi], normals[bi+1], normals[bi+2], 0.0f);
          
          if (!interpolated)
          {
            if (blend <= 0.0f)
            {
              //n = n * world;
              //n.rowVecMult3(world);
              n.rowVecMult3(world*idsm);
            }
            else
            {
              //n = (1.0f - blend) * (n * pworld) + blend * (n * nworld);
              //n = (1.0f - blend) * rowVecMult3(n, pworld) + blend * rowVecMult3(n, nworld);
              n = (1.0f - blend) * rowVecMult3(n, pworld*pidsm) + blend * rowVecMult3(n, nworld*nidsm);
            }
          }
          else
          {
             n.rowVecMult3(idsm);
          }
            
#if UT_MAJOR_VERSION_INT >= 12
          nh.getAIFTuple()->set(nh.getAttribute(), prim->getVertexOffset(l), n.x(), 0);
          nh.getAIFTuple()->set(nh.getAttribute(), prim->getVertexOffset(l), n.y(), 1);
          nh.getAIFTuple()->set(nh.getAttribute(), prim->getVertexOffset(l), n.z(), 2);
#else
          nh.setElement(&(prim->getVertex(l)));
          nh.setF(n.x(), 0);
          nh.setF(n.y(), 1);
          nh.setF(n.z(), 2);
#endif
        }
      }
      else
      {
        for (int k=0, l=0; k<nv; ++k, ++l)
        {              
          // this are the reference indices, is this ok for interpolated normals?
          int bi = mPolyNormalIndices[j+k] * 3;
          
          UT_Vector4 n(normals[bi], normals[bi+1], normals[bi+2], 0.0f);

          if (!interpolated)
          {
            if (blend <= 0.0f)
            {
              //n = n * world;
              //n.rowVecMult3(world);
              n.rowVecMult3(world*idsm);
            }
            else
            {
              //n = (1.0f - blend) * (n * pworld) + blend * (n * nworld);
              //n = (1.0f - blend) * rowVecMult3(n, pworld) + blend * rowVecMult3(n, nworld);
              n = (1.0f - blend) * rowVecMult3(n, pworld*pidsm) + blend * rowVecMult3(n, nworld*nidsm);
            }
          }
          else
          {
             n.rowVecMult3(idsm);
          }
          
#if UT_MAJOR_VERSION_INT >= 12
          nh.getAIFTuple()->set(nh.getAttribute(), prim->getVertexOffset(l), n.x(), 0);
          nh.getAIFTuple()->set(nh.getAttribute(), prim->getVertexOffset(l), n.y(), 1);
          nh.getAIFTuple()->set(nh.getAttribute(), prim->getVertexOffset(l), n.z(), 2);
#else
          nh.setElement(&(prim->getVertex(l)));
          nh.setF(n.x(), 0);
          nh.setF(n.y(), 1);
          nh.setF(n.z(), 2);
#endif 
        }
      }
      
      j += nv;
    }
    
    if (interpolated)
    {
      delete[] normals;
    }
  }
#endif // SUPPORTS_NORMALS

#ifdef SUPPORTS_UVS
  // add uvs
  if (mUVSetNames.size() > 0)
  {
#if UT_MAJOR_VERSION_INT >= 12
    GA_Attribute *uvmap = GetOrCreateStringAttrib(gdp, "uvmap");
    GA_Attribute *uvusage = GetOrCreateStringAttrib(gdp, "uvusage");
#else
    GB_Attribute *uvmap = GetOrCreateStringAttrib(gdp, "uvmap");
    GB_Attribute *uvusage = GetOrCreateStringAttrib(gdp, "uvusage");
#endif
    
    std::string useduvs = getName();
    
    std::map<std::string, std::string> mappedNames;
    
    getMappedNames(uvmap, mappedNames);
    
    for (size_t i=0; i<mUVSetNames.size(); ++i)
    {
      UVSet &uvSet = mUVSets[mUVSetNames[i]];
      
      if (uvSet.numUVs <= 0)
      {
        continue;
      }
      
      std::string hname;
      
      std::map<std::string, std::string>::iterator it = mappedNames.find(mUVSetNames[i]);
      if (it != mappedNames.end())
      {
        hname = it->second;
      }
      else
      {
#if UT_MAJOR_VERSION_INT >= 12
        hname = newVertexAttribName(gdp, GA_STORECLASS_FLOAT, "uv");
#else
        hname = newVertexAttribName(gdp, GB_ATTRIB_FLOAT, "uv");
#endif
        addNameMapping(uvmap, mUVSetNames[i], hname);
      }
      
      useduvs += " " + hname;
      
#if UT_MAJOR_VERSION_INT >= 12
      GA_RWAttributeRef uvh = gdp->findVertexAttribute(hname.c_str());
      if (!uvh.isValid())
      {
        float def[3] = {0.0f, 0.0f, 0.0f};
        GA_Defaults defVal(def, 3);
        uvh = gdp->addFloatTuple(GA_ATTRIB_VERTEX, GA_SCOPE_PUBLIC, hname.c_str(), 3, defVal);
      }
#else
      GEO_AttributeHandle uvh = gdp->getVertexAttribute(hname.c_str());
      if (!uvh.isAttributeValid())
      {
        float def[3] = {0.0f, 0.0f, 0.0f};
        gdp->addVertexAttrib(hname.c_str(), 3*sizeof(float), GB_ATTRIB_FLOAT, (void*)def);
        uvh = gdp->getVertexAttribute(hname.c_str());
      }
#endif
      
      for (int p=0, l=0; p<mNumPolygons; ++p)
      {
        int nv = mPolyVertexCount[p];
        
#if UT_MAJOR_VERSION_INT >= 12
        GEO_Primitive *prim = pml[p];
#else
        GEO_Primitive *prim = pml.entry(pmoffset + p);
#endif
           
        //for (int k=0; k<nv; ++k)
        for (int k=nv-1, m=0; k>=0; --k, ++m)
        {             
          int bi = uvSet.indices[l+k] * 2;
          
#if UT_MAJOR_VERSION_INT >= 12
          uvh.getAIFTuple()->set(uvh.getAttribute(), prim->getVertexOffset(m), uvSet.UVs[bi], 0);
          uvh.getAIFTuple()->set(uvh.getAttribute(), prim->getVertexOffset(m), uvSet.UVs[bi+1], 1);
          uvh.getAIFTuple()->set(uvh.getAttribute(), prim->getVertexOffset(m), 0.0f, 2);
#else
          uvh.setElement(&(prim->getVertex(m)));
          uvh.setF(uvSet.UVs[bi], 0);
          uvh.setF(uvSet.UVs[bi+1], 1);
          uvh.setF(0.0f, 2);
#endif
        }
        
        l += nv;
      }
    }
    
    addUsage(uvusage, useduvs);
  }
#endif  // SUPPORTS_UVS

#ifdef SUPPORTS_COLOURS
  // add colours
  if (mColourSetNames.size() > 0)
  {
#if UT_MAJOR_VERSION_INT >= 12
    GA_Attribute *colmap = GetOrCreateStringAttrib(gdp, "colmap");
    GA_Attribute *colusage = GetOrCreateStringAttrib(gdp, "colusage");
#else
    GB_Attribute *colmap = GetOrCreateStringAttrib(gdp, "colmap");
    GB_Attribute *colusage = GetOrCreateStringAttrib(gdp, "colusage");
#endif
    
    std::string usedcols = getName();
    
    std::map<std::string, std::string> mappedNames;

    getMappedNames(colmap, mappedNames);
    
    for (size_t i=0; i<mColourSetNames.size(); ++i)
    {
      ColourSet &colSet = mColourSets[mColourSetNames[i]];
      
      if (colSet.numColours <= 0)
      {
        continue;
      }
      
      std::string hname;
      
      std::map<std::string, std::string>::iterator it = mappedNames.find(mColourSetNames[i]);
      if (it != mappedNames.end())
      {
        hname = it->second;
      }
      else
      {
#if UT_MAJOR_VERSION_INT >= 12
        hname = newVertexAttribName(gdp, GA_STORECLASS_FLOAT, "col");
#else
        hname = newVertexAttribName(gdp, GB_ATTRIB_FLOAT, "col");
#endif
        addNameMapping(colmap, mColourSetNames[i], hname);
      }
      
      usedcols += " " + hname;
      
#if UT_MAJOR_VERSION_INT >= 12
      GA_RWAttributeRef colh = gdp->findVertexAttribute(hname.c_str());
      if (!colh.isValid())
      {
        float def[3] = {0.0f, 0.0f, 0.0f};
        GA_Defaults defVal(def, 3);
        colh = gdp->addFloatTuple(GA_ATTRIB_VERTEX, GA_SCOPE_PUBLIC, hname.c_str(), 3, defVal);
        colh.getAttribute()->setTypeInfo(GA_TYPE_COLOR);
      }
#else
      GEO_AttributeHandle colh = gdp->getVertexAttribute(hname.c_str());
      if (!colh.isAttributeValid())
      {
        float def[3] = {0.0f, 0.0f, 0.0f};
        gdp->addVertexAttrib(hname.c_str(), 3*sizeof(float), GB_ATTRIB_FLOAT, (void*)def);
        colh = gdp->getVertexAttribute(hname.c_str());
      }
#endif
      
      for (int p=0, l=0; p<mNumPolygons; ++p)
      {
        int nv = mPolyVertexCount[p];
        
#if UT_MAJOR_VERSION_INT >= 12
        GEO_Primitive *prim = pml[p];
#else
        GEO_Primitive *prim = pml.entry(pmoffset + p);
#endif
        
        for (int k=nv-1, m=0; k>=0; --k, ++m)
        {             
          int bi = colSet.indices[l+k] * colSet.numComponents;
          
#if UT_MAJOR_VERSION_INT >= 12
          colh.getAIFTuple()->set(colh.getAttribute(), prim->getVertexOffset(m), colSet.colours[bi], 0);
          colh.getAIFTuple()->set(colh.getAttribute(), prim->getVertexOffset(m), colSet.colours[bi+1], 1);
          colh.getAIFTuple()->set(colh.getAttribute(), prim->getVertexOffset(m), colSet.colours[bi+2], 2);
#else
          colh.setElement(&(prim->getVertex(m)));
          colh.setF(colSet.colours[bi], 0);
          colh.setF(colSet.colours[bi+1], 1);
          colh.setF(colSet.colours[bi+2], 2);
#endif
        }
        
        l += nv;
      }
    }
    
    addUsage(colusage, usedcols);
  }
#endif  // SUPPORTS_COLOURS
  
#ifdef SUPPORTS_CREASES
  // TODO
#endif

#ifdef SUPPORTS_HARDEDGES
  // add hard edge attribute

#if UT_MAJOR_VERSION_INT >= 12
  GA_RWAttributeRef heh = gdp->findVertexAttribute("hardEdge");
  if (!heh.isValid())
  {
    int def = 0;
    GA_Defaults defVal(&def, 1);
    heh = gdp->addIntTuple(GA_ATTRIB_VERTEX, GA_SCOPE_PUBLIC, "hardEdge", 1, defVal);
  }
#else  
  GEO_AttributeHandle heh = gdp->getVertexAttribute("hardEdge");
  if (!heh.isAttributeValid())
  {
    int def = 0;
    gdp->addVertexAttrib("hardEdge", sizeof(int), GB_ATTRIB_INT, (void*)&def);
    heh = gdp->getVertexAttribute("hardEdge");
  }
#endif
  
  for (int i=0; i<mNumHardEdges; ++i)
  {
    int bi = i * 2;
    
    EdgeInfo edge(mHardEdges[bi], mHardEdges[bi+1]);
    
#if UT_MAJOR_VERSION_INT >= 12
    GA_Offset p0 = poffsets[mHardEdges[bi]];
    GA_Offset p1 = poffsets[mHardEdges[bi+1]];
#else
    GEO_Point *p0 = pl.entry(poffset + mHardEdges[bi]);
    GEO_Point *p1 = pl.entry(poffset + mHardEdges[bi+1]);
#endif
    
    std::map<EdgeInfo, std::vector<int> >::iterator it = edgeFaces.find(edge);
    
    if (it != edgeFaces.end())
    {
      for (size_t j=0; j<it->second.size(); ++j)
      {
        int pi = it->second[j];
        
        int nv = mPolyVertexCount[pi];
        
#if UT_MAJOR_VERSION_INT >= 12
        GEO_Primitive *prim = pml[pi];
#else
        GEO_Primitive *prim = pml.entry(pmoffset + pi);
#endif
        
        for (int k=0; k<nv; ++k)
        {
#if UT_MAJOR_VERSION_INT >= 12
          GA_Offset v0 = prim->getVertexOffset(k);
          GA_Offset v1 = prim->getVertexOffset((k+1)%nv);
          if ((v0 == p0 && v1 == p1) ||
              (v0 == p1 && v1 == p0))
          {
            heh.getAIFTuple()->set(heh.getAttribute(), v0, 1, 0);
            break;
          }
#else
          GEO_Vertex &v0 = prim->getVertex(k);
          GEO_Vertex &v1 = prim->getVertex((k+1)%nv);
          if ((v0.getPt() == p0 && v1.getPt() == p1) ||
              (v0.getPt() == p1 && v1.getPt() == p0))
          {
            heh.setElement(&v0);
            heh.setI(1, 0);
            break;
          }
#endif
        }
      }
    }
  }
#endif // SUPPORTS_HARDEDGES
  
  // other attributes

  // Instance number attribute
#if UT_MAJOR_VERSION_INT >= 12
  GA_RWAttributeRef aInstNum = gdp->findAttribute(GA_ATTRIB_PRIMITIVE, GA_SCOPE_PUBLIC, "instance_num");
  if (!aInstNum.isValid())
  {
    aInstNum = gdp->addIntTuple(GA_ATTRIB_PRIMITIVE, GA_SCOPE_PUBLIC, "instance_num", 1);
  }
  for (size_t poi=0; poi<pmoffsets.size(); ++poi)
  {
    aInstNum.getAIFTuple()->set(aInstNum.get(), pmoffsets[poi], mInstanceNum, 0);
  }
#else
  GEO_AttributeHandle aInstNum = gdp->getAttribute(GEO_PRIMITIVE_DICT, "instance_num");
  if (!aInstNum.isAttributeValid())
  {
    gdp->addAttribute("instance_num", sizeof(int), GB_ATTRIB_INT, NULL, GEO_PRIMITIVE_DICT);
    aInstNum = gdp->getAttribute(GEO_PRIMITIVE_DICT, "instance_num");
  }
  for (int pmi=0; pmi<mNumPolygons; ++pmi)
  {
    aInstNum.setElement(pml.entry(pmoffset + pmi));
    aInstNum.setI(mInstanceNum, 0);
  }
#endif
  
#ifdef SUPPORTS_ATTRIBS
  Mesh *m0 = (obj0 ? (Mesh*)obj0 : 0);
  Mesh *m1 = (m0 && obj1 ? (Mesh*)obj1 : 0);
  
#if UT_MAJOR_VERSION_INT >= 12

  mVertexAttrib.create(gdp, GA_ATTRIB_VERTEX, voffsets);
  //mVertexAttrib.toHoudini(reader, opts.globalScale);
  mVertexAttrib.toHoudini(reader, opts.frame0, (m0 ? &(m0->mVertexAttrib) : 0), opts.frame1, (m1 ? &(m1->mVertexAttrib) : 0), blend, opts.globalScale);
  
  mPointAttrib.create(gdp, GA_ATTRIB_POINT, poffsets);
  //mPointAttrib.toHoudini(reader, opts.globalScale);
  mPointAttrib.toHoudini(reader, opts.frame0, (m0 ? &(m0->mPointAttrib) : 0), opts.frame1, (m1 ? &(m1->mPointAttrib) : 0), blend, opts.globalScale);

  mPrimitiveAttrib.create(gdp, GA_ATTRIB_PRIMITIVE, pmoffsets);
  //mPrimitiveAttrib.toHoudini(reader, opts.globalScale);
  mPrimitiveAttrib.toHoudini(reader, opts.frame0, (m0 ? &(m0->mPrimitiveAttrib) : 0), opts.frame1, (m1 ? &(m1->mPrimitiveAttrib) : 0), blend, opts.globalScale);

  poffsets.resize(1);
  poffsets[0] = 0;
  mDetailAttrib.create(gdp, GA_ATTRIB_DETAIL, poffsets);
  //mDetailAttrib.toHoudini(reader, opts.globalScale);
  mDetailAttrib.toHoudini(reader, opts.frame0, (m0 ? &(m0->mDetailAttrib) : 0), opts.frame1, (m1 ? &(m1->mDetailAttrib) : 0), blend, opts.globalScale);

#else
  
  mVertexAttrib.create(gdp, GEO_VERTEX_DICT, pmoffset, mNumPolygons);
  //mVertexAttrib.toHoudini(reader, opts.globalScale);
  mVertexAttrib.toHoudini(reader, opts.frame0, (m0 ? &(m0->mVertexAttrib) : 0), opts.frame1, (m1 ? &(m1->mVertexAttrib) : 0), blend, opts.globalScale);

  mPointAttrib.create(gdp, GEO_POINT_DICT, poffset, mNumVertices);
  //mPointAttrib.toHoudini(reader, opts.globalScale);
  mPointAttrib.toHoudini(reader, opts.frame0, (m0 ? &(m0->mPointAttrib) : 0), opts.frame1, (m1 ? &(m1->mPointAttrib) : 0), blend, opts.globalScale);

  mPrimitiveAttrib.create(gdp, GEO_PRIMITIVE_DICT, pmoffset, mNumPolygons);
  //mPrimitiveAttrib.toHoudini(reader, opts.globalScale);
  mPrimitiveAttrib.toHoudini(reader, opts.frame0, (m0 ? &(m0->mPrimitiveAttrib) : 0), opts.frame1, (m1 ? &(m1->mPrimitiveAttrib) : 0), blend, opts.globalScale);

  mDetailAttrib.create(gdp, GEO_DETAIL_DICT, 0, 0);
  //mDetailAttrib.toHoudini(reader, opts.globalScale);
  mDetailAttrib.toHoudini(reader, opts.frame0, (m0 ? &(m0->mDetailAttrib) : 0), opts.frame1, (m1 ? &(m1->mDetailAttrib) : 0), blend, opts.globalScale);

#endif
#endif
  
  // only first parent (don't support instance for now)
  if (getParent() != 0)
  {
    reader->setParent(getName(), getParent()->getName());
  }
  
  return true;
}

void Mesh::resolveParent(Reader *reader)
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

Transform* Mesh::getParent() const
{
  return mParent;
}

void Mesh::setParent(Transform *t)
{
  mParent = t;
  setParentObject(t);
}

bool Mesh::switchParent(Reader *reader, size_t idx)
{
  if (Object::switchParent(reader, idx))
  {
    mParent = dynamic_cast<Transform*>(mParentObject);
    return true;
  }
  return false;
}

void Mesh::cleanup()
{
  Object::cleanup();

  if (mPolyVertexCount)
  {
    delete[] mPolyVertexCount;
    mPolyVertexCount = 0;
  }
  if (mPolyVertexIndices)
  {
    delete[] mPolyVertexIndices;
    mPolyVertexIndices = 0;
  }
  if (mPositions)
  {
    delete[] mPositions;
    mPositions = 0;
  }
#ifdef SUPPORTS_NORMALS
  if (mPolyNormalIndices)
  {
    delete[] mPolyNormalIndices;
    mPolyNormalIndices = 0;
  }
  if (mNormals)
  {
    delete[] mNormals;
    mNormals = 0;
  }
#endif
#ifdef SUPPORTS_UVS
  std::map<std::string, UVSet>::iterator it = mUVSets.begin();
  while (it != mUVSets.end())
  {
    if (it->second.UVs)
    {
      delete[] it->second.UVs;
    }
    if (it->second.indices)
    {
      delete[] it->second.indices;
    }
    ++it;
  }
  mUVSetNames.clear();
  mUVSets.clear();
#endif
#ifdef SUPPORTS_COLOURS
  std::map<std::string, ColourSet>::iterator cit = mColourSets.begin();
  while (cit != mColourSets.end())
  {
    if (cit->second.colours)
    {
      delete[] cit->second.colours;
    }
    if (cit->second.indices)
    {
      delete[] cit->second.indices;
    }
    ++cit;
  }
  mColourSetNames.clear();
  mColourSets.clear();
#endif
#ifdef SUPPORTS_HARDEDGES
  if (mHardEdges)
  {
    delete[] mHardEdges;
  }
  mNumHardEdges = 0;
#endif
  
  mParent = 0;
  mNumPolygons = 0;
  mNumVertices = 0;
  mNumPolyVertices = 0;
#ifdef SUPPORTS_NORMALS
  mNumNormals = 0;
#endif
#ifdef SUPPORTS_ATTRIBS
  mPointAttrib.clear();
  mVertexAttrib.clear();
  mPrimitiveAttrib.clear();
  mDetailAttrib.clear();
#endif
}


#if UT_MAJOR_VERSION_INT >= 12
std::string Mesh::newVertexAttribName(GU_Detail *gdp, GA_StorageClass atype, const std::string &basename)
#else
std::string Mesh::newVertexAttribName(GU_Detail *gdp, GB_AttribType atype, const std::string &basename)
#endif
{
  char name[256];
  size_t j = 0;
  
  //sprintf(name, basename.c_str());
  strncpy(name, basename.c_str(), 256);

#if UT_MAJOR_VERSION_INT >= 12
  static const GA_AttributeOwner owners[] = {GA_ATTRIB_VERTEX};
  GA_RWAttributeRef ref = gdp->findAttribute(GA_SCOPE_PUBLIC, name, owners, 1);
  while (ref.isValid() && ref.getAttribute()->getStorageClass() == atype)
#else
#if UT_MAJOR_VERSION_INT >= 11
  while (gdp->findAttribute(name, atype, GEO_VERTEX_DICT).isValid())
#else
  while (gdp->findAttribute(name, atype, GEO_VERTEX_DICT) >= 0)
#endif
#endif
  {
    j += 1;
    sprintf(name, "%s%lu", basename.c_str(), j);
#if UT_MAJOR_VERSION_INT >= 12
    ref = gdp->findAttribute(GA_SCOPE_PUBLIC, name, owners, 1);
#endif
  }
  
  return name;
}

#if UT_MAJOR_VERSION_INT >= 12
void Mesh::getMappedNames(GA_Attribute *mapa, std::map<std::string, std::string> &mappedNames)
{
  const GA_AIFStringTuple *strTpl = mapa->getAIFSharedStringTuple();
  if (!strTpl)
  {
    strTpl = mapa->getAIFStringTuple();
    if (!strTpl)
    {
      return;
    }
  }
  const char *tmp = strTpl->getString(mapa, 0, 0);
  std::string in = (tmp ? tmp : "");
#else
void Mesh::getMappedNames(GB_Attribute *mapa, std::map<std::string, std::string> &mappedNames)
{
  std::string in = mapa->getIndex(0);
#endif
  mappedNames.clear();
  
  std::vector<std::string> items;
  std::vector<std::string> item;
  
  Split(in, ',', items);
  
  for (size_t i=0; i<items.size(); ++i)
  {
    if (Split(items[i], '=', item) == 2)
    {
      mappedNames[item[0]] = item[1];
    }
  }
}

#if UT_MAJOR_VERSION_INT >= 12
void Mesh::addNameMapping(GA_Attribute *mapa, const std::string &from, const std::string &to)
{
  const GA_AIFStringTuple *strTpl = mapa->getAIFSharedStringTuple();
  if (!strTpl)
  {
    strTpl = mapa->getAIFStringTuple();
    if (!strTpl)
    {
      return;
    }
  }
  const char *tmp = strTpl->getString(mapa, 0, 0);
  std::string in = (tmp ? tmp : "");
#else
void Mesh::addNameMapping(GB_Attribute *mapa, const std::string &from, const std::string &to)
{
  std::string in = mapa->getIndex(0);
#endif
  if (in.length() > 0)
  {
    in += ",";
  }
  in += from + "=" + to;
#if UT_MAJOR_VERSION_INT >= 12
  strTpl->setString(mapa, 0, in.c_str(), 0);
#else
  mapa->renameIndex(0, in.c_str());
#endif
}

#if UT_MAJOR_VERSION_INT >= 12
void Mesh::addUsage(GA_Attribute *usea, const std::string &usage)
{
  const GA_AIFStringTuple *strTpl = usea->getAIFSharedStringTuple();
  if (!strTpl)
  {
    strTpl = usea->getAIFStringTuple();
    if (!strTpl)
    {
      return;
    }
  }
  const char *tmp = strTpl->getString(usea, 0, 0);
  std::string val = (tmp ? tmp : "");
#else
void Mesh::addUsage(GB_Attribute *usea, const std::string &usage)
{
  std::string val = usea->getIndex(0);
#endif
  if (val.length() > 0)
  {
    val += ",";
  }
  val += usage;
#if UT_MAJOR_VERSION_INT >= 12
  strTpl->setString(usea, 0, val.c_str(), 0);
#else
  usea->renameIndex(0, val.c_str());
#endif
}

#ifdef SUPPORTS_UVS
long Mesh::getUVSetIndex(const std::string &name, bool create)
{
  if (name.length() > 0)
  {
    std::vector<std::string>::iterator it = std::find(mUVSetNames.begin(), mUVSetNames.end(), name);
    size_t index = mUVSetNames.size();
    if (it == mUVSetNames.end())
    {
      if (!create)
      {
        return -1;
      }
      mUVSetNames.push_back(name);
      
      UVSet uvSet = {0, 0, 0};
      mUVSets[name] = uvSet;
    }
    else
    {
      index = it - mUVSetNames.begin();
    }
    return (long) index;
  }
  return -1;
}
#endif

#ifdef SUPPORTS_COLOURS
long Mesh::getColourSetIndex(const std::string &name, bool create)
{
  if (name.length() > 0)
  {
    std::vector<std::string>::iterator it = std::find(mColourSetNames.begin(), mColourSetNames.end(), name);
    size_t index = mColourSetNames.size();
    if (it == mColourSetNames.end())
    {
      if (!create)
      {
        return -1;
      }
      mColourSetNames.push_back(name);
      
      ColourSet colourSet = {0, 0, 0, 0};
      mColourSets[name] = colourSet;
    }
    else
    {
      index = it - mColourSetNames.begin();
    }
    return (long) index;
  }
  return -1;
}
#endif


