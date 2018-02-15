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

#include <sched.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>

#include "iFPGA.h"

using namespace std;
using namespace AAL;

#define MAX_page_count 2048

iFPGA::iFPGA(RuntimeClient *rtc, uint32_t _page_count, uint32_t _page_size_in_cache_lines) :
#ifdef HARPv1
m_AFUService(NULL),
#else
m_pALIBufferService(NULL),
m_pALIMMIOService(NULL),
m_pALIResetService(NULL),
#endif
m_pAALService(NULL),
m_runtimeClient(rtc),
m_Result(0),
m_DSMVirt(NULL),
m_DSMPhys(0),
m_DSMSize(0)
{
	page_size_in_cache_lines = _page_size_in_cache_lines;
	page_count = _page_count;

	m_InputVirt = (btVirtAddr*)malloc(page_count*sizeof(btVirtAddr));
	m_InputPhys = (btPhysAddr*)malloc(page_count*sizeof(btPhysAddr));
	m_InputSize = (btWSSize*)malloc(page_count*sizeof(btWSSize));
	m_OutputVirt = (btVirtAddr*)malloc(page_count*sizeof(btVirtAddr));
	m_OutputPhys = (btPhysAddr*)malloc(page_count*sizeof(btPhysAddr));
	m_OutputSize = (btWSSize*)malloc(page_count*sizeof(btWSSize));

	for (uint32_t i = 0; i < page_count; i++) {
		m_InputVirt[i] = NULL;
		m_InputPhys[i] = 0;
		m_InputSize[i] = 0;
		m_OutputVirt[i] = NULL;
		m_OutputPhys[i] = 0;
		m_OutputSize[i] = 0;
	}
#ifdef HARPv1
	SetSubClassInterface(iidServiceClient, dynamic_cast<IServiceClient *>(this));
	SetInterface(iidCCIClient, dynamic_cast<ICCIClient *>(this));
#else
	SetInterface(iidServiceClient, dynamic_cast<IServiceClient *>(this));
#endif

	m_Sem.Create(0, 1);
	allocateSuccess = allocateWorkspace();
}

iFPGA::~iFPGA() {
#ifdef HARPv1
	// Release the Workspaces and wait for all three then Release the Service
	for(uint32_t i = 0; i < page_count; i++) {
		m_AFUService->WorkspaceFree(m_InputVirt[i],  TransactionID(i+1));
		m_Sem.Wait();
	}
	for(uint32_t i = 0; i < page_count; i++) {
		m_AFUService->WorkspaceFree(m_OutputVirt[i],  TransactionID(page_count+i+1));
		m_Sem.Wait();
	}
	m_AFUService->WorkspaceFree(m_DSMVirt, TransactionID(0));
	m_Sem.Wait();
	(dynamic_ptr<IAALService>(iidService, m_pAALService))->Release(TransactionID());
	m_Sem.Wait();

	m_runtimeClient->end();
#else
	// Clean-up and return
	for(int i = 0; i < page_count; i++) {
		m_pALIBufferService->bufferFree(m_OutputVirt[i]);
	}
	for(int i = 0; i < page_count; i++) {
		m_pALIBufferService->bufferFree(m_InputVirt[i]);
	}
	m_pALIBufferService->bufferFree(m_DSMVirt);
	// Freed all three so now Release() the Service through the Services IAALService::Release() method
	(dynamic_ptr<IAALService>(iidService, m_pAALService))->Release(TransactionID());
	m_Sem.Wait();

	m_runtimeClient->m_Runtime.stop();
	m_runtimeClient->m_Sem.Wait();
#endif
	m_runtimeClient->m_Sem.Destroy();
	m_Sem.Destroy();

	free(m_InputVirt);
	free(m_InputPhys);
	free(m_InputSize);
	free(m_OutputVirt);
	free(m_OutputPhys);
	free(m_OutputSize);
}

void iFPGA::writeToMemory32(char inOrOut, uint32_t dat32, uint32_t address32)
{
	int whichPage = 0;
	int addressInPage = 0;
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
	int whichPage = 0;
	int addressInPage = 0;
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
	int whichPage = 0;
	int addressInPage = 0;
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
	int whichPage = 0;
	int addressInPage = 0;
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
	int whichPage = 0;
	int addressInPage = 0;
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
	int whichPage = 0;
	int addressInPage = 0;
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
	int whichPage = 0;
	int addressInPage = 0;
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
	int whichPage = 0;
	int addressInPage = 0;
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

void iFPGA::doTransaction()
{
	// Assert Device Reset
	CSR_WRITE32(this, CSR_CTL, 0);

	// De-assert Device Reset
	CSR_WRITE32(this, CSR_CTL, 1);

	volatile bt32bitCSR *StatusAddr = (volatile bt32bitCSR *)(m_DSMVirt  + DSM_STATUS_TEST_COMPLETE);

	// Start the test
	CSR_WRITE32(this, CSR_CTL, 3);

	// Wait for test completion
	while( 0 == *StatusAddr ) {
		SleepNano(100);
	}

	*StatusAddr = 0;
}

char iFPGA::allocateWorkspace() {
	// Request our AFU.

	// NOTE: This example is bypassing the Resource Manager's configuration record lookup
	// mechanism.  This code is work around code and subject to change. But it does
	//	illustrate the utility of having different implementations of a service all
	// readily available and bound at run-time.
	NamedValueSet Manifest;
	NamedValueSet ConfigRecord;

#ifdef HARPv1

#if defined( HWAFU ) /* Use FPGA hardware */
	ConfigRecord.Add(AAL_FACTORY_CREATE_CONFIGRECORD_FULL_SERVICE_NAME, "libHWCCIAFU");
	ConfigRecord.Add(keyRegAFU_ID,"C000C966-0D82-4272-9AEF-FE5F84570612");
	ConfigRecord.Add(AAL_FACTORY_CREATE_CONFIGRECORD_FULL_AIA_NAME, "libAASUAIA");
#elif defined ( ASEAFU ) /* Use ASE based RTL simulation */
	ConfigRecord.Add(AAL_FACTORY_CREATE_CONFIGRECORD_FULL_SERVICE_NAME, "libASECCIAFU");
	ConfigRecord.Add(AAL_FACTORY_CREATE_SOFTWARE_SERVICE,true);
#else /* default is Software Simulator */
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

	for(uint32_t i = 0; i < page_count; i++) { // Input
		m_AFUService->WorkspaceAllocate(CL(page_size_in_cache_lines), TransactionID(i+1));
		m_Sem.Wait();
	}
	for(uint32_t i = 0; i < page_count; i++) // Output
	{
		m_AFUService->WorkspaceAllocate(CL(page_size_in_cache_lines), TransactionID(page_count+i+1));
		m_Sem.Wait();
	}

	MSG("Zeroing allocated pages.");
	for(uint32_t i = 0; i < page_count*page_size_in_cache_lines*8; i++)
	{
		// writeToMemory64('i', 0, i);
		writeToMemory64('o', 0, i);
	}

	if (m_Result == 0) {
		// Clear the DSM
		memset((void *)m_DSMVirt, 0, m_DSMSize);

		// Set DSM base, high then low
		m_AFUService->CSRWrite64(CSR_AFU_DSM_BASEL, m_DSMPhys);

		// If ASE, give it some time to catch up
#if defined ( ASEAFU )
		SleepSec(5);
#endif /* ASE AFU */

		// Assert Device Reset
		m_AFUService->CSRWrite(CSR_CTL, 0);

		// De-assert Device Reset
		m_AFUService->CSRWrite(CSR_CTL, 1);

		m_AFUService->CSRWrite(CSR_ADDR_RESET, 0);

		// Source pages
		m_AFUService->CSRWrite(CSR_SRC_ADDR, 0);
		m_AFUService->CSRWrite(CSR_ADDR_RESET, 1);
		for(uint32_t i = 0; i < page_count; i++)
		{
		// Set input workspace address
			m_AFUService->CSRWrite(CSR_SRC_ADDR, CACHELINE_ALIGNED_ADDR(m_InputPhys[i]));
		}
		m_AFUService->CSRWrite(CSR_SRC_ADDR, 0);

		// Destination pages
		m_AFUService->CSRWrite(CSR_DST_ADDR, 0);
		m_AFUService->CSRWrite(CSR_ADDR_RESET, 2);
		for(uint32_t i = 0; i < page_count; i++)
		{
		// Set output workspace address
			m_AFUService->CSRWrite(CSR_DST_ADDR, CACHELINE_ALIGNED_ADDR(m_OutputPhys[i]));
		}
		m_AFUService->CSRWrite(CSR_DST_ADDR, 0);

		m_AFUService->CSRWrite(CSR_ADDR_RESET, 0xFFFFFFFF);

		// Set the test mode
		m_AFUService->CSRWrite(CSR_CFG, 0);
	}
	return 0;
#else

#if defined( HWAFU ) /* Use FPGA hardware */
	// Service Library to use
	ConfigRecord.Add(AAL_FACTORY_CREATE_CONFIGRECORD_FULL_SERVICE_NAME, "libHWALIAFU");

	// the AFUID to be passed to the Resource Manager. It will be used to locate the appropriate device.
	ConfigRecord.Add(keyRegAFU_ID,"C000C966-0D82-4272-9AEF-FE5F84570612");


	// indicate that this service needs to allocate an AIAService, too to talk to the HW
	ConfigRecord.Add(AAL_FACTORY_CREATE_CONFIGRECORD_FULL_AIA_NAME, "libaia");

#elif defined ( ASEAFU )         /* Use ASE based RTL simulation */
	Manifest.Add(keyRegHandle, 20);

	ConfigRecord.Add(AAL_FACTORY_CREATE_CONFIGRECORD_FULL_SERVICE_NAME, "libASEALIAFU");
	ConfigRecord.Add(AAL_FACTORY_CREATE_SOFTWARE_SERVICE,true);
#else                            /* default is Software Simulator */
#if 0 // NOT CURRRENTLY SUPPORTED
	ConfigRecord.Add(AAL_FACTORY_CREATE_CONFIGRECORD_FULL_SERVICE_NAME, "libSWSimALIAFU");
	ConfigRecord.Add(AAL_FACTORY_CREATE_SOFTWARE_SERVICE,true);
#endif
	return -1;
#endif

	// Add the Config Record to the Manifest describing what we want to allocate
	Manifest.Add(AAL_FACTORY_CREATE_CONFIGRECORD_INCLUDED, &ConfigRecord);

	// in future, everything could be figured out by just giving the service name
	Manifest.Add(AAL_FACTORY_CREATE_SERVICENAME, "iFPGA");

	MSG("Allocating Service");

	// Allocate the Service and wait for it to complete by sitting on the
	// semaphore. The serviceAllocated() callback will be called if successful.
	// If allocation fails the serviceAllocateFailed() should set m_bIsOK appropriately.
	// (Refer to the serviceAllocated() callback to see how the Service's interfaces
	// are collected.)
	m_runtimeClient->m_Runtime.allocService(dynamic_cast<IBase *>(this), Manifest);
	m_Sem.Wait();
	if(!m_bIsOK){
		ERR("Allocation failed\n");
		return -1;
	}

	// Now that we have the Service and have saved the IALIBuffer interface pointer
	// we can now Allocate the 3 Workspaces used by the NLB algorithm. The buffer allocate
	// function is synchronous so no need to wait on the semaphore
	// Device Status Memory (DSM) is a structure defined by the NLB implementation.
	// User Virtual address of the pointer is returned directly in the function
	if( ali_errnumOK != m_pALIBufferService->bufferAllocate(DSM_SIZE, &m_DSMVirt)){
		m_bIsOK = false;
		m_Result = -1;
		return -1;
	}
	// Save the size and get the IOVA from teh User Virtual address. The HW only uses IOVA.
	m_DSMSize = DSM_SIZE;
	m_DSMPhys = m_pALIBufferService->bufferGetIOVA(m_DSMVirt);
	if(0 == m_DSMPhys){
		m_bIsOK = false;
		m_Result = -1;
		return -1;
	}

	// Repeat for the Input and Output Buffers
	for(int i = 0; i < page_count; i++) { // Input
		if( ali_errnumOK != m_pALIBufferService->bufferAllocate(CL(page_size_in_cache_lines), &m_InputVirt[i]) ) {
			m_bIsOK = false;
			m_Sem.Post(1);
			m_Result = -1;
			return -1;
		}
		m_InputSize[i] = CL(page_size_in_cache_lines);
		m_InputPhys[i] = m_pALIBufferService->bufferGetIOVA(m_InputVirt[i]);
		if(0 == m_InputPhys[i]) {
			m_bIsOK = false;
			m_Result = -1;
			return -1;
		}
	}
	for(int i = 0; i < page_count; i++) { // Input
		if( ali_errnumOK != m_pALIBufferService->bufferAllocate(CL(page_size_in_cache_lines), &m_OutputVirt[i]) ) {
			m_bIsOK = false;
			m_Sem.Post(1);
			m_Result = -1;
			return -1;
		}
		m_OutputSize[i] = CL(page_size_in_cache_lines);
		m_OutputPhys[i] = m_pALIBufferService->bufferGetIOVA(m_OutputVirt[i]);
		if(0 == m_OutputPhys[i]) {
			m_bIsOK = false;
			m_Result = -1;
			return -1;
		}
	}

	MSG("Zeroing allocated pages.");
	for(int i = 0; i < page_count*page_size_in_cache_lines*8; i++) {
		// writeToMemory64('i', 0, i);
		writeToMemory64('o', 0, i);
	}

	if(true == m_bIsOK) {
		// Clear the DSM
		memset(m_DSMVirt, 0, m_DSMSize);

		// Initiate AFU Reset
		m_pALIResetService->afuReset();

		// Initiate DSM Reset
		// Set DSM base, high then low
		m_pALIMMIOService->mmioWrite64(CSR_AFU_DSM_BASEL, m_DSMPhys);

		// Assert AFU reset
		m_pALIMMIOService->mmioWrite32(CSR_CTL, 0);

		//	De-Assert AFU reset
		m_pALIMMIOService->mmioWrite32(CSR_CTL, 1);

		// Populate page table
		m_pALIMMIOService->mmioWrite32(CSR_ADDR_RESET, 0);

		// Source pages
		m_pALIMMIOService->mmioWrite32(CSR_SRC_ADDR, 0);
		m_pALIMMIOService->mmioWrite32(CSR_ADDR_RESET, 1);
		for(int i = 0; i < page_count; i++) {
		// Set input workspace address
			m_pALIMMIOService->mmioWrite32(CSR_SRC_ADDR, CACHELINE_ALIGNED_ADDR(m_InputPhys[i]));
		}
		m_pALIMMIOService->mmioWrite32(CSR_SRC_ADDR, 0);

		// Destination pages
		m_pALIMMIOService->mmioWrite32(CSR_DST_ADDR, 0);
		m_pALIMMIOService->mmioWrite32(CSR_ADDR_RESET, 2);
		for(int i = 0; i < page_count; i++) {
			// Set output workspace address
			m_pALIMMIOService->mmioWrite32(CSR_DST_ADDR, CACHELINE_ALIGNED_ADDR(m_OutputPhys[i]));
		}
		m_pALIMMIOService->mmioWrite32(CSR_DST_ADDR, 0);

		// Verify page tables
		m_pALIMMIOService->mmioWrite32(CSR_ADDR_RESET, 3);
		m_pALIMMIOService->mmioWrite32(CSR_ADDR_RESET, 4);
		CSR_WRITE32(this, CSR_CTL, 3);
		SleepSec(1);
		for(int i = 0; i < page_count; i++) {
			while ( readFromMemory64('o', i*8) == 0 ) {
				SleepNano(100);
			}
			uint64_t address = readFromMemory64('o', i*8) & 0x3FFFFFFFFFF;
			printf("src_addr %d: %zx \n", i, address);
			if (address != (uint64_t)CACHELINE_ALIGNED_ADDR(m_InputPhys[i])) {
				printf("Page table verification failed for src_addr: %zx, supposed to be: %zx \n", address, (uint64_t)CACHELINE_ALIGNED_ADDR(m_InputPhys[i]) );
				return -1;
			}
		}
		for(int i = 0; i < page_count; i++) {
			while (readFromMemory64('o', (i+page_count)*8) == 0 ) {
				SleepNano(100);
			}
			uint64_t address = readFromMemory64('o', (i+page_count)*8) & 0x3FFFFFFFFFF;
			printf("dst_addr %d: %zx \n", i, address);
			if (address != (uint64_t)CACHELINE_ALIGNED_ADDR(m_OutputPhys[i])) {
				printf("Page table verification failed for dst_addr: %zx, supposed to be: %zx \n", address, (uint64_t)CACHELINE_ALIGNED_ADDR(m_OutputPhys[i]) );
				return -1;
			}
		}
		printf("Page tables verified!\n");

		m_pALIMMIOService->mmioWrite32(CSR_ADDR_RESET, 0xFFFFFFFF);
		// Assert AFU reset
		m_pALIMMIOService->mmioWrite32(CSR_CTL, 0);
		//	De-Assert AFU reset
		m_pALIMMIOService->mmioWrite32(CSR_CTL, 1);
		// Verified page tables

		printf("Resetted!\n"); fflush(stdout);

		// Set the test mode
		m_pALIMMIOService->mmioWrite32(CSR_CFG, 1 << 12 /*QPI or PCIe*/ | 0 /*write through enable*/ );

		printf("CSR_CFG is written!\n"); fflush(stdout);
	}
	return 0;
#endif
}

// We must implement the IServiceClient interface (IServiceClient.h):
// <begin IServiceClient interface>
void iFPGA::serviceAllocated(IBase *pServiceBase, TransactionID const &rTranID) {
#ifdef HARPv1
	m_pAALService = pServiceBase;
	ASSERT(NULL != m_pAALService);
	// Documentation says CCIAFU Service publishes ICCIAFU as subclass interface
	m_AFUService = subclass_ptr<ICCIAFU>(pServiceBase);

	ASSERT(NULL != m_AFUService);
	if ( NULL == m_AFUService ) {
		return;
	}
#else
	m_pAALService = pServiceBase;
	ASSERT(NULL != m_pAALService);
	if ( NULL == m_pAALService ) {
		m_bIsOK = false;
		return;
	}

	// Documentation says HWALIAFU Service publishes
	// IALIBuffer as subclass interface. Used in Buffer Allocation and Free
	m_pALIBufferService = dynamic_ptr<IALIBuffer>(iidALI_BUFF_Service, pServiceBase);
	ASSERT(NULL != m_pALIBufferService);
	if ( NULL == m_pALIBufferService ) {
		m_bIsOK = false;
		return;
	}

	// Documentation says HWALIAFU Service publishes
	// IALIMMIO as subclass interface. Used to set/get MMIO Region
	m_pALIMMIOService = dynamic_ptr<IALIMMIO>(iidALI_MMIO_Service, pServiceBase);
	ASSERT(NULL != m_pALIMMIOService);
	if ( NULL == m_pALIMMIOService ) {
		m_bIsOK = false;
		return;
	}

	// Documentation says HWALIAFU Service publishes
	// IALIReset as subclass interface. Used for resetting the AFU
	m_pALIResetService = dynamic_ptr<IALIReset>(iidALI_RSET_Service, pServiceBase);
	ASSERT(NULL != m_pALIResetService);
	if ( NULL == m_pALIResetService ) {
		m_bIsOK = false;
		return;
	}
#endif

	MSG("Service Allocated");
	m_Sem.Post(1);
}

void iFPGA::serviceAllocateFailed(const IEvent &rEvent) {
	ERR("Failed to allocate a Service");
#ifdef HARPv1
	IExceptionTransactionEvent * pExEvent = dynamic_ptr<IExceptionTransactionEvent>(iidExTranEvent, rEvent);
	ERR(pExEvent->Description());
#else
	PrintExceptionDescription(rEvent);
	m_bIsOK = false;
#endif

	++m_Result; // Remember the error
	m_Sem.Post(1);
}

#ifdef HARPv1
void iFPGA::serviceFreed(TransactionID const &rTranID) {
	MSG("Service Freed");
	// Unblock Main()
	m_Sem.Post(1);
}
#else
void iFPGA::serviceReleased(TransactionID const &rTranID) {
	MSG("Service Released");
	// Unblock Main()
	m_Sem.Post(1);
}
void iFPGA::serviceReleaseRequest(IBase *pServiceBase, const IEvent &rEvent) {
	MSG("Service unexpected requested back");
	if(NULL != m_pAALService){
		IAALService *pIAALService = dynamic_ptr<IAALService>(iidService, m_pAALService);
		ASSERT(pIAALService);
		pIAALService->Release(TransactionID());
	}
}
void iFPGA::serviceReleaseFailed(const IEvent &rEvent) {
	ERR("Failed to release a Service");
	PrintExceptionDescription(rEvent);
	m_bIsOK = false;
	m_Sem.Post(1);
}
#endif
void iFPGA::serviceEvent(const IEvent &rEvent) {
	ERR("unexpected event 0x" << hex << rEvent.SubClassID());
}
// <end IServiceClient interface>

#ifdef HARPv1
// <ICCIClient>
void iFPGA::OnWorkspaceAllocated(TransactionID const &TranID, btVirtAddr WkspcVirt, btPhysAddr WkspcPhys, btWSSize WkspcSize) {
	AutoLock(this);

	if (TranID.ID() == 0)
	{
		m_DSMVirt = WkspcVirt;
		m_DSMPhys = WkspcPhys;
		m_DSMSize = WkspcSize;
		MSG("Got DSM");
		printf("DSM Virt:%p, Phys:%lu, Size:%llu\n", m_DSMVirt, m_DSMPhys, m_DSMSize);
		m_Sem.Post(1);
	}
	else if(TranID.ID() >= 1 && TranID.ID() <= (int)page_count)
	{
		int index = TranID.ID()-1;
		m_InputVirt[index] = WkspcVirt;
		m_InputPhys[index] = WkspcPhys;
		m_InputSize[index] = WkspcSize;
		//MSG("Got Input Workspace");
		//printf("Input Virt:%x, Phys:%x, Size:%d\n", m_InputVirt[index], m_InputPhys[index], m_InputSize[index]);
		m_Sem.Post(1);
	}
	else if(TranID.ID() >= (int)page_count+1 && TranID.ID() <= 2*(int)page_count)
	{
		int index = TranID.ID()-(page_count+1);
		m_OutputVirt[index] = WkspcVirt;
		m_OutputPhys[index] = WkspcPhys;
		m_OutputSize[index] = WkspcSize;
		//MSG("Got Output Workspace");
		//printf("Output Virt:%x, Phys:%x, Size:%d\n", m_OutputVirt[index], m_OutputPhys[index], m_OutputSize[index]);
		m_Sem.Post(1);
	}
	else
	{
		++m_Result;
		ERR("Invalid workspace type: " << TranID.ID());
	}
}

void iFPGA::OnWorkspaceAllocateFailed(const IEvent &rEvent)
{
	IExceptionTransactionEvent * pExEvent = dynamic_ptr<IExceptionTransactionEvent>(iidExTranEvent, rEvent);
	ERR("OnWorkspaceAllocateFailed");
	ERR(pExEvent->Description());

	++m_Result;                     // Remember the error
	m_Sem.Post(1);
}

void iFPGA::OnWorkspaceFreed(TransactionID const &TranID) {
	m_Sem.Post(1);
}

void iFPGA::OnWorkspaceFreeFailed(const IEvent &rEvent) {
	IExceptionTransactionEvent * pExEvent = dynamic_ptr<IExceptionTransactionEvent>(iidExTranEvent, rEvent);
	ERR("OnWorkspaceAllocateFailed");
	ERR(pExEvent->Description());
	++m_Result;                     // Remember the error
	m_Sem.Post(1);
}
// </ICCIClient>
#endif