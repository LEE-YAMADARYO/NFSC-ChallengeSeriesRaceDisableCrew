#include "pch.h"
#include "MinHook/include/MinHook.h"
#include <windows.h>
#include <cstdint>
#include <vector>
#include <string>

// 比赛 ID 在 GRaceParameters 对象中的偏移量
#define GRACEPARAMETERS_RACEID_OFFSET 0x38

// GRaceStatus 全局单例的地址
#define GRACESTATUS_GLOBAL_ADDR 0x00A98284

// GRaceParameters 对象指针在 GRaceStatus 对象中的偏移量
#define GRACESTATUS_RACEPARAMS_OFFSET 0x6A1C

// sub_61BA70 函数的地址
#define GET_ISCREWRACE_ADDR 0x61BA70

// 存储从配置文件中读取的比赛 ID 哈希值
std::vector<uint32_t> g_RaceIDsToDisable;

// 原始函数的指针
typedef int(__stdcall* tGetIsCrewRace)();
tGetIsCrewRace pGetIsCrewRace = nullptr;

// 函数声明
void ReadRaceIDsFromIni();

// Hook 函数
int __stdcall hkGetIsCrewRace() {
    // 1. 获取 GRaceStatus 对象的地址
    uint32_t* pRaceStatus = reinterpret_cast<uint32_t*>(GRACESTATUS_GLOBAL_ADDR);

    // 2. 从 GRaceStatus 对象中获取 GRaceParameters 对象的地址
    uint32_t pRaceParameters_addr = *(uint32_t*)((char*)(*pRaceStatus) + GRACESTATUS_RACEPARAMS_OFFSET);

    if (pRaceParameters_addr != 0) {
        // 3. 从 GRaceParameters 对象中读取比赛 ID 哈希值
        uint32_t currentRaceID = *(uint32_t*)(pRaceParameters_addr + GRACEPARAMETERS_RACEID_OFFSET);

        // 4. 检查当前比赛 ID 是否在配置文件列表中
        for (uint32_t raceID : g_RaceIDsToDisable) {
            if (currentRaceID == raceID) {
                // 如果匹配，返回 0 禁用车队成员
                return 0;
            }
        }
    }

    // 5. 如果不匹配或比赛参数对象不存在，则调用原始函数，保持游戏原有的逻辑
    return pGetIsCrewRace();
}

// 从配置文件中读取比赛 ID 哈希值
void ReadRaceIDsFromIni() {
    char modulePath[MAX_PATH];
    // 获取当前 ASI 的完整路径
    GetModuleFileNameA(GetModuleHandleA("DisableCrew.asi"), modulePath, MAX_PATH);
    std::string iniPath = modulePath;

    // 移除文件名，保留目录路径
    size_t lastSlash = iniPath.find_last_of("\\/");
    if (std::string::npos != lastSlash) {
        iniPath.erase(lastSlash + 1);
    }

    // 拼接配置文件名
    iniPath += "DisableCrewRaceHash.ini";

    const int max_lines = 100; // 配置文件遍历行数
    for (int i = 1; i <= max_lines; ++i) {
        char key[32];
        sprintf_s(key, "RaceID_%d", i);

        char value[20];
        GetPrivateProfileStringA("RaceIDs", key, "", value, sizeof(value), iniPath.c_str());

        if (strlen(value) > 0) {
            // 使用 try-catch 块处理可能的转换错误
            try {
                uint32_t raceID = std::stoul(value, nullptr, 0);
                g_RaceIDsToDisable.push_back(raceID);
            }
            catch (...) {
                // 若存在转换错误则忽略并继续读取下一行
            }
        }
        else {
            // 找到空值时，则停止读取
            break;
        }
    }
}

// 初始化 Hook
void Init() {
    ReadRaceIDsFromIni(); // 读取配置文件

    if (MH_Initialize() != MH_OK) {
        return;
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(GET_ISCREWRACE_ADDR), reinterpret_cast<LPVOID>(&hkGetIsCrewRace), reinterpret_cast<LPVOID*>(&pGetIsCrewRace)) != MH_OK) {
        MH_Uninitialize();
        return;
    }

    if (MH_EnableHook(reinterpret_cast<LPVOID>(GET_ISCREWRACE_ADDR)) != MH_OK) {
        MH_Uninitialize();
        return;
    }
}

// 取消 Hook
void Shutdown() {
    MH_DisableHook(reinterpret_cast<LPVOID>(GET_ISCREWRACE_ADDR));
    MH_RemoveHook(reinterpret_cast<LPVOID>(GET_ISCREWRACE_ADDR));
    MH_Uninitialize();
}

// DLL 入口点
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        Init();
    }
    else if (reason == DLL_PROCESS_DETACH) {
        Shutdown();
    }
    return TRUE;
}