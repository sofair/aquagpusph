/*
 *  This file is part of AQUAgpusph, a free CFD program based on SPH.
 *  Copyright (C) 2012  Jose Luis Cercos Pita <jl.cercos@upm.es>
 *
 *  AQUAgpusph is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  AQUAgpusph is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with AQUAgpusph.  If not, see <http://www.gnu.org/licenses/>.
 */

// ----------------------------------------------------------------------------
// Include the Problem setup manager header
// ----------------------------------------------------------------------------
#include <ScreenManager.h>

// ----------------------------------------------------------------------------
// Include the main header
// ----------------------------------------------------------------------------
#include <CalcServer/Portal/Portal.h>

// ----------------------------------------------------------------------------
// Include the calculation server
// ----------------------------------------------------------------------------
#include <CalcServer.h>

namespace Aqua{ namespace CalcServer{ namespace Portal{

Portal::Portal(InputOutput::ProblemSetup::sphPortal *portal)
	: Kernel("Portal")
	, mPath(0)
	, mPortal(portal)
	, program(0)
	, kernel(0)
{
	InputOutput::ProblemSetup *P = InputOutput::ProblemSetup::singleton();
	if(!mPortal){
	    printf("ERROR (Portal::Portal): Empty portal reference pointer, can't continue!\n");
	    exit(1);
	}
	//! 1st.- Get data
	int nChar = strlen(P->OpenCL_kernels.portal);
	if(nChar <= 0) {
	    printf("ERROR (Portal::Portal): mPath of Portal kernel is empty.\n");
	    exit(2);
	}
	mPath = new char[nChar+4];
	if(!mPath) {
	    printf("ERROR (Portal::Portal): Can't allocate memory for path.\n");
	    exit(3);
	}
	strcpy(mPath, P->OpenCL_kernels.portal);
	strcat(mPath, ".cl");
	//! 2nd.- Setup the kernel
	local_work_size = 256;
	global_work_size = globalWorkSize(local_work_size);
	if(setupOpenCL()) {
	    exit(4);
	}
	printf("\tINFO (Portal::Portal): Portal ready to work!\n");
}

Portal::~Portal()
{
	if(kernel)clReleaseKernel(kernel); kernel=0;
	if(program)clReleaseProgram(program); program=0;
	if(mPath) delete[] mPath; mPath=0;
}

bool Portal::execute()
{
	unsigned int i;
	InputOutput::ProblemSetup *P = InputOutput::ProblemSetup::singleton();
	InputOutput::ScreenManager *S = InputOutput::ScreenManager::singleton();
	CalcServer *C = CalcServer::singleton();
	cl_int flag=0;
	if(!mPortal){
	    S->addMessage(3, (char*)"(Portal::execute): Portal data unavailable.\n");
	    return true;
	}
	// printf("%f,%f\n", mPortal->out.normal.x, mPortal->out.normal.y);
	//! Send all variables to the server
	flag |= sendArgument(kernel, 0, sizeof(cl_mem ), (void*)&(C->imove));
	flag |= sendArgument(kernel, 1, sizeof(cl_mem ), (void*)&(C->posin));
	flag |= sendArgument(kernel, 2, sizeof(cl_uint), (void*)&(C->n));
	flag |= sendArgument(kernel, 3, sizeof(InputOutput::ProblemSetup::sphPortal), (void*)mPortal);
	if(flag != CL_SUCCESS) {
	    S->addMessage(3, (char*)"(Portal::execute): Imposible to send arguments to portal kernel.\n");
	    return true;
	}
	//! Execute the kernel
	#ifdef HAVE_GPUPROFILE
	    cl_event event;
	    cl_ulong end, start;
	    profileTime(0.f);
	    flag = clEnqueueNDRangeKernel(C->command_queue, kernel, 1, NULL, &global_work_size, NULL, 0, NULL, &event);
	#else
	    flag = clEnqueueNDRangeKernel(C->command_queue, kernel, 1, NULL, &global_work_size, NULL, 0, NULL, NULL);
	#endif
	if(flag != CL_SUCCESS) {
	    S->addMessage(3, (char*)"(Portal::execute): Can't execute the kernel.\n");
	    if(flag == CL_INVALID_KERNEL_ARGS)
	        S->addMessage(3, (char*)"\tInvalid kernel arguments.\n");
	    else if(flag == CL_INVALID_WORK_GROUP_SIZE)
	        S->addMessage(3, (char*)"\tInvalid local work group size.\n");
	    else if(flag == CL_INVALID_WORK_ITEM_SIZE)
	        S->addMessage(3, (char*)"\tInvalid local work group size (greather than maximum allowed value).\n");
	    else if(flag == CL_OUT_OF_RESOURCES)
	        S->addMessage(3, (char*)"\tDevice out of resources.\n");
	    else if(flag == CL_MEM_OBJECT_ALLOCATION_FAILURE)
	        S->addMessage(3, (char*)"\tAllocation error at device.\n");
	    else if(flag == CL_OUT_OF_HOST_MEMORY)
	        S->addMessage(3, (char*)"\tfailure to allocate resources required by the OpenCL implementation on the host.\n");
	    return true;
	}
	//! Profile the kernel execution
	#ifdef HAVE_GPUPROFILE
	    flag = clWaitForEvents(1, &event);
	    if(flag != CL_SUCCESS) {
	        S->addMessage(2, (char*)"(Portal::execute): Can't wait to kernels end.\n");
	        return true;
	    }
	    flag |= clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, 0);
	    flag |= clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, 0);
	    if(flag != CL_SUCCESS) {
	        S->addMessage(3, (char*)"(Portal::execute): Can't profile kernel execution.\n");
	        return true;
	    }
	    profileTime(profileTime() + (end - start)/1000.f);  // 10^-3 ms
	#endif
	return false;
}

bool Portal::setupOpenCL()
{
	CalcServer *C = CalcServer::singleton();
	printf("\tINFO (Portal::SetupOpenCL): Using OpenCL script \"%s\"\n", mPath);
	//! Load the kernels
	if(!loadKernelFromFile(&kernel, &program, C->context, C->device, mPath, "Portal", ""))
	    return true;
	return false;
}

}}}  // namespaces