// Display-only: shows the first moment (mean light-space depth) as grey.
Texture2D<float2> Moments : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

[numthreads(8, 8, 1)] void main(uint3 dispatchThreadId : SV_DispatchThreadID) {
	uint width;
	uint height;
	OutputTexture.GetDimensions(width, height);
	if (dispatchThreadId.x >= width || dispatchThreadId.y >= height)
		return;

	float depth = Moments.Load(int3(dispatchThreadId.xy, 0)).x;
	OutputTexture[dispatchThreadId.xy] = float4(depth.xxx, 1.0);
}
