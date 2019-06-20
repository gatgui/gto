Houdini Gto ROP usage
  
  Required attributes
    positions: name=P, class=point, type=vector
               name=Point Num, class=vertex, type=int[1]
    
  Optional attributes
    uvs      : name=uv|uv<n>, class=vertex, type=float[2]|float[3] (**)
    colors   : name=col|col<n>, class=vertex, type=float[3]
    normals  : name=N, class=point|vertex, type=vector
  
  Hierarchy
    Create transform object for each non leaf name in "parenting" detail property
    Use matrix as set in "transforms_<frame>_<subframe>". Identity in any other case.
  
  Naming in GTO
    2 Major case
    
    NOTE: primitive group whose name starts with "_" are not counted
    
    1/ No primitive group in geometry (see note above)
       Use geometry name as parent name
       Use SOP connected to Gto ROP as shape name
    
    2/ 1 or more primitive group(s) in geometry
       a) Get group name
       b) Lookup for parent name(s) in "parenting" detail property
          if found ->
            Use first name in list of parents
          else ->
            if group name ends with "Shape" ->
              Use group name with "Shape" stripped as parent name
              Use group name as shape name
            else ->
              Use group name as parent name
              Use group name with "Shape" appended as shape name
  
  Hard edges handling
    Use "hardEdge" vertex attribute (int[1])
      a value of 1 means that the edge going from that vertex to next vertex in the
      primitive is a hard edge
  
  Special details attributes
    parenting
      "," separated list of "<childName> <parentName>( <parentName>)*"
    
    transforms_<frame>_<subframe>
      "," separated list of "<name> <matrix>" (<matrix> being 16 numbers)
    
    uvmap
      "," comma separated list of "<gtoSetName>=<houdiniAttrName>"
      use to remap houdini attribute name to UV set name in GTO
    
    uvusage
      "," separated list of "<shapeName> <houdiniAttrName>( <houdiniAttrName>)*"
      if shape name to be exported appears in the list, only export those uv sets
    
    colmap
      "," comma separated list of "<gtoSetName>=<houdiniAttrName>"
      use to remap houdini attribute name to color set name in GTO
    
    colusage
      "," separated list of "<shapeName> <houdiniAttrName>( <houdiniAttrName>)*"
      if shape name to be exported appears in the list, only export those color sets


