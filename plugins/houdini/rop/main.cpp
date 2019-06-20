#include <UT/UT_DSOVersion.h>
#include <UT/UT_Interrupt.h>
#include <ROP/ROP_Node.h>
#include <ROP/ROP_Templates.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <CH/CH_LocalVariable.h>
#include <OP/OP_OperatorTable.h>
#include <HOM/HOM_Module.h>
#include "writer.h"
#include <sstream>

#ifndef round
template <typename T>
inline T round(const T &v)
{
   return T( floor( double(v) + 0.5 ) );
}
#endif

class ROP_GtoExport : public ROP_Node
{
  public:
    
    enum ParameterOffset
    {
      PARAM_SOP_PATH = 0,
      PARAM_OUT_FILE,
      PARAM_NO_PRIM_GROUP,
      PARAM_ANIM,
      PARAM_DIFF,
      PARAM_BIN,
      PARAM_GLOBAL_SCALE
    };
    
    ROP_GtoExport(OP_Network *net, const char *name, OP_Operator *op)
      : ROP_Node(net, name, op)
    {
      mParamBaseIndex = getParmList()->getParmIndex("sopPath");
    }
    
    virtual ~ROP_GtoExport()
    {
    }
    
#if UT_MAJOR_VERSION_INT < 12 || (UT_MAJOR_VERSION_INT == 12 && UT_MINOR_VERSION_INT < 5)
    virtual unsigned disableParms()
    {
      unsigned changed = 0;
      
      changed = ROP_Node::disableParms();
      
      changed += enableParm(mParamBaseIndex + PARAM_DIFF, isAnimated());
      
      return changed;
    }
#else
    virtual bool updateParmsFlags()
    {
      bool upd = false;
      
      upd = ROP_Node::updateParmsFlags();
      
      upd = upd || enableParm(mParamBaseIndex + PARAM_DIFF, isAnimated());
      
      return upd;
    }
#endif
    
    virtual int startRender(int nframes, fpreal s, fpreal e)
    {
      // if we export a non-topology changing shape, export ref here
      mStartTime = s;
      mEndTime = e;
      mNumFrames = nframes;
      
      int rv = 1;
      
      if (error() < UT_ERROR_ABORT)
      {
        executePreRenderScript(s);
      }

      if (initSim())
      {
        initSimulationOPs();
      }
      
      if (!isAnimated())
      {
        // export once here only
        HouGtoWriter writer(s, false, getGlobalScale(), ignorePrimitiveGroups());
        
        std::string path = getOutputFile();
        path += ".gto";
        
        std::string sop = getSOPPath();
        
        if (writer.open(path.c_str(), isBinary() ? Gto::Writer::BinaryGTO : Gto::Writer::TextGTO))
        {
          if (!writer.addSOP(getSOPPath()))
          {
            addError(UT_ERROR_INSTREAM, writer.getErrorString().c_str());
            rv = 0;
          }
          else
          {
            writer.write();
          }
          writer.close();
        }
        else
        {
          std::ostringstream oss;
          oss << "Could not read GTO file \"" << path << "\"";
          addError(UT_ERROR_FILE_NOT_FOUND, oss.str().c_str());
          rv = 0;
        }
      }
      
      return rv;
    }
    
    virtual ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt *boss=0)
    {
      executePreFrameScript(time);
      
      ROP_RENDER_CODE rv = ROP_CONTINUE_RENDER;
      
      if (isAnimated())
      {
        HouGtoWriter writer(time, isDiff(), getGlobalScale(), ignorePrimitiveGroups());
        
        char frameext[16];
        
        // time in frame?
        HOM_Module &hom = HOM();
        float now = hom.timeToFrame(time);
        
        int fullframe = int(floor(now));
        int subframe  = int(round((now - fullframe) * 1000.0f));
        
        sprintf(frameext, ".%04d.%03d.gto", fullframe, subframe);
        
        std::string path = getOutputFile();
        path += frameext;
        
        std::string sop = getSOPPath();
        
        if (writer.open(path.c_str(), isBinary() ? Gto::Writer::BinaryGTO : Gto::Writer::TextGTO))
        {
          if (!writer.addSOP(getSOPPath()))
          {
            addError(UT_ERROR_INSTREAM, writer.getErrorString().c_str());
            rv = ROP_ABORT_RENDER;
          }
          else
          {
            writer.write();
          }
          writer.close();
        }
        else
        {
          std::ostringstream oss;
          oss << "Could not read GTO file \"" << path << "\"";
          addError(UT_ERROR_FILE_NOT_FOUND, oss.str().c_str());
          rv = ROP_ABORT_RENDER;
        }
      }
      
      if (error() < UT_ERROR_ABORT)
      {
        executePostFrameScript(time);
      }
      
      return rv;
    }
    
    virtual ROP_RENDER_CODE endRender()
    {
      if (error() < UT_ERROR_ABORT)
      {
        executePostRenderScript(mEndTime);
      }
      
      return ROP_CONTINUE_RENDER;
    }
    
    
    static PRM_Template Parameters[];
    
    static CH_LocalVariable Variables[];
    
    static OP_Node* Create(OP_Network *net, const char *name, OP_Operator *op)
    {
      return new ROP_GtoExport(net, name, op);
    }
    
    
  protected:
    
    bool isAnimated()
    {
      return (evalFloat(mParamBaseIndex + PARAM_ANIM, 0, 0) > 0.0f);
    }
    
    bool isDiff()
    {
      return (evalFloat(mParamBaseIndex + PARAM_DIFF, 0, 0) > 0.0f);
    }
    
    bool isBinary()
    {
      return (evalFloat(mParamBaseIndex + PARAM_BIN, 0, 0) > 0.0f);
    }
    
    bool initSim()
    {
      return (evalFloat(mParamBaseIndex + PARAM_BIN + 1, 0, 0) > 0.0f);
    }

    std::string getSOPPath()
    {
      UT_String val;
      evalString(val, mParamBaseIndex + PARAM_SOP_PATH, 0, 0);
      return std::string(val.nonNullBuffer());
    }
    
    std::string getOutputFile()
    {
      UT_String val;
      
      evalString(val, mParamBaseIndex + PARAM_OUT_FILE, 0, 0);
      
      std::string outFile = std::string(val.nonNullBuffer());
      std::string part;
      int number;
      
      // remove any frame or extension information
      size_t p0 = outFile.find_last_of("/\\");
      size_t p1 = outFile.rfind('.');
      
      while (p1 != std::string::npos && p1 > p0)
      {
        part = outFile.substr(p1+1);
        if (!strcasecmp(part.c_str(), "gto") ||
            sscanf(part.c_str(), "%d", &number) == 1)
        {
          outFile = outFile.substr(0, p1);
          p1 = outFile.rfind('.');
        }
        else
        {
          break;
        }
      }
      
      return outFile;
    }

    float getGlobalScale()
    {
      return evalFloat(mParamBaseIndex + PARAM_GLOBAL_SCALE, 0, 0);
    }
    
    bool ignorePrimitiveGroups()
    {
      return (evalFloat(mParamBaseIndex + PARAM_NO_PRIM_GROUP, 0, 0) > 0.0f);
    }
    
  protected:
    
    int mParamBaseIndex;
    float mStartTime;
    float mEndTime;
    int mNumFrames;
};

static PRM_Name ParameterNames[] =
{
  PRM_Name("sopPath", "SOP Path"),
  PRM_Name("outputFile", "Output File"),
  PRM_Name("ignorePrimGroups", "Ignore Primitive Groups"),
  PRM_Name("animation", "Animation"),
  PRM_Name("diff", "Diff"),
  PRM_Name("binary", "Binary Gto"),
  PRM_Name("globalScale", "Global Scale")
};

static PRM_Default ParameterDefaults[] =
{
  PRM_Default(0.0f, ""),
  PRM_Default(0.0f, ""),
  PRM_Default(0.0f),
  PRM_Default(0.0f),
  PRM_Default(0.0f),
  PRM_Default(1.0f),
  PRM_Default(1.0f)
};

static PRM_Range scaleRange = PRM_Range(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_UI, 10);

PRM_Template ROP_GtoExport::Parameters[] =
{
  theRopTemplates[ROP_RENDER_TPLATE],
  theRopTemplates[ROP_RENDERDIALOG_TPLATE],
  theRopTemplates[ROP_TRANGE_TPLATE],
  theRopTemplates[ROP_FRAMERANGE_TPLATE],
  theRopTemplates[ROP_TAKENAME_TPLATE],
  PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH, 1, &ParameterNames[0], &ParameterDefaults[0], 0, 0, 0, &PRM_SpareData::sopPath),
  PRM_Template(PRM_FILE, 1, &ParameterNames[1], &ParameterDefaults[1]),
  PRM_Template(PRM_TOGGLE, 1, &ParameterNames[2], &ParameterDefaults[2]),
  PRM_Template(PRM_TOGGLE, 1, &ParameterNames[3], &ParameterDefaults[3]),
  PRM_Template(PRM_TOGGLE, 1, &ParameterNames[4], &ParameterDefaults[4]),
  PRM_Template(PRM_TOGGLE, 1, &ParameterNames[5], &ParameterDefaults[5]),
  PRM_Template(PRM_FLT, 1, &ParameterNames[6], &ParameterDefaults[6], 0, &scaleRange),
#if UT_MAJOR_VERSION_INT < 12 || (UT_MAJOR_VERSION_INT == 12 && UT_MINOR_VERSION_INT < 5)
  theRopTemplates[ROP_IFD_INITSIM_TPLATE],
#else
  theRopTemplates[ROP_INITSIM_TPLATE],
#endif
  theRopTemplates[ROP_TPRERENDER_TPLATE],
  theRopTemplates[ROP_PRERENDER_TPLATE],
  theRopTemplates[ROP_LPRERENDER_TPLATE],
  theRopTemplates[ROP_TPREFRAME_TPLATE],
  theRopTemplates[ROP_PREFRAME_TPLATE],
  theRopTemplates[ROP_LPREFRAME_TPLATE],
  theRopTemplates[ROP_TPOSTFRAME_TPLATE],
  theRopTemplates[ROP_POSTFRAME_TPLATE],
  theRopTemplates[ROP_LPOSTFRAME_TPLATE],
  theRopTemplates[ROP_TPOSTRENDER_TPLATE],
  theRopTemplates[ROP_POSTRENDER_TPLATE],
  theRopTemplates[ROP_LPOSTRENDER_TPLATE],
  PRM_Template()
};

CH_LocalVariable ROP_GtoExport::Variables[] =
{
  {0, 0, 0}
};

DLLEXPORT void newDriverOperator(OP_OperatorTable *table)
{
  table->addOperator(new OP_Operator("ROP_GtoExport",
                                     "Gto Export",
                                     ROP_GtoExport::Create,
                                     ROP_GtoExport::Parameters,
                                     1,
                                     1,
                                     ROP_GtoExport::Variables,
                                     0));
}
