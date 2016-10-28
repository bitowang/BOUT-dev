from common import *
# Define some constants, functions

import sys

# Read the stencils
functions=[]
inFunc=0
with open("stencils_cleaned.cxx","r") as f:
    fi=-1
    for line in f:
        line=line.strip()
        line=line.replace("result","result_") # avoid to overwrite result
        if line == '':
            continue
        if line.find("{") > -1: #start of function
            if inFunc==0:
                functions.append([])
                fi+=1
            inFunc+=1
        if inFunc:
            functions[fi].append(line);
        if line.find("}") > -1:
            inFunc-=1

func_db=dict()

dir=['x', 'y', 'z']
#dir=['x','y']
for d in dir:
    func_db[d]=[]

class function:
    name=""
    body=[]
    stag=False
    flux=False
    guards=3
    mbf=None

functions_=functions
functions=[]
for f in functions_:
    func=function()
    func.body=f
    func.name=f[0].split()[1].split("(")[0]
    if f[0].find("stag") > 0:
        # They are staggered
        func.stag=True
    else:
        func.stag=False
    if f[0].find("BoutReal") == 0 and f[0].find('stencil') > 0:
        #for i in f:
        #    print i
        if func.name=='DDX_CWENO3':
            #print << sys.stderr , "Skipping DDX_CWENO3 ..."
            continue;
        #print fname
        if (f[0].find("BoutReal V") == -1 and f[0].find("BoutReal F") == -1 ):
            # They have one stencil
            #print f[0]
            func.flux=False
        else:
            func.flux=True
        functions.append(func)
    if f[0].find("Mesh::boundary_derivs_pair") == 0 and f[0].find('stencil') > 0:
        if (f[0].find("Mesh::boundary_derivs_pair V") == -1 and f[0].find("Mesh::boundary_derivs_pair F") == -1 ):
            # not a flux functions - has only on stencil as argument
            func.flux=False
        else:
            func.flux=True
        functions.append(func)
null_func=function()
null_func.name='NULL'
null_func.body=["{","result_.inner = 0;","result_.outer = 0;","}"]
functions.append(null_func)
for f in functions:
    ls=""
    for l in f.body:
        ls+=l
    inv=ls[::-1].find("}")+1
    ls=ls[ls.find("{")+1:-inv]
    fu=ls.split(";")
    mymax=len(fu)
    #for i in range(len(fu)):
    i=0
    while i < mymax:
        fu[i]+=';'
        if fu[i].find("}")>-1:
            l=fu[i]
            t=l.split("}")
            for ti in t[:-1]:
                fu.insert(i,ti+"}")
                i+=1
                mymax+=1
            fu[i]=t[-1]
        i+=1
            
    f.body=fu

#    print fu
#    print f.name
#exit(7)
        
for f in functions:
    f.guards=1
    for l in f.body:
        for off in ['pp','mm','p2','m2']:
            if l.find(off)>-1:
                f.guards=2
                #print >> sys.stderr , f.name ,"has 2 guard cells"
    #if f.guards == 1:
        #print >> sys.stderr , f.name ,"has 1 guard cells"
        
for f in functions:
    if f.flux == False:
        if f.stag:
            for d in dir:
                tmp="cart_diff_%s_%s_%%s"%(d,f.name)
                func_db[d].append([f.name,"NULL",tmp%"off",tmp%"on"])
        else:
            #not staggerd
            for d in dir:
                tmp="cart_diff_%s_%s_%%s"%(d,f.name)
                func_db[d].append([f.name,"cart_diff_%s_%s_norm"%(d,f.name),"NULL","NULL"])
for db in func_db:
    func_db[db].append(["NULL","NULL","NULL","NULL"])


def replace_stencil(line,sten,fname,field,mode,sten_name,d,update=None,z0=None):
    if update==None:
        update=sten_name=="main"
#    if z0 is not None:
#        print >>sys.stderr,' ... z0'
    pos=line.find(sten)
    part_of_offset=['p','m','c']
    for i in range(2,9):
        part_of_offset.append(str(i))
    while pos > -1:
        end=pos+len(sten)
        while line[end] in part_of_offset:
            end+=1
        off=line[pos+2:end]
        try:
            line_=line
            line=line[:pos]+get_diff(off_diff[mode][sten_name][off],fname,field,d,update,z0)+line[end:]
        except:
            print >>sys.stderr,line_,mode,sten_name,off,sten
            print >>sys.stderr,off_diff[mode][sten_name]
            raise
        pos=line.find(sten)
    return line

def parse_body(sten,field,mode,d,z0=None):
    body=""
    result=''
    result_=['']*2
    for line in sten.body[:-1]:
        if sten.flux:
            try:
                line=replace_stencil(line,'v.',"v_in",field,mode,sten.mbf,d,z0=z0)
                line=replace_stencil(line,'f.',"f_in",field,"norm",sten.mbf,d,z0=z0)
            except:
                import sys
                print >>sys.stderr , sten.name, sten.mbf
                raise
        else:
            line=replace_stencil(line,'f.',"in",field,mode,sten.mbf,d,z0=z0)
        #body+=replace_stencil(line,'f.',"f_in",field,"norm",off,d)
        if line.find("return") == -1:
            if sten.mbf == 'main':
                body+= "     "+line+"\n"
            else:
                toPrint=True
                for resi,res in enumerate(["result_.inner", "result_.outer"]):
                    if line.find(res) > -1:
                        tmp=line[line.index(res)+len(res):]
                        if tmp.find("=") > -1:
                            if result_[resi]!='':
                                import sys
                                print >> sys.stderr ,"Did not expect another defintion of",res
                                print >> sys.stderr ,"The last one was %s = %s"%(res,result_[resi])
                                print >> sys.stderr ,"thise one is ",line
                                exit(1)
                            result_[resi]=tmp[tmp.index("=")+1:]
                            toPrint=False
                if toPrint:
                    if line.find("=") > -1:
                        import sys
                        print >> sys.stderr ,"Failed to parse - unexpected line: ",line
                        print >> sys.stderr ,result_
                        print >> sys.stderr ,line
                        exit(1)

        else:
            if sten.mbf == 'main':
                #body+= "    "+get_diff('c()',"result",field,d)+"= "+
                result=line[len("return")+line.index("return"):]+"\n"
                #print >> sys.stderr,"bla"
            else:
                returned=line[len("return")+line.index("return"):]
    return [body, result, result_]
def get_for_loop_z(sten,field,stag):
    d='z'
    print '  if (Nz > 3) {'
    for d2 in dirs[field]:
        if d!=d2:
            print "  for (int %s = 0 ; %s < N%s; ++%s ){"%(d2,d2,d2,d2)
    #print '    int z;'
    sten.mbf='main'
    body,result,_ = parse_body(sten,field,stag,d)
    for i in range(guards_[0]):
        print '    {'
        print '      int z=%d;'%i
        body,result,_ = parse_body(sten,field,stag,d,z0=i+1)
        print body
        print "      "+get_diff('c()',"result",field,d)+"=",
        if result:
            print result;
        else:
            print 'result_;'
        print '    }'
    print '    for (int z=%d;z<Nz-%d;++z) {'%(guards_[0],guards_[1])
    body,result,_ = parse_body(sten,field,stag,d)
    print body
    print "      "+get_diff('c()',"result",field,d)+"=",
    if result:
        print result;
    else:
        print 'result_;'
    print '    }'
    for i in range(guards_[1],0,-1):
        print '    {'
        print '      int z=Nz-%d;'%i
        body,result,_ = parse_body(sten,field,stag,d,z0=-i)
        print body
        print "      "+get_diff('c()',"result",field,d)+"=",
        if result:
            print result;
        else:
            print 'result_;'
        print '    }'
    for d2 in dirs[field]:
        if d!=d2:
            print "  }"
    print '  } else {'
    for d2 in dirs[field]:
        if d!=d2:
            print "  for (int %s = 0 ; %s < N%s; ++%s ){"%(d2,d2,d2,d2)
    print '    for (int z=0;z<Nz;++z) {'
    body,result,_ = parse_body(sten,field,stag,d,z0='secure')
    print body
    print "      "+get_diff('c()',"result",field,d)+"=",
    if result:
        print result;
    else:
        print 'result_;'
    print '    }'
    for d2 in dirs[field]:
        if d!=d2:
            print "  }"
    print '  }'
                                                # if d=='z': # aka z                                                 
def get_for_loop(d,mode,field,guards,sten_name ):
    if sten_name == 'main':
        dp=guards[0]
        dm=-guards[1]
        for d2 in dirs[field]:
            if d==d2:
                print "  for (int %s = %d; %s < N%s%+d; ++%s ){"%(d,dp,d,d,dm,d)
            else:
                print "  for (int %s = 0 ; %s < N%s; ++%s ){"%(d2,d2,d2,d2)
    else:
        #if max(guards_) < 1:
            
            #print >>sys.stderr,guards_
            #print >>sys.stderr,d,mode,field,guards,sten_name
            #fuu
        if sten_name == 'forward':
            print "    int "+d,"=%d ;"%(guards_[0]-1)
        else:
            print "    "+d,"=N%s"%d,"-%d ;"%(guards_[1])
            if guards_[1]>2:
                print >>sys.stderr,guards_
        for d2 in perp_dir[field][d]:
            print "    for (int "+d2,"=0; "+d2,"< N"+d2,";++"+d2,") {"
            

def get_for_end(d,field, sten_name):
    if sten_name != 'main':
        for d2 in perp_dir[field][d]:
            print "    }"
    else:
        for d2 in dirs[field]:
            print "  }"
    #if sten_name == 'backward':
    #    print "  }"

def get_diff(diff,fname,field,d,update=False,z0=None):
    #diff=
    #print >> sys.stderr, diff
    global use_field_operator
    if use_field_operator:
        ret=fname+"("
    else:
        ret=fname+"_ptr["
    for d2 in dirs[field][1:]:
        if not use_field_operator:
            ret+="("
    first=True
    for d2 in dirs[field]:
        if not first:
            if use_field_operator:
                ret+=','
            else:
                ret+=')*N%s + '%d2
        first=False
        if (d2 == d):
            #do smart stuff
            global guards_
            diffd=diff2[diff]
            if update:
                if abs(diffd) > 2:
                    sdfadfasdf
                if diffd > guards_[1]:
                    guards_[1]=diffd
                elif -diffd > guards_[0]:
                    guards_[0]=-diffd
            #if d != 'z':
            if d=='z' and z0 is not None:
                # We want to generate code for the interp_to case with wrapping
                #print >> sys.stderr, z0, diffd
                if z0 == 'secure':
                    ret+="+(("+d+"%+d"%(diffd)
                    if diffd < 0:
                        ret+='+%d*Nz'%(-diffd)
                    ret+=')%Nz)'
                elif z0 > 0 and -diffd >= z0:
                    ret+=d2+"%+d+Nz"%(diffd)
                elif z0 < 0 and -diffd <= z0:
                    ret+=d2+"%+d-Nz"%(diffd)
                else:
                    ret+=d2+"%+d"%(diffd)
            else:
                ret+=d2+"%+d"%(diffd)
            #else:
            #    if diffd > 0:
            #        ret+='z'+'p'*(diffd)
            #    else:
            #        ret+='z'+'m'*(-diffd)
        else:
            ret+=d2
    if use_field_operator:
        ret+=")\n    \t\t\t\t"
    else:
        ret+="]\n    \t\t\t\t"
    return ret
    # if diff != 'c()':
    #     return "%s[i.%s%s]"%(fname,d,diff)
    # else:
    #     return "%s[i]"%(fname)

def get_pointer(field, field_type,const):
    if const:
        print "  const BoutReal * __restrict__",
    else:
        print "  BoutReal * __restrict__",
    print "%s_ptr = &"%field,field,"(",
    first=True
    for d in dirs[field_type]:
        if not first:
            print ',',
        first=False
        print "0",
    print ");"
        

def gen_functions_normal(to_gen):
    import sys
    #for f_ar in to_gen:
    #    print >>sys.stderr,f_ar[0],f_ar[1]
    #exit(1)
    for f_ar in to_gen:
        mode=f_ar[3]
        #for d in dir:
        d=f_ar[2]
        field=f_ar[1]
        #tmp="cart_diff_%s_%s_%%s"%(d,f.name)
        #for mode in modes:
        name=f_ar[0]
        flux=f_ar[-1]
        warn()
        print "static void "+name+"_"+field.lower()+"(BoutReal * __restrict__ result_ptr,",
        if flux:
            print "const BoutReal * __restrict__ v_in_ptr,",
            print "const BoutReal * __restrict__ f_in_ptr) {"
        else:
            print "const BoutReal * __restrict__ in_ptr) {"
        for d2 in dirs[field]:
            print "  const int N%s = mesh->LocalN%s;"%(d2,d2)
        stencils={'main':None,
                  'forward':None,
                  'backward':None}
        for func in functions:
            if func.name==f_ar[4]:
                stencils['main']=func
                stencils['main'].mbf='main'
            if func.name==f_ar[5]:
                stencils['forward']=func
                stencils['forward'].mbf='forward'
            if func.name==f_ar[6]:
                stencils['backward']=func
                stencils['backward'].mbf='backward'
        #start with main file:
        todo=['main','forward','backward']
        if d=='z':
            todo=[todo[0]]
        # only reset before main
        global guards_
        guards_=[0,0]
        for sten_name in todo:
            sten=stencils[sten_name]
            if sten is None:
                import sys
                print >>sys.stderr,f_ar
                for func in functions:
                    print >>sys.stderr,func.name
                print >>sys.stderr,stencils
                print >>sys.stderr,"#error unexpected: sten is None for sten_name %s !"%sten_name
                exit(1)
            if sten_name=='main':
                try:
                    guards=stencils[sten_name].guards #numGuards[f_ar[4]]
                except:
                    print >>sys.stderr,stencils
                    print >>sys.stderr,sten_name
                    raise
            if sten_name=='backward' and mode=='on' and guards ==1:
                #print "  }"
                continue;
            if sten_name=='forward' and mode=='off' and guards ==1:
                #print "  if (mesh->%sstart > 0){"%d
                #print "    DataIterator i(0,mesh->LocalNx,0,mesh->LocalNy,0,mesh->LocalNz);"
                print "    int",d,";"
                continue;
            if d=='z':
                get_for_loop_z(sten,field,mode)
            else:
                #print >>sys.stderr, sten.name, sten.mbf
                body, result, result_ = parse_body(sten,field,mode,d)
                get_for_loop(d,mode,field,guards_,sten_name)
                if body+result == '' and sten.mbf == 'main':
                    print >> sys.stderr, sten.body
                    fuuuu
                print body
                if sten.mbf == 'main':
                    print "      "+get_diff('c()',"result",field,d)+"=",
                    if result:
                        print result;
                    else:
                        print 'result_;'
                else:
                    #if sten_name != 'main':
                    if result_[0] != '':
                        print "      "+get_diff('c()',"result",field,d)+"="+result_[0]
                    else:
                        import sys
                        print >>sys.stderr,result_, body,sten.body
                        print "      "+get_diff('c()',"result",field,d)+"=result_.inner;"
                    #print "      if (mesh->%sstart >1 ){"%d

                    if guards > 1:
                        #print >> sys.stderr, stencils[todo[0]].body[0]
                        #print >> sys.stderr, stencils[todo[0]].guards
                        if ( sten_name == 'backward' and mode == 'on' ) or \
                           ( sten_name == 'forward' and mode == 'off' ):
                            pass# dont do anything ...
                        else:
                            if sten_name == 'forward':
                                print "        "+get_diff('m()',"result",field,d)+"=" ,
                            else:
                                print "        "+get_diff('p()',"result",field,d)+"=" ,
                            if result_[1] != '':
                                print result_[1]
                            else:
                                print "result_.outer;"
                    #print "      }"
                get_for_end(d,field,sten_name)
            
        print "}"
        warn()
        print "static",field,name,"(const",field,
        if flux:
            print "&v_in, const",field,"&f_in){"
        else:
            print "&in){"
        print '  //output.write("Using method %s!\\n");'%name
        print '#ifdef CHECK'
        print '  if (mesh->LocalN%s < %d) {'%(d,sum(guards_)+1)
        print '    throw BoutException("AiolosMesh::%s - Not enough guards cells to take derivative!");'%(name)
        print '  }'
        print '#endif'
        print " ",field,"result;"
        print "  result.allocate();"
        print "  checkData(result);"
        get_pointer("result",field,False)
        if flux:
            get_pointer("v_in",field,True)
            get_pointer("f_in",field,True)
        else:
            get_pointer("in",field,True)
        if flux:
            print "  "+name+"_"+field.lower()+"(result_ptr,v_in_ptr,f_in_ptr);"
        else:
            print "  "+name+"_"+field.lower()+"(result_ptr,in_ptr);"
        if mode == "on":
            print "  result.setLocation(CELL_%sLOW);"%d.upper()
        elif mode == "off":
            print "  result.setLocation(CELL_CENTRE);"
        else:
            if flux:
                print "  result.setLocation(f_in.getLocation());"
            else:
                print "  result.setLocation(in.getLocation());"
        print """  checkData(result);
  return result;
}
"""
#exit(1)


field=fields[0]#3d
use_field_operator=True
def get_interp_vals(order,pos):
    import numpy as np
    rhs=np.zeros(order)
    rhs[0]=1
    mat=np.zeros((order,order))
    x0=-pos+(order-1.)/2.
    #print >> sys.stderr, x0,pos,(order-1.)/2.,pos+(order-1.)/.2
    for i in range(order):
        x=x0-i;
        for j in range(order):
            mat[j,i]=x**j*np.math.factorial(j)
    #print mat
    facs=np.dot(np.linalg.inv(mat),rhs)
    return facs
def get_interp_sten(order,pos):
    if order%2:
        print >>sys.stderr, "Cannot handle uneven order!"
        exit(4)
    oh=order/2
    if pos:
        oh-=pos/abs(pos)
    vals=get_interp_vals(order,pos)
    ret=""
    first=True
    for i in range(order):
        #if not first:
        #    ret+="+"
        #first=False
        ret+="%+.5e*"%vals[i]
        if i < oh:
            ret+='f.m'+(str(oh-i) if oh-i > 1 else "")
        else:
            ret+='f.p'+(str(i-oh+1) if i-oh+1 > 1 else "" )
    #print >> sys.stderr, ret
    return ret+" ;"
            
interp=['',"return "+get_interp_sten(4,0),'']
#["return ( 9.*(s.m + s.p) - s.mm - s.pp ) / 16.;"]
for mode in ['on','off']:
  for order in [4]:
    for d in dirs[field]:
        global guards_
        guards_=[0,0]
        line=interp[1]
        sten_name="main"
        line=replace_stencil(line,'f.',"in",field,mode,sten_name,d)
        print "static void interp_to_%s_%s_%s("%(mode,field,d),
        if use_field_operator:
            print field+"& result, const "+field+" & in){"
        else:
            print "BoutReal * __restrict__ result_ptr, const BoutReal * __restrict__ in_ptr){"
        for d2 in dirs[field]:
            print "  const int N%s = mesh->LocalN%s;"%(d2,d2)
        if d == 'z':
            sten=function()
            sten.body=interp
            get_for_loop_z(sten,field,mode)
        else:
            body= "    "+get_diff('c()',"result",field,d)+"= "+line[len("return")+line.index("return"):]+"\n"
            #print >> sys.stderr , guards_
            get_for_loop(d,mode,field,guards_,sten_name)
            print body
            guards__=guards_
            get_for_end(d,field,sten_name)
            #if d != 'z':
            sten_names=["forward","backward"]
            for sten_name in sten_names:
                sten_name_index=sten_names.index(sten_name)
                sign=-1
                if sten_name == "forward":
                    sign=1
                _sign=sign
                if d=='z':
                    sign=0
                get_for_loop(d,mode,field,guards_,sten_name)
                print "      "+get_diff('c()',"result",field,d)+"=",
                print replace_stencil(get_interp_sten(4,sign),'f.',"in",field,mode,"main",d,False,z0=((guards_[sten_name_index])*_sign))
                #print '      throw BoutException("AiolosMesh - not yet done properly :P - don`t use Aiolos ...");'
                #print >> sys.stderr, guards_, guards__
                guards_=guards__
                if order/2 > 1:
                    if ( sten_name == 'backward' and mode == 'on' ) or \
                       ( sten_name == 'forward' and mode == 'off' ):
                        pass# dont do anything ...
                    else:
                        if sten_name == 'forward':
                            print "        "+get_diff('m()',"result",field,d)+"=" ,
                        else:
                            print "        "+get_diff('p()',"result",field,d)+"=" ,
                        print replace_stencil(get_interp_sten(4,sign*2),'f.',"in",field,mode,"main",d,False,z0=((guards_[sten_name_index])*_sign))
                        guards_=guards__
                get_for_end(d,field,sten_name)
            #else:
            
        print "}"

print "const Field3D AiolosMesh::interp_to_do(const Field3D &f, CELL_LOC loc) const {"
print "  Field3D result;"
print "  result.allocate();"
print "  if (f.getLocation() != CELL_CENTRE){"
print "    // we first need to go back to centre before we can go anywhere else"
print "    switch (f.getLocation()){"
for d in dirs[field]:
    print "    case CELL_%sLOW:"%d.upper()
    if use_field_operator:
        print "      interp_to_off_%s_%s(result,f);"%(field,d)
    else:
        print "      interp_to_off_%s_%s(&result(0,0,0),&f(0,0,0));"%(field,d)
    print "      result.setLocation(CELL_CENTRE);"
    print "      // return or interpolate again"
    print "      return interp_to(result,loc);"
    print "      break;"
print "    default:"
print '      throw BoutException("AiolosMesh::interp_to: Cannot interpolate to %s!",strLocation(loc));'
print "    }"
print "  }"
print "  // we are in CELL_CENTRE and need to go somewhere else ..."
print "  switch (loc){"
for d in dirs[field]:
    print "    case CELL_%sLOW:"%d.upper()
    if use_field_operator:
        print "      interp_to_on_%s_%s(result,f);"%(field,d)
    else:
        print "      interp_to_on_%s_%s(&result(0,0,0),&f(0,0,0));"%(field,d)
    print "      result.setLocation(CELL_%sLOW);"%d.upper()
    print "      // return or interpolate again"
    print "      return interp_to(result,loc);"
    print "      break;"
print "    default:"
print '      throw BoutException("AiolosMesh::interp_to: Cannot interpolate to %s!",strLocation(loc));'
print "    }"
print "}"

use_field_operator=False

# # print at end, so functions are defined
# d=dir[0]
# for entry in func_db[d][:-1]:
#     print "BoutReal %s(stencil &f);"%entry[0]
# for d in dir:
#     print "static AiolosMesh::cart_diff_lookup_table diff_lookup_%s [] = {"%d
#     for entry in func_db[d]:
#         print "\t{",
#         t=''
#         for field in entry:
#             print t,field,"\t",
#             t=','
#         print '},'
#     print "};"




# for d in dir:
#     warn()
#     du=d.upper()
#     if d != "z":
#         print "const Field3D AiolosMesh::apply%sdiff(const Field3D &var, Mesh::deriv_func func, Mesh::inner_boundary_deriv_func func_in, Mesh::outer_boundary_deriv_func func_out, CELL_LOC loc ){"%du
#     else:
#         print "const Field3D AiolosMesh::apply%sdiff(const Field3D &var, Mesh::deriv_func func, CELL_LOC loc ){"%du
#     print """  cart_diff_lookup_table chosen;
#   for (int i=0;;++i){
#     output.write("maybe: %sp == %sp?\\n",diff_lookup_%s[i].func,func);
#     if (diff_lookup_%s[i].func==func){
#       chosen=diff_lookup_%s[i];
#       break;
#     }
#     if (diff_lookup_%s[i].func==NULL){
#       throw BoutException("Diff method not found!");
#     }
#   }
#   if ( loc == CELL_%sLOW && var.getLocation() != CELL_%sLOW ){
#     auto result=chosen.on(var);
#     return result;
#   } else if ( var.getLocation() == CELL_%sLOW  && ! (loc == CELL_CENTRE )){
#     return chosen.off(var);
#   } else {
#     if (loc != var.getLocation() && loc != CELL_DEFAULT) throw BoutException("Didn't expect this - I wouldn't interpolate");
#     auto result=chosen.norm(var);
#     return result;
#   }
# }
# """%('%','%',d,d,d,d,du,du,du)
