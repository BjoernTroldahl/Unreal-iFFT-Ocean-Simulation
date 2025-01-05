#include "Math/Vector.isph"

#define PING_PONG_SLOTS 4
#define GRID_SIZE 256
#define NUM_CASCADES 4

struct int2
{
	int X;
	int Y;
};
struct int3
{
	int X;
	int Y;
	int Z;
};
struct int4
{
	int X;
	int Y;
	int Z;
	int W;
};
struct float2
{
	float X;
	float Y;
};
struct float3
{
	float X;
	float Y;
	float Z;
};
struct float4
{
	float X;
	float Y;
	float Z;
	float W;
};
struct FOceanFFTData
{
    const int GridSize;
    const int NumCascades;

    const int HalfGridSize;
    const float DisplacementFactor;

    const int NumButterflyPasses;

    // per cascade params    
    double Amplitude[NUM_CASCADES];
    double WindDirectionality[NUM_CASCADES];
    double Choppiness[NUM_CASCADES];
    double PatchLength[NUM_CASCADES];
    double ShortWaveCutoff[NUM_CASCADES];
    double LongWaveCutoff[NUM_CASCADES];
    double WindTighten[NUM_CASCADES];

    // misc params
    float RepeatPeriod;
    float Gravity;
    float BaseFrequency;

    // wind control params
    float WindSpeed;
    float WindDirection;
    double WindDir[2];
    
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

inline float length(const float2 Value)
{
	return sqrt(Value.X * Value.X + Value.Y * Value.Y);
}
inline int2 MakeInt2(const int X, const int Y)
{
	int2 Result;
	Result.X = X;
	Result.Y = Y;
	return Result;
}
inline int3 MakeInt3(const int X, const int Y, const int Z)
{
	int3 Result;
	Result.X = X;
	Result.Y = Y;
	Result.Z = Z;
	return Result;
}
inline int4 MakeInt4(const int X, const int Y, const int Z, const int W)
{
	int4 Result;
	Result.X = X;
	Result.Y = Y;
	Result.Z = Z;
	Result.W = W;
	return Result;
}
inline uniform int4 MakeInt4(const uniform int X, const uniform int Y, const uniform int Z, const uniform int W)
{
	uniform int4 Result;
	Result.X = X;
	Result.Y = Y;
	Result.Z = Z;
	Result.W = W;
	return Result;
}
inline float2 MakeFloat2(const int X, const int Y)
{
	float2 Result;
	Result.X = X;
	Result.Y = Y;
	return Result;
}
inline float2 MakeFloat2(const float X, const float Y)
{
	float2 Result;
	Result.X = X;
	Result.Y = Y;
	return Result;
}
inline float3 MakeFloat3(const float X, const float Y, const float Z)
{
	float3 Result;
	Result.X = X;
	Result.Y = Y;
	Result.Z = Z;
	return Result;
}
inline float4 MakeFloat4(const float X, const float Y, const float Z, const float W)
{
	float4 Result;
	Result.X = X;
	Result.Y = Y;
	Result.Z = Z;
	Result.W = W;
	return Result;
}
inline float2 operator*(float2 A, float Scale)
{
    float2 Result;
    Result.X = A.X * Scale;
    Result.Y = A.Y * Scale;
    return Result;
}
inline float2 operator*(float2 A, float2 B)
{
    float2 Result;
    Result.X = A.X * B.X;
    Result.Y = A.Y * B.Y;
    return Result;
}
inline float2 operator/(float2 A, float Scale)
{
    float2 Result;
    Result.X = A.X / Scale;
    Result.Y = A.Y / Scale;
    return Result;
}
inline float3 operator*(float3 A, float Scale)
{
    float3 Result;
    Result.X = A.X * Scale;
    Result.Y = A.Y * Scale;
    Result.Z = A.Z * Scale;
    return Result;
}
inline float3 operator*(float3 A, float3 B)
{
    float3 Result;
    Result.X = A.X * B.X;
    Result.Y = A.Y * B.Y;
    Result.Z = A.Z * B.Z;
    return Result;
}
inline float3 operator/(float3 A, float Scale)
{
    float3 Result;
    Result.X = A.X / Scale;
    Result.Y = A.Y / Scale;
    Result.Z = A.Z / Scale;
    return Result;
}
inline float3 operator+(float3 A, float3 B)
{
    float3 Result;
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    Result.Z = A.Z + B.Z;
    return Result;
}
inline float3 operator-(float3 A, float3 B)
{
    float3 Result;
    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    Result.Z = A.Z - B.Z;
    return Result;
}
inline int GetIndex(int X, int Y, int Z, int GridSize)
{
    return X + Y * GridSize + Z * GridSize * GridSize;
}
inline float2 jAdd(float2 c0, float2 c1)
{
    return MakeFloat2(
        c0.X + c1.X,
        c0.Y + c1.Y
    );
}
inline float2 jMul(float2 c0, float2 c1)
{
    return MakeFloat2(
        c0.X * c1.X - c0.Y * c1.Y,
        c0.X * c1.Y + c0.Y * c1.X
    );
}
uint ReverseBits(uint v) 
{
    // swap odd and even bits
    v = ((v >> 1) & 0x55555555) | ((v & 0x55555555) << 1);
    // swap consecutive pairs
    v = ((v >> 2) & 0x33333333) | ((v & 0x33333333) << 2);
    // swap nibbles ... 
    v = ((v >> 4) & 0x0F0F0F0F) | ((v & 0x0F0F0F0F) << 4);
    // swap bytes
    v = ((v >> 8) & 0x00FF00FF) | ((v & 0x00FF00FF) << 8);
    // swap 2-byte long pairs
    return ( v >> 16             ) | ( v               << 16);
}

void GetRowPositions(
    const varying int X,
    const uniform int Y,
    varying int2& Position,
    varying int2& TexturePos
)
{
    Position = MakeInt2(X, Y);
    TexturePos = MakeInt2(X, Y);
}

void GetColPositions(
    const varying int X,
    const uniform int Y,
    varying int2& Position,
    varying int2& TexturePos
)
{
    Position = MakeInt2(X, Y);
    TexturePos = MakeInt2(Y, X);
}

void InitializeButterflyArray(
    const uniform bool bRowPass,
    const uniform int Y,
    const uniform int CascadeIndex,
    const uniform FOceanFFTData& OceanData,
    uniform float PingPongArrayX[],
    uniform float PingPongArrayY[],
    uniform float PingPongArrayZ[])
{
    foreach(X = 0 ... OceanData.GridSize) 
    {
        int2 Position;
        int2 TexturePos;

        // Transpose the texture position for the column pass
        if(bRowPass) { 
            GetRowPositions(X, Y, Position, TexturePos);
        } else {
            GetColPositions(X, Y, Position, TexturePos);
        }

        // Use a uniform offset and the X value from the foreach so the compiler
        // can optimize the consecutive array accesses
        const int Index = GetIndex(TexturePos.X, TexturePos.Y, CascadeIndex, OceanData.GridSize);

        // Load GRID 6 complex values from grid
        float HK_DX_DY_Texture_XR = OceanData.FFTGridDispXReal[Index];
        float HK_DX_DY_Texture_XI = OceanData.FFTGridDispXImag[Index];
        float HK_DX_DY_Texture_YR = OceanData.FFTGridDispYReal[Index];
        float HK_DX_DY_Texture_YI = OceanData.FFTGridDispYImag[Index];
        float HK_DX_DY_Texture_ZR = OceanData.FFTGridDispZReal[Index];
        float HK_DX_DY_Texture_ZI = OceanData.FFTGridDispZImag[Index];

        // These accesses require a scatter to store the value - nothing we can really
        // do about it
        int PingPongIndex = Position.X * PING_PONG_SLOTS;
        PingPongArrayX[PingPongIndex] = HK_DX_DY_Texture_XR;
        PingPongArrayY[PingPongIndex] = HK_DX_DY_Texture_YR;
        PingPongArrayZ[PingPongIndex] = HK_DX_DY_Texture_ZR;

        PingPongIndex = 1 + Position.X * PING_PONG_SLOTS;
        PingPongArrayX[PingPongIndex] = HK_DX_DY_Texture_XI;
        PingPongArrayY[PingPongIndex] = HK_DX_DY_Texture_YI;
        PingPongArrayZ[PingPongIndex] = HK_DX_DY_Texture_ZI;
    }
}

void GetButterflyValues(
    const uniform int NumButterflyPasses,
    const uniform int GridSize,
    const uniform int PassIndex, 
    const varying int X, 
    varying int2& Indices, 
    varying float2& Weights)
{
	uniform int SectionWidth = 2 << PassIndex;
	int HalfSectionWidth = SectionWidth / 2;
	
	int SectionStartOffset = X & ~(SectionWidth - 1);
	int HalfSectionOffset = X & (HalfSectionWidth - 1);
	int SectionOffset= X & (SectionWidth - 1);
	
	sincos(2.0f * PI * SectionOffset / (float)SectionWidth, &Weights.Y, &Weights.X);
	Weights.Y = -Weights.Y;
	
	Indices.X = SectionStartOffset + HalfSectionOffset;
	Indices.Y = SectionStartOffset + HalfSectionOffset + HalfSectionWidth;
	
	if (PassIndex == 0)
	{
        uint xReversed = ReverseBits((uint32)Indices.X);
        uint yReversed = ReverseBits((uint32)Indices.Y);

        uint xShifted = xReversed >> (32 - NumButterflyPasses);
        uint yShifted = yReversed >> (32 - NumButterflyPasses);

		Indices = MakeInt2(
            xShifted & (GridSize - 1),
            yShifted & (GridSize - 1)
        );
	}
}

void ButterflyPass(
    const uniform int NumButterflyPasses,
    const uniform int GridSize,
    const uniform float PingPongArrayX[],
    const uniform float PingPongArrayY[],
    const uniform float PingPongArrayZ[],
    const uniform int PassIndex,
    const varying int X,
    const uniform int T0,
    const uniform int T1,
    varying float3& ResultR,
    varying float3& ResultI
)
{
    int2 Indices;
	float2 Weights;
	
	GetButterflyValues(NumButterflyPasses, GridSize, PassIndex, X, Indices, Weights);
	
    int Index = T0 + Indices.X * PING_PONG_SLOTS;
	float3 InputR1 = MakeFloat3(
        PingPongArrayX[Index],
        PingPongArrayY[Index],
        PingPongArrayZ[Index]
    );
    Index = T1 + Indices.X * PING_PONG_SLOTS;
	float3 InputI1 = MakeFloat3(
        PingPongArrayX[Index],
        PingPongArrayY[Index],
        PingPongArrayZ[Index]
    );
	
    Index = T0 + Indices.Y * PING_PONG_SLOTS;
	float3 InputR2 = MakeFloat3(
        PingPongArrayX[Index],
        PingPongArrayY[Index],
        PingPongArrayZ[Index]
    );
    Index = T1 + Indices.Y * PING_PONG_SLOTS;
	float3 InputI2 = MakeFloat3(
        PingPongArrayX[Index],
        PingPongArrayY[Index],
        PingPongArrayZ[Index]
    );
	
	ResultR = (InputR1 + InputR2 * Weights.X + InputI2 * Weights.Y) * 0.5;
	ResultI = (InputI1 - InputR2 * Weights.Y + InputI2 * Weights.X) * 0.5;    
}

void ButterflyPassFinalNoI(
    const uniform int NumButterflyPasses,
    const uniform int GridSize,
    const uniform int PassIndex,    
    const uniform float PingPongArrayX[],
    const uniform float PingPongArrayY[],
    const uniform float PingPongArrayZ[],
    const varying int X,
    const uniform int T0,
    const uniform int T1,
    varying float3& ResultR)
{
	int2 Indices;
	float2 Weights;

	GetButterflyValues(NumButterflyPasses, GridSize, PassIndex, X, Indices, Weights);
	
    int Index = T0 + Indices.X * PING_PONG_SLOTS;
    float3 InputR1 = MakeFloat3(
        PingPongArrayX[Index],
        PingPongArrayY[Index],
        PingPongArrayZ[Index]
    );
	
    Index = T0 + Indices.Y * PING_PONG_SLOTS;
	float3 InputR2 = MakeFloat3(
        PingPongArrayX[Index],
        PingPongArrayY[Index],
        PingPongArrayZ[Index]
    );
    Index = T1 + Indices.Y * PING_PONG_SLOTS;
	float3 InputI2 = MakeFloat3(
        PingPongArrayX[Index],
        PingPongArrayY[Index],
        PingPongArrayZ[Index]
    );
 
	ResultR = (InputR1 + InputR2 * Weights.X + InputI2 * Weights.Y) * 0.5;
} 

uniform int4 CalculateButterflyPasses(
    const varying int Y,
    const uniform int NumButterflyPasses,
    const uniform int GridSize,
    uniform float PingPongArrayX[],
    uniform float PingPongArrayY[],
    uniform float PingPongArrayZ[])
{
    // Perform the butterfly passes - ensure all columns have been processed
    // before moving on the to the next pass

    uniform int4 TextureIndices = MakeInt4(0,1,2,3);

    // Repeat code for number of iFFT passes - perform each butterfly pass for all columns
    // before moving to the next butterfly pass
    for(uniform int PassIndex = 0; PassIndex < NumButterflyPasses - 1; PassIndex++)
    {
        foreach(X = 0 ... GridSize) 
        {
            int2 Position = MakeInt2(X, Y);
            float3 Real;
            float3 Imaginary;
    
            ButterflyPass(
                NumButterflyPasses, 
                GridSize, 
                PingPongArrayX,
                PingPongArrayY,
                PingPongArrayZ,
                PassIndex, 
                Position.X, 
                TextureIndices.X, 
                TextureIndices.Y, 
                Real, 
                Imaginary);

            int Index = TextureIndices.Z + Position.X * PING_PONG_SLOTS;
            PingPongArrayX[Index] = Real.X;
            PingPongArrayY[Index] = Real.Y;
            PingPongArrayZ[Index] = Real.Z;

            Index = TextureIndices.W + Position.X * PING_PONG_SLOTS;
            PingPongArrayX[Index] = Imaginary.X;
            PingPongArrayY[Index] = Imaginary.Y;
            PingPongArrayZ[Index] = Imaginary.Z;
        }

        TextureIndices = MakeInt4(TextureIndices.Z, TextureIndices.W, TextureIndices.X, TextureIndices.Y);
    }

    return TextureIndices;
}

void FinalRowPass(
    const uniform int Y,
    const uniform int CascadeIndex,
    const uniform int4 TextureIndices,
    const uniform float PingPongArrayX[],
    const uniform float PingPongArrayY[],
    const uniform float PingPongArrayZ[],
    uniform FOceanFFTData& OceanData
)
{
    foreach(X = 0 ... OceanData.GridSize) 
    {
        int2 Position;
        int2 TexturePos;
        GetRowPositions(X, Y, Position, TexturePos);

        // Perform the final butterfly pass and write the outputs
        float3 Real;
        float3 Imaginary;
        ButterflyPass(
            OceanData.NumButterflyPasses, 
            OceanData.GridSize,
            PingPongArrayX,
            PingPongArrayY,
            PingPongArrayZ,
            OceanData.NumButterflyPasses - 1, 
            Position.X, 
            TextureIndices.X, 
            TextureIndices.Y, 
            Real, 
            Imaginary);

        // Generate the index based on the texture position (which is transposed for the col pass)
        int Index = GetIndex(TexturePos.X, TexturePos.Y, CascadeIndex, OceanData.GridSize);

        //Store resulting complex values into FFTGrid
        OceanData.FFTGridDispXReal[Index] = Real.X;
        OceanData.FFTGridDispXImag[Index] = Imaginary.X;
        OceanData.FFTGridDispYReal[Index] = Real.Y;
        OceanData.FFTGridDispYImag[Index] = Imaginary.Y;
        OceanData.FFTGridDispZReal[Index] = Real.Z;
        OceanData.FFTGridDispZImag[Index] = Imaginary.Z;
    }
}

void FinalColPass(
    const uniform int Y,
    const uniform int CascadeIndex,
    const uniform int4 TextureIndices,
    const uniform float PingPongArrayX[],
    const uniform float PingPongArrayY[],
    const uniform float PingPongArrayZ[],
    uniform FOceanFFTData& OceanData
)
{
    foreach(X = 0 ... OceanData.GridSize) 
    {
        int2 Position;
        int2 TexturePos;
        GetColPositions(X, Y, Position, TexturePos);

        // Perform the final butterfly pass and write the outputs
        float3 Real;
        ButterflyPassFinalNoI(
            OceanData.NumButterflyPasses, 
            OceanData.GridSize,
            OceanData.NumButterflyPasses - 1,
            PingPongArrayX,
            PingPongArrayY,
            PingPongArrayZ,
            Position.X, 
            TextureIndices.X, 
            TextureIndices.Y, 
            Real);

        // flip the sign on alternating grid positions
        float PositionIndex = Position.X + Position.Y;
        if (floor(PositionIndex / 2) != (PositionIndex / 2))
        {
            Real = Real * -1.f;
        }
    
        Real = Real * 100.f / OceanData.DisplacementFactor;

        // Generate the index based on the texture position (which is transposed for the col pass)
        int Index = GetIndex(TexturePos.X, TexturePos.Y, CascadeIndex, OceanData.GridSize);

        //Store resulting complex values into FFTGrid
        OceanData.DisplacementGridX[Index] = Real.X;
        OceanData.DisplacementGridY[Index] = Real.Y;
        OceanData.DisplacementGridZ[Index] = Real.Z;
    }
}

export void FOceanFFTCalculator_TimeStepRow(    
    const uniform int StartRow,
    const uniform int EndRow,
    const uniform float AnimationTime,
    uniform FOceanFFTData& OceanData)
{
    foreach(X = StartRow ... EndRow, Y = 0 ... OceanData.GridSize, Z = 0 ... OceanData.NumCascades) 
    {
        // Replicate the thread id from the GPU algorithm
        int3 ThreadId = MakeInt3(X, Y, Z);

        // Retrive WaveVector from thread index
        float2 WaveVector = MakeFloat2(ThreadId.X - OceanData.HalfGridSize, ThreadId.Y - OceanData.HalfGridSize); 
        WaveVector = WaveVector * (2.0f * PI);
        WaveVector = WaveVector / OceanData.PatchLength[ThreadId.Z];

        // Calculate magnitude of WaveVector
        float k = length(WaveVector);

        // Dispersion. Angular frequency from wave vector
        float Freq = sqrt(OceanData.Gravity * k);

        // Quantize frequency to multiple of base frequency
        Freq = floor(Freq / OceanData.BaseFrequency) * OceanData.BaseFrequency;

        float Phase = Freq * AnimationTime;

        // Load initial spectrum
        int Index = GetIndex(X, Y, Z, OceanData.GridSize);
        float4 h0 = MakeFloat4(
            OceanData.SpectrumGridX[Index],
            OceanData.SpectrumGridY[Index],
            OceanData.SpectrumGridZ[Index],
            OceanData.SpectrumGridW[Index]
        );

        // Calculate exponents for positive and negative spectrums
        float2 fourier_amp = MakeFloat2(h0.X, h0.Y);
        float2 fourier_amp_conj = MakeFloat2(h0.Z, h0.W);
        float2 SineCosine;
        sincos(Phase, &SineCosine.X, &SineCosine.Y);

        float2 exponent = MakeFloat2(SineCosine.Y, SineCosine.X);
        float2 exponent_inv = MakeFloat2(SineCosine.Y, -SineCosine.X);

        // Resulting displacements
        float2 DispZ = MakeFloat2(0, 0);
        float2 DispX = MakeFloat2(0, 0);
        float2 DispY = MakeFloat2(0, 0); 

        //Handle zero length case.
        if (k > 0.000001f)
        {
            //Complex multiplication of positive spectrum by exponent
            float2 c0 = jMul(fourier_amp, exponent);

            //Complex multiplication of negative spectrum by inverse exponent
            float2 c1 = jMul(fourier_amp_conj, exponent_inv);

            //Complex addition of positive and negative parts
            DispZ = jAdd(c0, c1);

            float CascadeChoppiness = OceanData.Choppiness[ThreadId.Z];

            //Calculate horizontal displacements by projecting vertical displacement on components of wave direction
            float2 dx = MakeFloat2(0.0f, WaveVector.X / k) * CascadeChoppiness;
            DispX = jMul(DispZ,dx);

            float2 dy = MakeFloat2(0.0f,  WaveVector.Y / k) * CascadeChoppiness;
            DispY = jMul(DispZ,dy);
        }

        //Store results into the grid
        OceanData.FFTGridDispXReal[Index] = DispX.X;
        OceanData.FFTGridDispXImag[Index] = DispX.Y;
        OceanData.FFTGridDispYReal[Index] = DispY.X;
        OceanData.FFTGridDispYImag[Index] = DispY.Y;
        OceanData.FFTGridDispZReal[Index] = DispZ.X;
        OceanData.FFTGridDispZImag[Index] = DispZ.Y;
    }
}

export void FOceanFFTCalculator_RowPass(
    const uniform int Y,
    const uniform int CascadeIndex,
    uniform FOceanFFTData& OceanData,
    uniform float PingPongArrayX[],
    uniform float PingPongArrayY[],
    uniform float PingPongArrayZ[]
)
{
    InitializeButterflyArray(
        true,
        Y,
        CascadeIndex,
        OceanData,
        PingPongArrayX,
        PingPongArrayY,
        PingPongArrayZ
    );

    uniform int4 TextureIndices = CalculateButterflyPasses(
        Y,
        OceanData.NumButterflyPasses,
        OceanData.GridSize,
        PingPongArrayX,
        PingPongArrayY,
        PingPongArrayZ
    );

    FinalRowPass(
        Y,
        CascadeIndex,
        TextureIndices,
        PingPongArrayX,
        PingPongArrayY,
        PingPongArrayZ,
        OceanData
    );
}

export void FOceanFFTCalculator_ColPass(
    const uniform int Y,
    const uniform int CascadeIndex,
    uniform FOceanFFTData& OceanData,
    uniform float PingPongArrayX[],
    uniform float PingPongArrayY[],
    uniform float PingPongArrayZ[]
)
{
    InitializeButterflyArray(
        false,
        Y,
        CascadeIndex,
        OceanData,
        PingPongArrayX,
        PingPongArrayY,
        PingPongArrayZ
    );

    uniform int4 TextureIndices = CalculateButterflyPasses(
        Y,
        OceanData.NumButterflyPasses,
        OceanData.GridSize,
        PingPongArrayX,
        PingPongArrayY,
        PingPongArrayZ
    );

    FinalColPass(
        Y,
        CascadeIndex,
        TextureIndices,
        PingPongArrayX,
        PingPongArrayY,
        PingPongArrayZ,
        OceanData
    );
}