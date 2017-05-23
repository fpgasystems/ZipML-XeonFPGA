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

#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>

#include "iFPGA.h"

using namespace AAL;

// <begin IServiceClient interface>
void iFPGA::serviceAllocated(IBase *pServiceBase, TransactionID const &rTranID)
{
	m_pAALService = pServiceBase;
	ASSERT(NULL != m_pAALService);

	// Documentation says CCIAFU Service publishes ICCIAFU as subclass interface
	m_AFUService = subclass_ptr<ICCIAFU>(pServiceBase);

	ASSERT(NULL != m_AFUService);
	if ( NULL == m_AFUService ) {
		return;
	}

	MSG("Service Allocated");
	m_Sem.Post(1);
}
void iFPGA::serviceAllocateFailed(const IEvent &rEvent)
{
	IExceptionTransactionEvent * pExEvent = dynamic_ptr<IExceptionTransactionEvent>(iidExTranEvent, rEvent);
	ERR("Failed to allocate a Service");
	ERR(pExEvent->Description());
	m_Sem.Post(1);
}
void iFPGA::serviceFreed(TransactionID const &rTranID)
{
	MSG("Service Freed");
	// Unblock Main()
	m_Sem.Post(1);
}
void iFPGA::serviceEvent(const IEvent &rEvent)
{
	ERR("unexpected event 0x" << hex << rEvent.SubClassID());
}

// <ICCIClient>
void iFPGA::OnWorkspaceAllocated(TransactionID const &TranID, btVirtAddr WkspcVirt, btPhysAddr WkspcPhys, btWSSize WkspcSize)
{
	AutoLock(this);

	if (TranID.ID() == 0)
	{
		m_DSMVirt = WkspcVirt;
		m_DSMPhys = WkspcPhys;
		m_DSMSize = WkspcSize;
		MSG("Got DSM");
		//printf("DSM Virt:%x, Phys:%x, Size:%d\n", m_DSMVirt, m_DSMPhys, m_DSMSize);
		m_Sem.Post(1);
	}
	else if(TranID.ID() >= 1 && TranID.ID() <= page_count)
	{
		int index = TranID.ID()-1;
		m_InputVirt[index] = WkspcVirt;
		m_InputPhys[index] = WkspcPhys;
		m_InputSize[index] = WkspcSize;
		// MSG("Got Input Workspace");
		// printf("Input Virt:%x, Phys:%x, Size:%d\n", m_InputVirt[index], m_InputPhys[index], m_InputSize[index]);
		m_Sem.Post(1);
	}
	else if(TranID.ID() >= page_count+1 && TranID.ID() <= 2*page_count)
	{
		int index = TranID.ID()-(page_count+1);
		m_OutputVirt[index] = WkspcVirt;
		m_OutputPhys[index] = WkspcPhys;
		m_OutputSize[index] = WkspcSize;
		// MSG("Got Output Workspace");
		// printf("Output Virt:%x, Phys:%x, Size:%d\n", m_OutputVirt[index], m_OutputPhys[index], m_OutputSize[index]);
		m_Sem.Post(1);
	}
	else
	{
		ERR("Invalid workspace type: " << TranID.ID());
	}
}
void iFPGA::OnWorkspaceAllocateFailed(const IEvent &rEvent)
{
	IExceptionTransactionEvent * pExEvent = dynamic_ptr<IExceptionTransactionEvent>(iidExTranEvent, rEvent);
	ERR("OnWorkspaceAllocateFailed");
	ERR(pExEvent->Description());
	m_Sem.Post(1);
}
void iFPGA::OnWorkspaceFreed(TransactionID const &TranID)
{
	// MSG("OnWorkspaceFreed");
	m_Sem.Post(1);
}
void iFPGA::OnWorkspaceFreeFailed(const IEvent &rEvent)
{
	IExceptionTransactionEvent * pExEvent = dynamic_ptr<IExceptionTransactionEvent>(iidExTranEvent, rEvent);
	ERR("OnWorkspaceAllocateFailed");
	ERR(pExEvent->Description());
	m_Sem.Post(1);
}



iFPGA::iFPGA(RuntimeClient *rtc):
m_pAALService(NULL),
m_runtimeClient(rtc),
m_AFUService(NULL),
m_DSMVirt(NULL),
m_DSMPhys(0),
m_DSMSize(0)
{
	//page_size_in_cache_lines = 128;
	page_size_in_cache_lines = 65536;
	
	for (int i = 0; i < page_count; i++)
	{
		m_InputVirt[i] = NULL;
		m_InputPhys[i] = 0;
		m_InputSize[i] = 0;
		m_OutputVirt[i] = NULL;
		m_OutputPhys[i] = 0;
		m_OutputSize[i] = 0;
	}

	SetSubClassInterface(iidServiceClient, dynamic_cast<IServiceClient *>(this));
	SetInterface(iidCCIClient, dynamic_cast<ICCIClient *>(this));
	m_Sem.Create(0, 1);

	allocateWorkspace();
}

iFPGA::~iFPGA()
{
	// Release the Workspaces and wait for all three then Release the Service
	for(int i = 0; i < page_count; i++)
	{
		m_AFUService->WorkspaceFree(m_InputVirt[i],  TransactionID(i+1));
		m_Sem.Wait();
	}
	for(int i = 0; i < page_count; i++)
	{
		m_AFUService->WorkspaceFree(m_OutputVirt[i],  TransactionID(page_count+i+1));
		m_Sem.Wait();
	}
	m_AFUService->WorkspaceFree(m_DSMVirt, TransactionID(0));
	m_Sem.Wait();
	(dynamic_ptr<IAALService>(iidService, m_pAALService))->Release(TransactionID());
	m_Sem.Wait();

	m_runtimeClient->end();

	m_Sem.Destroy();
}

void iFPGA::allocateWorkspace()
{
	// Request our AFU.

	// NOTE: This example is bypassing the Resource Manager's configuration record lookup
	// mechanism.  This code is work around code and subject to change. But it does
	// illustrate the utility of having different implementations of a service all
	// readily available and bound at run-time.
	NamedValueSet Manifest;
	NamedValueSet ConfigRecord;

#if defined( HWAFU )                /* Use FPGA hardware */

	ConfigRecord.Add(AAL_FACTORY_CREATE_CONFIGRECORD_FULL_SERVICE_NAME, "libHWCCIAFU");
	ConfigRecord.Add(keyRegAFU_ID,"C000C966-0D82-4272-9AEF-FE5F84570612");
	ConfigRecord.Add(AAL_FACTORY_CREATE_CONFIGRECORD_FULL_AIA_NAME, "libAASUAIA");

#elif defined ( ASEAFU )         /* Use ASE based RTL simulation */

	ConfigRecord.Add(AAL_FACTORY_CREATE_CONFIGRECORD_FULL_SERVICE_NAME, "libASECCIAFU");
	ConfigRecord.Add(AAL_FACTORY_CREATE_SOFTWARE_SERVICE,true);

#else                            /* default is Software Simulator */

	ConfigRecord.Add(AAL_FACTORY_CREATE_CONFIGRECORD_FULL_SERVICE_NAME, "libSWSimCCIAFU");
	ConfigRecord.Add(AAL_FACTORY_CREATE_SOFTWARE_SERVICE,true);

#endif
	Manifest.Add(AAL_FACTORY_CREATE_CONFIGRECORD_INCLUDED, ConfigRecord);
	Manifest.Add(AAL_FACTORY_CREATE_SERVICENAME, "iFPGA");
	MSG("Allocating Service");

	m_runtimeClient->getRuntime()->allocService(dynamic_cast<IBase *>(this), Manifest);
	m_Sem.Wait();

	m_AFUService->WorkspaceAllocate(DSM_SIZE, TransactionID(0));
	m_Sem.Wait();

	for(int i = 0; i < page_count; i++) // Input
	{
		m_AFUService->WorkspaceAllocate(CL(page_size_in_cache_lines), TransactionID(i+1));
		m_Sem.Wait();
	}
	for(int i = 0; i < page_count; i++) // Output
	{
		m_AFUService->WorkspaceAllocate(CL(page_size_in_cache_lines), TransactionID(page_count+i+1));
		m_Sem.Wait();
	}

	MSG("Zeroing allocated pages.");
	for(int i = 0; i < page_count*page_size_in_cache_lines*8; i++)
	{
		writeToMemory64('i', 0, i);
		writeToMemory64('o', 0, i);
	}
	
	// Clear the DSM
	memset((void *)m_DSMVirt, 0, m_DSMSize);

	// Set DSM base, high then low
	m_AFUService->CSRWrite64(CSR_AFU_DSM_BASEL, m_DSMPhys);

	// If ASE, give it some time to catch up
	#if defined ( ASEAFU )
		SleepSec(5);
	#endif

	// Assert Device Reset
	m_AFUService->CSRWrite(CSR_CTL, 0);

	// De-assert Device Reset
	m_AFUService->CSRWrite(CSR_CTL, 1);

	m_AFUService->CSRWrite(CSR_ADDR_RESET, 0);

	// Source pages
	m_AFUService->CSRWrite(CSR_SRC_ADDR, 0);
	m_AFUService->CSRWrite(CSR_ADDR_RESET, 1);
	for(int i = 0; i < page_count; i++)
	{
		// Set input workspace address
		m_AFUService->CSRWrite(CSR_SRC_ADDR, CACHELINE_ALIGNED_ADDR(m_InputPhys[i]));
	}
	m_AFUService->CSRWrite(CSR_SRC_ADDR, 0);

	// Destination pages
	m_AFUService->CSRWrite(CSR_DST_ADDR, 0);
	m_AFUService->CSRWrite(CSR_ADDR_RESET, 2);
	for(int i = 0; i < page_count; i++)
	{
		// Set output workspace address
		m_AFUService->CSRWrite(CSR_DST_ADDR, CACHELINE_ALIGNED_ADDR(m_OutputPhys[i]));
	}
	m_AFUService->CSRWrite(CSR_DST_ADDR, 0);

	m_AFUService->CSRWrite(CSR_ADDR_RESET, 0xFFFFFFFF);

	// Set the test mode
	m_AFUService->CSRWrite(CSR_CFG, 0);
}

void iFPGA::writeToMemory32(char inOrOut, uint32_t dat32, uint32_t address32)
{
	int whichPage;
	int addressInPage;
	if (page_size_in_cache_lines == 128)
	{
		whichPage = (address32 >> 11);
		addressInPage = address32 & 0x7FF;
	}
	else if (page_size_in_cache_lines == 65536)
	{
		whichPage = (address32 >> 20);
		addressInPage = address32 & 0xFFFFF;
	}

	if (inOrOut == 'i')
	{
		if(m_InputVirt[whichPage] != NULL)
		{
			uint32_t* tempPointer = (uint32_t*)m_InputVirt[whichPage];
			tempPointer[addressInPage] = dat32;
		}
	}
	else if (inOrOut == 'o')
	{
		if(m_OutputVirt[whichPage] != NULL)
		{
			uint32_t* tempPointer = (uint32_t*)m_OutputVirt[whichPage];
			tempPointer[addressInPage] = dat32;
		}
	}
}

uint32_t iFPGA::readFromMemory32(char inOrOut, uint32_t address32)
{
	int whichPage;
	int addressInPage;
	if (page_size_in_cache_lines == 128)
	{
		whichPage = (address32 >> 11);
		addressInPage = address32 & 0x7FF;
	}
	else if (page_size_in_cache_lines == 65536)
	{
		whichPage = (address32 >> 20);
		addressInPage = address32 & 0xFFFFF;
	}

	if (inOrOut == 'i')
	{
		if(m_InputVirt[whichPage] != NULL)
		{
			uint32_t* tempPointer = (uint32_t*)m_InputVirt[whichPage];
			return tempPointer[addressInPage];
		}
	}
	else if (inOrOut == 'o')
	{
		if(m_OutputVirt[whichPage] != NULL)
		{
			uint32_t* tempPointer = (uint32_t*)m_OutputVirt[whichPage];
			return tempPointer[addressInPage];
		}
	}
	
	return 0;
}

void iFPGA::writeToMemory64(char inOrOut, uint64_t dat64, uint32_t address64)
{
	int whichPage;
	int addressInPage;
	if (page_size_in_cache_lines == 128)
	{
		whichPage = (address64 >> 10);
		addressInPage = address64 & 0x3FF;
	}
	else if (page_size_in_cache_lines == 65536)
	{
		whichPage = (address64 >> 19);
		addressInPage = address64 & 0x7FFFF;
	}

	if (inOrOut == 'i')
	{
		if(m_InputVirt[whichPage] != NULL)
		{
			uint64_t* tempPointer = (uint64_t*)m_InputVirt[whichPage];
			tempPointer[addressInPage] = dat64;
		}
	}
	else if (inOrOut == 'o')
	{
		if(m_OutputVirt[whichPage] != NULL)
		{
			uint64_t* tempPointer = (uint64_t*)m_OutputVirt[whichPage];
			tempPointer[addressInPage] = dat64;
		}
	}
}

uint64_t iFPGA::readFromMemory64(char inOrOut, uint32_t address64)
{
	int whichPage;
	int addressInPage;
	if (page_size_in_cache_lines == 128)
	{
		whichPage = (address64 >> 10);
		addressInPage = address64 & 0x3FF;
	}
	else if (page_size_in_cache_lines == 65536)
	{
		whichPage = (address64 >> 19);
		addressInPage = address64 & 0x7FFFF;
	}

	if (inOrOut == 'i')
	{
		if(m_InputVirt[whichPage] != NULL)
		{
			uint64_t* tempPointer = (uint64_t*)m_InputVirt[whichPage];
			return tempPointer[addressInPage];
		}
	}
	else if (inOrOut == 'o')
	{
		if(m_OutputVirt[whichPage] != NULL)
		{
			uint64_t* tempPointer = (uint64_t*)m_OutputVirt[whichPage];
			return tempPointer[addressInPage];
		}
	}

	return 0;
}

void iFPGA::writeToMemoryDouble(char inOrOut, double dat, uint32_t address)
{
	int whichPage;
	int addressInPage;
	if (page_size_in_cache_lines == 128)
	{
		whichPage = (address >> 10);
		addressInPage = address & 0x3FF;
	}
	else if (page_size_in_cache_lines == 65536)
	{
		whichPage = (address >> 19);
		addressInPage = address & 0x7FFFF;
	}

	if (inOrOut == 'i')
	{
		if(m_InputVirt[whichPage] != NULL)
		{
			double* tempPointer = (double*)m_InputVirt[whichPage];
			tempPointer[addressInPage] = dat;
		}
	}
	else if (inOrOut == 'o')
	{
		if(m_OutputVirt[whichPage] != NULL)
		{
			double* tempPointer = (double*)m_OutputVirt[whichPage];
			tempPointer[addressInPage] = dat;
		}
	}
}

double iFPGA::readFromMemoryDouble(char inOrOut, uint32_t address)
{
	int whichPage;
	int addressInPage;
	if (page_size_in_cache_lines == 128)
	{
		whichPage = (address >> 10);
		addressInPage = address & 0x3FF;
	}
	else if (page_size_in_cache_lines == 65536)
	{
		whichPage = (address >> 19);
		addressInPage = address & 0x7FFFF;
	}

	if (inOrOut == 'i')
	{
		if(m_InputVirt[whichPage] != NULL)
		{
			double* tempPointer = (double*)m_InputVirt[whichPage];
			return tempPointer[addressInPage];
		}
	}
	else if (inOrOut == 'o')
	{
		if(m_OutputVirt[whichPage] != NULL)
		{
			double* tempPointer = (double*)m_OutputVirt[whichPage];
			return tempPointer[addressInPage];
		}
	}

	return 0;
}

void iFPGA::writeToMemoryFloat(char inOrOut, float dat, uint32_t address)
{
	int whichPage;
	int addressInPage;
	if (page_size_in_cache_lines == 128)
	{
		whichPage = (address >> 11);
		addressInPage = address & 0x7FF;
	}
	else if (page_size_in_cache_lines == 65536)
	{
		whichPage = (address >> 20);
		addressInPage = address & 0xFFFFF;
	}

	if (inOrOut == 'i')
	{
		if(m_InputVirt[whichPage] != NULL)
		{
			float* tempPointer = (float*)m_InputVirt[whichPage];
			tempPointer[addressInPage] = dat;
		}
	}
	else if (inOrOut == 'o')
	{
		if(m_OutputVirt[whichPage] != NULL)
		{
			float* tempPointer = (float*)m_OutputVirt[whichPage];
			tempPointer[addressInPage] = dat;
		}
	}
}

float iFPGA::readFromMemoryFloat(char inOrOut, uint32_t address)
{
	int whichPage;
	int addressInPage;
	if (page_size_in_cache_lines == 128)
	{
		whichPage = (address >> 11);
		addressInPage = address & 0x7FF;
	}
	else if (page_size_in_cache_lines == 65536)
	{
		whichPage = (address >> 20);
		addressInPage = address & 0xFFFFF;
	}

	if (inOrOut == 'i')
	{
		if(m_InputVirt[whichPage] != NULL)
		{
			float* tempPointer = (float*)m_InputVirt[whichPage];
			return tempPointer[addressInPage];
		}
	}
	else if (inOrOut == 'o')
	{
		if(m_OutputVirt[whichPage] != NULL)
		{
			float* tempPointer = (float*)m_OutputVirt[whichPage];
			return tempPointer[addressInPage];
		}
	}

	return 0;
}

void iFPGA::writeToMemory64bitBCD(char inOrOut, double dat, uint32_t address)
{
	char* s = (char*)calloc(16, sizeof(char));
	sprintf(s, "%.10f", dat);

	printf("Converted C-string: %s\n", s);

	uint64_t BCD = 0;

	for(int i = 0; i < 16; i++)
	{
		uint64_t leftshift = (15-i)*4;
		if (s[i] == '-')
		{
			uint64_t temp = 0xC;
			BCD += (temp << leftshift);
		}
		else if (s[i] == '.')
		{
			uint64_t temp = 0xD;
			BCD += (temp << leftshift);
		}
		else if (s[i] == '0')
		{
			uint64_t temp = 0x0;
			BCD += (temp << leftshift);
		}
		else if (s[i] == '1')
		{
			uint64_t temp = 0x1;
			BCD += (temp << leftshift);
		}
		else if (s[i] == '2')
		{
			uint64_t temp = 0x2;
			BCD += (temp << leftshift);
		}
		else if (s[i] == '3')
		{
			uint64_t temp = 0x3;
			BCD += (temp << leftshift);
		}
		else if (s[i] == '4')
		{
			uint64_t temp = 0x4;
			BCD += (temp << leftshift);
		}
		else if (s[i] == '5')
		{
			uint64_t temp = 0x5;
			BCD += (temp << leftshift);
		}
		else if (s[i] == '6')
		{
			uint64_t temp = 0x6;
			BCD += (temp << leftshift);
		}
		else if (s[i] == '7')
		{
			uint64_t temp = 0x7;
			BCD += (temp << leftshift);
		}
		else if (s[i] == '8')
		{
			uint64_t temp = 0x8;
			BCD += (temp << leftshift);
		}
		else if (s[i] == '9')
		{
			uint64_t temp = 0x9;
			BCD += (temp << leftshift);
		}
	}

	int whichPage;
	int addressInPage;
	if (page_size_in_cache_lines == 128)
	{
		whichPage = (address >> 10);
		addressInPage = address & 0x3FF;
	}
	else if (page_size_in_cache_lines == 65536)
	{
		whichPage = (address >> 19);
		addressInPage = address & 0x7FFFF;
	}

	if (inOrOut == 'i')
	{
		if(m_InputVirt[whichPage] != NULL)
		{
			uint64_t* tempPointer = (uint64_t*)m_InputVirt[whichPage];
			tempPointer[addressInPage] = BCD;
		}
	}
	else if (inOrOut == 'o')
	{
		if(m_OutputVirt[whichPage] != NULL)
		{
			uint64_t* tempPointer = (uint64_t*)m_OutputVirt[whichPage];
			tempPointer[addressInPage] = BCD;
		}
	}
}

void iFPGA::doTransaction()
{
	// Assert Device Reset
	m_AFUService->CSRWrite(CSR_CTL, 0);

	//SleepSec(3);

	// De-assert Device Reset
	m_AFUService->CSRWrite(CSR_CTL, 1);

	volatile bt32bitCSR *StatusAddr = (volatile bt32bitCSR *)(m_DSMVirt  + DSM_STATUS_TEST_COMPLETE);

	// Start the test
	m_AFUService->CSRWrite(CSR_CTL, 3);

	// Wait for test completion
	while( 0 == *StatusAddr )
	{
		SleepMicro(1);
	}
	*StatusAddr = 0;
}

/*
int main(int argc, char *argv[])
{
	RuntimeClient  runtimeClient;
	
	int page_size_in_cache_lines = 65536;
	//int page_size_in_cache_lines = 128;

	iFPGA theApp(&runtimeClient, page_size_in_cache_lines);
	if(!runtimeClient.isOK()){
		ERR("Runtime Failed to Start");
		exit(1);
	}


	int numberOfDecimals = 32;

	for (int i = 0; i < 8; i++){
		theApp.writeToMemory64bitBCD('i', 0.707106, i);
	}
	for (int i = 8; i < 16; i++){
		theApp.writeToMemory64bitBCD('i', -103.2, i);
	}
	for (int i = 16; i < 24; i++){
		theApp.writeToMemory64bitBCD('i', -0.0405, i);
	}
	for (int i = 24; i < 32; i++){
		theApp.writeToMemory64bitBCD('i', 4.01, i);
	}

	theApp.m_AFUService->CSRWrite(CSR_READ_OFFSET, 0);
	theApp.m_AFUService->CSRWrite(CSR_WRITE_OFFSET, 0);
	theApp.m_AFUService->CSRWrite(CSR_NUM_LINES, numberOfDecimals/8);

	theApp.doTransaction();

	FILE* f;
	f = fopen("outputMemory.txt", "w");
	for (int i = 0; i < numberOfDecimals; i++) {
		uint64_t temp = theApp.readFromMemory64('o', i);
		uint32_t temp2 = temp & 0xFFFFFFFF;
		uint32_t temp3 = (temp >> 32) & 0xFFFFFFFF;
		fprintf(f, "%x %x\n", temp3, temp2);
	}
	fclose(f);

	MSG("Done");
	return 0;
}
*/
