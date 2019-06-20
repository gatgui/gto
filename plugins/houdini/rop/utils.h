#ifndef __ROP_GtoExport_utils_h__
#define __ROP_GtoExport_utils_h__

#include "object.h"
#include <map>
#include <string>
#include <set>
#include <vector>
#include <UT/UT_Version.h>
#include <UT/UT_Matrix4.h>
#include <UT/UT_Vector3.h>
#if UT_MAJOR_VERSION_INT >= 12
#  include <GA/GA_Attribute.h>
#else
#  include <GB/GB_Attribute.h>
#endif
#include <GU/GU_Detail.h>

typedef std::map<std::string, std::set<std::string> > ParentsInfo;
typedef std::map<std::string, UT_Matrix4> TransformsInfo;

void Strip(std::string &s);
size_t Split(const std::string &val, char delim, std::vector<std::string> &vals, bool allowEmpty=false, bool stripVals=true);

std::string GetTransformsInfoKey(float frame);
std::string GetParentsInfoKey();

#if UT_MAJOR_VERSION_INT >= 12
const GA_Attribute* GetStringAttrib(const GU_Detail *gdp, const std::string &name);
void GetTransformsInfo(const GA_Attribute *attr, TransformsInfo &info);
void GetParentsInfo(const GA_Attribute *attr, ParentsInfo &info);
#else
const GB_Attribute* GetStringAttrib(const GU_Detail *gdp, const std::string &name);
void GetTransformsInfo(const GB_Attribute *attr, TransformsInfo &info);
void GetParentsInfo(const GB_Attribute *attr, ParentsInfo &info);
#endif

#endif
