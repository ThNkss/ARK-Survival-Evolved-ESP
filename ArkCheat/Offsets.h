#pragma once
#include <vector>

namespace offsets
{
    // ====== World & Names ======
    constexpr uintptr_t gworld = 0x49DC2A8; // UWorld*
    constexpr uintptr_t persistentLevel = 0xF8;      // UWorld::PersistentLevel
    constexpr uintptr_t uWorldOwningGameInstance = 0x290;      // UWorld::PersistentLevel
    constexpr uintptr_t gameInstanceLocalPlayers = 0x38;      // UWorld::PersistentLevel
    constexpr uintptr_t localPlayerPlayerController = 0x30;      // UWorld::PersistentLevel
    constexpr uintptr_t playerControllerPawn = 0x488;      // UWorld::PersistentLevel
    constexpr uintptr_t aActorRootComponent = 0xF8;      // UWorld::PersistentLevel
    constexpr uintptr_t levels = 0x268;     // UWorld::Levels[] (TArray<ULevel*>)
    constexpr uintptr_t levelsCount = 0x270;     // TArray Count for Levels[]
    constexpr uintptr_t actors = 0x88;      // ULevel::AActors[] (TArray<AActor*>)
    constexpr uintptr_t actorsCount = 0x90;      // TArray Count for AActors[]
    constexpr uintptr_t actorName = 0x18;      // TArray Count for AActors
    constexpr uintptr_t actorClass = 0x10;         // TArray Count for AActors[]
    constexpr uintptr_t statusComponent = 0xCD0;
    constexpr uintptr_t level = 0x6CC;
    constexpr uintptr_t isFemale = 0x0;
    constexpr uintptr_t cameraManager = 0x4F0;    
    constexpr uintptr_t viewMatrix = 0x4F0;
    constexpr uintptr_t gname = 0x49B0378; // GNames*
    constexpr uintptr_t gnameChunk = 0x404;     // size of chunk in GNames
    constexpr uintptr_t fName = 0x10;     // FName address offset
    
    // ====== GUObjectArray ======
    constexpr uintptr_t guobject = 0x49BFAA0; // GUObjectArray*
    constexpr uintptr_t guobjectChunk = 0x10;      // GUObjectArray chunk offset
    constexpr uintptr_t guobjectChunkCount = 0x2C;      // chunk count

    // ====== Object Reading ======
    constexpr uintptr_t fUObjectItemSize = sizeof(uintptr_t) * 2; // typically 16 bytes in UE4.5

    constexpr uintptr_t uobjectClass = 0x10;  // UObject::Class
    constexpr uintptr_t uobjectFName = 0x18;  // UObject::Name

    // ====== Actor / SceneComponent ======
    constexpr uintptr_t actorRootComponent = 0x250;// AActor::RootComponent

    constexpr uintptr_t relativeLocation = 0x158;   // USceneComponent::RelativeLocation FVector
    constexpr uintptr_t relativeLocationX = relativeLocation + 0x0;
    constexpr uintptr_t relativeLocationY = relativeLocation + 0x4;
    constexpr uintptr_t relativeLocationZ = relativeLocation + 0x8;


    // Offset to CameraCache within APlayerCameraManager
    constexpr uintptr_t cameraCache = 0x4D0;

    // Offset to POV within CameraCache (after TimeStamp)
    constexpr uintptr_t pov = 0x8;

    // Relative offsets within POV (FMinimalViewInfo)
    constexpr uintptr_t povLocation = 0x0;  // FVector Location
    constexpr uintptr_t povRotation = 0xC;  // FRotator Rotation (after Location's 12 bytes)
    constexpr uintptr_t povFOV = 0x28;      // float FOV (Location 12 + Rotation 12 = 0x18)
    constexpr uintptr_t povAspectRatio = 0x30;  // float AspectRatio (adjust based on full FMinimalViewInfo layout)

}
