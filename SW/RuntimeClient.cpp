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

#include "RuntimeClient.h"

using namespace AAL;

///////////////////////////////////////////////////////////////////////////////
///
///  MyRuntimeClient Implementation
///
///////////////////////////////////////////////////////////////////////////////
RuntimeClient::RuntimeClient()
#ifdef HARPv1
: m_pRuntime(NULL), m_isOK(false)
#else
: m_Runtime(this)
#endif
{
	NamedValueSet configArgs;
	NamedValueSet configRecord;

	m_Sem.Create(0, 1);

#ifdef HARPv1
	// Publish our interface
	SetSubClassInterface(iidRuntimeClient, dynamic_cast<IRuntimeClient *>(this));

	// Using Hardware Services requires the Remote Resource Manager Broker Service
	//  Note that this could also be accomplished by setting the environment variable
	//   XLRUNTIME_CONFIG_BROKER_SERVICE to librrmbroker
#if defined( HWAFU )
	configRecord.Add(XLRUNTIME_CONFIG_BROKER_SERVICE, "librrmbroker");
	configArgs.Add(XLRUNTIME_CONFIG_RECORD,configRecord);
#endif
	if(!m_Runtime.start(this, configArgs)) {
	  m_isOK = false;
	  return;
	}
	m_Sem.Wait();
#else
 	// Register our Client side interfaces so that the Service can acquire them.
	// SetInterface() is inherited from CAASBase
	SetInterface(iidRuntimeClient, dynamic_cast<IRuntimeClient *>(this));

#if defined( HWAFU )
	// Specify that the remote resource manager is to be used.
	configRecord.Add(AALRUNTIME_CONFIG_BROKER_SERVICE, "librrmbroker");
	configArgs.Add(AALRUNTIME_CONFIG_RECORD, &configRecord);
#endif

	// Start the Runtime and wait for the callback by sitting on the semaphore.
	// the runtimeStarted() or runtimeStartFailed() callbacks should set m_bIsOK appropriately.
	if(!m_Runtime.start(configArgs)){
		m_bIsOK = false;
		return;
	}
	m_Sem.Wait();
	m_bIsOK = true;
#endif
}

RuntimeClient::~RuntimeClient() {
	m_Sem.Destroy();
}

btBool RuntimeClient::isOK() {
#ifdef HARPv1
	return m_isOK;
#else
	return m_bIsOK;
#endif
}

void RuntimeClient::runtimeStarted(IRuntime *pRuntime, const NamedValueSet &rConfigParms) {
#ifdef HARPv1
	// Save a copy of our runtime interface instance.
	m_pRuntime = pRuntime;
	m_isOK = true;
#else
	m_bIsOK = true;
#endif
	m_Sem.Post(1);
}

void RuntimeClient::end() {
	m_Runtime.stop();
	m_Sem.Wait();
}

void RuntimeClient::runtimeStopped(IRuntime *pRuntime) {
	MSG("Runtime stopped");
#ifdef HARPv1
	m_isOK = false;
#else
	m_bIsOK = false;
#endif
	m_Sem.Post(1);
}

void RuntimeClient::runtimeStartFailed(const IEvent &rEvent) {
#ifdef HARPv1
	IExceptionTransactionEvent * pExEvent = dynamic_ptr<IExceptionTransactionEvent>(iidExTranEvent, rEvent);
	ERR("Runtime start failed");
	ERR(pExEvent->Description());
#else
	ERR("Runtime start failed");
	PrintExceptionDescription(rEvent);
#endif
}

#ifndef HARPv1
void RuntimeClient::runtimeStopFailed(const IEvent &rEvent)
{
	MSG("Runtime stop failed");
	m_bIsOK = false;
	m_Sem.Post(1);
}
#endif

void RuntimeClient::runtimeAllocateServiceFailed( IEvent const &rEvent) {
#ifdef HARPv1
	IExceptionTransactionEvent * pExEvent = dynamic_ptr<IExceptionTransactionEvent>(iidExTranEvent, rEvent);
	ERR("Runtime AllocateService failed");
	ERR(pExEvent->Description());
#else
	ERR("Runtime AllocateService failed");
	PrintExceptionDescription(rEvent);
#endif
}

void RuntimeClient::runtimeAllocateServiceSucceeded(IBase *pClient, TransactionID const &rTranID) {
	MSG("Runtime Allocate Service Succeeded");
}

void RuntimeClient::runtimeEvent(const IEvent &rEvent) {
	MSG("Generic message handler (runtime)");
}

#ifdef HARPv1
IRuntime * RuntimeClient::getRuntime() {
	return m_pRuntime;
}
#endif