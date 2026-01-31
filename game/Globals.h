#pragma once

struct Window;
struct GpuExecutor;
struct CaveSystem;
struct EntityManager;
struct RendererInstanceStorage;
struct AtlasRegion;
struct UISystem;
struct ParticleSystem;

extern Window* window;
extern GpuExecutor* gpuExecutor;
extern CaveSystem* caveSystem;
extern EntityManager* ecs;
extern RendererInstanceStorage *instanceStorage;
extern AtlasRegion *atlasRegions;
extern UISystem *uiSystem;
extern ParticleSystem *particleSystem;

void InitGlobals();
