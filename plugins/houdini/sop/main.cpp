#include <UT/UT_DSOVersion.h>
#include <UT/UT_Interrupt.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_Conditional.h>
#include <CH/CH_LocalVariable.h>
#include <OP/OP_OperatorTable.h>
#include <SOP/SOP_Node.h>
#include <set>
#include <fstream>
#include "fileUtils.h"
#include "reader.h"
#include "log.h"

#ifndef round
template <typename T>
inline T round(const T &v)
{
  return T( floor( double(v) + 0.5 ) );
}
#endif

class SOP_GtoImport : public SOP_Node
{
  public:
    
    enum ParameterOffset
    {
      PARAM_REF_FILE = 0,
      PARAM_FRM_FILE,
      PARAM_SHAPE_NAME,
      PARAM_START_FRAME,
      PARAM_END_FRAME,
      PARAM_FRAME_HOLD,
      PARAM_INSTANCE,
      PARAM_SEPARATE_INSTANCE,
      PARAM_MIRROR_CORRECTED,
      PARAM_CACHE,
      PARAM_GLOBAL_SCALE,
      PARAM_BOUNDS_ONLY,
      PARAM_IGNORE_TRANSFORMS,
      PARAM_IGNORE_VISIBILITY,
      PARAM_COUNT
    };
    
    SOP_GtoImport(OP_Network *parent, const char *name, OP_Operator *entry)
      : SOP_Node(parent, name, entry)
      , mLastCookRef("")
      , mLastCookFrm("")
      , mLastShapeName("")
      , mLastCookTime(-1000000.0f)
      , mLastCookCompMirror(true)
      , mLastCookSepInstance(false)
      , mLastCookInstance(true)
      , mLastCookCache(false)
      , mLastFrameHold(true)
      , mLastGlobalScale(1.0f)
      , mLastBoundsOnly(false)
      , mLastIgnoreTransforms(false)
      , mLastIgnoreVisibility(true)
    {
      mParamBaseIndex = getParmList()->getParmIndex("referenceFile");
    }
    
    virtual ~SOP_GtoImport()
    {
      UnrefCache(this);
    }
    
    static PRM_Conditional InstanceCond;
    static PRM_Conditional InstanceOptsCond;
    static PRM_Conditional IgnoreTransformCond;
    static PRM_Conditional IgnoreVisibilityCond;
    
    static PRM_Name ParameterNames[];
    
    static PRM_Default ParameterDefaults[];
    
    static PRM_Template Parameters[];
    
    static CH_LocalVariable Variables[];
    
    static OP_Node* Create(OP_Network *parent, const char *name, OP_Operator *entry)
    {
      return new SOP_GtoImport(parent, name, entry);
    }
    
    
  protected:

    std::string mLastCookRef;
    std::string mLastCookFrm;
    std::string mLastShapeName;
    float mLastCookTime;
    bool mLastCookSepInstance;
    bool mLastCookCompMirror;
    bool mLastCookInstance;
    bool mLastCookCache;
    bool mLastFrameHold;
    float mLastGlobalScale;
    bool mLastBoundsOnly;
    bool mLastIgnoreTransforms;
    bool mLastIgnoreVisibility;
    
    bool needsCooking(const std::string &ref,
                      const std::string &frm,
                      const std::string &shp,
                      float start,
                      float end,
                      float time,
                      bool hold,
                      bool compMirror,
                      bool sepInstance,
                      bool instance,
                      bool cache,
                      float globalScale,
                      bool boundsOnly,
                      bool ignoreTransforms,
                      bool ignoreVisibility)
    {
      bool rv = false;

      float cookFrame = (hold ? (time < start ? start : (time > end ? end : time)) : time);

      if (shp != mLastShapeName)
      {
        rv = true;
      }
      else if (ref != mLastCookRef)
      {
        // if cache is on
        if (mLastCookCache)
        {
          UnrefCache(this, true, false);
        }
        rv = true;
      }
      else if (frm != mLastCookFrm)
      {
        // if cache is on
        if (mLastCookCache)
        {
          bool full = (mLastCookRef.length() == 0);
          UnrefCache(this, full, !full);
        }
        rv = true;
      }
      else if (frm.length() > 0)
      {
        rv = (fabs(cookFrame - mLastCookTime) > 0.000001f);
      }

      if (hold != mLastFrameHold && (time < start || time > end))
      {
        rv = true;
      }
      
      if (compMirror != mLastCookCompMirror)
      {
        rv = true;
      }
      
      if (sepInstance != mLastCookSepInstance)
      {
        rv = true;
      }
      
      if (instance != mLastCookInstance)
      {
        rv = true;
      }
      
      if (cache != mLastCookCache)
      {
        rv = true;
        if (!cache)
        {
          // Will force re-cache for any other SOP_GtoImport instance that may refers to the same gto file(s)
          FlushCache(this);
        }
      }

      if (globalScale != mLastGlobalScale)
      {
        rv = true;
      }

      if (boundsOnly != mLastBoundsOnly)
      {
        rv = true;
      }

      if (ignoreTransforms != mLastIgnoreTransforms)
      {
        rv = true;
      }
      
      if (ignoreVisibility != mLastIgnoreVisibility)
      {
        rv = true;
      }

      if (!rv)
      {
        // now also check ref file and frame file timestamp?
        // maybe as an option?
      }

      mLastCookRef = ref;
      mLastCookFrm = frm;
      mLastShapeName = shp;
      mLastCookTime = cookFrame;
      mLastFrameHold = hold;
      mLastCookCompMirror = compMirror;
      mLastCookSepInstance = sepInstance;
      mLastCookInstance = instance;
      mLastCookCache = cache;
      mLastGlobalScale = globalScale;
      mLastBoundsOnly = boundsOnly;
      mLastIgnoreTransforms = ignoreTransforms;
      mLastIgnoreVisibility = ignoreVisibility;

      return rv;
    }

    virtual OP_ERROR cookMySop(OP_Context &context)
    {
      //Log::Enable();
      //Log::Print("SOP_GtoImport::cookMySop");

      UT_Interrupt *progress = UTgetInterrupt();
      
      if (error() >= UT_ERROR_ABORT)
      {
        return error();
      }

      //flags().timeDep = 1;
      
      std::string refFile = getReferenceFile();
      std::string frmFile = getFrameFile();
      std::string shapeName = getShapeName();
      std::string frmFileNoSub;
      float startFrame = getStartFrame();
      float endFrame = getEndFrame();
      float now = context.getFloatFrame();
      bool hold = holdFrame();
      bool compMirror  = compuetMirrorCorrected();
      bool sepInstance = computeSepInstance();
      bool instance = computeInstance();
      bool cache = useCache();
      float globalScale = getGlobalScale();
      bool boundsOnly = getBoundsOnly();
      bool ignoreTransforms = getIgnoreTransforms();
      bool ignoreVisibility = getIgnoreVisibility();

      flags().timeDep = (frmFile.length() > 0 ? 1 : 0);

      if (!needsCooking(refFile, frmFile, shapeName, startFrame, endFrame, now, hold, compMirror, sepInstance, instance, cache, globalScale, boundsOnly, ignoreTransforms, ignoreVisibility))
      {
        //Log::Print("  doesn't need to be cooked");
        progress->opEnd();
        return error();
      }

      ExpandOptions opts;
      opts.computeMirror = compMirror;
      opts.computeInstance = instance;
      opts.separateInstance = sepInstance;
      opts.ignoreTransforms = ignoreTransforms;
      opts.ignoreVisibility = ignoreVisibility;
      opts.globalScale = globalScale;
      opts.shapeName = shapeName;
      opts.frame0 = NULL;
      opts.frame1 = NULL;
      opts.progress = progress;
      
      std::string readerShape = (cache ? "*" : shapeName);
      
      bool success = false;
      std::string errMsg = "";
      
      if (refFile.length() > 0)
      {
        // re-add .gto extension
        refFile += ".gto";
      }

      if (progress->opStart("Importing GTO"))
      {
        if (refFile.length() == 0 && frmFile.length() == 0)
        {
          //Log::Print("  nothing to cook");
          progress->opEnd();
          return error();
        }
        
        gdp->clearAndDestroy();
        
        if (FileExists(refFile))
        {
          Reader *rref = (cache ? FindCachedGto(this, refFile, boundsOnly, false) : 0 );
          
          if (!rref)
          {
            rref = new Reader(readerShape, boundsOnly, false);
            
            if (!rref->open(refFile.c_str()))
            {
              errMsg = "Could not read reference file \"" + refFile + "\"";
              delete rref;
              rref = 0;
            }
            else if (cache)
            {
              rref->close();
              CacheGto(this, refFile, rref, boundsOnly, false);
            }
          }
          
          if (rref)
          {
            if (frmFile.length() > 0)
            {
              if (now < startFrame)
              {
                if (!hold)
                {
                  progress->opEnd();
                  return error();
                }
                now = startFrame;
              }
              else if (now > endFrame)
              {
                if (!hold)
                {
                  progress->opEnd();
                  return error();
                }
                now = endFrame;
              }
              //now = (now < startFrame ? startFrame : (now > endFrame ? endFrame : now));
              
              frmFileNoSub = frmFile;

              char frameext[16];
              
              int fullframe = int(floor(now));
              int subframe  = int(round((now - fullframe) * 1000.0f));
              
              sprintf(frameext, ".%04d.%03d.gto", fullframe, subframe);
              
              frmFile += frameext;

              sprintf(frameext, ".%04d.gto", fullframe);

              frmFileNoSub += frameext;
              
              std::string pfrmFile, nfrmFile;
              Reader *frm0 = 0, *frm1 = 0;
              float blend = -1.0f;
              
              float tprev, tnext;

              bool valid = FileExists(frmFile);
              if (!valid && subframe == 0)
              {
                frmFile = frmFileNoSub;
                valid = FileExists(frmFile);
              }

              if (valid)
              {
                tprev = now;
                tnext = now;
                frm0 = (cache ? FindCachedGto(this, frmFile, boundsOnly, true) : 0);
                if (!frm0)
                {
                  frm0 = new Reader(readerShape, boundsOnly, true);
                  
                  if (frm0->open(frmFile.c_str()))
                  {
                    frm0->setFrame(now);
                    
                    if (cache)
                    {
                      frm0->close();
                      CacheGto(this, frmFile, frm0, boundsOnly, true);
                    }
                  }
                  else
                  {
                    delete frm0;
                    frm0 = 0;
                  }
                }
              }
              else
              {
                blend = GetPreviousAndNextFrame(frmFile, pfrmFile, nfrmFile, &tprev, &tnext);
                
                frm0 = (cache ? FindCachedGto(this, pfrmFile, boundsOnly, true) : 0);
                if (!frm0)
                {
                  frm0 = new Reader(readerShape, boundsOnly, true);
                  
                  if (frm0->open(pfrmFile.c_str()))
                  {
                    frm0->setFrame(tprev);
                    
                    if (cache)
                    {
                      frm0->close();
                      CacheGto(this, pfrmFile, frm0, boundsOnly, true);
                    }
                  }
                  else
                  {
                    delete frm0;
                    frm0 = 0;
                  }
                }
                
                frm1 = (cache ? FindCachedGto(this, nfrmFile, boundsOnly, true) : 0);
                if (!frm1)
                {
                  frm1 = new Reader(readerShape, boundsOnly, true);
                  
                  if (frm1->open(nfrmFile.c_str()))
                  {
                    frm1->setFrame(tnext);
                    
                    if (cache)
                    {
                      frm1->close();
                      CacheGto(this, nfrmFile, frm1, boundsOnly, true);
                    }
                  }
                  else
                  {
                    delete frm1;
                    frm1 = 0;
                  }
                }
              }
              
              opts.frame0 = frm0;
              opts.frame1 = frm1;
              if (rref->toHoudini(context, gdp, progress, opts, frm0, frm1, blend, tprev, tnext))
              {
                success = true;
              }
              else
              {
                errMsg = "Invalid data in \"" + refFile + "\"";
                if (blend < 0.0f)
                {
                  errMsg += ", \"" + frmFile + "\"";
                }
                else
                {
                  if (pfrmFile.length() > 0)
                  {
                    errMsg += ", \"" + pfrmFile + "\"";
                  }
                  if (nfrmFile.length() > 0)
                  {
                    errMsg += ", \"" + nfrmFile + "\"";
                  }
                }
              }
              
              if (!cache)
              {
                if (frm0)
                {
                  delete frm0;
                }
                if (frm1)
                {
                  delete frm1;
                }
              }
            }
            else
            {
              if (rref->toHoudini(context, gdp, progress, opts))
              {
                success = true;
              }
              else
              {
                errMsg = "Invalid data in \"" + refFile + "\"";
              }
            }
            
            if (!cache)
            {
              delete rref;
            }
          }
        }
        else
        {
          float now = context.getFloatFrame();
          
          if (now < startFrame)
          {
            if (!hold)
            {
              progress->opEnd();
              return error();
            }
            now = startFrame;
          }
          else if (now > endFrame)
          {
            if (!hold)
            {
              progress->opEnd();
              return error();
            }
            now = endFrame;
          }
          //now = (now < startFrame ? startFrame : (now > endFrame ? endFrame : now));
          
          frmFileNoSub = frmFile;

          char frameext[16];
          
          int fullframe = int(floor(now));
          int subframe  = int(round((now - fullframe) * 1000.0f));
          
          sprintf(frameext, ".%04d.%03d.gto", fullframe, subframe);
          
          frmFile += frameext;
          
          sprintf(frameext, ".%04d.gto", fullframe);

          frmFileNoSub += frameext;

          bool valid = FileExists(frmFile);
          if (!valid && subframe == 0)
          {
            frmFile = frmFileNoSub;
            valid = FileExists(frmFile);
          }

          if (!valid)
          {
            std::string pfrmFile, nfrmFile;
            float pframe, nframe;

            float blend = GetPreviousAndNextFrame(frmFile, pfrmFile, nfrmFile, &pframe, &nframe);
            
            // no subframe interpolation for frames only
            // for particles, a work around is to set reference file to any file containing particles in the sequence
            // ...
            if (blend > 0.5f)
            {
              frmFile = nfrmFile;
              now = nframe;
            }
            else
            {
              frmFile = pfrmFile;
              now = pframe;
            }
          }
          
          Reader *frm = (cache ? FindCachedGto(this, frmFile, boundsOnly, false) : 0);
            
          if (!frm)
          {
            frm = new Reader(readerShape, boundsOnly, false);

            if (frm->open(frmFile.c_str()))
            {
              frm->setFrame(now);
              if (cache)
              {
                frm->close();
                CacheGto(this, frmFile, frm, boundsOnly, false);
              }
            }
            else
            {
              errMsg = "Could not read full frame file \"" + frmFile + "\"";
              delete frm;
              frm = 0;
            }
          }
          
          if (frm)
          {
            if (frm->toHoudini(context, gdp, progress, opts))
            {
              success = true;
            }
            else
            {
              errMsg = "Invalid data in \"" + frmFile + "\"";
            }
            if (!cache)
            {
              delete frm;
            }
          }
        }
      }
      
      if (!success)
      {
        // set error code and message
      }
      
      progress->opEnd();
      return error();
    }
    
    
  private:
    
    void dirmap(std::string &path)
    {
#ifdef _WIN32
      if (path.length() > 1 && path[0] == '/')
      {
        path = "z:" + path;
      }
#else
      if (path.length() > 2 && (path[0] == 'z' || path[0] == 'z') && path[1] == ':')
      {
        path = path.substr(2);
      }
#endif
    }

    void forceRefresh()
    {
      mLastCookTime = std::numeric_limits<float>::max();
    }

    std::string getReferenceFile()
    {
      UT_String val;
      evalString(val, mParamBaseIndex + PARAM_REF_FILE, 0, 0);

      std::string refFile = std::string(val.nonNullBuffer());
      dirmap(refFile);

      // if reference file is empty and we have a frame file, try to read the .ref file
      if (refFile.length() == 0)
      {
        std::string tmp = getFrameFile();
        if (tmp.length() > 0)
        {
          tmp += ".ref";
          std::ifstream f(tmp.c_str());
          if (f.is_open())
          {
            std::getline(f, refFile);
            dirmap(refFile);
          }
        }
      }
      
      // remove .gto extension if any
      size_t p0 = refFile.find_last_of("/\\");
      size_t p1 = refFile.rfind('.');
      if (p1 != std::string::npos && p1 > p0 && !strcasecmp(refFile.substr(p1+1).c_str(), "gto"))
      {
        refFile = refFile.substr(0, p1);
      }
      
      return refFile;
    }
    
    std::string getFrameFile()
    {
      UT_String val;
      
      evalString(val, mParamBaseIndex + PARAM_FRM_FILE, 0, 0);
      
      std::string frmFile = std::string(val.nonNullBuffer());
      dirmap(frmFile);
      
      // remove any frame or extension information
      int frame, subFrame;
      if (!GetGtoFrameAndSubFrame(frmFile, frame, subFrame, &frmFile))
      {
        // remove .gto extension if any
        size_t p0 = frmFile.find_last_of("/\\");
        size_t p1 = frmFile.rfind('.');
        if (p1 != std::string::npos && p1 > p0 && !strcasecmp(frmFile.substr(p1+1).c_str(), "gto"))
        {
          frmFile = frmFile.substr(0, p1);
        }
      }
      
      return frmFile;
    }
    
    std::string getShapeName()
    {
      UT_String val;
      
      evalString(val, mParamBaseIndex + PARAM_SHAPE_NAME, 0, 0);
      
      return std::string(val.nonNullBuffer());
    }
    
    float getStartFrame()
    {
      return evalFloat(mParamBaseIndex + PARAM_START_FRAME, 0, 0);
    }
    
    float getEndFrame()
    {
      return evalFloat(mParamBaseIndex + PARAM_END_FRAME, 0, 0);
    }
    
    bool computeInstance()
    {
      return (evalFloat(mParamBaseIndex + PARAM_INSTANCE, 0, 0) > 0.0f);
    }

    bool computeSepInstance()
    {
      return (evalFloat(mParamBaseIndex + PARAM_SEPARATE_INSTANCE, 0, 0) > 0.0f);
    }
    
    bool compuetMirrorCorrected()
    {
      return (evalFloat(mParamBaseIndex + PARAM_MIRROR_CORRECTED, 0, 0) > 0.0f);
    }

    bool useCache()
    {
      return (evalFloat(mParamBaseIndex + PARAM_CACHE, 0, 0) > 0.0f);
    }

    float getGlobalScale()
    {
      return evalFloat(mParamBaseIndex + PARAM_GLOBAL_SCALE, 0, 0);
    }
    
    bool holdFrame()
    {
      return (evalFloat(mParamBaseIndex + PARAM_FRAME_HOLD, 0, 0) > 0.0f);
    }

    bool getBoundsOnly()
    {
      return (evalFloat(mParamBaseIndex + PARAM_BOUNDS_ONLY, 0, 0) > 0.0f);
    }
    
    bool getIgnoreTransforms()
    {
      return (evalFloat(mParamBaseIndex + PARAM_IGNORE_TRANSFORMS, 0, 0) > 0.0f);
    }
    
    bool getIgnoreVisibility()
    {
      return (evalFloat(mParamBaseIndex + PARAM_IGNORE_VISIBILITY, 0, 0) > 0.0f);
    }
    
  private:
    
    int mParamBaseIndex;
    
  // Cache
  
  public:
    
    struct CacheEntry
    {
      Reader *reader;
      std::set<SOP_GtoImport*> referers;
    };
    
    static std::string GetCacheKey(const std::string &fname, bool boundsOnly)
    {
      std::string key = fname;
      
      size_t p0 = 0;
      size_t p1 = key.find('\\', p0);
      
      while (p1 != std::string::npos)
      {
        key[p1] = '/';
        p0 = p1 + 1;
        p1 = key.find('\\', p0);
      }
      
#ifdef _WIN32
      p0 = 0;
      while (p0 < key.length())
      {
        if (key[p0] >= 'A' && key[p0] <= 'Z')
        {
          key[p0] = 'a' + (key[p0] - 'A');
        }
        ++p0;
      }
#endif

      key += (boundsOnly ? ":0" : ":1");
      
      return key;
    }
    
    static void FlushCache(SOP_GtoImport *referer)
    {
      std::map<std::string, CacheEntry>::iterator it;
      std::set<SOP_GtoImport*>::iterator rit;
      
      it = msDiffCache.begin();
      while (it != msDiffCache.end())
      {
        rit = it->second.referers.find(referer);
        if (rit != it->second.referers.end())
        {
          for (rit=it->second.referers.begin(); rit!=it->second.referers.end(); ++rit)
          {
            if ((*rit) != referer)
            {
              (*rit)->forceRefresh();
            }
          }
          std::map<std::string, CacheEntry>::iterator tmp = it;
          it->second.referers.clear();
          delete it->second.reader;
          ++it;
          msDiffCache.erase(tmp);
          continue;
        }
        ++it;
      }
      
      it = msFullCache.begin();
      while (it != msFullCache.end())
      {
        rit = it->second.referers.find(referer);
        if (rit != it->second.referers.end())
        {
          for (rit=it->second.referers.begin(); rit!=it->second.referers.end(); ++rit)
          {
            if ((*rit) != referer)
            {
              (*rit)->forceRefresh();
            }
          }
          std::map<std::string, CacheEntry>::iterator tmp = it;
          it->second.referers.clear();
          delete it->second.reader;
          ++it;
          msFullCache.erase(tmp);
          continue;
        }
        ++it;
      }
    }
    
    static void UnrefCache(SOP_GtoImport *referer, bool full=true, bool diff=true)
    {
      std::map<std::string, CacheEntry>::iterator it;
      std::set<SOP_GtoImport*>::iterator rit;
      
      if (diff)
      {
        it = msDiffCache.begin();
        while (it != msDiffCache.end())
        {
          rit = it->second.referers.find(referer);
          if (rit != it->second.referers.end())
          {
            it->second.referers.erase(rit);
            if (it->second.referers.size() == 0)
            {
              std::map<std::string, CacheEntry>::iterator tmp = it;
              delete it->second.reader;
              ++it;
              msDiffCache.erase(tmp);
              continue;
            }
          }
          ++it;
        }
      }
      
      if (full)
      {
        it = msFullCache.begin();
        while (it != msFullCache.end())
        {
          rit = it->second.referers.find(referer);
          if (rit != it->second.referers.end())
          {
            it->second.referers.erase(rit);
            if (it->second.referers.size() == 0)
            {
              std::map<std::string, CacheEntry>::iterator tmp = it;
              delete it->second.reader;
              ++it;
              msFullCache.erase(tmp);
              continue;
            }
          }
          ++it;
        }
      }
    }
    
    static Reader* FindCachedGto(SOP_GtoImport *referer, const std::string &fname, bool boundsOnly, bool asDiff)
    {
      std::map<std::string, CacheEntry>::iterator it;
      std::string key = GetCacheKey(fname, boundsOnly);
      
      if (asDiff)
      {
        it = msDiffCache.find(key);
        if (it != msDiffCache.end())
        {
          it->second.referers.insert(referer);
          return it->second.reader;
        }
        else
        {
          return 0;
        }
      }
      else
      {
        it = msFullCache.find(key);
        if (it != msFullCache.end())
        {
          it->second.referers.insert(referer);
          return it->second.reader;
        }
        else
        {
          return 0;
        }
      }
    }
    
    static void CacheGto(SOP_GtoImport *referer, const std::string &fname, Reader *reader, bool boundsOnly, bool asDiff)
    {
      std::map<std::string, CacheEntry>::iterator it;
      std::string key = GetCacheKey(fname, boundsOnly);
      
      //size_t p0 = 0;
      
      if (asDiff)
      {
        it = msDiffCache.find(key);
        if (it != msDiffCache.end())
        {
          if (it->second.reader != reader)
          {
            // should i really delete it?
            // -> this should never happen using cache except of the same gto
            //    file is read from 2 different threads at the same time...
            delete it->second.reader;
            it->second.reader = reader;
          }
        }
        else
        {
          msDiffCache[key].reader = reader;
        }
        msDiffCache[key].referers.insert(referer);
      }
      else
      {
        it = msFullCache.find(key);
        if (it != msFullCache.end())
        {
          if (it->second.reader != reader)
          {
            // should i really delete it?
            // -> this should never happen using cache except of the same gto
            //    file is read from 2 different threads at the same time...
            delete it->second.reader;
            it->second.reader = reader;
          }
        }
        else
        {
          msFullCache[key].reader = reader;
        }
        msFullCache[key].referers.insert(referer);
      }
    }
    
    
  protected:
    
    static std::map<std::string, CacheEntry> msDiffCache;
    static std::map<std::string, CacheEntry> msFullCache;
};

std::map<std::string, SOP_GtoImport::CacheEntry> SOP_GtoImport::msDiffCache;
std::map<std::string, SOP_GtoImport::CacheEntry> SOP_GtoImport::msFullCache;

#define DECLARE_PARAM(type, count, idx) \
  PRM_Template(type, count, &SOP_GtoImport::ParameterNames[idx], &SOP_GtoImport::ParameterDefaults[idx])

#define DECLARE_PARAM_COND(type, count, idx, cond) \
  PRM_Template(type, count, &SOP_GtoImport::ParameterNames[idx], &SOP_GtoImport::ParameterDefaults[idx], 0, 0, 0, 0, 1, 0, cond)

#define DECLARE_RANGE_PARAM(type, count, idx, range) \
  PRM_Template(type, count, &SOP_GtoImport::ParameterNames[idx], &SOP_GtoImport::ParameterDefaults[idx], 0, &range)


PRM_Conditional SOP_GtoImport::InstanceOptsCond("{ computeInstance == 0 } { shapeName != \"*\" }", PRM_CONDTYPE_DISABLE);
PRM_Conditional SOP_GtoImport::InstanceCond("{ shapeName != \"*\" }", PRM_CONDTYPE_DISABLE);
PRM_Conditional SOP_GtoImport::IgnoreTransformCond("{ shapeName != \"*\" }", PRM_CONDTYPE_DISABLE);
PRM_Conditional SOP_GtoImport::IgnoreVisibilityCond("{ shapeName != \"*\" }", PRM_CONDTYPE_DISABLE);


PRM_Name SOP_GtoImport::ParameterNames[] =
{
  PRM_Name("referenceFile", "Reference file"),
  PRM_Name("frameFile", "Frame file"),
  PRM_Name("shapeName", "Shape name"),
  PRM_Name("startFrame", "Start frame"),
  PRM_Name("endFrame", "End frame"),
  PRM_Name("frameHold", "Frame hold"),
  PRM_Name("computeInstance", "Compute Instances"),
  PRM_Name("computeSepInstance", "Separate Instance Groups"),
  PRM_Name("mirrorCorrected", "Instance Mirror Corrected"),
  PRM_Name("cacheFiles", "Cache GTO files"),
  PRM_Name("globalScale", "Global Scale"),
  PRM_Name("boundsOnly", "Bounding Box Only"),
  PRM_Name("ignoreTransforms", "Ignore Transforms"),
  PRM_Name("ignoreVisibility", "Ignore Visibility")
};

PRM_Default SOP_GtoImport::ParameterDefaults[] =
{
  PRM_Default(0.0f, ""), // refFile
  PRM_Default(0.0f, ""), // frameFile
  PRM_Default(0.0f, "*"), // shapeName
  PRM_Default(1.0f), // startFrame
  PRM_Default(1.0f), // endFrame
  PRM_Default(1.0f), // frameHold
  PRM_Default(1.0f), // computeInstance
  PRM_Default(0.0f), // computeSepInstance
  PRM_Default(0.0f), // mirrorCorrected
  PRM_Default(0.0f), // cacheFile
  PRM_Default(1.0f), // globalScale
  PRM_Default(0.0f), // boundsOnly
  PRM_Default(0.0f), // ignoreTransforms
  PRM_Default(1.0f)
};

static PRM_Range frameRange = PRM_Range(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_UI, 100);
static PRM_Range scaleRange = PRM_Range(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_UI, 10);

PRM_Template SOP_GtoImport::Parameters[] =
{
  DECLARE_PARAM(PRM_FILE, 1, SOP_GtoImport::PARAM_REF_FILE),
  DECLARE_PARAM(PRM_FILE, 1, SOP_GtoImport::PARAM_FRM_FILE),
  DECLARE_PARAM(PRM_STRING, 1, SOP_GtoImport::PARAM_SHAPE_NAME),
  DECLARE_RANGE_PARAM(PRM_FLT, 1, SOP_GtoImport::PARAM_START_FRAME, frameRange),
  DECLARE_RANGE_PARAM(PRM_FLT, 1, SOP_GtoImport::PARAM_END_FRAME, frameRange),
  DECLARE_PARAM(PRM_TOGGLE, 1, SOP_GtoImport::PARAM_FRAME_HOLD),
  DECLARE_PARAM_COND(PRM_TOGGLE, 1, SOP_GtoImport::PARAM_INSTANCE, &SOP_GtoImport::InstanceCond),
  DECLARE_PARAM_COND(PRM_TOGGLE, 1, SOP_GtoImport::PARAM_SEPARATE_INSTANCE, &SOP_GtoImport::InstanceOptsCond),
  DECLARE_PARAM_COND(PRM_TOGGLE, 1, SOP_GtoImport::PARAM_MIRROR_CORRECTED, &SOP_GtoImport::InstanceOptsCond),
  DECLARE_PARAM(PRM_TOGGLE, 1, SOP_GtoImport::PARAM_CACHE),
  DECLARE_RANGE_PARAM(PRM_FLT, 1, SOP_GtoImport::PARAM_GLOBAL_SCALE, scaleRange),
  DECLARE_PARAM(PRM_TOGGLE, 1, SOP_GtoImport::PARAM_BOUNDS_ONLY),
  DECLARE_PARAM_COND(PRM_TOGGLE, 1, SOP_GtoImport::PARAM_IGNORE_TRANSFORMS, &SOP_GtoImport::IgnoreTransformCond),
  DECLARE_PARAM_COND(PRM_TOGGLE, 1, SOP_GtoImport::PARAM_IGNORE_VISIBILITY, &SOP_GtoImport::IgnoreVisibilityCond),
  PRM_Template()
};

#undef DECLARE_PARAM
#undef DECLARE_PARAM_COND
#undef DECLARE_RANGE_PARAM

CH_LocalVariable SOP_GtoImport::Variables[] =
{
  {0, 0, 0}
};

// --- Houdini SOP entry point

/*
// When Houdini calls this functions it's already too late!

static void dsoExit(void*)
{
  SOP_GtoImport::ClearCache();
}
*/

DLLEXPORT void newSopOperator(OP_OperatorTable *table)
{
  //UT_Exit::addExitCallback(dsoExit);

  table->addOperator(
    new OP_Operator("SOP_GtoImport",
                    "Gto Importer",
                    SOP_GtoImport::Create,
                    SOP_GtoImport::Parameters,
                    0,
                    0,
                    SOP_GtoImport::Variables,
                    OP_FLAG_GENERATOR));
}
