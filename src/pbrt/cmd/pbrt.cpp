#include <vector>
#inlcude <string>

//main program
int main(int argc, char *argv[])
{
	//convert command line arguments to string
	std::vector<std::string> args = GetCommandLineArguments(argv);

	//declare variables for parsed command line
	PBRTOptions options;
	std::vector<std::string> filenames;

	//process command-line arguments

	//init pbrt
	InitPBRT(options);

	//parse provided scene description files
	BasicScene scene;
	BasicSceneBuilder builder(&scene);
	ParseFiles(&builder, filenames);

	//render the scene
	if (Options->useGPU || options->wavefront)
		RenderWavefront(scene);
	else
		RenderCPU(scene);

	//clean up after render
	CleanupPBRT();
}