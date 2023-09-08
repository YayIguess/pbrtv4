#include "integrators.hpp"

//integrator method definitions
pstd::optional<ShapeIntersection> Integrator::Intersect(const Ray &ray, Float tMax) const
{
	if(aggregate)
		return aggregate.Intersect(ray, tMax);
	else
		return {};
}

bool Integrator::IntersectP(const Ray &ray, Float tMax) const
{
	if (aggregate)
		return aggregate.IntersectP(ray, tMax);
	else
		return false;
}

void ImageTileIntegrator::Render()
{
	//declare common vars for rendering image in tiles
	ThreadLocal<ScratchBuffer> scratchBuffers(
		[]() { return ScratchBuffer(); }
		);

	ThreadLocal<Sampler> samplers(
		[this]() { return samplerPrototype.Clone(); }
		);

	Bounds2i pixelBounds = camera.GetFilm().PixelBounds();
	int spp = samplerPrototype.SamplesPerPixel();
	ProgressReporter progress(int64_t(spp) * pixelBounds.Area(), "Rendering", Options->quiet);
	int waveStart = 0, waveEnd = 1, nextWaveSize = 1;

	//render image in waves
	while(waveStart<spp)
	{
		//render current wave's image tiles in parallel
		//update start and end wave
		//optionally write current image to disk
	}
}