#ifndef __SOP_GtoImport_mesh_h_
#define __SOP_GtoImport_mesh_h_

#include "object.h"
#include "userattr.h"
#if UT_MAJOR_VERSION_INT >= 12
#  include <GA/GA_Attribute.h>
#else
#  include <GB/GB_Attribute.h>
#endif
#include <set>

#define MAX_UVS 100
#define MAX_COLOURS 100

#define SUPPORTS_NORMALS
#define SUPPORTS_UVS
#define SUPPORTS_COLOURS
#define SUPPORTS_HARDEDGES
//#define SUPPORTS_CREASES
#define SUPPORTS_ATTRIBS

class Transform;

class Mesh : public Object
{
  public:
    
    enum
    {
      // Components
      READ_POINTS = OBJECT_COMP_MAX,
      READ_NORMALS,
      READ_INDICES,
      READ_ELEMENTS,
      READ_MAPPINGS,
      READ_CREASES,
      READ_COLOURS,
      READ_FLAGS,
      READ_POINTATTRIBS,
      READ_PRIMITIVEATTRIBS,
      READ_VERTEXATTRIBS,
      READ_DETAILATTRIBS,
      // Properties
      READ_POLY_VERTEX_COUNT = OBJECT_PROP_MAX,
      READ_VERTEX_INDICES,
      READ_VERTEX_VALUES,
      READ_NORMAL_INDICES,
      READ_NORMAL_VALUES,
      READ_VCREASE_INDICES,
      READ_VCREASE_VALUES,
      READ_ECREASE_INDICES,
      READ_ECREASE_VALUES,
      READ_HARDEDGE_VALUES,
      READ_COLOUR_INDICES,
      READ_COLOUR_VALUES = READ_COLOUR_INDICES+MAX_COLOURS,
      READ_UV_INDICES = READ_COLOUR_VALUES+MAX_COLOURS,
      READ_UV_VALUES= READ_UV_INDICES+MAX_UVS,
      READ_BOUNDS = READ_UV_VALUES+MAX_UVS,
      READ_MAX
    };
    
#ifdef SUPPORTS_UVS
    struct UVSet
    {
      int numUVs;
      float *UVs;
      int *indices;
    };
#endif
    
#ifdef SUPPORTS_COLOURS
    struct ColourSet
    {
      int numColours;
      int numComponents;
      float *colours;
      int *indices;
    };
#endif

#ifdef SUPPORTS_HARDEDGES
    struct EdgeInfo
    {
      int v0;
      int v1;
      
      inline EdgeInfo()
        : v0(-1), v1(-1)
      {
      }
      
      inline EdgeInfo(int i0, int i1)
        : v0(i0 < i1 ? i0 : i1), v1(i0 < i1 ? i1 : i0)
      {
      }
      
      inline EdgeInfo(const EdgeInfo &rhs)
        : v0(rhs.v0), v1(rhs.v1)
      {
      }
      
      inline ~EdgeInfo()
      {
      }
      
      inline EdgeInfo& operator=(const EdgeInfo &rhs)
      {
        v0 = rhs.v0;
        v1 = rhs.v1;
        return *this;
      }
      
      inline bool operator==(const EdgeInfo &rhs) const
      {
        return (v0 == rhs.v0 && v1 == rhs.v1);
      }
      
      inline bool operator!=(const EdgeInfo &rhs) const
      {
        return !operator==(rhs);
      }
      
      inline bool operator<(const EdgeInfo &rhs) const
      {
        return (v0 < rhs.v0 ? true : (v0 == rhs.v0 ? (v1 < rhs.v1) : false));
      }
    };
#endif
    
    Mesh(const std::string &name, bool boundsOnly, bool asDiff=false);

    virtual ~Mesh();
    
    
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
      
#if UT_MAJOR_VERSION_INT >= 12
    std::string newVertexAttribName(GU_Detail *gdp, GA_StorageClass atype, const std::string &basename);
    void getMappedNames(GA_Attribute *mapa, std::map<std::string, std::string> &mappedNames);
    void addNameMapping(GA_Attribute *mapa, const std::string &from, const std::string &to);
    void addUsage(GA_Attribute *usea, const std::string &usage);
#else
    std::string newVertexAttribName(GU_Detail *gdp, GB_AttribType atype, const std::string &basename);
    void getMappedNames(GB_Attribute *mapa, std::map<std::string, std::string> &mappedNames);
    void addNameMapping(GB_Attribute *mapa, const std::string &from, const std::string &to);
    void addUsage(GB_Attribute *usea, const std::string &usage);
#endif
    
#ifdef SUPPORTS_UVS
    long getUVSetIndex(const std::string &name, bool create=true);
#endif
    
#ifdef SUPPORTS_COLOURS
    long getColourSetIndex(const std::string &name, bool create=true);
#endif
    
  protected:
    
    Transform *mParent;
    
    int mNumPolygons;
    int mNumPolyVertices;
    unsigned short *mPolyVertexCount;
    
    int mNumVertices;
    int *mPolyVertexIndices;
    float *mPositions;

    UT_Vector4 mMirrorFlag;
    
#ifdef SUPPORTS_NORMALS
    int mNumNormals;
    int *mPolyNormalIndices;
    float *mNormals;
#endif
    
#ifdef SUPPORTS_UVS
    std::vector<std::string> mUVSetNames;
    std::map<std::string, UVSet> mUVSets;
#endif
    
#ifdef SUPPORTS_COLOURS
    std::vector<std::string> mColourSetNames;
    std::map<std::string, ColourSet> mColourSets;
#endif
    
#ifdef SUPPORTS_HARDEDGES
    int mNumHardEdges;
    int *mHardEdges;
#endif
    
#ifdef SUPPORTS_ATTRIBS
    AttribDict mPointAttrib;
    AttribDict mPrimitiveAttrib;
    AttribDict mVertexAttrib;
    AttribDict mDetailAttrib;
#endif

    bool mBoundsOnly;
    float mBounds[6];
    
    float mMinVelocity[3];
    float mMaxVelocity[3];
    float mMaxVelocityMag;
};

#endif
