/* Local asset preparation. Ships only this source and authored edit rules.
 * Input is built from the player's own bundles.nxa by HellForge build-data.
 * This tool never opens the installed game's files for writing.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "punisher_asset_dependencies.generated.h"

#define MAX_INPUT (64u * 1024u * 1024u)
static const char stock_archive_sha[] = "9BAFE9A5DBE8D219723AC573ED2FE4367FFE07F74DAC9831312A96D812F972CB";
static const char stock_resource_sha[] = "F8E38F6BEBA1AF4A8C880EBBF96C858714698A577A2EB16B6DBDC11644514571";
static const char accepted_resource_sha[] = "8ED0353BBA573BA4528E964EF50C6A19370D354D48D4E1507D00077F23C621F1";
static const uint32_t transition_offsets[] = {9070788u, 9115708u, 9124692u};

static uint32_t read32(const unsigned char *p) { uint32_t v; memcpy(&v,p,4); return v; }
static uint64_t read64(const unsigned char *p) { uint64_t v; memcpy(&v,p,8); return v; }
static void write32(unsigned char *p,uint32_t v) { memcpy(p,&v,4); }
static void write64(unsigned char *p,uint64_t v) { memcpy(p,&v,8); }

static int sha256(const unsigned char *bytes, size_t size, char hex[65]) {
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    DWORD object_size = 0, written = 0;
    unsigned char digest[32], *object = NULL;
    int ok = 0;
    if (size > MAX_INPUT) return 0;
    if (BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0) < 0) goto done;
    if (BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,(PUCHAR)&object_size,sizeof(object_size),&written,0) < 0) goto done;
    object = (unsigned char *)malloc(object_size);
    if (!object) goto done;
    if (BCryptCreateHash(alg,&hash,object,object_size,NULL,0,0) < 0) goto done;
    if (BCryptHashData(hash,(PUCHAR)bytes,(ULONG)size,0) < 0) goto done;
    if (BCryptFinishHash(hash,digest,sizeof(digest),0) < 0) goto done;
    for (unsigned i=0; i<32; ++i) snprintf(hex+2*i,3,"%02X",digest[i]);
    hex[64]=0; ok=1;
done:
    if (hash) BCryptDestroyHash(hash);
    free(object);
    if (alg) BCryptCloseAlgorithmProvider(alg,0);
    return ok;
}

static unsigned char *read_file(const wchar_t *path, size_t *length) {
    FILE *file = _wfopen(path,L"rb");
    unsigned char *bytes = NULL;
    if (!file) return NULL;
    if (_fseeki64(file,0,SEEK_END) != 0) goto done;
    long long size = _ftelli64(file);
    if (size < 0 || size > MAX_INPUT || _fseeki64(file,0,SEEK_SET) != 0) goto done;
    bytes=(unsigned char *)malloc((size_t)size+1u);
    if (!bytes) goto done;
    if (fread(bytes,1,(size_t)size,file)!=(size_t)size) { free(bytes); bytes=NULL; goto done; }
    *length=(size_t)size;
done:
    fclose(file);
    return bytes;
}

static wchar_t *joined(const wchar_t *left,const wchar_t *right,const wchar_t *separator) {
    size_t a=wcslen(left),b=wcslen(right),c=wcslen(separator);
    if (a+b+c>32000) return NULL;
    wchar_t *path=(wchar_t *)calloc(a+b+c+1,sizeof(wchar_t));
    if (path) { memcpy(path,left,a*sizeof(wchar_t)); memcpy(path+a,separator,c*sizeof(wchar_t)); memcpy(path+a+c,right,(b+1)*sizeof(wchar_t)); }
    return path;
}

static int empty_sidecar(const wchar_t *archive,const wchar_t *suffix) {
    wchar_t *path=joined(archive,suffix,L"");
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (!path) return 0;
    int found=GetFileAttributesExW(path,GetFileExInfoStandard,&info);
    DWORD error=found?ERROR_SUCCESS:GetLastError();
    free(path);
    if (!found) return error==ERROR_FILE_NOT_FOUND;
    return !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && info.nFileSizeHigh==0 && info.nFileSizeLow==0;
}

static int write_new(const wchar_t *folder,const wchar_t *name,const void *bytes,DWORD size) {
    wchar_t *path=joined(folder,name,L"\\");
    if (!path) return 0;
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    free(path);
    if (file==INVALID_HANDLE_VALUE) return 0;
    DWORD written=0;
    int ok=WriteFile(file,bytes,size,&written,NULL) && written==size && FlushFileBuffers(file);
    CloseHandle(file);
    return ok;
}

static int prepare_palette(const wchar_t *input,const wchar_t *output) {
    static const char cpu_sha[]="12450BDDEE99118098E9BD8C809D8DDC92E3C3604D77E2BA9502837514BC05CA";
    static const char gpu_sha[]="10AC9A69575D3A68FA1EF0CD564D0D09361125ED1DD019D9667BF6554523CF1A";
    static const char after_gpu_sha[]="CC48E646FD78F8D9E11F41615B3A267B2A4CFC68E4092B1D8CD0D8524B0E2B42";
    static const wchar_t *const names[]={L"49f1972458f7ccec.patch_1",L"e16e0aa7740716a2.patch_1"};
    static const unsigned columns[]={0,5,6};
    size_t size=0,gpu_size=0;
    wchar_t *gpu_path=joined(input,L".gpu_resources",L"");
    unsigned char *bytes=read_file(input,&size),*gpu=gpu_path?read_file(gpu_path,&gpu_size):NULL;
    free(gpu_path);
    char digest[65];
    int result=3;
    if (!bytes || !gpu || gpu_size!=1984 || !empty_sidecar(input,L".stream")
        || !sha256(bytes,size,digest) || strcmp(digest,cpu_sha)
        || !sha256(gpu,gpu_size,digest) || strcmp(digest,gpu_sha)) {
        fprintf(stderr,"Private material/LUT source fingerprint failed; no output created.\n"); goto done;
    }
    /* The accepted finish has two authored stages, each rounded to stored binary16:
     * white-row RGB at columns 0,5,6 is first multiplied by .9, then by 2/3.
     * All other rows, alpha values and lower mip bytes remain untouched.
     */
    for (unsigned c=0;c<3;++c) for (unsigned rgb=0;rgb<3;++rgb) {
        size_t at=(23u+columns[c])*8u+rgb*2u;
        _Float16 value;
        memcpy(&value,gpu+at,2);
        value=(_Float16)((double)value*0.9);
        value=(_Float16)((double)value*2.0/3.0);
        memcpy(gpu+at,&value,2);
    }
    result=4;
    if (!sha256(gpu,gpu_size,digest) || strcmp(digest,after_gpu_sha)) {
        fprintf(stderr,"Rebuilt private LUT differs from the accepted source.\n"); goto done;
    }
    result=5;
    if (!CreateDirectoryW(output,NULL)) { fprintf(stderr,"Output must be a new directory; refusing overwrite.\n"); goto done; }
    result=6;
    for (unsigned i=0;i<2;++i) {
        wchar_t *gpu_name=joined(names[i],L".gpu_resources",L"");
        wchar_t *stream_name=joined(names[i],L".stream",L"");
        int ok=gpu_name && stream_name && write_new(output,names[i],bytes,(DWORD)size)
            && write_new(output,gpu_name,gpu,(DWORD)gpu_size) && write_new(output,stream_name,"",0);
        free(gpu_name);free(stream_name);
        if (!ok) { fprintf(stderr,"Private asset output incomplete.\n"); goto done; }
    }
    static const char receipt[]=
        "{\n  \"result\": \"PASS\",\n  \"recipe\": \"arsenal-punisher-palette-v1\",\n"
        "  \"build\": \"25480438\",\n  \"rebuilt_resource_instances\": 4,\n"
        "  \"lut_gpu_sha256\": \"8CC3FBA6D9D9BF3274686E6E0F1BA7AA2A1C1C70565C3254C192EE1E37C54ED2\"\n}\n";
    if (!write_new(output,L"native-asset-preparation.json",receipt,(DWORD)strlen(receipt))) {
        fprintf(stderr,"Private asset receipt was not written.\n"); goto done;
    }
    printf("{\"result\":\"PASS\",\"recipe\":\"arsenal-punisher-palette-v1\",\"gpu_file_sha256\":\"%s\"}\n",digest);
    result=0;
done:
    free(bytes);free(gpu);
    return result;
}

static int prepare_dependencies(const wchar_t *input,const wchar_t *output) {
    size_t size=0;
    unsigned char *bytes=read_file(input,&size);
    char digest[65];
    int result=3;
    if (!bytes || !sha256(bytes,size,digest)
        || strcmp(digest,"72333EB7AD7C3D5827E3020C70EE102C348316FCE17A97B81A472E982656B815")
        || !empty_sidecar(input,L".stream") || !empty_sidecar(input,L".gpu_resources")) {
        fprintf(stderr,"Stock startup package fingerprint failed; no output created.\n"); goto done;
    }
    uint64_t offset=read64(bytes+120);
    uint32_t length=read32(bytes+160);
    result=4;
    if (offset>size || length!=64832u || length!=size-offset
        || !sha256(bytes+(size_t)offset,length,digest)
        || strcmp(digest,"B9161444248CB140337B33E177E74A037827DECE44630BA7AB9B2DC5C3367D1C")
        || read32(bytes+(size_t)offset+8)!=4051u) {
        fprintf(stderr,"Stock dependency-list layout failed.\n"); goto done;
    }
    const size_t extra=sizeof(punisher_asset_dependencies);
    unsigned char *grown=(unsigned char *)realloc(bytes,size+extra);
    if (!grown) { fprintf(stderr,"Dependency buffer allocation failed.\n"); goto done; }
    bytes=grown;
    for (unsigned i=0;i<79;++i) {
        write64(bytes+size+16u*i,punisher_asset_dependencies[i][0]);
        write64(bytes+size+16u*i+8,punisher_asset_dependencies[i][1]);
    }
    write32(bytes+(size_t)offset+8,4130u);
    write32(bytes+160,length+(uint32_t)extra);
    write64(bytes+32,((uint64_t)length+extra+255u)&~UINT64_C(255));
    size+=extra;
    if (!sha256(bytes+(size_t)offset,length+extra,digest)
        || strcmp(digest,"8AA84DAE5E4E84B7B543078FBF4F21A14A8705A9AABBF67384477CD0C0584555")) {
        fprintf(stderr,"Rebuilt startup dependencies differ from the accepted source.\n"); goto done;
    }
    result=5;
    if (!CreateDirectoryW(output,NULL)) { fprintf(stderr,"Output must be a new directory; refusing overwrite.\n"); goto done; }
    result=6;
    static const char receipt[]=
        "{\n  \"result\": \"PASS\",\n  \"recipe\": \"arsenal-punisher-dependencies-v1\",\n"
        "  \"build\": \"25480438\",\n  \"appended_identifiers\": 79,\n  \"appended_lua_resources\": 0\n}\n";
    if (!write_new(output,L"9ba626afa44a3aa3.patch_0",bytes,(DWORD)size)
        || !write_new(output,L"9ba626afa44a3aa3.patch_0.gpu_resources","",0)
        || !write_new(output,L"9ba626afa44a3aa3.patch_0.stream","",0)
        || !write_new(output,L"native-asset-preparation.json",receipt,(DWORD)strlen(receipt))) {
        fprintf(stderr,"Startup dependency output incomplete.\n"); goto done;
    }
    printf("{\"result\":\"PASS\",\"recipe\":\"arsenal-punisher-dependencies-v1\",\"resource_sha256\":\"%s\"}\n",digest);
    result=0;
done:
    free(bytes);
    return result;
}

#include "magazine_recipe.inc"

int wmain(int argc,wchar_t **argv) {
    if (argc!=5 || (wcscmp(argv[1],L"--animation") && wcscmp(argv[1],L"--palette")
        && wcscmp(argv[1],L"--dependencies") && wcscmp(argv[1],L"--magazine")) || wcscmp(argv[3],L"--out")) {
        fprintf(stderr,"usage: native_asset_prepare (--animation | --palette | --dependencies | --magazine) <stock patch> --out <new directory>\n");
        return 2;
    }
    if (!wcscmp(argv[1],L"--palette")) return prepare_palette(argv[2],argv[4]);
    if (!wcscmp(argv[1],L"--dependencies")) return prepare_dependencies(argv[2],argv[4]);
    if (!wcscmp(argv[1],L"--magazine")) return prepare_magazine(argv[2],argv[4]);
    size_t size=0;
    unsigned char *bytes=read_file(argv[2],&size);
    char digest[65];
    int result=3;
    if (!bytes) { fprintf(stderr,"Could not read the bounded stock input.\n"); return result; }
    if (!sha256(bytes,size,digest) || strcmp(digest,stock_archive_sha)) {
        fprintf(stderr,"Refusing unknown stock archive; no output created.\n"); goto done;
    }
    result=4;
    if (size<184 || read32(bytes)!=0xF0000011u || read32(bytes+4)!=1 || read32(bytes+8)!=1
        || read64(bytes+80)!=UINT64_C(0xA486D4045106165C) || read32(bytes+88)!=1
        || read64(bytes+104)!=UINT64_C(0x4D1C334D294DFA97)
        || read64(bytes+112)!=UINT64_C(0xA486D4045106165C)
        || read32(bytes+164)!=0 || read32(bytes+168)!=0
        || !empty_sidecar(argv[2],L".stream") || !empty_sidecar(argv[2],L".gpu_resources")) {
        fprintf(stderr,"Animation resource ownership/layout or sidecar guard failed.\n"); goto done;
    }
    uint64_t offset=read64(bytes+120);
    uint32_t length=read32(bytes+160);
    if (length!=11323840u || offset>size || length>size-offset
        || !sha256(bytes+(size_t)offset,length,digest) || strcmp(digest,stock_resource_sha)) {
        fprintf(stderr,"Animation resource fingerprint failed.\n"); goto done;
    }
    for (unsigned i=0; i<sizeof(transition_offsets)/sizeof(transition_offsets[0]); ++i) {
        if (transition_offsets[i]>length-4 || read32(bytes+(size_t)offset+transition_offsets[i])!=UINT32_MAX) {
            fprintf(stderr,"Animation transition guard failed.\n"); goto done;
        }
    }
    for (unsigned i=0; i<sizeof(transition_offsets)/sizeof(transition_offsets[0]); ++i)
        write32(bytes+(size_t)offset+transition_offsets[i],40u);
    if (!sha256(bytes+(size_t)offset,length,digest) || strcmp(digest,accepted_resource_sha)) {
        fprintf(stderr,"Rebuilt animation differs from the accepted source.\n"); goto done;
    }
    result=5;
    if (!CreateDirectoryW(argv[4],NULL)) { fprintf(stderr,"Output must be a new directory; refusing overwrite.\n"); goto done; }
    result=6;
    static const char receipt[] =
        "{\n  \"result\": \"PASS\",\n  \"recipe\": \"arsenal-animation-v1\",\n"
        "  \"build\": \"25480438\",\n  \"authored_u32_edits\": 3,\n"
        "  \"resource_sha256\": \"8ED0353BBA573BA4528E964EF50C6A19370D354D48D4E1507D00077F23C621F1\"\n}\n";
    if (!write_new(argv[4],L"18235e0c9ec0e636.patch_0",bytes,(DWORD)size)
        || !write_new(argv[4],L"18235e0c9ec0e636.patch_0.gpu_resources","",0)
        || !write_new(argv[4],L"18235e0c9ec0e636.patch_0.stream","",0)
        || !write_new(argv[4],L"native-asset-preparation.json",receipt,(DWORD)strlen(receipt))) {
        fprintf(stderr,"Output incomplete; no acceptance receipt is valid.\n"); goto done;
    }
    printf("{\"result\":\"PASS\",\"recipe\":\"arsenal-animation-v1\",\"resource_sha256\":\"%s\"}\n",digest);
    result=0;
done:
    free(bytes);
    return result;
}
