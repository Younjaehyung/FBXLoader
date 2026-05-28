#include "pch.h"



wstring s2ws(const string& s)
{
	int32 len;
	int32 slength = static_cast<int32>(s.length()) + 1;
	len = ::MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	::MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	wstring ret(buf);
	delete[] buf;
	return ret;
}

string ws2s(const wstring& s)
{
	// 수정: 기존 코드는 null terminator까지 string에 포함해 빈 경로도 "\0" 길이 1 문자열이 되었다.
	// 그 결과 empty() 검사가 실패해서 NormalMap fallback이 실행되지 않았으므로 실제 문자 길이만 변환한다.
	if (s.empty())
		return {};

	int32 len;
	int32 slength = static_cast<int32>(s.length());
	len = ::WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, 0, 0, 0, 0);
	string r(len, '\0');
	if (len > 0)
		::WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, &r[0], len, 0, 0);
	return r;
}
