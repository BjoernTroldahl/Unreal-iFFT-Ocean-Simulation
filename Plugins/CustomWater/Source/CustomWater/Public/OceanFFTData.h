#pragma once

#include "CoreMinimal.h"

// Needed to emulate Random properly
#define GPU_GRID_SIZE 256 
#define BUTTERFLY_COUNT 8

#define GRID_SIZE 256
#define NUM_CASCADES 4
#define PING_PONG_SLOTS 4

#define BATCH_COUNT 32

struct FOceanFFTData
{

public:

    const int32 GridSize = GRID_SIZE;
    const int32 NumCascades = NUM_CASCADES;

    const int32 HalfGridSize = GRID_SIZE / 2;
    const float DisplacementFactor = (256 / GRID_SIZE) * (256 / GRID_SIZE);

    const int32 ButterflyCount = BUTTERFLY_COUNT;

    // per cascade params    
    double Amplitude[NUM_CASCADES] = { 168000.f, 64000.f, 4000.f, 240.f };
    double WindDirectionality[NUM_CASCADES] = { 1.f, 1.f, 1.f, 1.f };
    double Choppiness[NUM_CASCADES] = { 1.5, 1.5, 1.5, 1.5 };
    double PatchLength[NUM_CASCADES] = { 10.f, 28.f, 432.f, 2000.f };
    double ShortWaveCutoff[NUM_CASCADES] = { 0.0002, 0.004, 4.f, 60.f };
    double LongWaveCutoff[NUM_CASCADES] = { 1.f, 0.25, 0.125, 0.04 };
    double WindTighten[NUM_CASCADES] = { 1.f, 1.f, 1.f, 1.f };

    // misc params
    float RepeatPeriod = 1000.f;
    float Gravity = 9.8f;
    float BaseFrequency = 2.f * PI / RepeatPeriod;

    // wind control params
    float WindSpeed = 16.f;
    float WindDirection = 90.f;
    double WindDir[2]; // FVector2

    // grids that contain calculation data
    float FFTGridDispXReal[GRID_SIZE * GRID_SIZE * NUM_CASCADES];
    float FFTGridDispXImag[GRID_SIZE * GRID_SIZE * NUM_CASCADES];
    float FFTGridDispYReal[GRID_SIZE * GRID_SIZE * NUM_CASCADES];
    float FFTGridDispYImag[GRID_SIZE * GRID_SIZE * NUM_CASCADES];
    float FFTGridDispZReal[GRID_SIZE * GRID_SIZE * NUM_CASCADES];
    float FFTGridDispZImag[GRID_SIZE * GRID_SIZE * NUM_CASCADES];

    float DisplacementGridX[GRID_SIZE * GRID_SIZE * NUM_CASCADES];
    float DisplacementGridY[GRID_SIZE * GRID_SIZE * NUM_CASCADES];
    float DisplacementGridZ[GRID_SIZE * GRID_SIZE * NUM_CASCADES];

    float SpectrumGridX[GRID_SIZE * GRID_SIZE * NUM_CASCADES];
    float SpectrumGridY[GRID_SIZE * GRID_SIZE * NUM_CASCADES];
    float SpectrumGridZ[GRID_SIZE * GRID_SIZE * NUM_CASCADES];
    float SpectrumGridW[GRID_SIZE * GRID_SIZE * NUM_CASCADES];
};