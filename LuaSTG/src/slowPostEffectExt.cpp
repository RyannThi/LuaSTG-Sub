﻿#include "slowPostEffectExt.hpp"
#include "AppFrame.h"

#include <atomic>
#include <string>
#include <unordered_map>

#include <d3d9.h>
#include <d3dcompiler.h>
#include <wrl.h>

namespace DirectX
{
    using PFN_D3DCompile = HRESULT (WINAPI *)(
        LPCVOID, SIZE_T, LPCSTR, // src, size, name
        CONST D3D_SHADER_MACRO*, ID3DInclude*, // macro, include
        LPCSTR, LPCSTR, UINT, UINT, // entry, target, flag1, flag2
        ID3DBlob**, ID3DBlob**); // blob, error
    
    class ShaderCompiler
    {
    private:
        HMODULE D3DCompilerDLL;
        PFN_D3DCompile D3DCompileP;
    public:
        bool compile(const char* source, size_t size, const char* name, const char* entry, const char* target, ID3DBlob** code)
        {
            // check
            if (!D3DCompileP)
            {
                ::OutputDebugStringA("D3DCompiler module does not exist.\n");
                return false;
            }
            if (source == nullptr || size == 0)
            {
                ::OutputDebugStringA("invalid parameter, source cannot be null, size must be larger than 0.\n");
                return false;
            }
            if (name == nullptr || entry == nullptr || target == nullptr || code == nullptr)
            {
                ::OutputDebugStringA("invalid parameter, name, entry, target or code cannot be null.\n");
                return false;
            }
            
            // clear
            *code = NULL;
            
            // compile shader
            UINT flags1 = 0;
            #ifdef _DEBUG
            flags1 |= D3DCOMPILE_DEBUG;
            flags1 |= D3DCOMPILE_SKIP_OPTIMIZATION;
            flags1 |= D3DCOMPILE_ENABLE_STRICTNESS;
            flags1 |= D3DCOMPILE_IEEE_STRICTNESS;
            flags1 |= D3DCOMPILE_OPTIMIZATION_LEVEL0; // same as D3DCOMPILE_SKIP_OPTIMIZATION?
            flags1 |= D3DCOMPILE_WARNINGS_ARE_ERRORS;
            #endif
            ID3DBlob* errmsg = NULL;
            const HRESULT clret = D3DCompileP(
                source, size, name,
                NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                entry, target, flags1, 0,
                code, &errmsg);
            if (clret != S_OK && errmsg != NULL)
            {
                ::OutputDebugStringA((char*)errmsg->GetBufferPointer());
                errmsg->Release();
                errmsg = NULL;
            }
            if (clret != S_OK)
            {
                ::OutputDebugStringA("compile shader \"");
                ::OutputDebugStringA(name);
                ::OutputDebugStringA("\" failed: \n");
                return false;
            }
            
            return true;
        }
        bool compileFromFile(const char* path, const char* entry, const char* target, ID3DBlob** code)
        {
            // check
            if (!D3DCompileP)
            {
                ::OutputDebugStringA("D3DCompiler module does not exist.\n");
                return false;
            }
            if (!path || !entry || !target || !code)
            {
                ::OutputDebugStringA("invalid parameter, path, entry, target or code can't be null.\n");
                return false;
            }
            
            // convert path encoding
            const int wcharcnt = ::MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
            if (wcharcnt <= 0)
            {
                ::OutputDebugStringA("invalid file path.\n");
                return false;
            }
            WCHAR* wcharbuf = static_cast<WCHAR*>(::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, wcharcnt * sizeof(WCHAR)));
            struct __wcharbuf
            {
                WCHAR*& _data;
                __wcharbuf(WCHAR*& data) : _data(data) {}
                ~__wcharbuf() { ::HeapFree(::GetProcessHeap(), 0, _data); _data = NULL; }
            } __wcharbuf(wcharbuf);
            if (!wcharbuf)
            {
                ::OutputDebugStringA("allocate memory failed.\n");
                return false;
            }
            const int wcharcov = ::MultiByteToWideChar(CP_UTF8, 0, path, -1, wcharbuf, wcharcnt);
            if (wcharcov <= 0)
            {
                ::OutputDebugStringA("convert file path encoding failed.\n");
                return false;
            }
            
            // read file
            HANDLE hfile = ::CreateFileW(wcharbuf, FILE_GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            struct __hfile
            {
                HANDLE& _handle;
                __hfile(HANDLE& handle) : _handle(handle) {}
                ~__hfile() { ::CloseHandle(_handle); _handle = INVALID_HANDLE_VALUE; }
            } __hfile(hfile);
            if (hfile == INVALID_HANDLE_VALUE)
            {
                ::OutputDebugStringW(L"open file \"");
                ::OutputDebugStringW(wcharbuf);
                ::OutputDebugStringW(L"\" failed.\n");
                return false;
            }
            LARGE_INTEGER filesize = {};
            if (FALSE == GetFileSizeEx(hfile, &filesize))
            {
                ::OutputDebugStringW(L"get file \"");
                ::OutputDebugStringW(wcharbuf);
                ::OutputDebugStringW(L"\" size failed.\n");
                return false;
            }
            assert(filesize.QuadPart < 0x40000000);
            char* codebuf = static_cast<char*>(::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, filesize.LowPart * sizeof(char)));
            struct __codebuf
            {
                char*& _data;
                __codebuf(char*& data) : _data(data) {}
                ~__codebuf() { ::HeapFree(::GetProcessHeap(), 0, _data); _data = NULL; }
            } __codebuf(codebuf);
            if (!codebuf)
            {
                ::OutputDebugStringW(L"allocate memory failed.\n");
                return false;
            }
            DWORD freadcnt = 0;
            const BOOL freadret = ReadFile(hfile, codebuf, filesize.LowPart, &freadcnt, NULL);
            if (freadret == FALSE || freadcnt != filesize.LowPart)
            {
                ::OutputDebugStringW(L"read file \"");
                ::OutputDebugStringW(wcharbuf);
                ::OutputDebugStringW(L"\" failed.\n");
                return false;
            }
            
            // compile shader
            return compile(codebuf, filesize.LowPart, path, entry, target, code);
        }
    public:
        ShaderCompiler()
        {
            D3DCompilerDLL = NULL;
            D3DCompileP = NULL;
            const WCHAR* D3DCompilerDLLNames[] = {
                L"D3DCompiler_47.dll",
                L"D3DCompiler_46.dll",
                L"D3DCompiler_45.dll", // may not exist?
                L"D3DCompiler_44.dll", // RTM verion (beta version)
                L"D3DCompiler_43.dll",
                L"D3DCompiler_42.dll",
                L"D3DCompiler_41.dll",
                L"D3DCompiler_40.dll",
            };
            for (size_t idx = 0; idx < 8; idx += 1)
            {
                HMODULE DLL = ::LoadLibraryW(D3DCompilerDLLNames[idx]);
                if (DLL)
                {
                    D3DCompilerDLL = DLL;
                    D3DCompileP = (PFN_D3DCompile)::GetProcAddress(DLL, "D3DCompile");
                    assert(D3DCompileP != NULL);
                    break;
                }
            }
        }
        ~ShaderCompiler()
        {
            if (D3DCompilerDLL)
                ::FreeLibrary(D3DCompilerDLL);
            D3DCompilerDLL = NULL;
            D3DCompileP = NULL;
        }
    };
}

namespace slow
{
    class Object
    {
    private:
        std::atomic_int32_t _ref_count;
    public:
        virtual int32_t retain() { const int32_t refcnt = _ref_count.fetch_add(1) + 1; return refcnt; }
        virtual int32_t release() { const int32_t refcnt = _ref_count.fetch_sub(1) - 1; if (refcnt <= 0) { delete this; } return refcnt; }
    public:
        Object() : _ref_count(1) {}
        virtual ~Object() {}
    };
}

namespace slow { namespace effect {
    static DirectX::ShaderCompiler g_hlslc;
    
    class RenderDeviceEventListener : public f2dRenderDeviceEventListener
    {
    public:
        void OnRenderDeviceLost()
        {
        }
        void OnRenderDeviceReset()
        {
        }
    };
    static RenderDeviceEventListener g_RenderDeviceEventListener;
    
    struct SimplePipeline
    {
        IDirect3DVertexShader9* VertexShader = NULL;
        IDirect3DPixelShader9*  PixelShader  = NULL;
        
        void setVertexShader(IDirect3DVertexShader9* VS)
        {
            if (VertexShader) VertexShader->Release();
            VertexShader = VS;
            if (VertexShader) VertexShader->AddRef();
        }
        void setPixelShader(IDirect3DPixelShader9*  PS)
        {
            if (PixelShader) PixelShader->Release();
            PixelShader = PS;
            if (PixelShader) PixelShader->AddRef();
        }
        void clear()
        {
            if (VertexShader)
            {
                VertexShader->Release();
                VertexShader = NULL;
            }
            if (PixelShader)
            {
                PixelShader->Release();
                PixelShader = NULL;
            }
        }
    };
    
    
    
    void disableFixedPipeline(IDirect3DDevice9* ctx)
    {
        ctx->SetRenderState(D3DRS_CLIPPING, FALSE);
        ctx->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
        ctx->SetRenderState(D3DRS_LIGHTING, FALSE);
        ctx->SetRenderState(D3DRS_FOGENABLE, FALSE);
        ctx->SetRenderState(D3DRS_POINTSPRITEENABLE, FALSE);
        ctx->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    }
    
    enum PrimitiveType : uint8_t
    {
        PointList     = 1,
        LineList      = 2,
        LineStrip     = 3,
        TriangleList  = 4,
        TriangleStrip = 5,
        TriangleFan   = 6,
    };
    
    enum BlendMode : uint8_t
    {
        Disable =  0, // 关闭混合
        Alpha   =  1, // alpha混合
        Zero    =  2, // 保持为画布颜色
        One     =  3, // 设置为画笔颜色
        Min     =  4, // 最小颜色值（RGB每通道值）
        Max     =  5, // 最大颜色值（RGB每通道值）
        Add     =  6, // 相加
        Sub     =  7, // 画笔颜色减去画布颜色
        RevSub  =  8, // 画布颜色减去画笔颜色（PhotoShop减去）
        Mul     =  9, // 相乘（PhotoShop叠加）
        Inv     = 10, // 反色，通过设置画笔颜色来控制反色程度，白色为完全反色，黑色为不反色
        Screen  = 11, // PhotoShop滤色
    };
    
    enum VertexColorMode : uint8_t
    {
        Disable = 0, // 不使用顶点色
        Zero    = 0, // 直接使用纹理色
        One     = 1, // 直接使用顶点色
        Mul     = 2, // 顶点色与纹理色相乘
        Add     = 3, // 顶点色与纹理色相加
        Sub     = 4, // 纹理色减去顶点色
        RevSub  = 5, // 顶点色减去纹理色
    };
    
    enum TextureAddressMode : uint8_t
    {
        Wrap   = 1,
        Mirror = 2,
        Clamp  = 3,
        Border = 4,
    };
    
    enum TextureFilterMode : uint8_t
    {
        None        = 0,
        Point       = 1,
        Linear      = 2,
        Anisotropic = 3,
    };
    
    struct Vertex
    {
        float x, y, z;
        uint32_t color;
        float u, v;
    };
    
    typedef uint16_t Index;
    
    struct Command
    {
        IDirect3DTexture9* texture;
        
        uint16_t vertex;
        uint16_t index;
        
        PrimitiveType primitive;
        BlendMode blend;
        VertexColorMode vertcol;
        TextureAddressMode texaddr;
        
        TextureFilterMode texfilt;
    };
    
    std::unordered_map<std::string, SimplePipeline> g_pipeline;
    bool addPipeline(const std::string& name, const SimplePipeline& pipeline)
    {
        auto it = g_pipeline.find(name);
        if (it != g_pipeline.end())
        {
            it->second.clear();
            g_pipeline.erase(it);
        }
        try
        {
            g_pipeline.emplace(name, pipeline);
            return true;
        }
        catch (...)
        {
        }
        return false;
    }
    bool findPipeline(const std::string& name, SimplePipeline*& pipeline)
    {
        auto it = g_pipeline.find(name);
        if (it != g_pipeline.end())
        {
            pipeline = &(it->second);
            return true;
        }
        return false;
    }
    void removePipeline(const std::string& name)
    {
        auto it = g_pipeline.find(name);
        if (it != g_pipeline.end())
        {
            it->second.clear();
            g_pipeline.erase(it);
        }
    }
    void clearPipelinePool()
    {
        for (auto& v : g_pipeline)
        {
            v.second.clear();
        }
        g_pipeline.clear();
    }
    
    void bindEngine()
    {
        
    }
    void unbindEngine()
    {
        clearPipelinePool();
    }
    
}}
