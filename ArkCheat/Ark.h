#include <string>
#include <chrono> 
#include <algorithm>
#include <mutex>
#include <thread>
#include <cmath> 
#include "Memory.h"
#include "Offsets.h"
#include "imgui.h"
#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
struct FMatrix
{
    float M[4][4];
};

struct ActorConfig
{
    std::string Name;
    int minlvl;
    int maxlvl;
    ActorConfig()
    {
        minlvl = 0;
        maxlvl = 999;
    }
};

struct CheatConfig
{
    std::vector<ActorConfig> Actors;
    bool OceanData;
    CheatConfig()
    {
        OceanData = true;
    }
};

struct FVector
{
    float X, Y, Z;
    // Dot product for FVector
    
};


struct FRotator 
{
    float Pitch, Yaw, Roll;  // In degrees (UE uses degrees internally)
};

struct ActorInfo 
{
    std::string className;
    FVector relativeLocation;
    bool isFemale;
    int level;
    ActorInfo()
    {
        isFemale = true;
    }
};

inline float Dot(const FVector& a, const FVector& b)
{
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

uintptr_t gname;
uintptr_t chunks;
std::vector<ActorInfo> g_actorsCache;
std::mutex g_configMutex; // Ensure this is defined globally alongside g_cacheMutex // Global cache for ESP drawing
std::mutex g_cacheMutex;
CheatConfig g_config;
float screenWidth;
float screenHeight;

// Initialize globals (call this once at program start)
void initGlobals(Memory& ark)
{
    if (!gname) 
    {
        gname = ark.GetModuleAddress(L"ShooterGame.exe") + offsets::gname;
    }
    if (!chunks) 
    {
        chunks = ark.Read<uintptr_t>(gname);
    }
    screenWidth = 1920.0;
    screenHeight = 1080.0;

}

void DrawText(float x, float y, const std::string& text, uint32_t color = 0xFFFFFFFF);  // ARGB color
void DrawLine(float x1, float y1, float x2, float y2, uint32_t color = 0xFFFFFFFF);




std::string getName(uintptr_t index, Memory &ark)
{
    // Step 1: GNames base
    if(!gname)
        uintptr_t gname = ark.GetModuleAddress(L"ShooterGame.exe") + offsets::gname;

    // Step 2: First pointer hop: chunks array
    if (!chunks)
        uintptr_t chunks = ark.Read<uintptr_t>(gname);

    // Step 3: Chunk math 
    uintptr_t chunkIndex = index / 16384;
    uintptr_t slotIndex = index % 16384;

    // Step 4: Second pointer hop: chunk pointer
    uintptr_t chunk = ark.Read<uintptr_t>(chunks + chunkIndex * sizeof(uintptr_t));

    // Step 5: Third pointer hop: FNameEntry* slot
    uintptr_t slot = ark.Read<uintptr_t>(chunk + slotIndex * sizeof(uintptr_t));

    if (!slot)
        return std::string(); // empty if null pointer

    char nameBuffer[0x400] = { 0 };
    SIZE_T bytesRead = 0;
    if (!::ReadProcessMemory(ark.processHandle, reinterpret_cast<LPCVOID>(slot + offsets::fName), nameBuffer, sizeof(nameBuffer) - 1, &bytesRead) || bytesRead == 0)
    {
        return std::string();
    }

    // Ensure null termination
    nameBuffer[sizeof(nameBuffer) - 1] = '\0';

    // Validate string content (optional: check for printable characters)
    for (size_t i = 0; i < bytesRead && nameBuffer[i] != '\0'; ++i)
    {
        if (!std::isprint(static_cast<unsigned char>(nameBuffer[i])))
        {
            nameBuffer[i] = '\0'; // Truncate at first non-printable character
            break;
        }
    }

    return std::string(nameBuffer);
}

std::vector<ActorInfo> findObjectsByClassName(Memory& ark)
{
    std::vector<ActorInfo> results;

    uintptr_t gWorld = ark.GetModuleAddress(L"ShooterGame.exe") + offsets::gworld;
    uintptr_t uWorld = ark.Read<uintptr_t>(gWorld);
    if (!uWorld) return results;

    std::vector<uintptr_t> levels;

    // PersistentLevel
    uintptr_t persistentLevel = ark.Read<uintptr_t>(uWorld + offsets::persistentLevel);
    if (persistentLevel)
        levels.push_back(persistentLevel);

    /*// Sublevels array
    uintptr_t levelsPtr = ark.Read<uintptr_t>(uWorld + offsets::levels);
    int32_t levelsCount = ark.Read<int32_t>(uWorld + offsets::levelsCount);
    for (int i = 0; i < levelsCount; i++)
    {
        uintptr_t level = ark.Read<uintptr_t>(levelsPtr + (sizeof(uintptr_t) * i));
        if (level) levels.push_back(level);
    }*/

    // Loop over levels
    for (uintptr_t level : levels)
    {
        uintptr_t actorsPtr = ark.Read<uintptr_t>(level + offsets::actors);
        int32_t actorsCount = ark.Read<int32_t>(level + offsets::actorsCount);

        for (int j = 0; j < actorsCount; j++)
        {
            if (results.size() > 100) break;
            uintptr_t actor = ark.Read<uintptr_t>(actorsPtr + (j * sizeof(uintptr_t)));
            if (!actor) continue;

            uintptr_t actorClass = ark.Read<uintptr_t>(actor + offsets::actorClass);

            int32_t nameindex = ark.Read<int32_t>(actor + offsets::uobjectFName);

            std::string className = getName(nameindex, ark);

            bool match = false;
            ActorInfo temp;
            temp.className = className;

            for (const auto& config : g_config.Actors)
            {
                if (className.find(config.Name) != std::string::npos)
                {
                    uintptr_t actorStatusComponent = ark.Read<uintptr_t>(actor + offsets::statusComponent);

                    int actorLevel = ark.Read<int>(actorStatusComponent + offsets::level);
                    actorLevel += ark.Read<int>(actorStatusComponent + offsets::level + sizeof(int));
                    if (className.empty() || actorLevel < 0) continue;
                    
                    if(actorLevel >= config.minlvl && actorLevel <= config.maxlvl)
                    {
                        temp.level = actorLevel;
                        //temp.isFemale = ark.Read<bool>(actor + offsets::isFemale);
                        match = true;
                        break;
                    }
                }
            }

            if (!match) continue;

            uintptr_t rootComponent = ark.Read<uintptr_t>(actor + offsets::actorRootComponent);
            if (rootComponent)
                temp.relativeLocation = ark.Read<FVector>(rootComponent + offsets::relativeLocation);

            if(temp.relativeLocation.X)
                results.push_back(temp);
        }
    }
    return results;
}
// Function to get local player position
FVector getLocalPlayerPosition(Memory& ark, uintptr_t uworld) 
{
    uintptr_t owningGameInstance = ark.Read<uintptr_t>(uworld + offsets::uWorldOwningGameInstance);
    uintptr_t localPlayers = ark.Read<uintptr_t>(owningGameInstance + offsets::gameInstanceLocalPlayers);
    uintptr_t localPlayer = ark.Read<uintptr_t>(localPlayers + 0x0);  // First player (index 0)
    uintptr_t playerController = ark.Read<uintptr_t>(localPlayer + offsets::localPlayerPlayerController);
    uintptr_t pawn = ark.Read<uintptr_t>(playerController + offsets::playerControllerPawn);
    uintptr_t rootComponent = ark.Read<uintptr_t>(pawn + offsets::actorRootComponent);
    return ark.Read<FVector>(rootComponent + offsets::relativeLocation);
}

// Background thread to update cache periodically
void updateActorsThread(Memory& ark)
{
    while (true)
    {
        auto newActors = findObjectsByClassName(ark);  // Adjust search and maxDistance
        {
            std::lock_guard<std::mutex> lock(g_cacheMutex);
            g_actorsCache = std::move(newActors);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));  // Update every 200ms (adjust 100-500ms)
    }
}

FMatrix MatrixMultiply(const FMatrix& a, const FMatrix& b) {
    FMatrix result = { 0 };
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result.M[i][j] = a.M[i][0] * b.M[0][j] + a.M[i][1] * b.M[1][j] +
                a.M[i][2] * b.M[2][j] + a.M[i][3] * b.M[3][j];
        }
    }
    return result;
}

FMatrix GetViewMatrix(Memory& ark)
{
    FMatrix viewProjMatrix = { 0 };

    uintptr_t base = ark.GetModuleAddress(L"ShooterGame.exe") + offsets::gworld;
    uintptr_t uWorld = ark.Read<uintptr_t>(base);
    if (!uWorld) return viewProjMatrix;

    uintptr_t owningGameInstance = ark.Read<uintptr_t>(uWorld + offsets::uWorldOwningGameInstance);
    uintptr_t localPlayers = ark.Read<uintptr_t>(owningGameInstance + offsets::gameInstanceLocalPlayers);
    uintptr_t localPlayer = ark.Read<uintptr_t>(localPlayers + 0x0);
    uintptr_t playerController = ark.Read<uintptr_t>(localPlayer + offsets::localPlayerPlayerController);
    uintptr_t cameraManager = ark.Read<uintptr_t>(playerController + offsets::cameraManager);
    if (!cameraManager) return viewProjMatrix;

    uintptr_t povAddr = cameraManager + offsets::cameraCache + offsets::pov;

    FVector camLocation = ark.Read<FVector>(povAddr + offsets::povLocation);
    FRotator camRotation = ark.Read<FRotator>(povAddr + offsets::povRotation);
    float fov = ark.Read<float>(povAddr + offsets::povFOV);
    float aspectRatio = ark.Read<float>(povAddr + offsets::povAspectRatio);
    if (aspectRatio <= 0.f) aspectRatio = 16.0f / 9.0f;

    if (camLocation.X == 0 && camLocation.Y == 0 && camLocation.Z == 0)
        return viewProjMatrix;

    // Euler -> basis (matching ARK/Unreal convention)
    float pitch = camRotation.Pitch * (PI / 180.0f);
    float yaw = camRotation.Yaw * (PI / 180.0f);
    float roll = camRotation.Roll * (PI / 180.0f);

    float SP = sinf(pitch), CP = cosf(pitch);
    float SY = sinf(yaw), CY = cosf(yaw);
    float SR = sinf(roll), CR = cosf(roll);

    FVector forward = { CP * CY, CP * SY, SP };
    FVector right = { SR * SP * CY - CR * SY, SR * SP * SY + CR * CY, -SR * CP };
    FVector up = { -(CR * SP * CY + SR * SY), -(CR * SP * SY - SR * CY), CR * CP };

    // Projection scales
    float fovRad = fov * (PI / 180.0f);
    float yScale = 1.0f / tanf(fovRad / 2.0f);
    float xScale = yScale / aspectRatio;

    // Build a matrix whose rows produce:
    // clipX = xScale * dot(world - cam, right)
    // clipY = yScale * dot(world - cam, up)
    // clipW =        dot(world - cam, forward)
    // So that ndcX = clipX / clipW = (viewX * xScale)/viewZ
    viewProjMatrix.M[0][0] = xScale * right.X;
    viewProjMatrix.M[0][1] = xScale * right.Y;
    viewProjMatrix.M[0][2] = xScale * right.Z;
    viewProjMatrix.M[0][3] = -xScale * Dot(camLocation, right);

    viewProjMatrix.M[1][0] = yScale * up.X;
    viewProjMatrix.M[1][1] = yScale * up.Y;
    viewProjMatrix.M[1][2] = yScale * up.Z;
    viewProjMatrix.M[1][3] = -yScale * Dot(camLocation, up);

    // (Optional depth row — left zero here. You can set a projection depth if needed.)
    viewProjMatrix.M[2][0] = 0.0f;
    viewProjMatrix.M[2][1] = 0.0f;
    viewProjMatrix.M[2][2] = 0.0f;
    viewProjMatrix.M[2][3] = 0.0f;

    // clipW row (we put forward vector here so clipW == viewZ)
    viewProjMatrix.M[3][0] = forward.X;
    viewProjMatrix.M[3][1] = forward.Y;
    viewProjMatrix.M[3][2] = forward.Z;
    viewProjMatrix.M[3][3] = -Dot(camLocation, forward);

    return viewProjMatrix;
}


bool WorldToScreen(const FVector& worldPos, const FMatrix& viewProjMatrix,
    float screenWidth, float screenHeight, float& screenX, float& screenY)
{
    float clipX = worldPos.X * viewProjMatrix.M[0][0] + worldPos.Y * viewProjMatrix.M[0][1] + worldPos.Z * viewProjMatrix.M[0][2] + viewProjMatrix.M[0][3];
    float clipY = worldPos.X * viewProjMatrix.M[1][0] + worldPos.Y * viewProjMatrix.M[1][1] + worldPos.Z * viewProjMatrix.M[1][2] + viewProjMatrix.M[1][3];
    
    float clipZ = worldPos.X * viewProjMatrix.M[2][0] + worldPos.Y * viewProjMatrix.M[2][1] + worldPos.Z * viewProjMatrix.M[2][2] + viewProjMatrix.M[2][3];
    float clipW = worldPos.X * viewProjMatrix.M[3][0] + worldPos.Y * viewProjMatrix.M[3][1] + worldPos.Z * viewProjMatrix.M[3][2] + viewProjMatrix.M[3][3];

    
    if (clipW <= 0.001f) return false;

    float ndcX = clipX / clipW;
    float ndcY = clipY / clipW;

    screenX = (screenWidth / 2.0f) * (1.0f + ndcX);
    screenY = (screenHeight / 2.0f) * (1.0f - ndcY);

    return (screenX >= 0 && screenX <= screenWidth && screenY >= 0 && screenY <= screenHeight);
}

void RenderDinos(Memory& ark, const std::vector<ActorInfo>& dinoPositions, float screenWidth = 1920.0f, float screenHeight = 1080.0f) 
{
    // Get view-proj matrix  // Invalid matrix
    FMatrix viewMatrix = GetViewMatrix(ark);
    if (viewMatrix.M[0][0] == 0) return;

    // Get player position (example; adjust offsets as needed)
    uintptr_t base = ark.GetModuleAddress(L"ShooterGame.exe") + offsets::gworld;
    uintptr_t uWorld = ark.Read<uintptr_t>(base);
    if (!uWorld) return;

    FVector playerPos = getLocalPlayerPosition(ark, uWorld);

    // Loop through dino positions
    for (const auto& dinoPos : dinoPositions) 
    {
        // Calculate distance to player (optional filter)
        float dist = sqrtf(
            powf(dinoPos.relativeLocation.X - playerPos.X, 2) +
            powf(dinoPos.relativeLocation.Y - playerPos.Y, 2) +
            powf(dinoPos.relativeLocation.Z - playerPos.Z, 2)
        );  // Skip if too far but not added

        // World to screen
        float screenX, screenY;
        if (!WorldToScreen(dinoPos.relativeLocation, viewMatrix, screenWidth, screenHeight,screenX, screenY)) continue;  // Not on screen

        // Draw ESP elements (examples)
        // 1. Draw text with distance
        std::string safeName = dinoPos.className.substr(0, std::min<size_t>(dinoPos.className.size(), 64));
        std::string label = std::to_string(dinoPos.level) + " " + safeName;
        DrawText(screenX, screenY, label, (dinoPos.isFemale ? 0xFFFF5555 : 0xFF0055FFF) ); 



        // 2. Optional: Draw line from screen center to dino (for aiming or visual)
        float centerX = screenWidth / 2.0f;
        float centerY = 0;
        DrawLine(centerX, centerY, screenX, screenY, (dinoPos.isFemale ? 0xFFFF5555 : 0xFF0055FFF));
    }
}

ImU32 ConvertColor(uint32_t argb) 
{
    uint8_t a = (argb >> 24) & 0xFF;
    uint8_t r = (argb >> 16) & 0xFF;
    uint8_t g = (argb >> 8) & 0xFF;
    uint8_t b = argb & 0xFF;
    return IM_COL32(r, g, b, a);
}

void DrawText(float x, float y, const std::string& text, uint32_t color) 
{
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();  
    drawList->AddText(ImVec2(x, y), ConvertColor(color), text.c_str());
}

void DrawLine(float x1, float y1, float x2, float y2, uint32_t color) 
{
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), ConvertColor(color));
}