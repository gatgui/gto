import excons
import excons.tools.python
import sys
import glob


static_build = True
use_zlib = False
zlib_dir = None
zlib_name = None
pythonPrefix = excons.tools.python.ModulePrefix() + "/" + excons.tools.python.Version()

lib_type = "staticlib"
lib_defs = []
lib_incdirs = ["lib"]
lib_libdirs = []
lib_libs = []
lib_extrasrcs = []

oth_defs = []
oth_incdirs = []
oth_libdirs = []
oth_libs = []


env = excons.MakeBaseEnv()


static = ARGUMENTS.get("static", None)
if static is None:
    shared = ARGUMENTS.get("shared", "0")
    try:
        static_build = (int(shared) == 0)
    except:
        pass
else:
    try:
        static_build = (int(static) != 0)
    except:
        pass

try:
    use_zlib = int(ARGUMENTS.get("use-zlib", "0"))
except:
    pass


if sys.platform == "win32":
    lib_defs.append("REGEX_STATIC")
    lib_defs.append("_SCL_SECURE_NO_WARNINGS")
    lib_incdirs.append("regex/src")
    lib_extrasrcs = ["regex/src/regex.c"]

if not static_build:
    lib_type = "sharedlib"
    lib_defs.append("GTO_EXPORT")
else:
    lib_defs.append("GTO_STATIC")
    oth_defs.append("GTO_STATIC")

if use_zlib:
    
    lib_defs.append("GTO_SUPPORT_ZIP")
    oth_defs.append("GTO_SUPPORT_ZIP")

    if sys.platform == "win32":
        import zipfile
        zf = zipfile.ZipFile("zlib-1.2.3.zip")
        zf.extractall()
        
        zlib_inc = "zlib-1.2.3/include"
        zlib_lib = "zlib-1.2.3/lib/%s" % excons.arch_dir

        lib_incdirs.append(zlib_inc)
        if not static_build:
            lib_libdirs.append(zlib_lib)
            lib_libs.append("zlib")

        oth_incdirs.append(zlib_inc)
        if static_build:
            oth_libdirs.append(zlib_lib)
            oth_libs.append("zlib")
    else:
        if not static_build:
            lib_libs.append("z")
        else:
            oth_libs.append("z")


targets = [
    {"name": "gtoLib",
     "alias": "lib",
     "type": lib_type,
     "incdirs": lib_incdirs,
     "libdirs": lib_libdirs,
     "libs": lib_libs,
     "defs": lib_defs,
     "srcs": glob.glob("lib/Gto/*.cpp") + lib_extrasrcs,
     "install": {"include/Gto": glob.glob("lib/Gto/*.h")}
    },
    {"name": "_gto",
     "alias": "python",
     "type": "dynamicmodule",
     "prefix": pythonPrefix,
     "ext": excons.tools.python.ModuleExtension(),
     "defs": oth_defs,
     "incdirs": ["plugins/python/src/gto"] + oth_incdirs,
     "libdirs": oth_libdirs,
     "srcs": glob.glob("plugins/python/src/gto/*.cpp"),
     "libs": ["gtoLib"] + oth_libs,
     "custom": [excons.tools.python.SoftRequire],
     "install": {pythonPrefix: ["python/gto/gto.py", "python/gtoContainer/gtoContainer.py"]}
    },
    {"name": "gtoinfo",
     "alias": "tools",
     "type": "program",
     "defs": oth_defs,
     "incdirs": oth_incdirs,
     "libdirs": oth_libdirs,
     "libs": ["gtoLib"] + oth_libs,
     "srcs": ["bin/gtoinfo/main.cpp"],
    }
]

excons.DeclareTargets(env, targets)
