#include "Globals.h"
#include "Window.h"
#include "GpuExecutor.h"
#include "CaveSystem.h"
#include "EntityManager.h"
#include "RendererInstanceStorage.h"
#include "Atlas.h"
#include "RendererUISystem.h"

Window* window = nullptr;
GpuExecutor* gpuExecutor = nullptr;
CaveSystem* caveSystem = nullptr;
EntityManager* ecs = nullptr;
RendererInstanceStorage *instanceStorage = nullptr;
AtlasRegion *atlasRegions = nullptr;
RendererUISystem *uiSystem = nullptr;
ParticleSystem *particleSystem = (ParticleSystem*)malloc(sizeof(ParticleSystem));

const uint32_t WIDTH = 1920;
const uint32_t HEIGHT = 1080;

void InitGlobals() {
    // TODO Reconsider usage of new here.
    window = new Window(WIDTH, HEIGHT, "StrongestSnake");

    ecs = new EntityManager();

    instanceStorage = new RendererInstanceStorage();

    atlasRegions = new AtlasRegion[MAX_ATLAS_ENTRIES];

    gpuExecutor = new GpuExecutor();
    gpuExecutor->init();

    uiSystem = new RendererUISystem();
    uiSystem->init(gpuExecutor->application, gpuExecutor->swapchain);

    ParticleSystem tmp = CreateParticleSystem(gpuExecutor->application, gpuExecutor->swapchain);
    memcpy(particleSystem, &tmp, sizeof(ParticleSystem));

    caveSystem = new CaveSystem();
}
