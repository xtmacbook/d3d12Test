#include <Windows.h>
#include <wrl.h>

#include <dxgi1_4.h>
#include <d3d12.h>

#include <DirectXMath.h>


#include <string>

inline std::wstring AnsiToWstring(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

class D3DUtil
{
public:

};

class DxException
{
public:
    DxException() = default;

    DxException(HRESULT hr, const std::wstring & functionName, const std::wstring & filename, int lineNumber);

    std::wstring toString()const;

    HRESULT         m_ErrorCode = S_OK;
    std::wstring    m_functionName;
    std::wstring    m_filename;
    int             m_lineNumber = -1;
};

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWstring(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

void errorExit();