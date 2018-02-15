// Copyright (c) 2007-2015, Intel Corporation
//
// Redistribution  and  use  in source  and  binary  forms,  with  or  without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of  source code  must retain the  above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name  of Intel Corporation  nor the names of its contributors
//   may be used to  endorse or promote  products derived  from this  software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
// IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT OWNER  OR CONTRIBUTORS BE
// LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
// CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT LIMITED  TO,  PROCUREMENT  OF
// SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
// INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,  WHETHER IN
// CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//****************************************************************************

#ifndef RUNTIME_CLIENT
#define RUNTIME_CLIENT

#ifdef HARPv1
	#include <aalsdk/AAL.h>
	#include <aalsdk/xlRuntime.h>
	#include <aalsdk/AALLoggerExtern.h> // Logger

	#include <aalsdk/service/ICCIAFU.h>
	#include <aalsdk/service/ICCIClient.h>
#else
	#include <aalsdk/AALTypes.h>
	#include <aalsdk/Runtime.h>
	#include <aalsdk/AALLoggerExtern.h>

	#include <aalsdk/service/IALIAFU.h>
#endif

using namespace AAL;

// Convenience macros for printing messages and errors.
#ifdef MSG
# undef MSG
#endif // MSG
#define MSG(x) std::cout << __AAL_SHORT_FILE__ << ':' << __LINE__ << ':' << __AAL_FUNC__ << "() : " << x << std::endl
#ifdef ERR
# undef ERR
#endif // ERR
#define ERR(x) std::cerr << __AAL_SHORT_FILE__ << ':' << __LINE__ << ':' << __AAL_FUNC__ << "() **Error : " << x << std::endl

//****************************************************************************
// UN-COMMENT appropriate #define in order to enable either Hardware or ASE.
//    DEFAULT is to use Software Simulation.
//****************************************************************************
//#define  HWAFU
#define  ASEAFU

/// @brief   Define our Runtime client class so that we can receive the runtime started/stopped notifications.
///
/// We implement a Service client within, to handle AAL Service allocation/free.
/// We also implement a Semaphore for synchronization with the AAL runtime.
class RuntimeClient : public CAASBase, public IRuntimeClient {
public:
	RuntimeClient();
	~RuntimeClient();

	void end();
#ifdef HARPv1
	IRuntime* getRuntime();
#endif
	btBool isOK();

	// <begin IRuntimeClient interface>
	void runtimeStarted(IRuntime* pRuntime, const NamedValueSet &rConfigParms);
	void runtimeStopped(IRuntime *pRuntime);
	void runtimeStartFailed(const IEvent &rEvent);
#ifndef HARPv1
	void runtimeStopFailed(const IEvent &rEvent);
	void runtimeCreateOrGetProxyFailed(IEvent const &rEvent){}; //Not used
#endif 
	void runtimeAllocateServiceFailed( IEvent const &rEvent);
	void runtimeAllocateServiceSucceeded(IBase* pClient,TransactionID const &rTranID);
	void runtimeEvent(const IEvent &rEvent);
	// <end IRuntimeClient interface>

#ifdef HARPv1
	IRuntime        *m_pRuntime;  // Pointer to AAL runtime instance.
	btBool           m_isOK;      // Status
#endif
	Runtime          m_Runtime;   // AAL Runtime
	CSemaphore       m_Sem;       // For synchronizing with the AAL runtime.
};

#endif