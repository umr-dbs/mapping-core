
#ifndef MAPPING_NO_OPENCL

#include "raster/raster.h"
#include "raster/raster_priv.h"
#include "raster/typejuggling.h"
#include "raster/profiler.h"
#include "raster/opencl.h"

//#include <iostream>
#include <fstream>
#include <sstream>
//#include <memory>
#include <mutex>
#include <atomic>
//#include <string>

namespace RasterOpenCL {

static std::mutex opencl_mutex;
static std::atomic<int> initialization_status(0); // 0: not initialized, 1: success, 2: failure


static cl::Platform platform;

static cl::Context context;

static std::vector<cl::Device> deviceList;
static cl::Device device;

static cl::CommandQueue queue;


void init() {
	if (initialization_status == 0) {
		Profiler::Profiler p("CL_INIT");
		std::lock_guard<std::mutex> guard(opencl_mutex);
		if (initialization_status == 0) {
			// ok, let's initialize everything. Default to "failure".
			initialization_status = 2;

			// Platform
			std::vector<cl::Platform> platformList;
			cl::Platform::get(&platformList);
			if (platformList.size() == 0)
				throw PlatformException("No CL platforms found");
			//printf("Platform number is: %d\n", (int) platformList.size());
			platform = platformList[platformList.size()-1];

			/*
			for (size_t i=0;i<platformList.size();i++) {
				std::string platformVendor;
				platformList[i].getInfo((cl_platform_info)CL_PLATFORM_VENDOR, &platformVendor);
				printf("Platform vendor %d is: %s\n", (int) i, platformVendor.c_str());
			}
			*/

			// Context
			cl_context_properties cprops[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties)(platform)(), 0};
			try {
				context = cl::Context(
					CL_DEVICE_TYPE_CPU, // _CPU
					cprops
				);
			}
			catch (const cl::Error &e) {
				printf("Error %d: %s\n", e.err(), e.what());

				throw;
			}

			// Device
			deviceList = context.getInfo<CL_CONTEXT_DEVICES>();
			if (deviceList.size() == 0)
				throw PlatformException("No CL devices found");
			device = deviceList[0];


			// Command Queue
			// CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE is not set
			queue = cl::CommandQueue(context, device);

			initialization_status = 1;
		}
	}

	if (initialization_status != 1)
		throw PlatformException("could not initialize opencl");
}

void free() {
	std::lock_guard<std::mutex> guard(opencl_mutex);
	if (initialization_status == 1) {
		platform = cl::Platform();

		context = cl::Context();

		deviceList.clear();
		device = cl::Device();

		queue = cl::CommandQueue();
	}

	initialization_status = 0;
}

cl::Platform *getPlatform() {
	return &platform;
}

cl::Context *getContext() {
	return &context;
}

cl::Device *getDevice() {
	return &device;
}

cl::CommandQueue *getQueue() {
	return &queue;
}

cl::Kernel addProgram(const std::string &sourcecode, const char *kernelname) {
	cl::Program::Sources source(1, std::make_pair(sourcecode.c_str(), sourcecode.length()));
	cl::Program program(context, source);
	try {
		program.build(deviceList,"");
	}
	catch (cl::Error e) {
		throw PlatformException(std::string("Error building cl::Program: ")+kernelname+": "+program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(deviceList[0]));
	}

	cl::Kernel kernel(program, kernelname);
	return kernel;
}


static std::string readFileAsString(const char *filename) {
	std::ifstream file(filename);
	if (!file.is_open())
		throw OpenCLException("Unable to open CL code file");
	return std::string(std::istreambuf_iterator<char>(file), (std::istreambuf_iterator<char>()));

}

cl::Kernel addProgramFromFile(const char *filename, const char *kernelname) {
	return addProgram(readFileAsString(filename), kernelname);
}



void setKernelArgByGDALType(cl::Kernel &kernel, cl_uint arg, GDALDataType datatype, double value) {
	switch(datatype) {
		case GDT_Byte: kernel.setArg<uint8_t>(arg, value);return;
		case GDT_Int16: kernel.setArg<int16_t>(arg, value);return;
		case GDT_UInt16: kernel.setArg<uint16_t>(arg, value);return;
		case GDT_Int32: kernel.setArg<int32_t>(arg, value);return;
		case GDT_UInt32: kernel.setArg<uint32_t>(arg, value);return;
		case GDT_Float32: kernel.setArg<float>(arg, value);return;
		case GDT_Float64:
			throw MetadataException("Unsupported data type: Float64");
		case GDT_CInt16:
			throw MetadataException("Unsupported data type: CInt16");
		case GDT_CInt32:
			throw MetadataException("Unsupported data type: CInt32");
		case GDT_CFloat32:
			throw MetadataException("Unsupported data type: CFloat32");
		case GDT_CFloat64:
			throw MetadataException("Unsupported data type: CFloat64");
		default:
			throw MetadataException("Unknown data type");
	}
}

/*
struct RasterInfo {
	cl_double origin[3];
	cl_double scale[3];

	cl_double min, max, no_data;

	cl_uint size[3];

	cl_ushort epsg;
	cl_ushort has_no_data;
};

static const std::string rasterinfo_source(
"typedef struct {"
"	double origin[3];"
"	double scale[3];"
"	double min, max, no_data;"
"	uint size[3];"
"	ushort epsg;"
"	ushort has_no_data;"
"} RasterInfo;\n"
);
*/
struct RasterInfo {
	cl_uint size[3];
	cl_float origin[3];
	cl_float scale[3];

	cl_float min, max, no_data;

	cl_ushort epsg;
	cl_ushort has_no_data;
};

static const std::string rasterinfo_source(
"typedef struct {"
"	uint size[3];"
"	float origin[3];"
"	float scale[3];"
"	float min, max, no_data;"
"	ushort epsg;"
"	ushort has_no_data;"
"} RasterInfo;\n"
"#define R(t,x,y) (t ## _data[y * t ## _info->size[0] + x])\n"
);

void setKernelArgAsRasterinfo(cl::Kernel &kernel, cl_uint arg, GenericRaster *raster) {
	RasterInfo ri;
	for (int i=0;i<raster->lcrs.dimensions;i++) {
		ri.size[i] = raster->lcrs.size[i];
		ri.origin[i] = raster->lcrs.origin[i];
		ri.scale[i] = raster->lcrs.scale[i];
	}
	for (int i=raster->lcrs.dimensions;i<3;i++) {
		ri.size[i] = 1;
		ri.origin[i] = 0.0;
		ri.scale[i] = 1.0;
	}
	ri.epsg = raster->lcrs.epsg;

	ri.min = raster->dd.min;
	ri.max = raster->dd.max;
	ri.no_data = raster->dd.has_no_data ? raster->dd.no_data : 0.0;
	ri.has_no_data = raster->dd.has_no_data;

	kernel.setArg(arg, sizeof(RasterInfo), &ri);
}

cl::Buffer *getBufferWithRasterinfo(GenericRaster *raster) {
	RasterInfo ri;
	memset(&ri, 0, sizeof(RasterInfo));
	for (int i=0;i<raster->lcrs.dimensions;i++) {
		ri.size[i] = raster->lcrs.size[i];
		ri.origin[i] = raster->lcrs.origin[i];
		ri.scale[i] = raster->lcrs.scale[i];
	}
	for (int i=raster->lcrs.dimensions;i<3;i++) {
		ri.size[i] = 1;
		ri.origin[i] = 0.0;
		ri.scale[i] = 1.0;
	}
	ri.epsg = raster->lcrs.epsg;

	ri.min = raster->dd.min;
	ri.max = raster->dd.max;
	ri.no_data = raster->dd.has_no_data ? raster->dd.no_data : 0.0;
	ri.has_no_data = raster->dd.has_no_data;

	try {
		cl::Buffer *buffer = new cl::Buffer(
			*RasterOpenCL::getContext(),
			CL_MEM_READ_ONLY,
			sizeof(RasterInfo),
			nullptr
		);
		RasterOpenCL::getQueue()->enqueueWriteBuffer(*buffer, CL_TRUE, 0, sizeof(RasterInfo), &ri);

		return buffer;
	}
	catch (cl::Error &e) {
		std::stringstream ss;
		ss << "CL Error in getBufferWithRasterinfo(): " << e.err() << ": " << e.what();
		throw OpenCLException(ss.str());
	}
}

const std::string &getRasterInfoStructSource() {
	return rasterinfo_source;
}





CLProgram::CLProgram() : kernel(nullptr), argpos(0), finished(false), iteration_type(0), in_rasters(), out_rasters(), pointcollections() {
}
CLProgram::~CLProgram() {
	reset();
}
void CLProgram::addInRaster(GenericRaster *raster) {
	if (kernel)
		throw OpenCLException("addInRaster() must be called before compile()");
	in_rasters.push_back(raster);
}
void CLProgram::addOutRaster(GenericRaster *raster) {
	if (kernel)
		throw OpenCLException("addOutRaster() must be called before compile()");
	if (iteration_type == 0)
		iteration_type = 1;
	out_rasters.push_back(raster);
}

size_t CLProgram::addPointCollection(PointCollection *pc) {
	if (kernel)
		throw OpenCLException("addPointCollection() must be called before compile()");
	if (iteration_type == 0)
		iteration_type = 2;
	pointcollections.push_back(pc);
	return pointcollections.size() - 1;
}

void CLProgram::addPointCollectionPositions(size_t idx) {
	if (!kernel || finished)
		throw OpenCLException("addPointCollectionPositions() should only be called between compile() and run()");

	if (sizeof(cl_double2) != sizeof(Point))
		throw OpenCLException("sizeof(cl_double2) != sizeof(Point), cannot use opencl on PointCollections");

	PointCollection *pc = pointcollections.at(idx);
	size_t size = sizeof(Point) * pc->collection.size();

	auto clbuffer = new cl::Buffer(
		*RasterOpenCL::getContext(),
		CL_MEM_USE_HOST_PTR,
		size,
		pc->collection.data()
	);
	scratch_buffers.push_back(clbuffer);
	auto clhostptr = RasterOpenCL::getQueue()->enqueueMapBuffer(*clbuffer, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 0, size);
	scratch_maps.push_back(clhostptr);

	kernel->setArg(argpos++, *clbuffer);
}

void CLProgram::addPointCollectionAttribute(size_t idx, const std::string &name) {
	if (!kernel || finished)
		throw OpenCLException("addPointCollectionAttribute() should only be called between compile() and run()");

	PointCollection *pc = pointcollections.at(idx);
	auto &vec = pc->local_md_value.getVector(name);
	size_t size = sizeof(double) * vec.size();

	auto clbuffer = new cl::Buffer(
		*RasterOpenCL::getContext(),
		CL_MEM_USE_HOST_PTR,
		size,
		vec.data()
	);
	scratch_buffers.push_back(clbuffer);
	auto clhostptr = RasterOpenCL::getQueue()->enqueueMapBuffer(*clbuffer, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 0, size);
	scratch_maps.push_back(clhostptr);

	kernel->setArg(argpos++, *clbuffer);
}

template<typename T>
struct getCLTypeName {
	static const char *execute(Raster2D<T> *) {
		return RasterTypeInfo<T>::cltypename;
	}
};

void CLProgram::compile(const std::string &sourcecode, const char *kernelname) {
	// TODO: here, we could add everything into our cache.
	// key: hash(source) . (in-types) . (out-types) . kernelname

	if (iteration_type == 0)
		throw OpenCLException("No raster or pointcollection added, cannot iterate");

	std::stringstream assembled_source;
	assembled_source << RasterOpenCL::getRasterInfoStructSource();
	for (decltype(in_rasters.size()) idx = 0; idx < in_rasters.size(); idx++) {
		assembled_source << "typedef " << callUnaryOperatorFunc<getCLTypeName>(in_rasters[idx]) << " IN_TYPE" << idx << ";\n";
		// TODO: the first case is an optimization that may reduce our ability to cache programs.
		if (!in_rasters[idx]->dd.has_no_data)
			assembled_source << "#define ISNODATA"<<idx<<"(v,i) (false)\n";
		else if (in_rasters[idx]->dd.datatype == GDT_Float32 || in_rasters[idx]->dd.datatype == GDT_Float64)
			assembled_source << "#define ISNODATA"<<idx<<"(v,i) (i->has_no_data && (isnan(v) || v == i->no_data))\n";
		else
			assembled_source << "#define ISNODATA"<<idx<<"(v,i) (i->has_no_data && v == i->no_data)\n";
	}
	for (decltype(out_rasters.size()) idx = 0; idx < out_rasters.size(); idx++) {
		assembled_source << "typedef " << callUnaryOperatorFunc<getCLTypeName>(out_rasters[idx]) << " OUT_TYPE" << idx << ";\n";
	}

	assembled_source << sourcecode;

	std::string final_source = assembled_source.str();

	cl::Program program;
	try {
		cl::Program::Sources sources(1, std::make_pair(final_source.c_str(), final_source.length()));
		program = cl::Program(context, sources);
		program.build(deviceList,"");
	}
	catch (const cl::Error &e) {
		std::stringstream msg;
		msg << "Error building cl::Program: " << kernelname << ": " << e.what() << " " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(deviceList[0]);
		throw OpenCLException(msg.str());
	}

	try {
		kernel = new cl::Kernel(program, kernelname);

		for (decltype(in_rasters.size()) idx = 0; idx < in_rasters.size(); idx++) {
			GenericRaster *raster = in_rasters[idx];
			raster->setRepresentation(GenericRaster::Representation::OPENCL);
			kernel->setArg(argpos++, *raster->getCLBuffer());
			kernel->setArg(argpos++, *raster->getCLInfoBuffer());
		}
		for (decltype(out_rasters.size()) idx = 0; idx < out_rasters.size(); idx++) {
			GenericRaster *raster = out_rasters[idx];
			raster->setRepresentation(GenericRaster::Representation::OPENCL);
			kernel->setArg(argpos++, *raster->getCLBuffer());
			kernel->setArg(argpos++, *raster->getCLInfoBuffer());
		}
		for (decltype(pointcollections.size()) idx = 0; idx < pointcollections.size(); idx++) {
			PointCollection *points = pointcollections[idx];
			int size = points->collection.size();
			kernel->setArg<cl_int>(argpos++, (cl_int) size);
		}
	}
	catch (const cl::Error &e) {
		delete kernel;
		kernel = nullptr;
		std::stringstream msg;
		msg << "CL Error in compile(): " << e.err() << ": " << e.what();
		throw OpenCLException(msg.str());
	}

}

void CLProgram::compileFromFile(const char *filename, const char *kernelname) {
	compile(readFileAsString(filename), kernelname);
}

void CLProgram::run() {
	try {
		cl::Event event = run(nullptr);
		event.wait();
	}
	catch (cl::Error &e) {
		std::stringstream ss;
		ss << "CL Error: " << e.err() << ": " << e.what();
		throw OpenCLException(ss.str());
	}
}

cl::Event CLProgram::run(std::vector<cl::Event>* events_to_wait_for) {
	if (!kernel)
		throw OpenCLException("Cannot run() before compile()");

	if (finished)
		throw OpenCLException("Cannot run() a CLProgram twice (TODO: lift this restriction? Use case?)");

	finished = true;
	cl::Event event;

	cl::NDRange range;
	if (iteration_type == 1)
		range = cl::NDRange(out_rasters[0]->lcrs.size[0], out_rasters[0]->lcrs.size[1]);
	else if (iteration_type == 2)
		range = cl::NDRange(pointcollections[0]->collection.size());
	else
		throw OpenCLException("Unknown iteration_type, cannot create range");

	try {
		RasterOpenCL::getQueue()->enqueueNDRangeKernel(*kernel,
			cl::NullRange, // Offset
			range, // Global
			cl::NullRange, // local
			events_to_wait_for, //events to wait for
			&event //event to create
		);
		return event;
	}
	catch (cl::Error &e) {
		std::stringstream ss;
		ss << "CL Error: " << e.err() << ": " << e.what();
		throw OpenCLException(ss.str());
	}
	cleanScratch();
}


void CLProgram::cleanScratch() {
	size_t buffers = scratch_buffers.size();
	for (size_t i=0;i<buffers;i++) {
		auto clbuffer = scratch_buffers[i];
		if (!clbuffer)
			continue;
		if (i < scratch_maps.size()) {
			auto clhostptr = scratch_maps[i];
			RasterOpenCL::getQueue()->enqueueUnmapMemObject(*clbuffer, clhostptr);
		}
		scratch_buffers[i] = nullptr;
		delete clbuffer;
	}
	scratch_buffers.clear();
	scratch_maps.clear();
}

void CLProgram::reset() {
	if (kernel) {
		delete kernel;
		kernel = nullptr;
	}
	cleanScratch();
	argpos = 0;
	finished = false;
	iteration_type = 0;
	in_rasters.clear();
	out_rasters.clear();
	pointcollections.clear();
}


} // End namespace RasterOpenCL

#endif
