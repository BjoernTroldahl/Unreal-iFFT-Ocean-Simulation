#include "OceanFFTCalculator.h"
#include "HAL/IConsoleManager.h"
#include "OceanFFTCalculator.ispc.generated.h"

#include "NiagaraSystem.h"

static TAutoConsoleVariable<int32> CVarShowOceanDisplacement(
    TEXT("ocean.ShowDisplacement"),
    0,
    TEXT("If true, will show the displacement calculated on the CPU"),
    ECVF_Default);

void FOceanFFTCalculator::Initialize()
{
    checkf((BatchSize * BATCH_COUNT) == OceanData.GridSize, TEXT("GridSize should be evenly divisible by BatchCount."));

    float WindDirectionRadians = PI / 180.f * OceanData.WindDirection;
    OceanData.WindDir[0] = FMath::Sin(WindDirectionRadians);
    OceanData.WindDir[1] = FMath::Cos(WindDirectionRadians);

    InitializeSpectrum();
}

void FOceanFFTCalculator::Calculate(UWorld* World)
{
    // don't do anything if it was already calculated this frame
    if (CalculatedEngineTime >= World->TimeSeconds) return;

    OCEAN_SCOPE_CYCLE_COUNTER(OceanCalculate);

    float AnimationTime = FMath::Fmod(World->TimeSeconds, OceanData.RepeatPeriod);

    CalculateGridTimeStep(AnimationTime);
    CalculateRowPasses();
    CalculateColPasses();

    CalculatedEngineTime = World->TimeSeconds;
}

void FOceanFFTCalculator::InitializeSpectrum()
{
    FVector4 OneMinusWindDirectionality = FVector4(
        1.f - OceanData.WindDirectionality[0],
        1.f - OceanData.WindDirectionality[1],
        1.f - OceanData.WindDirectionality[2],
        1.f - OceanData.WindDirectionality[3]
    );

    ParallelFor(BATCH_COUNT, [&](int32 BatchIndex)
        {
            int32 StartX = BatchIndex * BatchSize;
            for (int x = StartX; x < StartX + BatchSize; x++)
            {
                for (int y = 0; y < OceanData.GridSize; y++)
                {
                    for (int z = 0; z < OceanData.NumCascades; z++)
                    {
                        int32 RandomCounterDeterministic = 0;

                        int32 Index = (x - OceanData.HalfGridSize) + (y - OceanData.HalfGridSize) * GPU_GRID_SIZE + z * GPU_GRID_SIZE * GPU_GRID_SIZE;
                        float Random1 = Random(Index, 0, 0, RandomCounterDeterministic) * 2 * PI;
                        float Random2 = Random(Index, 0, 0, RandomCounterDeterministic) * 2 * PI;
                        float Random3 = Random(Index, 0, 0, RandomCounterDeterministic) * 2 * PI;
                        float Random4 = Random(Index, 0, 0, RandomCounterDeterministic) * 2 * PI;

                        FVector4 RandomMagnitudes = FVector4(Random1, Random1, Random2, Random2);
                        FVector4 RandomPhases = FVector4(FMath::Sin(Random3), FMath::Cos(Random3), FMath::Sin(Random4), FMath::Cos(Random4));
                        FVector4 ComplexAmplitudes = RandomMagnitudes * RandomPhases;

                        PopulateSpectrum(OneMinusWindDirectionality, ComplexAmplitudes, FIntVector(x, y, z));
                    }
                }
            }
        });
}

void FOceanFFTCalculator::PopulateSpectrum(FVector4 OneMinusWindDirectionality, FVector4 ComplexAmplitudes, FIntVector ThreadId)
{
    // // Retrive WaveVector from thread index. 
    FIntPoint Position = FIntPoint(ThreadId.X, ThreadId.Y);
    FVector2D WaveVector = FVector2D(Position.X - OceanData.HalfGridSize, Position.Y - OceanData.HalfGridSize);
    WaveVector *= 2.0f * PI;

    //Our parameters for each of cascade are stored as components of a 4D vector.
    //We access them, treating 4D vector as array of 4 scalars,
    //And our Z component of thread index is the index of cascade, current compute thread belongs to.
    WaveVector /= OceanData.PatchLength[ThreadId.Z];

    // Calculate magnitude of WaveVector
    float k = WaveVector.Size();

    //Result is magnitude of positive and negative spectrums
    FVector2D Result = FVector2D(0, 0);

    // Handle zero length case
    if (k > 0.0001f)
    {
        // // Calculate direction of wave vector
        FVector2D k_norm = WaveVector.GetSafeNormal();

        // Calculate dampening factor for wave directions not aligning with the wind
        // Positive spectrum is in R component, negative spectrum is in G component.
        FVector2D WindFactor;
        FVector2D WindDirVector = FVector2D(OceanData.WindDir[0], OceanData.WindDir[1]);
        WindFactor.X = k_norm.Dot(WindDirVector);
        WindFactor.Y = (-k_norm).Dot(WindDirVector);
        FVector2D WindFactorAbs = WindFactor.GetAbs();
        WindFactorAbs = FVector2D(
            FMath::Pow(WindFactorAbs.X, OceanData.WindTighten[ThreadId.Z]),
            FMath::Pow(WindFactorAbs.Y, OceanData.WindTighten[ThreadId.Z])
        );

        // Reduce magnitude of the waves, travelling in negative direction
        WindFactorAbs.X *= WindFactor.X > 0 ? 1 : OneMinusWindDirectionality[ThreadId.Z];
        WindFactorAbs.Y *= WindFactor.Y > 0 ? 1 : OneMinusWindDirectionality[ThreadId.Z];
        WindFactor = WindFactorAbs;

        // Phillips Ocean spectrum calculation
        float L = OceanData.WindSpeed * OceanData.WindSpeed / OceanData.Gravity;
        float UpperPart = exp(-1.0f / ((k * L) * (k * L)));
        float Spectrum = OceanData.Amplitude[ThreadId.Z] * UpperPart / (k * k * k * k);

        // Dampen waves, shorter than user controlled threshold
        Spectrum *= FMath::Exp(-(k * k) * OceanData.ShortWaveCutoff[ThreadId.Z]);
        Spectrum *= k < OceanData.LongWaveCutoff[ThreadId.Z] ? 0 : 1;

        // Only wind factor is different between positive and negative spectrums
        Result = FVector2D(FMath::Sqrt(Spectrum * WindFactor.X), FMath::Sqrt(Spectrum * WindFactor.Y));
        Result *= 1.0f / FMath::Sqrt(2.0f);
    }

    // Lastly, obtain complex amplitude/phases.
    FVector4 PositiveAndNegativeSpectrum = FVector4(Result.X, Result.X, Result.Y, Result.Y);
    PositiveAndNegativeSpectrum *= ComplexAmplitudes;

    // And obtain conjugate of negative spectrum by flipping its imaginary part. 
    PositiveAndNegativeSpectrum.W *= -1;

    // Prepare index of the grid, at which result will be stored and write the result.
    int32 Index = GetIndex(ThreadId.X, ThreadId.Y, ThreadId.Z);
    OceanData.SpectrumGridX[Index] = PositiveAndNegativeSpectrum.X;
    OceanData.SpectrumGridY[Index] = PositiveAndNegativeSpectrum.Y;
    OceanData.SpectrumGridZ[Index] = PositiveAndNegativeSpectrum.Z;
    OceanData.SpectrumGridW[Index] = PositiveAndNegativeSpectrum.W;
}

float FOceanFFTCalculator::Random(int32 Seed1, int32 Seed2, int32 Seed3, int32& DeterministicSeed)
{
    // emulates the rand function in NiagaraEmitterInstanceShader.usf

    DeterministicSeed++;

    uint32 x = Seed1 * 1664525 + 1013904223;
    uint32 y = Seed2 * 1664525 + 1013904223;
    uint32 z = (DeterministicSeed | (Seed3 << 16)) * 1664525 + 1013904223;

    x += y * z;
    y += z * x;
    z += x * y;
    x += y * z;
    y += z * x;
    z += x * y;

    return float((x >> 8) & 0x00ffffff) / 16777216.0; // 0x01000000 == 16777216
}

void FOceanFFTCalculator::CalculateGridTimeStep(float AnimationTime)
{
    OCEAN_SCOPE_CYCLE_COUNTER(VectorTimeStep);

    ParallelFor(BATCH_COUNT, [&](int32 BatchIndex)
        {
            int32 StartX = BatchIndex * BatchSize;

            ispc::FOceanFFTCalculator_TimeStepRow(
                StartX,
                StartX + BatchSize,
                AnimationTime,
                (ispc::FOceanFFTData&)OceanData
            );
        });
}

void FOceanFFTCalculator::CalculateRowPasses()
{
    ParallelFor(BATCH_COUNT, [&](int32 BatchIndex)
        {
            // each task needs it own ping pong array
            float PingPongArrayX[GRID_SIZE * PING_PONG_SLOTS];
            float PingPongArrayY[GRID_SIZE * PING_PONG_SLOTS];
            float PingPongArrayZ[GRID_SIZE * PING_PONG_SLOTS];

            int32 StartY = BatchIndex * BatchSize;
            for (int Y = StartY; Y < StartY + BatchSize; Y++)
            {
                for (int32 CascadeIndex = 0; CascadeIndex < OceanData.NumCascades; CascadeIndex++)
                {
                    CalculateRowPass(Y, CascadeIndex, PingPongArrayX, PingPongArrayY, PingPongArrayZ);
                }
            }
        });
}

void FOceanFFTCalculator::CalculateRowPass(
    int32 Y,
    int32 CascadeIndex,
    float* PingPongArrayX,
    float* PingPongArrayY,
    float* PingPongArrayZ
)
{
    OCEAN_SCOPE_CYCLE_COUNTER(VectorRowPass);

    ispc::FOceanFFTCalculator_RowPass(
        Y,
        CascadeIndex,
        (ispc::FOceanFFTData&)OceanData,
        PingPongArrayX,
        PingPongArrayY,
        PingPongArrayZ
    );
}

void FOceanFFTCalculator::CalculateColPasses()
{
    ParallelFor(BATCH_COUNT, [&](int32 BatchIndex)
        {
            float PingPongArrayX[GRID_SIZE * PING_PONG_SLOTS];
            float PingPongArrayY[GRID_SIZE * PING_PONG_SLOTS];
            float PingPongArrayZ[GRID_SIZE * PING_PONG_SLOTS];

            int32 StartY = BatchIndex * BatchSize;
            for (int Y = StartY; Y < StartY + BatchSize; Y++)
            {
                for (int32 CascadeIndex = 0; CascadeIndex < OceanData.NumCascades; CascadeIndex++)
                {
                    CalculateColPass(Y, CascadeIndex, PingPongArrayX, PingPongArrayY, PingPongArrayZ);
                }
            }
        });
}

void FOceanFFTCalculator::CalculateColPass(
    int32 Y,
    int32 CascadeIndex,
    float* PingPongArrayX,
    float* PingPongArrayY,
    float* PingPongArrayZ)
{
    OCEAN_SCOPE_CYCLE_COUNTER(VectorColPass);

    ispc::FOceanFFTCalculator_ColPass(
        Y,
        CascadeIndex,
        (ispc::FOceanFFTData&)OceanData,
        PingPongArrayX,
        PingPongArrayY,
        PingPongArrayZ
    );
}

void FOceanFFTCalculator::ShowDebugDisplacementPoints(UWorld* World, const FVector& CharacterLocation)
{
    if (CVarShowOceanDisplacement.GetValueOnGameThread())
    {
        OCEAN_SCOPE_CYCLE_COUNTER(OceanShowDebugDisplacementPoints);

        // get location to nearest meter and use that as the center of the points
        FVector Location = FVector(
            FMath::RoundHalfFromZero(CharacterLocation.X * DebugGridCellSize) / DebugGridCellSize,
            FMath::RoundHalfFromZero(CharacterLocation.Y * DebugGridCellSize) / DebugGridCellSize,
            0
        );

        const float DebugGridHalfSize = DebugGridSize / 2.f;

        for (int Y = 0; Y < DebugGridSize; Y++)
        {
            for (int X = 0; X < DebugGridSize; X++)
            {
                FVector GridPointLocation = FVector(
                    Location.X + ((X - DebugGridHalfSize) * DebugGridCellSize),
                    Location.Y + ((Y - DebugGridHalfSize) * DebugGridCellSize),
                    0.f
                );

                FVector Displacement = GetDisplacementAtPoint(GridPointLocation);
                FColor Color = FColor(0.f, 255.f, 0.f, 255.f);

                if (X == DebugGridHalfSize && Y == DebugGridHalfSize)
                    Color = FColor(255.f, 0.f, 0.f, 255.f);

                DrawDebugPoint(World, GridPointLocation * Scale / MultiplyScale + Displacement / Scale / OverlapScale, 5.f, Color, false, 0.f, 0);

                if ((X - DebugGridHalfSize) >= -3 && (X - DebugGridHalfSize) <= 3 &&
                    (Y - DebugGridHalfSize) >= -3 && (Y - DebugGridHalfSize) <= 3)
                {
                    DrawDebugString(World, GridPointLocation * Scale / MultiplyScale, LexToString(Displacement.Z), nullptr, Color, 0.f);
                }
            }
        }
    }
}

FVector FOceanFFTCalculator::GetCascadeValue(FVector PointLocation, int32 CascadeIndex)
{
    float U = FMath::Frac(PointLocation.X / OceanData.PatchLength[CascadeIndex] / CentimetersPerMeter);
    float V = FMath::Frac(PointLocation.Y / OceanData.PatchLength[CascadeIndex] / CentimetersPerMeter);
    FIntVector4 Indexes = GetBoundingArrayIndexesFromUV(U, V, OceanData.GridSize, true);

    int32 Index00 = GetIndex(Indexes.X, Indexes.Y, CascadeIndex);
    int32 Index01 = GetIndex(Indexes.X, Indexes.W, CascadeIndex);
    int32 Index10 = GetIndex(Indexes.Z, Indexes.Y, CascadeIndex);
    int32 Index11 = GetIndex(Indexes.Z, Indexes.W, CascadeIndex);

    return BilinearInterpolation(
        U,
        V,
        FVector(OceanData.DisplacementGridX[Index00], OceanData.DisplacementGridY[Index00], OceanData.DisplacementGridZ[Index00]), // Value00
        FVector(OceanData.DisplacementGridX[Index01], OceanData.DisplacementGridY[Index01], OceanData.DisplacementGridZ[Index01]), // Value01
        FVector(OceanData.DisplacementGridX[Index10], OceanData.DisplacementGridY[Index10], OceanData.DisplacementGridZ[Index10]), // Value10
        FVector(OceanData.DisplacementGridX[Index11], OceanData.DisplacementGridY[Index11], OceanData.DisplacementGridZ[Index11]), // Value11
        OceanData.GridSize
    );
}

FVector FOceanFFTCalculator::GetDisplacementAtPoint(FVector PointLocation)
{
    FVector Displacement = GetCascadeValue(PointLocation, 0);
    Displacement += GetCascadeValue(PointLocation, 1);
    Displacement += GetCascadeValue(PointLocation, 2);
    Displacement += GetCascadeValue(PointLocation, 3);
    return Displacement;
}

FIntVector4 FOceanFFTCalculator::GetBoundingArrayIndexesFromUV(float U, float V, int32 ArraySize, bool bWrap)
{
    const float X = U * (float)(ArraySize)-0.5f;
    const float Y = V * (float)(ArraySize)-0.5f;

    int32 X1 = FMath::Floor(X);
    int32 X2 = FMath::Floor(X + 1);
    int32 Y1 = FMath::Floor(Y);
    int32 Y2 = FMath::Floor(Y + 1);

    if (bWrap)
    {
        X1 = WrapIndex(X1, ArraySize);
        X2 = WrapIndex(X2, ArraySize);
        Y1 = WrapIndex(Y1, ArraySize);
        Y2 = WrapIndex(Y2, ArraySize);
        return FIntVector4(X1, Y1, X2, Y2);
    }
    else
    {
        X1 = ClampIndex(X1, ArraySize);
        X2 = ClampIndex(X2, ArraySize);
        Y1 = ClampIndex(Y1, ArraySize);
        Y2 = ClampIndex(Y2, ArraySize);
        return FIntVector4(X1, Y1, X2, Y2);
    }
}

int32 FOceanFFTCalculator::WrapIndex(const int32 Index, const int32 ArraySize)
{
    if (Index < 0)
        return ArraySize - 1;

    if (Index >= ArraySize)
        return 0;

    return Index;
}

int32 FOceanFFTCalculator::ClampIndex(const int32 Index, const int32 ArraySize)
{
    if (Index < 0)
        return 0;

    if (Index >= ArraySize)
        return ArraySize - 1;

    return Index;
}