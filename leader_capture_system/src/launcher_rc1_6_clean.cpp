#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "advapi32.lib")

static const char* kDllName = "HOI3_LeaderCapture.dll";
static const char* kRuntimeLogName = "HOI3_LEADER_CAPTURE_AUTO_TRANSFER_R06B.log";
static const char* kDefaultProcessName = "hoi3_tfh.exe";
static const char* kDefaultCompatibleNames = "hoi3_tfh.exe";

static void WaitBeforeExit() {
    printf("\nPress Enter to close this launcher window...\n");
    fflush(stdout);
    getchar();
}

static bool GetLauncherDir(char outDir[MAX_PATH]) {
    if(!GetModuleFileNameA(nullptr,outDir,MAX_PATH)) return false;
    char* slash=strrchr(outDir,'\\');
    if(!slash) return false;
    *slash=0;
    return true;
}

static bool IsAbsolutePath(const char* path) {
    if(!path || !path[0]) return false;
    if((path[0] && path[1]==':') || (path[0]=='\\' && path[1]=='\\')) return true;
    return false;
}

static void ResolveAgainstLauncherDir(const char* raw,char outPath[MAX_PATH]) {
    outPath[0]=0;
    if(!raw || !raw[0]) return;
    if(IsAbsolutePath(raw)) {
        strcpy_s(outPath,MAX_PATH,raw);
        return;
    }
    char dir[MAX_PATH]={};
    if(GetLauncherDir(dir)) sprintf_s(outPath,MAX_PATH,"%s\\%s",dir,raw);
    else strcpy_s(outPath,MAX_PATH,raw);
}

static const char* BaseNameOnly(const char* path) {
    if(!path)return "";
    const char* a=strrchr(path,'\\');
    const char* b=strrchr(path,'/');
    const char* p=a;
    if(!p || (b && b>p))p=b;
    return p?p+1:path;
}

static DWORD FindPid(const char* exe) {
    if(!exe || !exe[0]) return 0;
    HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(s==INVALID_HANDLE_VALUE)return 0;
    PROCESSENTRY32 pe={}; pe.dwSize=sizeof(pe);
    DWORD pid=0;
    if(Process32First(s,&pe)){
        do{
            if(_stricmp(pe.szExeFile,exe)==0){pid=pe.th32ProcessID;break;}
        }while(Process32Next(s,&pe));
    }
    CloseHandle(s); return pid;
}

static DWORD FindFromSemicolonList(const char* names,char outName[MAX_PATH]) {
    if(!names || !names[0]) return 0;
    char work[2048]={};
    strcpy_s(work,sizeof(work),names);
    char* ctx=nullptr;
    char* tok=strtok_s(work,";",&ctx);
    while(tok){
        while(*tok==' ' || *tok=='\t') tok++;
        char* end=tok+strlen(tok);
        while(end>tok && (end[-1]==' ' || end[-1]=='\t')) *--end=0;
        if(tok[0]){
            DWORD pid=FindPid(tok);
            if(pid){ if(outName) strcpy_s(outName,MAX_PATH,tok); return pid; }
        }
        tok=strtok_s(nullptr,";",&ctx);
    }
    return 0;
}

static bool AlreadyLoaded(DWORD pid,const char* dllName){
    HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    if(s==INVALID_HANDLE_VALUE)return false;
    MODULEENTRY32 me={};me.dwSize=sizeof(me);
    bool found=false;
    if(Module32First(s,&me)){
        do{ if(_stricmp(me.szModule,dllName)==0){found=true;break;} }
        while(Module32Next(s,&me));
    }
    CloseHandle(s);return found;
}

static bool Sha256File(const char* path,char outHex[65],unsigned long long* outSize){
    HANDLE f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(f==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER sz={};
    if(!GetFileSizeEx(f,&sz)){CloseHandle(f);return false;}
    if(outSize)*outSize=(unsigned long long)sz.QuadPart;
    HCRYPTPROV prov=0; HCRYPTHASH hash=0;
    if(!CryptAcquireContextA(&prov,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)){CloseHandle(f);return false;}
    if(!CryptCreateHash(prov,CALG_SHA_256,0,0,&hash)){CryptReleaseContext(prov,0);CloseHandle(f);return false;}
    BYTE buf[1<<15];DWORD got=0; bool ok=true;
    while(ReadFile(f,buf,sizeof(buf),&got,nullptr) && got){ if(!CryptHashData(hash,buf,got,0)){ok=false;break;} }
    BYTE digest[32];DWORD cb=sizeof(digest);
    if(ok)ok=!!CryptGetHashParam(hash,HP_HASHVAL,digest,&cb,0);
    if(ok){
        static const char* hexd="0123456789abcdef";
        for(int i=0;i<32;i++){outHex[i*2]=hexd[digest[i]>>4];outHex[i*2+1]=hexd[digest[i]&15];}
        outHex[64]=0;
    }
    CryptDestroyHash(hash);CryptReleaseContext(prov,0);CloseHandle(f); return ok;
}

struct Config {
    char gameExeRaw[MAX_PATH];
    char processName[MAX_PATH];
    char compatibleNames[2048];
    bool autoLaunch;
    DWORD waitMs;
};

static void ReadIni(Config* c){
    ZeroMemory(c,sizeof(*c));
    strcpy_s(c->processName,MAX_PATH,kDefaultProcessName);
    strcpy_s(c->compatibleNames,sizeof(c->compatibleNames),kDefaultCompatibleNames);
    c->autoLaunch=false; c->waitMs=60000;
    char dir[MAX_PATH]={}, ini[MAX_PATH]={};
    if(!GetLauncherDir(dir)) return;
    sprintf_s(ini,"%s\\leader_capture.ini",dir);
    GetPrivateProfileStringA("General","GameExe","",c->gameExeRaw,MAX_PATH,ini);
    GetPrivateProfileStringA("General","ProcessName",kDefaultProcessName,c->processName,MAX_PATH,ini);
    GetPrivateProfileStringA("General","CompatibleProcessNames",kDefaultCompatibleNames,c->compatibleNames,sizeof(c->compatibleNames),ini);
    c->autoLaunch=GetPrivateProfileIntA("General","AutoLaunch",0,ini)!=0;
    c->waitMs=(DWORD)GetPrivateProfileIntA("General","WaitMs",60000,ini);
}

static bool StartGame(const char* gameExe,DWORD* outPid){
    char mutablePath[MAX_PATH]={}; strcpy_s(mutablePath,gameExe);
    char work[MAX_PATH]={}; strcpy_s(work,gameExe);
    char* slash=strrchr(work,'\\'); if(slash)*slash=0; else strcpy_s(work,".");
    STARTUPINFOA si={};si.cb=sizeof(si);PROCESS_INFORMATION pi={};
    BOOL ok=CreateProcessA(nullptr,mutablePath,nullptr,nullptr,FALSE,0,nullptr,work,&si,&pi);
    if(!ok){printf("CreateProcess failed %lu for: %s\n",GetLastError(),gameExe);return false;}
    *outPid=pi.dwProcessId;
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess); return true;
}

static int Inject(DWORD pid,const char* dllPath){
    if(AlreadyLoaded(pid,kDllName)){printf("Already injected: %s\n",kDllName);return 0;}
    HANDLE p=OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|
                         PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid);
    if(!p){printf("OpenProcess failed %lu\n",GetLastError());return 20;}
    SIZE_T len=strlen(dllPath)+1;
    void* remote=VirtualAllocEx(p,nullptr,len,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!remote){CloseHandle(p);printf("VirtualAllocEx failed %lu\n",GetLastError());return 21;}
    if(!WriteProcessMemory(p,remote,dllPath,len,nullptr)){
        printf("WriteProcessMemory failed %lu\n",GetLastError());
        VirtualFreeEx(p,remote,0,MEM_RELEASE);CloseHandle(p);return 22;
    }
    FARPROC load=GetProcAddress(GetModuleHandleA("kernel32.dll"),"LoadLibraryA");
    HANDLE th=CreateRemoteThread(p,nullptr,0,(LPTHREAD_START_ROUTINE)load,remote,0,nullptr);
    if(!th){printf("CreateRemoteThread failed %lu\n",GetLastError());VirtualFreeEx(p,remote,0,MEM_RELEASE);CloseHandle(p);return 23;}
    WaitForSingleObject(th,10000);
    DWORD module=0;GetExitCodeThread(th,&module);
    CloseHandle(th);VirtualFreeEx(p,remote,0,MEM_RELEASE);CloseHandle(p);
    if(!module){printf("LoadLibrary returned NULL\n");return 24;}
    printf("Injected PID=%lu DLL_module=0x%08lX\n",pid,module); return 0;
}

static bool GetRuntimeLogPath(char out[MAX_PATH]){
    char temp[MAX_PATH]={}; DWORD n=GetTempPathA(MAX_PATH,temp);
    if(!n || n>=MAX_PATH)return false;
    sprintf_s(out,MAX_PATH,"%s%s",temp,kRuntimeLogName);return true;
}

static int InspectRuntimeGate(DWORD waitMs){
    char path[MAX_PATH]={}; if(!GetRuntimeLogPath(path))return 0;
    DWORD start=GetTickCount();
    do{
        HANDLE f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
        if(f!=INVALID_HANDLE_VALUE){
            LARGE_INTEGER sz={};
            if(GetFileSizeEx(f,&sz) && sz.QuadPart>0){
                DWORD cap=(DWORD)((sz.QuadPart>262143)?262143:sz.QuadPart);
                char* buf=(char*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,cap+1);
                if(buf){
                    DWORD got=0;ReadFile(f,buf,cap,&got,nullptr);buf[got]=0;
                    bool ready=strstr(buf,"[READY] gameplay_writes=ENABLED")!=nullptr;
                    bool fatal=strstr(buf,"[FATAL]")!=nullptr;
                    HeapFree(GetProcessHeap(),0,buf);CloseHandle(f);
                    if(ready)return 1; if(fatal)return -1;
                    Sleep(100);continue;
                }
            }
            CloseHandle(f);
        }
        Sleep(100);
    }while(GetTickCount()-start<waitMs);
    return 0;
}

int main(int argc,char** argv){
    printf("HOI3 Leader Capture RC1.6 CLEAN / process+path portable launcher\n");
    printf("Default target: hoi3_tfh.exe. Other compatible process names must be explicitly configured; no fixed game install path is required.\n");
    printf("SHA-256 is identity only. DLL call/signature validation remains the safety gate.\n\n");

    Config cfg={}; ReadIni(&cfg);
    char explicitExe[MAX_PATH]={};

    // argv[1]: either an existing EXE path (drag/drop supported) or a process filename.
    if(argc>1){
        DWORD attr=GetFileAttributesA(argv[1]);
        if(attr!=INVALID_FILE_ATTRIBUTES && !(attr&FILE_ATTRIBUTE_DIRECTORY)){
            strcpy_s(explicitExe,argv[1]);
            strcpy_s(cfg.processName,BaseNameOnly(argv[1]));
            cfg.autoLaunch=true;
        }else{
            strcpy_s(cfg.processName,argv[1]);
        }
    }

    DWORD pid=0;
    char selectedName[MAX_PATH]={};
    if(cfg.processName[0]){
        pid=FindPid(cfg.processName);
        if(pid)strcpy_s(selectedName,cfg.processName);
    }
    if(!pid) pid=FindFromSemicolonList(cfg.compatibleNames,selectedName);

    if(!pid && cfg.autoLaunch){
        char gameExe[MAX_PATH]={};
        if(explicitExe[0]) strcpy_s(gameExe,explicitExe);
        else if(cfg.gameExeRaw[0]) ResolveAgainstLauncherDir(cfg.gameExeRaw,gameExe);
        else {
            // No fixed path: first try hoi3_tfh.exe next to the launcher.
            ResolveAgainstLauncherDir("hoi3_tfh.exe",gameExe);
            if(GetFileAttributesA(gameExe)==INVALID_FILE_ATTRIBUTES) gameExe[0]=0;
        }

        if(!gameExe[0]){
            printf("AutoLaunch is enabled, but no game EXE path was supplied/found.\n");
            printf("No absolute path is required: either start HOI3 manually, or drag your compatible EXE onto this launcher,\n");
            printf("or set GameExe to a relative/absolute path in leader_capture.ini.\n");
            WaitBeforeExit();return 2;
        }

        strcpy_s(cfg.processName,BaseNameOnly(gameExe));
        strcpy_s(selectedName,cfg.processName);
        char sha[65]={};unsigned long long size=0;
        if(Sha256File(gameExe,sha,&size)){
            printf("GameExe identity (diagnostic only):\n  path=%s\n  size=%llu\n  SHA256=%s\n",gameExe,size,sha);
            printf("No SHA-256 hard refusal is performed.\n");
        }
        if(!StartGame(gameExe,&pid)){WaitBeforeExit();return 5;}
        DWORD start=GetTickCount();
        do{Sleep(250);DWORD found=FindPid(cfg.processName);if(found){pid=found;break;}}
        while(GetTickCount()-start<cfg.waitMs);
        if(!pid){printf("Timed out waiting for %s\n",cfg.processName);WaitBeforeExit();return 6;}
        Sleep(1500);
    }

    if(!pid){
        printf("No configured HOI3 process is currently running.\n");
        printf("Primary ProcessName: %s\n",cfg.processName[0]?cfg.processName:"(blank)");
        printf("CompatibleProcessNames: %s\n",cfg.compatibleNames);
        printf("\nRecommended: start hoi3_tfh.exe manually, reach the main menu, then run this launcher.\n");
        printf("For a renamed compatible build, edit ProcessName/CompatibleProcessNames in leader_capture.ini only; no rebuild needed.\n");
        WaitBeforeExit();return 7;
    }

    printf("Target process: %s PID=%lu\n",selectedName[0]?selectedName:(cfg.processName[0]?cfg.processName:"(detected)"),pid);

    char dir[MAX_PATH]={}; if(!GetLauncherDir(dir)){printf("Cannot resolve launcher directory.\n");WaitBeforeExit();return 8;}
    char dll[MAX_PATH]={}; sprintf_s(dll,"%s\\%s",dir,kDllName);
    if(GetFileAttributesA(dll)==INVALID_FILE_ATTRIBUTES){printf("DLL not found next to launcher: %s\n",dll);WaitBeforeExit();return 8;}
    printf("DLL path (launcher-relative): %s\n",dll);

    // Prevent stale READY/FATAL from a previous run being mistaken for this injection.
    char logPath[MAX_PATH]={}; if(GetRuntimeLogPath(logPath)) DeleteFileA(logPath);

    int rc=Inject(pid,dll);
    if(rc!=0){
        printf("\nINJECTION FAILED: launcher exit code=%d\n",rc);
        printf("If OpenProcess failed with Windows error 5, run Launcher at the same/higher privilege level as HOI3.\n");
        WaitBeforeExit();return rc;
    }

    printf("Runtime log: %%TEMP%%\\%s\n",kRuntimeLogName);
    printf("Waiting for the DLL's own compatibility gate...\n");
    int gate=InspectRuntimeGate(5000);
    if(gate>0){
        printf("\nCOMPATIBILITY PASS: DLL reported [READY]; gameplay writes are enabled.\n");
    }else if(gate<0){
        printf("\nCOMPATIBILITY REJECTED: DLL reported [FATAL]. No gameplay hooks should be active.\n");
        WaitBeforeExit();return 30;
    }else{
        printf("\nInjection succeeded, but READY/FATAL was not observed within 5 seconds.\n");
        printf("Inspect the runtime log before assuming the mechanism is active.\n");
    }
    WaitBeforeExit();return 0;
}
