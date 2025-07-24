#ifndef _COMPUTE_FX_
#define _COMPUTE_FX_

#include "params.fx"

RWTexture2D<float4> g_rwtex_0 : register(u0);
//u0 : uav compute 전용임.
//g_rwtex_0 : shader에서 read,write가 가능해짐

// 쓰레드 그룹당 쓰레드 개수
// max : 1024 (CS_5.0)
// => x * y * z 가 1024를 넘기면 안됨

// 다중 쓰레드를 사용하여 하나의 쓰레드를 픽셀좌표로 활용하여 사용
// - 하나의 쓰레드 그룹은 하나의 다중처리기에서 실행
[numthreads(1024, 1, 1)]
void CS_Main(int3 threadIndex : SV_DispatchThreadID)
{
    if (threadIndex.y % 2 == 0)
        g_rwtex_0[threadIndex.xy] = float4(1.f, 0.f, 0.f, 1.f);
    else
        g_rwtex_0[threadIndex.xy] = float4(0.f, 1.f, 0.f, 1.f);
}

#endif