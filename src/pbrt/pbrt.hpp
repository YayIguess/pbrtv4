#ifndef PBRTV4_SRC_PBRT_PBRT_HPP_
#define PBRTV4_SRC_PBRT_PBRT_HPP_

#ifdef PBRT_FLOAT_AS_DOUBLE
	using Float = double
#else
	using Float = float
#endif

// Define Cache Line Size Constant
#ifdef PBRT_BUILD_GPU_RENDERER
#define PBRT_L1_CACHE_LINE_SIZE 128
#else
#define PBRT_L1_CACHE_LINE_SIZE 64
#endif

#endif //PBRTV4_SRC_PBRT_PBRT_HPP_
