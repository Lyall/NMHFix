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
int iResCount = 0;
int iDefaultViewportX = 854;
int iDefaultViewportY = 480;
float fHUDAspect = (float)640 / 854;

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
            spdlog::info("Skip Intro: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)IntroSkipScanResult + 0x7 - (uintptr_t)baseModule);
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

#ifndef NDEBUG
        static SafetyHookMid CurrentResolutionMidHook{};
        CurrentResolutionMidHook = safetyhook::create_mid(CurrentResolutionScanResult,
            [](SafetyHookContext& ctx)
            {
                if (ctx.esi)
                {
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

                    // Only log on resolution change and limit to 10
                    if ((iResX != iCurrentResX || iResY != iCurrentResY) && iResCount < 10) {
                        iCurrentResX = iResX;
                        iCurrentResY = iResY;
                        CalculateAspectRatio(true);
                    }
                    else if (iResCount >= 10) {
                        if (iResCount == 10) { spdlog::warn("Current Resolution: Log limit  of {} reached.", iResCount); }
                    }

                    iResCount++;
                }
            });
#else
        static SafetyHookMid CurrentResolutionMidHook{};
        CurrentResolutionMidHook = safetyhook::create_mid(CurrentResolutionScanResult,
            [](SafetyHookContext& ctx)
            {
                if (ctx.esi)
                {
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

                    // Only log on resolution change and limit to 10
                    if ((iResX != iCurrentResX || iResY != iCurrentResY) && iResCount < 10) {
                        iCurrentResX = iResX;
                        iCurrentResY = iResY;
                        CalculateAspectRatio(true);
                    }
                    else if (iResCount >= 10) {
                        if (iResCount == 10) { spdlog::warn("Current Resolution: Log limit  of {} reached.", iResCount); }
                    }

                    iResCount++;
                }
            });
#endif
    }
    else if (!CurrentResolutionScanResult)
    {
        spdlog::error("Current Resolution: Pattern scan failed.");
    }

    if (bFixRes) {
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
            OcclusionAspectMidHook = safetyhook::create_mid(OcclusionAspectScanResult,
                [](SafetyHookContext& ctx)
                {
                    ctx.xmm0.f32[0] = fAspectRatio;
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
}

void HUD()
{
    if (bFixHUD) {
        // Movies
        uint8_t* MovieSizeScanResult = Memory::PatternScan(baseModule, "0F ?? ?? ?? 83 ?? ?? ?? 83 ?? ?? ?? 83 ?? ?? ?? 8B ?? E8 ?? ?? ?? ??");
        uint8_t* MovieAspectScanResult = Memory::PatternScan(baseModule, "C7 44 ?? ?? ?? ?? ?? ?? 89 ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? ?? ??");
        if (MovieSizeScanResult && MovieAspectScanResult)
        {
            spdlog::info("Movies: Size: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MovieSizeScanResult + 0x4 - (uintptr_t)baseModule);
            spdlog::info("Movies: Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MovieAspectScanResult - (uintptr_t)baseModule);

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
                            *reinterpret_cast<int*>(ctx.eax + 0x20) = (int)fHUDWidth + (int)fHUDWidthOffset; // Width
                            *reinterpret_cast<int*>(ctx.eax + 0x18) = (int)fHUDWidthOffset;                  // Horizontal Offset
                        }
                        else if (fAspectRatio < fNativeAspect) {
                            *reinterpret_cast<int*>(ctx.eax + 0x24) = (int)fHUDHeight + (int)fHUDHeightOffset; // Height
                            *reinterpret_cast<int*>(ctx.eax + 0x1C) = (int)fHUDHeightOffset; // Vertical Offset
                        }
                    }
                });
        }
        else if (!MovieSizeScanResult || !MovieAspectScanResult)
        {
            spdlog::error("Movies: Pattern scan failed.");
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
    HUD();
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