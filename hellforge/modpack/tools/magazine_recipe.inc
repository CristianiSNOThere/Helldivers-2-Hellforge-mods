/* Rebuild the accepted magazine from local stock units and authored operations.
 * The input archive comes from magazine-inputs.json; no cooked mesh is embedded.
 */
#include <math.h>

typedef double MagMatrix[4][4];
typedef struct MagUnit { uint64_t id; const unsigned char *cpu,*gpu; uint32_t cpu_size,gpu_size; } MagUnit;
typedef struct MagBuffer { unsigned char *bytes; size_t size,capacity; } MagBuffer;

static float mag_float(const unsigned char *p) { float v; memcpy(&v,p,4); return v; }
static void mag_write_float(unsigned char *p,double value) { float v=(float)value; memcpy(p,&v,4); }
static uint16_t mag_u16(const unsigned char *p) { uint16_t v; memcpy(&v,p,2); return v; }
static void mag_identity(MagMatrix m) { memset(m,0,sizeof(MagMatrix)); for(unsigned i=0;i<4;++i)m[i][i]=1; }
static void mag_multiply(const MagMatrix a,const MagMatrix b,MagMatrix out) {
    MagMatrix result={{0}};
    for(unsigned i=0;i<4;++i) for(unsigned j=0;j<4;++j)
        for(unsigned k=0;k<4;++k) result[i][j]+=a[i][k]*b[k][j];
    memcpy(out,result,sizeof(result));
}
static int mag_inverse(const MagMatrix matrix,MagMatrix out) {
    double a[4][8]={{0}};
    for(unsigned i=0;i<4;++i) { for(unsigned j=0;j<4;++j)a[i][j]=matrix[i][j]; a[i][i+4]=1; }
    for(unsigned col=0;col<4;++col) {
        unsigned pivot=col;
        for(unsigned row=col+1;row<4;++row)if(fabs(a[row][col])>fabs(a[pivot][col]))pivot=row;
        if(fabs(a[pivot][col])<1e-20)return 0;
        if(pivot!=col)for(unsigned k=0;k<8;++k){double t=a[col][k];a[col][k]=a[pivot][k];a[pivot][k]=t;}
        double divisor=a[col][col];
        for(unsigned k=0;k<8;++k)a[col][k]/=divisor;
        for(unsigned row=0;row<4;++row)if(row!=col) {
            double scale=a[row][col];
            for(unsigned k=0;k<8;++k)a[row][k]-=scale*a[col][k];
        }
    }
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)out[i][j]=a[i][j+4];
    return 1;
}
static void mag_world(const unsigned char *unit,unsigned node,MagMatrix out) {
    uint32_t joint=read32(unit+52),count=read32(unit+joint),start=joint+16;
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
        out[row][col]=mag_float(unit+start+count*64u+node*64u+(col*4u+row)*4u);
}
static int mag_node(const unsigned char *unit,uint32_t hash,unsigned *node) {
    uint32_t joint=read32(unit+52),count=read32(unit+joint),names=joint+16+count*132u;
    unsigned found=0;
    for(unsigned i=0;i<count;++i)if(read32(unit+names+i*4u)==hash){*node=i;++found;}
    return found==1;
}
static int mag_set_world(unsigned char *out,const unsigned char *child,unsigned node,const MagMatrix desired) {
    uint32_t joint=read32(child+52),count=read32(child+joint),start=joint+16;
    unsigned parent=mag_u16(child+start+count*128u+node*4u+2u);
    MagMatrix world,inverse,local;
    mag_world(child,parent,world);
    if(!mag_inverse(world,inverse))return 0;
    mag_multiply(inverse,desired,local);
    unsigned char *lo=out+start+node*64u,*go=out+start+count*64u+node*64u;
    for(unsigned col=0;col<3;++col)for(unsigned row=0;row<3;++row)
        mag_write_float(lo+(col*3u+row)*4u,local[row][col]);
    for(unsigned row=0;row<3;++row)mag_write_float(lo+(9u+row)*4u,local[row][3]);
    for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)
        mag_write_float(go+(col*4u+row)*4u,desired[row][col]);
    return 1;
}
static int mag_append(MagBuffer *out,const void *data,size_t length,uint32_t *at) {
    size_t aligned=(out->size+15u)&~(size_t)15;
    if(aligned>out->capacity || length>out->capacity-aligned)return 0;
    memcpy(out->bytes+aligned,data,length);*at=(uint32_t)aligned;out->size=aligned+length;return 1;
}
static uint32_t mag_remap(uint32_t value,const uint32_t old[4],const uint32_t replacement[4]) {
    for(unsigned i=0;i<4;++i)if(value==old[i])return replacement[i];
    return value;
}
static int mag_rebuild(const MagUnit units[4],unsigned char **cpu_result,unsigned char **gpu_result) {
    const unsigned char *child=units[0].cpu,*parent=units[1].cpu,*grip=units[2].cpu,*donor=units[3].cpu;
    const unsigned char *donor_gpu=units[3].gpu;
    if(units[0].cpu_size!=6352 || units[1].cpu_size!=15120 || units[2].cpu_size!=3440
        || units[3].cpu_size!=4416 || units[3].gpu_size!=212618)return 0;
    MagBuffer out={(unsigned char *)calloc(15776,1),6352,15776};
    unsigned char *gpu=(unsigned char *)malloc(156362);
    if(!out.bytes || !gpu) {free(out.bytes);free(gpu);return 0;}
    memcpy(out.bytes,child,6352);
    uint32_t joint=read32(child+52),count=read32(child+joint),start=joint+16,hashes[19];
    if(count!=19)goto fail;
    for(unsigned i=0;i<19;++i)hashes[i]=read32(child+start+count*132u+i*4u);
    unsigned mount=0,goal=0;
    MagMatrix pm,inverse,desired,target;
    if(!mag_node(parent,0x7950E36Eu,&mount))goto fail;
    mag_world(parent,mount,pm);
    if(!mag_inverse(pm,inverse))goto fail;
    if(!mag_node(parent,0x57520ABFu,&goal))goto fail;
    mag_world(parent,goal,target);mag_multiply(inverse,target,desired);
    if(!mag_set_world(out.bytes,child,13,desired))goto fail;
    if(!mag_node(parent,0x67C94E4Bu,&goal))goto fail;
    mag_world(parent,goal,target);mag_multiply(inverse,target,desired);
    if(!mag_set_world(out.bytes,child,12,desired))goto fail;
    write32(out.bytes+read32(child+48),0);write32(out.bytes+read32(child+100),0);
    uint32_t base=0,mesh=read32(grip+100),old_hashes[4],replacement[4];
    if(!mag_append(&out,grip,3440,&base) || read32(grip+mesh)!=4)goto fail;
    for(unsigned i=0;i<4;++i) {
        uint32_t pos=mesh+read32(grip+mesh+4+i*4u);
        old_hashes[i]=read32(grip+pos+40);replacement[i]=hashes[15+i];
        write32(out.bytes+base+pos+40,hashes[15+i]);write32(out.bytes+base+pos+44,15+i);
        write32(out.bytes+base+pos+48,15+i);write32(out.bytes+base+mesh+20+i*4u,hashes[15+i]);
    }
    uint32_t lod=read32(grip+48),lod_count=read32(grip+lod);
    if(lod_count!=2)goto fail;
    for(unsigned i=0;i<lod_count;++i) {
        uint32_t pos=lod+read32(grip+lod+4+i*4u);
        write32(out.bytes+base+pos+4,mag_remap(read32(grip+pos+4),old_hashes,replacement));
    }
    unsigned char table[1316]={0};write32(table,8);
    size_t table_size=68;
    for(unsigned copy=0;copy<2;++copy)for(unsigned i=0;i<4;++i) {
        uint32_t pos=mesh+read32(grip+mesh+4+i*4u);
        uint32_t nm=read32(grip+pos+104),mo=read32(grip+pos+108);
        uint32_t ng=read32(grip+pos+120),go=read32(grip+pos+124);
        size_t info_size=128;
        if(mo+nm*4u>info_size)info_size=mo+nm*4u;
        if(go+ng*24u>info_size)info_size=go+ng*24u;
        if(info_size!=156 || table_size+info_size>sizeof(table))goto fail;
        unsigned node=copy?17:15,index=copy*4+i;
        write32(table+4+index*4u,(uint32_t)table_size);write32(table+36+index*4u,hashes[node]);
        memcpy(table+table_size,out.bytes+base+pos,info_size);
        write32(table+table_size+40,hashes[node]);write32(table+table_size+44,node);write32(table+table_size+48,node);
        table_size+=info_size;
    }
    uint32_t ignored=0;
    if(!mag_append(&out,table,table_size,&ignored))goto fail;
    unsigned char lt[236]={0};size_t lt_size=12;write32(lt,2);
    for(unsigned i=0;i<2;++i) {
        uint32_t pos=lod+read32(grip+lod+4+i*4u),nf=read32(grip+pos+12),ne=read32(grip+pos+16);
        unsigned char group[256]={0};size_t group_size=20u+(nf+ne)*4u;
        if(nf>4 || ne>4 || group_size>sizeof(group))goto fail;
        memcpy(group,out.bytes+base+pos,20);
        for(unsigned k=0;k<ne;++k) {
            uint32_t ep=pos+read32(grip+pos+20+k*4u),ni=read32(grip+ep+8);
            if(ni>8 || group_size+12u+ni*8u>sizeof(group))goto fail;
            write32(group+20+k*4u,(uint32_t)group_size);memcpy(group+group_size,grip+ep,8);
            write32(group+group_size+8,ni*2u);
            for(unsigned v=0;v<ni;++v) {
                uint32_t index=read32(grip+ep+12+v*4u);
                write32(group+group_size+12+v*4u,index);
                write32(group+group_size+12+ni*4u+v*4u,index+4u);
            }
            group_size+=12u+ni*8u;
        }
        for(unsigned k=0;k<nf;++k) {
            uint32_t fp=pos+read32(grip+pos+20+ne*4u+k*4u);
            if(group_size+40>sizeof(group))goto fail;
            write32(group+20+ne*4u+k*4u,(uint32_t)group_size);memcpy(group+group_size,grip+fp,40);
            static const double bounds[6]={-.04,-.12,-.06,.04,.15,.13};
            for(unsigned v=0;v<6;++v)mag_write_float(group+group_size+v*4u,bounds[v]);
            group_size+=40;
        }
        if(lt_size+group_size>sizeof(lt))goto fail;
        write32(lt+4+i*4u,(uint32_t)lt_size);memcpy(lt+lt_size,group,group_size);lt_size+=group_size;
    }
    if(lt_size!=236 || !mag_append(&out,lt,lt_size,&ignored) || (out.size+15u)/16u*16u!=11360)goto fail;
    if(!mag_append(&out,donor,4416,&base) || base!=11360 || out.size!=15776)goto fail;
    static const unsigned fields[5]={48,92,96,100,112};
    for(unsigned i=0;i<5;++i)write32(out.bytes+fields[i],base+read32(donor+fields[i]));
    uint32_t layout=read32(donor+92),lp=layout+read32(donor+layout+4);
    if(read32(donor+lp+352)!=4688 || read32(donor+lp+356)!=40
        || read32(donor+lp+416)!=0 || read32(donor+lp+420)!=187520
        || read32(donor+lp+424)!=187520 || read32(donor+lp+428)!=25098)goto fail;
    for(unsigned v=0;v<4688;++v)memcpy(gpu+v*28u,donor_gpu+v*40u+4,28);
    memcpy(gpu+4688u*28u,donor_gpu+187520,25098);
    const _Float16 blank_u=(_Float16).002910614013671875,blank_v=(_Float16).9951171875;
    for(unsigned v=0;v<4167;++v) {memcpy(gpu+v*28u+24,&blank_u,2);memcpy(gpu+v*28u+26,&blank_v,2);}
    memcpy(out.bytes+base+lp+8,donor+lp+28,100);memset(out.bytes+base+lp+108,0,60);
    write32(out.bytes+base+lp+328,5);write32(out.bytes+base+lp+356,28);
    write32(out.bytes+base+lp+420,4688u*28u);write32(out.bytes+base+lp+424,4688u*28u);
    mag_identity(desired);desired[0][0]=-1;desired[2][2]=-1;
    desired[1][3]=(float)-.160;desired[2][3]=(float).025;
    desired[1][3]=(float)(desired[1][3]-.020);desired[2][3]=(float)(desired[2][3]-.040);
    desired[2][3]=(float)(desired[2][3]+.010);
    double min_x=mag_float(donor_gpu+4),max_x=min_x,max_z=mag_float(donor_gpu+12);
    for(unsigned v=1;v<4688;++v) {
        double x=mag_float(donor_gpu+v*40u+4),z=mag_float(donor_gpu+v*40u+12);
        if(x<min_x)min_x=x;if(x>max_x)max_x=x;if(z>max_z)max_z=z;
    }
    MagMatrix expand;mag_identity(expand);expand[0][0]=1.10;expand[2][2]=1.10;
    expand[0][3]=(1.-1.10)*(min_x+max_x)/2.;expand[2][3]=(1.-1.10)*max_z;
    mag_multiply(desired,expand,desired);
    mesh=read32(donor+100);
    if(read32(donor+mesh)!=4)goto fail;
    for(unsigned i=0;i<4;++i) {
        uint32_t pos=mesh+read32(donor+mesh+4+i*4u);
        old_hashes[i]=read32(donor+pos+40);replacement[i]=hashes[15+i];
        if(!mag_set_world(out.bytes,child,15+i,desired))goto fail;
        write32(out.bytes+base+pos+40,hashes[15+i]);write32(out.bytes+base+pos+44,15+i);
        write32(out.bytes+base+pos+48,15+i);write32(out.bytes+base+pos+56,UINT32_MAX);
        write32(out.bytes+base+mesh+20+i*4u,hashes[15+i]);
    }
    lod=read32(donor+48);
    for(unsigned i=0;i<read32(donor+lod);++i) {
        uint32_t pos=lod+read32(donor+lod+4+i*4u);
        write32(out.bytes+base+pos+4,mag_remap(read32(donor+pos+4),old_hashes,replacement));
    }
    uint32_t material=base+read32(donor+112);
    write32(out.bytes+material,2);write32(out.bytes+material+4,0x161021F9u);write32(out.bytes+material+8,0x63F5AACDu);
    write64(out.bytes+material+12,UINT64_C(0x7CD9495D4A4B7477));write64(out.bytes+material+20,UINT64_C(0x5990E5EFBCA8AE21));
    *cpu_result=out.bytes;*gpu_result=gpu;return 1;
fail:
    free(out.bytes);free(gpu);return 0;
}

static int prepare_magazine(const wchar_t *input,const wchar_t *output) {
    size_t size=0,gpu_size=0;
    unsigned char *archive=read_file(input,&size),*input_gpu=NULL,*cpu=NULL,*gpu=NULL,*built=NULL;
    wchar_t *gpu_path=joined(input,L".gpu_resources",L"");
    if(gpu_path)input_gpu=read_file(gpu_path,&gpu_size);
    free(gpu_path);char digest[65];int result=3;
    if(!archive || !input_gpu || !empty_sidecar(input,L".stream")
        || !sha256(archive,size,digest) || strcmp(digest,"FAA18377E4AEC0E6B7F961507B79D7B47AB70C5DEDA0026CDD4A85E7AE8AF489")
        || !sha256(input_gpu,gpu_size,digest) || strcmp(digest,"913DB52A296D78D93CC0EE80DC29E52128ED5B9BF93DBA065CEEDDE96BEFC8DD")) {
        fprintf(stderr,"Stock magazine inputs differ; no output created.\n");goto done;
    }
    MagUnit units[4]={{0}};
    static const uint64_t ids[4]={UINT64_C(0x02CD7321CD8445F5),UINT64_C(0x05D8D8C073B9D502),
        UINT64_C(0x411C4FD07740A7DD),UINT64_C(0x48D2DDC8E9A80B1E)};
    result=4;
    if(read32(archive+4)!=1 || read32(archive+8)!=4)goto done;
    for(unsigned i=0;i<4;++i) {
        const unsigned char *row=archive+104+i*80u;
        uint64_t offset=read64(row+16),go=read64(row+32);
        uint32_t length=read32(row+56),gl=read32(row+64);
        if(read64(row)!=ids[i] || read64(row+8)!=UINT64_C(0xE0A48D0BE9A7453F)
            || offset>size || length>size-offset || go>gpu_size || gl>gpu_size-go)goto done;
        units[i]=(MagUnit){ids[i],archive+(size_t)offset,input_gpu+(size_t)go,length,gl};
    }
    if(!mag_rebuild(units,&cpu,&gpu)) {fprintf(stderr,"Magazine recipe layout failed.\n");goto done;}
    if(!sha256(cpu,15776,digest) || strcmp(digest,"643730C515148BE727E3E47A4A28B1755325224D1B76E04D6664DB87B310356E")) {
        fprintf(stderr,"Magazine CPU recipe mismatch: %s\n",digest);goto done;
    }
    if(!sha256(gpu,156362,digest) || strcmp(digest,"F2DD3B4EB32D0337525EDE383815EF6C141A31B4702F85E5DAABEC3D680A07C3")) {
        fprintf(stderr,"Magazine GPU recipe mismatch: %s\n",digest);goto done;
    }
    /* Canonical single-resource archive, as HellForge ResourceArchive.Write. */
    built=(unsigned char *)calloc(256+15776,1);if(!built)goto done;
    write32(built,0xF0000011u);write32(built+4,1);write32(built+8,1);
    write64(built+32,(15776u+255u)&~UINT64_C(255));write64(built+40,(156362u+255u)&~UINT64_C(255));
    write64(built+80,UINT64_C(0xE0A48D0BE9A7453F));write32(built+88,1);write32(built+96,16);write32(built+100,64);
    write64(built+104,UINT64_C(0x0B5D0EB081BEB889));write64(built+112,UINT64_C(0xE0A48D0BE9A7453F));write64(built+120,256);
    write32(built+160,15776);write32(built+168,156362);write32(built+172,16);write32(built+176,64);
    memcpy(built+256,cpu,15776);
    result=5;
    if(!CreateDirectoryW(output,NULL)) {fprintf(stderr,"Output must be new; refusing overwrite.\n");goto done;}
    result=6;
    static const wchar_t *const names[]={L"49f1972458f7ccec.patch_2",L"e16e0aa7740716a2.patch_2"};
    for(unsigned i=0;i<2;++i) {
        wchar_t *gn=joined(names[i],L".gpu_resources",L""),*sn=joined(names[i],L".stream",L"");
        int ok=gn && sn && write_new(output,names[i],built,256+15776)
            && write_new(output,gn,gpu,156362) && write_new(output,sn,"",0);
        free(gn);free(sn);if(!ok)goto done;
    }
    static const char receipt[]="{\n  \"result\": \"PASS\",\n  \"recipe\": \"arsenal-punisher-magazine-v1\",\n"
        "  \"build\": \"25480438\",\n  \"stock_units\": 4,\n  \"rebuilt_resource_instances\": 2\n}\n";
    if(!write_new(output,L"native-asset-preparation.json",receipt,(DWORD)strlen(receipt)))goto done;
    printf("{\"result\":\"PASS\",\"recipe\":\"arsenal-punisher-magazine-v1\",\"gpu_sha256\":\"%s\"}\n",digest);result=0;
done:
    free(archive);free(input_gpu);free(cpu);free(gpu);free(built);return result;
}
