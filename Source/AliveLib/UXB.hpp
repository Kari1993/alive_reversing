#pragma once

#include "FunctionFwd.hpp"
#include "BaseGameObject.hpp"
#include "BaseAliveGameObject.hpp"
#include "Path.hpp"
#include "Factory.hpp"
#include "Animation.hpp"

void UXB_ForceLink();

struct Path_UXB
{
    Path_TLV field_0_mBase;
    __int16 field_10_num_patterns;
    __int16 field_12_pattern;
    __int16 field_14_scale;
    __int16 field_16_state;
    int field_18_disabled_resources;
};

class UXB : public BaseAliveGameObject
{
public:
    EXPORT UXB * ctor_4DE9A0(Path_UXB *params, TlvItemInfoUnion itemInfo);

    EXPORT void sub_4DEED0(AnimationEx *a2);
    EXPORT void PlaySFX_4DE930(unsigned __int8 sfxIdx);

    /* virtual void VUpdate() override;
     virtual void VRender(int** pOrderingTable) override;
     virtual void VDestructor(signed int flags) override;
     virtual void VScreenChanged() override;
     virtual void vnullsub_4DC0F0() override;*/
public:
    BYTE gap116[2];
    WORD field_118;
    WORD field_11A;
    WORD field_11C_disabled_resources;
    BYTE gap11E[2];
    TlvItemInfoUnion field_120;
    DWORD field_124_next_state_frame;
    AnimationEx field_128_animation;
    WORD field_1C0_num_patterns;
    WORD field_1C2;
    WORD field_1C4_pattern;
    WORD field_1C6;
    WORD field_1C8;
    __int16 field_1CA;
};
ALIVE_ASSERT_SIZEOF(UXB, 0x1CC);