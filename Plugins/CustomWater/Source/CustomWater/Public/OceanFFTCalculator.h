#pragma once

#include "CoreMinimal.h"
#include "OceanFFTData.h"

class UNiagaraSystem;

DECLARE_STATS_GROUP(TEXT("Ocean"), STATGROUP_Ocean, STATCAT_Advanced);
#define OCEAN_SCOPE_CYCLE_COUNTER(Name) DECLARE_SCOPE_CYCLE_COUNTER(TEXT(#Name), STAT_##Name, STATGROUP_Ocean)

struct FOceanFFTCalculator {

public:
    // Wave Material Scale
    float Scale = 3.f;
    float OverlapScale = 16.f;
    float MultiplyScale = 100.f;

    void Initialize();
    void Calculate(UWorld* World);

    void ShowDebugDisplacementPoints(UWorld* World, const FVector& CharacterLocation);

    FVector GetDisplacementAtPoint(FVector PointLocation);

    // Calculation data
private:

    float CalculatedEngineTime = -1.f;

    // Value sampling and debugging
private:

    const int32 DebugGridSize = 10;
    const float DebugGridCellSize = 500.f;

    FVector GetCascadeValue(FVector PointLocation, int32 CascadeIndex);

    // Shader emulation logic
private:

    int32 BatchSize = GRID_SIZE / BATCH_COUNT;
    const float CentimetersPerMeter = 100.f;

    FOceanFFTData OceanData;

    void InitializeSpectrum();
    void PopulateSpectrum(FVector4 OneMinusWindDirectionality, FVector4 ComplexAmplitudes, FIntVector ThreadId);

    void CalculateGridTimeStep(float AnimationTime);

    void CalculateRowPasses();
    void CalculateColPasses();
    void CalculateRowPass(int32 Y, int32 CascadeIndex, float* PingPongArrayX, float* PingPongArrayY, float* PingPongArrayZ);
    void CalculateColPass(int32 Y, int32 CascadeIndex, float* PingPongArrayX, float* PingPongArrayY, float* PingPongArrayZ);

    // Utility
private:

    float Random(int32 Seed1, int32 Seed2, int32 Seed3, int32& DeterministicSeed);

    FORCEINLINE int32 GetIndex(int32 X, int32 Y, int32 Z)
    {
        return X + Y * OceanData.GridSize + Z * OceanData.GridSize * OceanData.GridSize;
    }

    FIntVector4 GetBoundingArrayIndexesFromUV(float U, float V, int32 ArraySize, bool bWrap);
    int32 WrapIndex(const int32 Index, const int32 ArraySize);
    int32 ClampIndex(const int32 Index, const int32 ArraySize);

    template<class TValue>
    static TValue BilinearInterpolation(float U, float V, TValue Value00, TValue Value01, TValue Value10, TValue Value11, int32 ArraySize);

};

template<class TValue>
TValue FOceanFFTCalculator::BilinearInterpolation(float U, float V, TValue Value00, TValue Value01, TValue Value10, TValue Value11, int32 ArraySize)
{
    const float fX = FMath::Frac(U * ArraySize - 0.5);
    const float fY = FMath::Frac(V * ArraySize - 0.5);

    float OneMinusfX = 1.f - fX;
    float OneMinusfY = 1.f - fY;

    TValue NewValue =
        (OneMinusfX * OneMinusfY) * Value00 +
        (OneMinusfX * fY) * Value01 +
        (fX * OneMinusfY) * Value10 +
        (fX * fY) * Value11;

    return NewValue;
};