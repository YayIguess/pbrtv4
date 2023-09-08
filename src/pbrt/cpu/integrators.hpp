#ifndef PBRTV4_SRC_PBRT_CPU_INTEGRATORS_HPP_
#define PBRTV4_SRC_PBRT_CPU_INTEGRATORS_HPP_

class Integrator {
 public:
	//integrator public methods
	virtual void Render() = 0;
	bool IntersectP(const Ray &ray, Float tMax = Infinity) const;
	pstd::optional<ShapeIntersection> Intersect(const Ray &ray, Float tMax) const;

	//integrator public members
	Primitive aggregate;
	std::vector<Light> lights;
 protected:
	//integrator protected methods
	Integrator(Primitive aggregate, std::vector<Light> lights) :
		aggregate(aggregate), lights(lights)
	{
		//integrator constructor implementation
		Bounds3f sceneBounds = aggregate ? aggregate.Bounds : Bounds3f();
		for(auto &light : lights)
		{
			light.Preprocess(sceneBounds);
			if(light.Type() == LightType::Infinite)
				infiniteLights.push_back(light);
		}
	};
};

class ImageTileIntegrator : public Integrator
{
 public:
	//imagetileintegrator public methods
	ImageTileIntegrator(Camera camera, Sampler sampler, Primitive aggregate, std::vector<Light> lights) :
		Integrator(aggregate, lights), camera(camera), samplerPrototype(sampler) {}

	void Render();

 protected:
	//imagetileintegrator protected members
	Camera camera;
	Sampler samplerPrototype;
};

#endif //PBRTV4_SRC_PBRT_CPU_INTEGRATORS_HPP_
