#include "stdafx.h"
#include "helper.hpp"

#include <inipp/inipp.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <safetyhook.hpp>

HMODULE baseModule = GetModuleHandle(NULL);

// Version
std::string sFixName = "NMHFix";
std::string sFixVer = "0.7.0";
std::string sLogFile = sFixName + ".log";

// Logger
std::shared_ptr<spdlog::logger> logger;
std::filesystem::path sExePath;
std::string sExeName;

// Ini
inipp::Ini<char> ini;
std::string sConfigFile = sFixName + ".ini";
std::pair DesktopDimensions = { 0,0 };

// Ini variables
bool bSkipIntro;
bool bFixRes;
bool bFixAspect;
bool bFixFOV;
bool bFixHUD;

// Aspect ratio + HUD stuff
float fPi = (float)3.141592653;
float fAspectRatio;
float fNativeAspect = (float)16 / 9;
float fAspectMultiplier;
float fHUDWidth;
float fHUDHeight;
float fHUDWidthOffset;
float fHUDHeightOffset;

// Variables
int iResX;
int iResY;
int iCurrentResX;
int iCurrentResY;
int iDefaultViewportX = 854;
int iDefaultViewportY = 480;
float fHUDAspect = (float)640 / 854;
uintptr_t HUDAspect1Addr;
uintptr_t HUDAspect2Addr;
uintptr_t HUDAspect3Addr;
uintptr_t HUDWidthAddr;
uintptr_t HUDBackgroundWidthAddr;
uintptr_t HUDBackgroundHeightAddr;
bool bIsHUD;

void CalculateAspectRatio(bool bLog)
{
    // Calculate aspect ratio
    fAspectRatio = (float)iCurrentResX / (float)iCurrentResY;
    fAspectMultiplier = fAspectRatio / fNativeAspect;

    // HUD variables
    fHUDWidth = iCurrentResY * fNativeAspect;
    fHUDHeight = (float)iCurrentResY;
    fHUDWidthOffset = (float)(iCurrentResX - fHUDWidth) / 2;
    fHUDHeightOffset = 0;
    if (fAspectRatio < fNativeAspect) {
        fHUDWidth = (float)iCurrentResX;
        fHUDHeight = (float)iCurrentResX / fNativeAspect;
        fHUDWidthOffset = 0;
        fHUDHeightOffset = (float)(iCurrentResY - fHUDHeight) / 2;
    }

    if (bLog) {
        // Log details about current resolution
        spdlog::info("----------");
        spdlog::info("Current Resolution: Resolution: {}x{}", iCurrentResX, iCurrentResY);
        spdlog::info("Current Resolution: fAspectRatio: {}", fAspectRatio);
        spdlog::info("Current Resolution: fAspectMultiplier: {}", fAspectMultiplier);
        spdlog::info("Current Resolution: fHUDWidth: {}", fHUDWidth);
        spdlog::info("Current Resolution: fHUDHeight: {}", fHUDHeight);
        spdlog::info("Current Resolution: fHUDWidthOffset: {}", fHUDWidthOffset);
        spdlog::info("Current Resolution: fHUDHeightOffset: {}", fHUDHeightOffset);
        spdlog::info("----------");
    }
}

void Logging()
{
    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(baseModule, exePath, MAX_PATH);
    sExePath = exePath;
    sExeName = sExePath.filename().string();
    sExePath = sExePath.remove_filename();

    // spdlog initialisation
    {
        try {
            logger = spdlog::basic_logger_st(sFixName.c_str(), sExePath.string() + sLogFile, true);
            spdlog::set_default_logger(logger);

            spdlog::flush_on(spdlog::level::debug);
            spdlog::info("----------");
            spdlog::info("{} v{} loaded.", sFixName.c_str(), sFixVer.c_str());
            spdlog::info("----------");
            spdlog::info("Path to logfile: {}", sExePath.string() + sLogFile);
            spdlog::info("----------");

            // Log module details
            spdlog::info("Module Name: {0:s}", sExeName.c_str());
            spdlog::info("Module Path: {0:s}", sExePath.string());
            spdlog::info("Module Address: 0x{0:x}", (uintptr_t)baseModule);
            spdlog::info("Module Timestamp: {0:d}", Memory::ModuleTimestamp(baseModule));
            spdlog::info("----------");
        }
        catch (const spdlog::spdlog_ex& ex) {
            AllocConsole();
            FILE* dummy;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            std::cout << "Log initialisation failed: " << ex.what() << std::endl;
            FreeLibraryAndExitThread(baseModule, 1);
        }
    }
}

void Configuration()
{
    // Initialise config
    std::ifstream iniFile(sExePath.string() + sConfigFile);
    if (!iniFile) {
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << "" << sFixName.c_str() << " v" << sFixVer.c_str() << " loaded." << std::endl;
        std::cout << "ERROR: Could not locate config file." << std::endl;
        std::cout << "ERROR: Make sure " << sConfigFile.c_str() << " is located in " << sExePath.string().c_str() << std::endl;
        FreeLibraryAndExitThread(baseModule, 1);
    }
    else {
        spdlog::info("Path to config file: {}", sExePath.string() + sConfigFile);
        ini.parse(iniFile);
    }

    // Parse config
    ini.strip_trailing_comments();

    inipp::get_value(ini.sections["Fix Resolution"], "Enabled", bFixRes);
    spdlog::info("Config Parse: bFixRes: {}", bFixRes);
    inipp::get_value(ini.sections["Fix Aspect Ratio"], "Enabled", bFixAspect);
    spdlog::info("Config Parse: bFixAspect: {}", bFixAspect);
    inipp::get_value(ini.sections["Fix FOV"], "Enabled", bFixFOV);
    spdlog::info("Config Parse: bFixFOV: {}", bFixFOV);
    inipp::get_value(ini.sections["Skip Intro"], "Enabled", bSkipIntro);
    spdlog::info("Config Parse: bSkipIntro: {}", bSkipIntro);
    inipp::get_value(ini.sections["Fix HUD"], "Enabled", bFixHUD);
    spdlog::info("Config Parse: bFixHUD: {}", bFixHUD);

    // Grab desktop resolution/aspect
    DesktopDimensions = Util::GetPhysicalDesktopDimensions();
    iCurrentResX = DesktopDimensions.first;
    iCurrentResY = DesktopDimensions.second;
    CalculateAspectRatio(false);
}

void IntroSkip()
{
    if (bSkipIntro)
    {
        // Intro Skip
        uint8_t* IntroSkipScanResult = Memory::PatternScan(baseModule, "C7 ?? ?? 00 00 00 00 C6 ?? ?? 00 C6 ?? ?? ?? ?? ?? 00 C7 ?? ?? ?? ?? ?? ?? ?? ?? ??");
        if (IntroSkipScanResult)
        {
            spdlog::info("Skip Intro: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)IntroSkipScanResult - (uintptr_t)baseModule);
            Memory::Write((uintptr_t)IntroSkipScanResult + 0xA, (BYTE)1); // inLogoSkip = true
            spdlog::info("Skip Intro: Patched instruction.");
        }
        else if (!IntroSkipScanResult)
        {
            spdlog::error("Skip Intro: Pattern scan failed.");
        }
    }
}

void Resolution()
{
    // Get current resolution
    uint8_t* CurrentResolutionScanResult = Memory::PatternScan(baseModule, "8B ?? ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? C7 ?? ?? ?? ?? ?? ??");
    if (CurrentResolutionScanResult)
    {
        spdlog::info("Current Resolution: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)CurrentResolutionScanResult - (uintptr_t)baseModule);
        static SafetyHookMid CurrentResolutionMidHook{};
        CurrentResolutionMidHook = safetyhook::create_mid(CurrentResolutionScanResult,
            [](SafetyHookContext& ctx)
            {
                if (ctx.esi)
                {
                    // Debug build with pdb uses different offsets for this function.
                    #ifndef NDEBUG
                    if (bFixRes)
                    {
                        *reinterpret_cast<int*>(ctx.esi + 0x18) = *reinterpret_cast<int*>(ctx.esi + 0x18) + *reinterpret_cast<int*>(ctx.esi + 0x10);
                        *reinterpret_cast<int*>(ctx.esi + 0x1C) = *reinterpret_cast<int*>(ctx.esi + 0x1C) + *reinterpret_cast<int*>(ctx.esi + 0x14);

                        *reinterpret_cast<int*>(ctx.esi + 0x10) = 0; // Hor
                        *reinterpret_cast<int*>(ctx.esi + 0x14) = 0; // Vert

                        iResX = *reinterpret_cast<int*>(ctx.esi + 0x18);
                        iResY = *reinterpret_cast<int*>(ctx.esi + 0x1C);
                    }
                    else
                    {
                        iResX = *reinterpret_cast<int*>(ctx.esi + 0x18) - *reinterpret_cast<int*>(ctx.esi + 0x10);
                        iResY = *reinterpret_cast<int*>(ctx.esi + 0x1C) - *reinterpret_cast<int*>(ctx.esi + 0x14);
                    }
                    #else
                    if (bFixRes)
                    {
                        *reinterpret_cast<int*>(ctx.esi + 0xD0) = *reinterpret_cast<int*>(ctx.esi + 0xD0) + *reinterpret_cast<int*>(ctx.esi + 0xC8);
                        *reinterpret_cast<int*>(ctx.esi + 0xD4) = *reinterpret_cast<int*>(ctx.esi + 0xD4) + *reinterpret_cast<int*>(ctx.esi + 0xCC);

                        *reinterpret_cast<int*>(ctx.esi + 0xC8) = 0; // Hor
                        *reinterpret_cast<int*>(ctx.esi + 0xCC) = 0; // Vert

                        iResX = *reinterpret_cast<int*>(ctx.esi + 0xD0);
                        iResY = *reinterpret_cast<int*>(ctx.esi + 0xD4);
                    }
                    else
                    {
                        iResX = *reinterpret_cast<int*>(ctx.esi + 0xD0) - *reinterpret_cast<int*>(ctx.esi + 0xC8);
                        iResY = *reinterpret_cast<int*>(ctx.esi + 0xD4) - *reinterpret_cast<int*>(ctx.esi + 0xCC);
                    }
                    #endif

                    // Only log on resolution change
                    if (iResX != iCurrentResX || iResY != iCurrentResY) {
                        iCurrentResX = iResX;
                        iCurrentResY = iResY;
                        CalculateAspectRatio(true);
                    }
                }
            });
    }
    else if (!CurrentResolutionScanResult)
    {
        spdlog::error("Current Resolution: Pattern scan failed.");
    }

    if (bFixRes) {
        // Viewport
        uint8_t* ViewportScanResult = Memory::PatternScan(baseModule, "66 0F ?? ?? 0F ?? ?? 5F ?? A3 ?? ?? ?? ??");
        if (ViewportScanResult)
        {
            spdlog::info("Viewport: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ViewportScanResult - (uintptr_t)baseModule);

            static SafetyHookMid ViewportMidHook{};
            ViewportMidHook = safetyhook::create_mid(ViewportScanResult,
                [](SafetyHookContext& ctx)
                {
                    ctx.eax = iDefaultViewportX;
                });
        }
        else if (!ViewportScanResult)
        {
            spdlog::error("Viewport: Pattern scan failed.");
        }
    }
}

void AspectRatio()
{
    if (bFixAspect) {
        // Aspect ratio
        uint8_t* OcclusionAspectScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? F3 0F ?? ?? ?? ?? ?? ?? 85 ?? 0F 84 ?? ?? ?? ?? C7 ?? ?? ?? 80 02 00 00");
        uint8_t* ShadowAspectScanResult = Memory::PatternScan(baseModule, "0F 57 ?? ?? ?? ?? ?? 0F 28 ?? F3 0F ?? ?? 0F 28 ?? C7 05 ?? ?? ?? ?? 00 00 00 00");
        if (OcclusionAspectScanResult && ShadowAspectScanResult)
        {
            spdlog::info("Occlusion Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)OcclusionAspectScanResult - (uintptr_t)baseModule);
            spdlog::info("Shadow Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ShadowAspectScanResult - (uintptr_t)baseModule);

            static SafetyHookMid OcclusionAspectMidHook{};
            OcclusionAspectMidHook = safetyhook::create_mid(OcclusionAspectScanResult + 0x4,
                [](SafetyHookContext& ctx)
                {
                    ctx.xmm0.f32[0] = 1.00f;
                });

            static SafetyHookMid ShadowAspectMidHook{};
            ShadowAspectMidHook = safetyhook::create_mid(ShadowAspectScanResult,
                [](SafetyHookContext& ctx)
                {
                    ctx.xmm0.f32[0] = fAspectRatio;
                });
        }
        else if (!OcclusionAspectScanResult || !ShadowAspectScanResult)
        {
            spdlog::error("Aspect Ratio: Pattern scan failed.");
        }
    }

    if (bFixHUD)
    {
        // Movies
        uint8_t* MovieSizeScanResult = Memory::PatternScan(baseModule, "0F ?? ?? ?? 83 ?? ?? ?? 83 ?? ?? ?? 83 ?? ?? ?? 8B ?? E8 ?? ?? ?? ??");
        uint8_t* MovieAspectScanResult = Memory::PatternScan(baseModule, "C7 44 ?? ?? ?? ?? ?? ?? 89 ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? ?? ??");
        if (MovieSizeScanResult && MovieAspectScanResult)
        {
            spdlog::info("HUD: Movies: Size: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MovieSizeScanResult - (uintptr_t)baseModule);
            spdlog::info("HUD: Movies: Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MovieAspectScanResult - (uintptr_t)baseModule);

            static SafetyHookMid MovieAspectMidHook{};
            MovieAspectMidHook = safetyhook::create_mid(MovieAspectScanResult,
                [](SafetyHookContext& ctx)
                {
                    if (fAspectRatio != fNativeAspect)
                    {
                        ctx.xmm0.f32[0] = fNativeAspect;
                    }
                });

            static SafetyHookMid MovieSizeMidHook{};
            MovieSizeMidHook = safetyhook::create_mid(MovieSizeScanResult + 0x4,
                [](SafetyHookContext& ctx)
                {
                    if (ctx.eax + 0x20)
                    {
                        if (fAspectRatio > fNativeAspect)
                        {
                            *reinterpret_cast<int*>(ctx.eax + 0x20) = (int)fHUDWidth + (int)fHUDWidthOffset;    // Width
                            *reinterpret_cast<int*>(ctx.eax + 0x18) = (int)fHUDWidthOffset;                     // Horizontal Offset
                        }
                        else if (fAspectRatio < fNativeAspect) {
                            *reinterpret_cast<int*>(ctx.eax + 0x24) = (int)fHUDHeight + (int)fHUDHeightOffset;  // Height
                            *reinterpret_cast<int*>(ctx.eax + 0x1C) = (int)fHUDHeightOffset;                    // Vertical Offset
                        }
                    }
                });
        }
        else if (!MovieSizeScanResult || !MovieAspectScanResult)
        {
            spdlog::error("HUD: Movies: Pattern scan failed.");
        }
    }
}

void FOV()
{
    if (bFixFOV)
    {
        // Global FOV
        uint8_t* FOVScanResult = Memory::PatternScan(baseModule, "F3 0F 11 ?? ?? F3 0F 11 ?? ?? ?? ?? ?? F3 0F 59 ?? ?? ?? ?? ?? 56");
        if (FOVScanResult)
        {
            spdlog::info("FOV: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)FOVScanResult - (uintptr_t)baseModule);

            static SafetyHookMid FOVMidHook{};
            FOVMidHook = safetyhook::create_mid(FOVScanResult,
                [](SafetyHookContext& ctx)
                {
                    if (fAspectRatio < fNativeAspect)
                    {
                        ctx.xmm0.f32[0] = atanf(tanf(ctx.xmm0.f32[0] * (fPi / 360)) / (fAspectRatio) * (fNativeAspect)) * (360 / fPi);
                    }               
                });
        }
        else if (!FOVScanResult)
        {
            spdlog::error("FOV: Pattern scan failed.");
        }
    }
}

void HUD()
{
    if (bFixHUD) {
        
        // Set Viewport
        uint8_t* SetViewportScanResult = Memory::PatternScan(baseModule, "C7 ?? ?? 00 00 F0 43 E8 ?? ?? ?? ??");
        if (SetViewportScanResult)
        {
            spdlog::info("HUD: SetViewport: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)SetViewportScanResult - (uintptr_t)baseModule);
            uintptr_t SetViewportFuncAddr = (uintptr_t)SetViewportScanResult + 0xC + *reinterpret_cast<std::int32_t*>(SetViewportScanResult + 0x8);
            spdlog::info("HUD: SetViewport: Function address is {:s}+{:x}", sExeName.c_str(), SetViewportFuncAddr - (uintptr_t)baseModule);

            static SafetyHookMid SetViewportMidHook{};
            SetViewportMidHook = safetyhook::create_mid(SetViewportFuncAddr,
                [](SafetyHookContext& ctx)
                {
                    if (fAspectRatio > fNativeAspect)
                    {
                        ctx.xmm3.f32[0] = 480.00f * fAspectRatio;
                    }
                });

            #ifndef NDEBUG
            static SafetyHookMid SetViewport2MidHook{};
            SetViewport2MidHook = safetyhook::create_mid(SetViewportFuncAddr + 0x62,
                [](SafetyHookContext& ctx)
                {
                    if (fAspectRatio > fNativeAspect)
                    {
                        ctx.xmm3.f32[0] = 854.00f;
                    }
                });
            #else
            static SafetyHookMid SetViewport2MidHook{};
            SetViewport2MidHook = safetyhook::create_mid(SetViewportFuncAddr + 0x60,
                [](SafetyHookContext& ctx)
                {
                    if (fAspectRatio > fNativeAspect)
                    {
                        ctx.xmm3.f32[0] = 854.00f;
                    }
                });
            #endif
        }
        else if (!SetViewportScanResult)
        {
            spdlog::error("HUD: SetViewport: Pattern scan failed.");
        }

        // HUD Aspect Ratio
        uint8_t* HUDAspect1ScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? E8 ?? ?? ?? ?? 8B ?? ?? ?? 8D ?? ?? F2 0F ?? ??");
        uint8_t* HUDAspect2ScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? ?? 8B ?? ?? ?? ?? ?? 0F 28 ?? 0F ?? F3 0F ?? ?? 8B ?? ??");
        if (HUDAspect1ScanResult && HUDAspect2ScanResult)
        {
            uintptr_t HUDAspect1Value = *reinterpret_cast<std::int32_t*>(HUDAspect1ScanResult + 0x4);
            uintptr_t HUDAspect2Value = *reinterpret_cast<std::int32_t*>(HUDAspect2ScanResult + 0x5);

            HUDAspect1Addr = HUDAspect1Value;
            #ifndef NDEBUG
            HUDAspect2Addr = HUDAspect2Value - 0xC;
            #else
            HUDAspect2Addr = HUDAspect2Value - 0x4;
            #endif  
            HUDAspect3Addr = HUDAspect2Value + 0xC;
            HUDWidthAddr = HUDAspect2Value + 0x10;

            spdlog::info("HUD: Aspect Ratio: To16x9Xpos: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDAspect1Addr - (uintptr_t)baseModule);
            spdlog::info("HUD: Aspect Ratio: ScreenTable 1: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDAspect2Addr - (uintptr_t)baseModule);
            spdlog::info("HUD: Aspect Ratio: ScreenTable 2: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDAspect3Addr - (uintptr_t)baseModule);
            spdlog::info("HUD: Aspect Ratio: Width: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDWidthAddr - (uintptr_t)baseModule);
        }
        else if (!HUDAspect1ScanResult || !HUDAspect2ScanResult)
        {
            spdlog::error("HUD: Aspect Ratio: Pattern scan failed.");
        }

        // HUD Backgrounds
        uint8_t* HUDBackgroundScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? 0F 57 ?? F7 ?? 03 ?? C1 ?? ?? 8B ??");
        if (HUDBackgroundScanResult)
        {
            uintptr_t HUDBackgroundValue = *reinterpret_cast<std::int32_t*>(HUDBackgroundScanResult + 0x4);

            HUDBackgroundWidthAddr = HUDBackgroundValue;
            HUDBackgroundHeightAddr = HUDBackgroundValue - 0x12C;

            spdlog::info("HUD: Backgrounds: Width: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDBackgroundWidthAddr - (uintptr_t)baseModule);
            spdlog::info("HUD: Backgrounds: Height: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDBackgroundHeightAddr - (uintptr_t)baseModule);
        }
        else if (!HUDBackgroundScanResult)
        {
            spdlog::error("HUD: Aspect Ratio: Pattern scan failed.");
        }

        // DrawBox
        uint8_t* DrawBoxScanResult = Memory::PatternScan(baseModule, "0F 5B ?? E8 ?? ?? ?? ?? C6 ?? ?? ?? ?? ?? 00 C3");
        if (DrawBoxScanResult)
        {
            spdlog::info("HUD: DrawBox: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)DrawBoxScanResult - (uintptr_t)baseModule);
            uintptr_t DrawBoxFuncAddr = (uintptr_t)DrawBoxScanResult + 0x8 + *reinterpret_cast<std::int32_t*>(DrawBoxScanResult + 0x4);
            spdlog::info("HUD: DrawBox: Function address is {:s}+{:x}", sExeName.c_str(), DrawBoxFuncAddr - (uintptr_t)baseModule);

            static SafetyHookMid DrawBoxMidHook{};
            DrawBoxMidHook = safetyhook::create_mid(DrawBoxFuncAddr,
                [](SafetyHookContext& ctx)
                {
                    if (HUDAspect1Addr && HUDAspect2Addr && HUDAspect3Addr) 
                    {
                        if (fAspectRatio > fNativeAspect)
                        {
                            //Memory::Write(HUDAspect1Addr, 640.00f / (480.00f * fAspectRatio));
                            //Memory::Write(HUDAspect2Addr, 640.00f / (480.00f * fAspectRatio));
                            //Memory::Write(HUDAspect3Addr, 640.00f / (480.00f * fAspectRatio));
                        }
                    }

                    if (HUDWidthAddr)
                    {
                        if (fAspectRatio > fNativeAspect)
                        {
                            //Memory::Write(HUDWidthAddr, (int)(480.00f * fAspectRatio));
                        }
                    }

                    if (HUDBackgroundWidthAddr && HUDBackgroundHeightAddr)
                    {
                        if (fAspectRatio > fNativeAspect) 
                        {
                            Memory::Write(HUDBackgroundWidthAddr, (480.00f * fAspectRatio));
                        }
                    }
                });         
        }
        else if (!DrawBoxScanResult)
        {
            spdlog::error("HUD: DrawBox: Pattern scan failed.");
        }

        // MTXOrtho
        uint8_t* MTXOrthoScanResult = Memory::PatternScan(baseModule, "0F 57 ?? F3 0F ?? ?? ?? F3 0F 10 ?? ?? ?? ?? ?? F3 0F ?? ?? 0F 28 ?? 0F 28 ??");
        if (MTXOrthoScanResult)
        {
            spdlog::info("HUD: MTXOrtho: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MTXOrthoScanResult - (uintptr_t)baseModule);

            static SafetyHookMid MTXOrthoOffsetMidHook{};
            MTXOrthoOffsetMidHook = safetyhook::create_mid(MTXOrthoScanResult + 0x3,
                [](SafetyHookContext& ctx)
                {
                    if (bIsHUD)
                    {
                        if (fAspectRatio > fNativeAspect) 
                        {
                            ctx.xmm3.f32[0] = -1.00f / fAspectMultiplier;
                        }
                    }
                });
        }
        else if (!MTXOrthoScanResult)
        {
            spdlog::error("HUD: MTXOrtho: Pattern scan failed.");
        }
    }
}

DWORD __stdcall Main(void*)
{
    Logging();
    Configuration();
    IntroSkip();
    Resolution();
    AspectRatio();
    FOV();
    //HUD();
    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
    )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
        if (mainHandle)
        {
            CloseHandle(mainHandle);
        }
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}