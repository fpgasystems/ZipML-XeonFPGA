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

///////////////////////////////////////////////////////////////////////////////
///
///  MyRuntimeClient Implementation
///
///////////////////////////////////////////////////////////////////////////////
RuntimeClient::RuntimeClient() :
m_Runtime(), // Instantiate the AAL Runtime
m_pRuntime(NULL),
m_isOK(false)
{
	NamedValueSet configArgs;
	NamedValueSet configRecord;

	// Publish our interface
	SetSubClassInterface(iidRuntimeClient, dynamic_cast<IRuntimeClient *>(this));

	m_Sem.Create(0, 1);

	// Using Hardware Services requires the Remote Resource Manager Broker Service
	// Note that this could also be accomplished by setting the environment variable
	// XLRUNTIME_CONFIG_BROKER_SERVICE to librrmbroker
#if defined( HWAFU )
	configRecord.Add(XLRUNTIME_CONFIG_BROKER_SERVICE, "librrmbroker");
	configArgs.Add(XLRUNTIME_CONFIG_RECORD,configRecord);
#endif

	if(!m_Runtime.start(this, configArgs)) {
		m_isOK = false;
		return;
	}
	m_Sem.Wait();
}

RuntimeClient::~RuntimeClient()
{
	m_Sem.Destroy();
}

btBool RuntimeClient::isOK()
{
	return m_isOK;
}

void RuntimeClient::runtimeStarted(IRuntime *pRuntime, const NamedValueSet &rConfigParms)
{
	// Save a copy of our runtime interface instance.
	m_pRuntime = pRuntime;
	m_isOK = true;
	m_Sem.Post(1);
}

void RuntimeClient::end()
{
	m_Runtime.stop();
	m_Sem.Wait();
}

void RuntimeClient::runtimeStopped(IRuntime *pRuntime)
{
	MSG("Runtime stopped");
	m_isOK = false;
	m_Sem.Post(1);
}

void RuntimeClient::runtimeStartFailed(const IEvent &rEvent)
{
	IExceptionTransactionEvent* pExEvent = dynamic_ptr<IExceptionTransactionEvent>(iidExTranEvent, rEvent);
	ERR("Runtime start failed");
	ERR(pExEvent->Description());
}

void RuntimeClient::runtimeAllocateServiceFailed( IEvent const &rEvent)
{
	IExceptionTransactionEvent* pExEvent = dynamic_ptr<IExceptionTransactionEvent>(iidExTranEvent, rEvent);
	ERR("Runtime AllocateService failed");
	ERR(pExEvent->Description());

}

void RuntimeClient::runtimeAllocateServiceSucceeded(IBase *pClient, TransactionID const &rTranID)
{
	MSG("Runtime Allocate Service Succeeded");
}

void RuntimeClient::runtimeEvent(const IEvent &rEvent)
{
	MSG("Generic message handler (runtime)");
}

IRuntime * RuntimeClient::getRuntime()
{
	return m_pRuntime;
}