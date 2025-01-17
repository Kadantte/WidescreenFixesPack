#include "stdafx.h"
#include <string_view>
#include "kananlib.hpp"
#include "GTA/ue.hpp"

enum eGameState
{
    GS_START_UP = 0,
    GS_INIT_LOGO_MPEG,
    GS_LOGO_MPEG,
    GS_INIT_INTRO_MPEG,
    GS_INTRO_MPEG,
    GS_INIT_ONCE,
    GS_INIT_FRONTEND,
    GS_FRONTEND,
    GS_INIT_PLAYING_GAME,
    GS_PLAYING_GAME,
};

float fHudScale = 1.0f;
float fRadarScale = 1.0f;

uint32_t* OnAMissionFlag;
uint8_t* ScriptSpace;
bool IsPlayerOnAMission()
{
    return *OnAMissionFlag && (ScriptSpace[*OnAMissionFlag] == 1);
}

namespace UUI_LegalScreen
{
    using UUI_LegalScreen = void;

    SafetyHookInline shShouldShow{};
    void ShouldShow(UUI_LegalScreen* LegalScreen, void* a2, int8_t* out)
    {
        shShouldShow.fastcall(LegalScreen, a2, out);
        *out = 0;
    }
}

HWND ghWnd;
void LockCursor()
{
    if (ghWnd == GetForegroundWindow())
    {
        RECT windowRect;
        POINT pt = { 0, 0 };

        GetClientRect(ghWnd, &windowRect);
        ClientToScreen(ghWnd, &pt);

        windowRect.left += pt.x + 1;
        windowRect.top += pt.y + 1;
        windowRect.right += pt.x - 1;
        windowRect.bottom += pt.y - 1;

        ClipCursor(&windowRect);
    }
    else
    {
        ClipCursor(NULL);
    }
}

SafetyHookInline shWndProc{};
LRESULT CustomWndProc(void* _this, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    ghWnd = hWnd;
    auto res = shWndProc.fastcall<LRESULT>(_this, hWnd, uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_ACTIVATEAPP:
    case WM_SETFOCUS:
    {
        LockCursor();
        break;
    }
    case WM_KILLFOCUS:
    case WM_CLOSE:
    case WM_DESTROY:
    {
        ClipCursor(NULL);
        break;
    }
    default:
        break;
    }

    return res;
}

bool bIsDefaultWidget = false;
SafetyHookInline shParseWidgetPositionLineFromFile{};
void ParseWidgetPositionLineFromFile(const char* widget, char bDefault)
{
    bIsDefaultWidget = bDefault;
    shParseWidgetPositionLineFromFile.fastcall(widget, bDefault);
}

int vscanf_hook(char* Buffer, char* Format, ...)
{
    std::string temp;
    std::string_view buf(Buffer);
    if (buf.contains("WIDGET_POSITION_PLAYER_INFO") || buf.contains("WIDGET_POSITION_RADAR"))
    {
        std::string PositionAndScale(255, '\0');
        float OriginX, OriginY, ScaleX, ScaleY;
        static float rOriginX2, rOriginY2, rScaleX2, rScaleY2;
        static float pOriginX2, pOriginY2, pScaleX2, pScaleY2;
        std::string Separator(255, '\0');
        std::string Name(255, '\0');
        auto r = sscanf(Buffer, Format, PositionAndScale.data(), &OriginX, &OriginY, &ScaleX, &ScaleY, Separator.data(), Name.data());
        if (r == 7)
        {
            if (buf.contains("WIDGET_POSITION_PLAYER_INFO"))
            {
                if (bIsDefaultWidget)
                {
                    pOriginX2 = OriginX;
                    pOriginY2 = OriginY;
                    pScaleX2 = ScaleX;
                    pScaleY2 = ScaleY;
                }
                else
                {
                    OriginX = pOriginX2;
                    OriginY = pOriginY2;
                    ScaleX = pScaleX2;
                    ScaleY = pScaleY2;
                }

                OriginX = OriginX + (ScaleX - (ScaleX * fHudScale));
                OriginY = OriginY - (ScaleY - (ScaleY * fHudScale));
                ScaleX *= fHudScale;
                ScaleY *= fHudScale;
            } 
            else if (buf.contains("WIDGET_POSITION_RADAR"))
            {
                if (bIsDefaultWidget)
                {
                    rOriginX2 = OriginX;
                    rOriginY2 = OriginY;
                    rScaleX2 = ScaleX;
                    rScaleY2 = ScaleY;
                }
                else
                {
                    OriginX = rOriginX2;
                    OriginY = rOriginY2;
                    ScaleX = rScaleX2;
                    ScaleY = rScaleY2;
                }

                OriginX = OriginX - (ScaleX - (ScaleX * fRadarScale));
                OriginY = OriginY + (ScaleY - (ScaleY * fRadarScale));
                ScaleX *= fRadarScale;
                ScaleY *= fRadarScale;
            }

            PositionAndScale.resize(PositionAndScale.find('\0'));
            Separator.resize(Separator.find('\0'));
            Name.resize(Name.find('\0'));
            temp += PositionAndScale;
            temp += " ";
            temp += std::to_string(OriginX);
            temp += " ";
            temp += std::to_string(OriginY);
            temp += " ";
            temp += std::to_string(ScaleX);
            temp += " ";
            temp += std::to_string(ScaleY);
            temp += " ";
            temp += Separator;
            temp += " ";
            temp += Name;
            Buffer = temp.data();
        }
    }

    int result;
    va_list arglist;
    va_start(arglist, Format);
    result = vsscanf(Buffer, Format, arglist);
    va_end(arglist);
    return result;
}

void Init()
{
    CIniReader iniReader("");
    auto bSkipIntro = iniReader.ReadInteger("MAIN", "SkipIntro", 1) != 0;
    auto bSkipMenu = iniReader.ReadInteger("MAIN", "SkipMenu", 1) != 0;
    static auto nIniSaveSlot = (int32_t)iniReader.ReadInteger("MAIN", "SaveSlot", 6) - 1;
    fHudScale = iniReader.ReadFloat("MAIN", "HudScale", 0.8f);
    fRadarScale = iniReader.ReadFloat("MAIN", "RadarScale", 0.75f);
    static auto bIniDisableFirstPersonAimForRifles = iniReader.ReadInteger("MAIN", "DisableFirstPersonAimForRifles", 1) != 0;
    static auto bImproveCameraPC = iniReader.ReadInteger("MAIN", "ImproveCameraPC", 1) != 0;
    if (fHudScale <= 0.0f) 
        fHudScale = 1.0f;
    if (fRadarScale <= 0.0f) 
        fRadarScale = 1.0f;
    if (nIniSaveSlot < 1 || nIniSaveSlot > 8)
        nIniSaveSlot = 5;

    if (bSkipIntro)
    {
        auto AcceptPrivacyPolicy = utility::scan_string(utility::get_executable(), "AcceptPrivacyPolicy", true);
        if (AcceptPrivacyPolicy)
        {
            auto AcceptPrivacyPolicyXrefs = utility::scan_references(utility::get_executable(), AcceptPrivacyPolicy.value());

            for (auto& it : AcceptPrivacyPolicyXrefs)
            {
                auto ref = utility::scan_reference(utility::get_executable(), it);
                if (ref)
                {
                    auto res = utility::resolve_displacement(ref.value() - 3);
                    if (res)
                    {
                        struct UUI_LegalScreenRef
                        {
                            const char* AcceptPrivacyPolicyStr;
                            void* AcceptPrivacyPolicy;
                            const char* CanSkipStr;
                            void* CanSkip;
                            const char* HasAcceptedPrivacyPolicyStr;
                            void* HasAcceptedPrivacyPolicy;
                            const char* ShouldShowStr;
                            void* ShouldShow;
                        };

                        auto pLegalScreen = reinterpret_cast<UUI_LegalScreenRef*>(*res);

                        if (std::string_view(pLegalScreen->ShouldShowStr) == "ShouldShow")
                        {
                            UUI_LegalScreen::shShouldShow = safetyhook::create_inline(pLegalScreen->ShouldShow, UUI_LegalScreen::ShouldShow);
                            break;
                        }
                    }
                }
            }
        }
    }

    if (bSkipMenu)
    {
        static uint8_t* gGameState = utility::GetSymbolFromFuncStart<decltype(gGameState)>("SaveGameForPause failed WaitForSave", [](INSTRUX& ix) { return ix.Instruction == ND_INS_MOV && ix.Operands[0].Type == ND_OP_MEM && ix.Operands[1].Type == ND_OP_IMM && ix.Operands[1].Info.Immediate.Imm == GS_PLAYING_GAME; });
        static char(*USanAndreasInterfaceResumeGameFromMenu)(void*) = nullptr;
        static void* CGenericGameStorageCheckSlotDataValid = nullptr;

        auto s = utility::scan_string(utility::get_executable(), L"GTAVCsf%d", true);
        if (s)
        {
            auto xrefs = utility::scan_displacement_references(utility::get_executable(), s.value());
            for (auto& it2 : xrefs)
            {
                INSTRUX ix{};
                auto ip = it2 + 4;

                const auto decoded_xor = utility::decode_one((uint8_t*)ip);
                if (decoded_xor->Instruction == ND_INS_XOR)
                {
                    const auto decoded_cmp = utility::decode_one((uint8_t*)ip + decoded_xor->Length);

                    if (decoded_cmp->Instruction == ND_INS_CMP && decoded_cmp->Operands[1].Type == ND_OP_IMM && decoded_cmp->Operands[1].Info.Immediate.Imm == 0xffffffffffffffff)
                    {
                        auto start = utility::find_function_start(ip);
                        if (start)
                        {
                            CGenericGameStorageCheckSlotDataValid = reinterpret_cast<void*>(start.value());
                            break;
                        }
                    }
                }

                if (CGenericGameStorageCheckSlotDataValid)
                    break;
            }
        }

        if (CGenericGameStorageCheckSlotDataValid)
        {
            auto xrefs = utility::scan_displacement_references(utility::get_executable(), (uintptr_t)CGenericGameStorageCheckSlotDataValid);
            for (auto& it : xrefs)
            {
                auto start = utility::find_function_start(it);
                if (start)
                {
                    INSTRUX ix{};
                    auto ip = start.value();

                    while (true)
                    {
                        const auto status = NdDecodeEx(&ix, (ND_UINT8*)ip, 1000, ND_CODE_64, ND_DATA_64);

                        if (!ND_SUCCESS(status)) {
                            break;
                        }

                        if (ix.Instruction == ND_INS_CMP && ix.Operands[0].Type == ND_OP_MEM && ix.Operands[1].Type == ND_OP_IMM && ix.Operands[1].Info.Immediate.Imm == GS_PLAYING_GAME)
                        {
                            if (ip < it)
                            {
                                auto disp = utility::resolve_displacement(ip);
                                if (disp && disp.value() == (uintptr_t)gGameState)
                                {
                                    auto start = utility::find_function_start(ip);
                                    if (start)
                                    {
                                        if (!utility::find_function_start_with_call(start.value()).has_value())
                                        {
                                            USanAndreasInterfaceResumeGameFromMenu = reinterpret_cast<decltype(USanAndreasInterfaceResumeGameFromMenu)>(start.value());
                                            break;
                                        }
                                    }
                                }
                            }
                        }

                        ip += ix.Length;
                    }
                }
            }
        }

        static uint8_t* byte_144E1DD34 = utility::GetSymbolFromFuncStart<decltype(gGameState)>(L"SocialClubSignIn", [](INSTRUX& ix) { return ix.Instruction == ND_INS_MOVZX && ix.Operands[0].Type == ND_OP_REG && ix.Operands[1].Type == ND_OP_MEM; });

        if (gGameState && USanAndreasInterfaceResumeGameFromMenu && byte_144E1DD34)
        {
            auto strref = utility::find_virtual_function_from_string_ref(utility::get_executable(), L"open /game/Maps/MainMenu", false);
            if (strref)
            {
                static safetyhook::MidHook USanAndreasInterfaceUpdateHook = {};
                USanAndreasInterfaceUpdateHook = safetyhook::create_mid(strref.value(), [](SafetyHookContext& regs)
                {
                    if (*gGameState == 7 && (*byte_144E1DD34 & 2) != 0)
                    {
                        USanAndreasInterfaceResumeGameFromMenu((void*)regs.rcx);
                    }
                    else if(*gGameState > 7)
                    {
                        USanAndreasInterfaceUpdateHook = {};
                    }
                });
            }
        }
    }

    ScriptSpace = utility::GetSymbolFromFuncStart<decltype(ScriptSpace)>("noname", [](INSTRUX& ix) { return ix.Instruction == ND_INS_LEA && ix.Operands[0].Type == ND_OP_REG && ix.Operands[1].Type == ND_OP_MEM; });
    OnAMissionFlag = utility::GetSymbolFromFuncStart<decltype(OnAMissionFlag)>("ENDSUBM", [](INSTRUX& ix) { return ix.Instruction == ND_INS_MOV && ix.Operands[0].Type == ND_OP_REG && ix.Operands[1].Type == ND_OP_MEM; });
    static void (*CHud_SetHelpMessage)(FString* a1, int a2, int a3, int a4, int a5) = nullptr;
    static void(*SaveToSlot)(int) = nullptr;
    static uintptr_t Slot = 0;

    CHud_SetHelpMessage = utility::GetFunctionCallAfterStringParam<decltype(CHud_SetHelpMessage)>("CHEAT2", 1);
    if (CHud_SetHelpMessage)
    {
        auto SaveGameForPause = utility::find_function_from_string_ref(utility::get_executable(), "SaveGameForPause failed WaitForSave", true);
        if (SaveGameForPause)
        {
            SaveToSlot = reinterpret_cast<decltype(SaveToSlot)>(SaveGameForPause.value());

            INSTRUX ix{};
            auto ip = (uintptr_t)SaveToSlot;

            while (true)
            {
                const auto status = NdDecodeEx(&ix, (ND_UINT8*)ip, 1000, ND_CODE_64, ND_DATA_64);

                if (!ND_SUCCESS(status)) {
                    break;
                }

                if (ix.Instruction == ND_INS_MOV && ix.Operands[0].Type == ND_OP_REG && ix.Operands[1].Type == ND_OP_IMM && ix.Operands[1].Info.Immediate.Imm == 8)
                {
                    Slot = ip + ix.Imm1Offset;
                    break;
                }

                ip += ix.Length;
            }
        }

        if (Slot && SaveToSlot)
        {
            auto DrawMessages = utility::find_function_from_string_ref(utility::get_executable(), "%d:%02d", true);
            if (DrawMessages)
            {
                auto DrawMessagesStart = utility::find_function_start_with_call(DrawMessages.value());
                if (DrawMessagesStart)
                {
                    static auto DrawMessagesHook = safetyhook::create_mid(DrawMessagesStart.value(), [](SafetyHookContext& regs)
                    {
                        static bool bF5LastState = false;
                        bool bF5CurState = GetKeyState(VK_F5) & 0x8000;

                        if (!bF5LastState && bF5CurState)
                        {
                            if (!IsPlayerOnAMission())
                            {
                                injector::WriteMemory<uint32_t>(Slot, 5, true);
                                SaveToSlot(nIniSaveSlot);
                                injector::WriteMemory<uint32_t>(Slot, 8, true);

                                FString test;
                                test.Data = const_cast<wchar_t*>(L"FESZ_WR");
                                test.Max = *test.Data ? int(std::wcslen(test.Data)) + 1 : 0;
                                test.Count = test.Max;

                                CHud_SetHelpMessage(&test, 0, 0, 0, 0);
                            }
                        }

                        bF5LastState = bF5CurState;
                    });
                }
            }
        }
    }

    //if (fHudScale != 1.0f || fRadarScale != 1.0f)
    {
        auto str = utility::scan_string(utility::get_executable(), "%s %f %f %f %f %s %s", true);
        if (str)
        {
            auto strref = utility::scan_reference(utility::get_executable(), str.value());

            if (strref)
            {
                auto start = utility::find_function_start_with_call(strref.value());
                if (start)
                {
                    shParseWidgetPositionLineFromFile = safetyhook::create_inline(start.value(), ParseWidgetPositionLineFromFile);

                    INSTRUX ix{};
                    auto ip = strref.value();

                    while (true)
                    {
                        const auto status = NdDecodeEx(&ix, (ND_UINT8*)ip, 100, ND_CODE_64, ND_DATA_64);

                        if (!ND_SUCCESS(status)) {
                            break;
                        }

                        if (ix.Instruction == ND_INS_CALLNR)
                        {
                            injector::MakeCALLTrampoline(ip, vscanf_hook, true);
                            break;
                        }

                        ip += ix.Length;
                    }
                }
            }
        }
    }

    if (bIniDisableFirstPersonAimForRifles)
    {
        auto pattern = hook::pattern("48 BD 80 01 00 00 04 60 00 00 4C 89 A4 24");
        if (!pattern.empty())
        {
            struct ProcessPlayerWeaponHook
            {
                void operator()(injector::reg_pack& regs)
                {
                    regs.rbp = 0x600400000180;

                    if (regs.rdi)
                    {
                        enum eWeaponType
                        {
                            WEAPONTYPE_M4 = 26,
                            WEAPONTYPE_RUGER = 27,
                            WEAPONTYPE_M60 = 32
                        };

                        switch (regs.rsi)
                        {
                        case WEAPONTYPE_M4:
                        case WEAPONTYPE_RUGER:
                        case WEAPONTYPE_M60:
                            regs.rdi = 0;
                            break;
                        default:
                            break;
                        }
                    }
                }
            }; injector::MakeInline<ProcessPlayerWeaponHook>(pattern.get_first(0), pattern.get_first(10));
        }
    }

    if (bImproveCameraPC)
    {
        auto pGetRawInputData = utility::scan_ptr(utility::get_executable(), (uintptr_t)GetRawInputData);
        if (pGetRawInputData)
        {
            auto pGetRawInputDataRef = utility::scan_displacement_reference(utility::get_executable(), pGetRawInputData.value());
            if (pGetRawInputDataRef)
            {
                auto pWndProc = utility::find_function_start_with_call(pGetRawInputDataRef.value());
                if (pWndProc)
                {
                    shWndProc = safetyhook::create_inline(pWndProc.value(), CustomWndProc);
                }
            }
        }

        auto str = utility::scan_string(utility::get_executable(), "Unimplemented ViceCity::CRenderer::RequestObjectsInFrustum", true);
        if (str)
        {
            auto strref = utility::scan_reference(utility::get_executable(), str.value());

            if (strref)
            {
                auto disasm = utility::get_disassembly_behind(strref.value());

                auto it = std::find_if(disasm.rbegin(), disasm.rend(), [](auto& op) {
                    return op.instrux.Instruction == ND_INS_CALLNR;
                });

                if (it != disasm.rend())
                {
                    auto res = utility::resolve_displacement(it->addr);
                    if (res)
                    {
                        static auto CCameraProcessHook = safetyhook::create_mid(res.value(), [](SafetyHookContext& regs)
                        {
                            LockCursor();
                        });
                    }
                }
            }
        }
    }
}

CEXP void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallbackAtGetSystemTimeAsFileTime(Init);
    });
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        if (!IsUALPresent()) { InitializeASI(); }
    }
    return TRUE;
}
