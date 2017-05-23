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

#ifndef IFPGA
#define IFPGA

#include "RuntimeClient.h"

#define EVENT_CASE(x) case x : MSG(#x);
#ifndef CL
	#define CL(x) ((x)*64)
#endif
#ifndef LOG2_CL
	#define LOG2_CL 6
#endif
#ifndef MB
	#define MB(x) ((x)*1024*1024)
#endif
#define RAND_RANGE(N) ((double)rand() / ((double)RAND_MAX + 1) * (N))

#define MAX_page_count 2048
#define DSM_SIZE MB(4)

#define CSR_AFU_DSM_BASEH			0x1a04
#define CSR_SRC_ADDR				0x1a20
#define CSR_DST_ADDR				0x1a24
#define CSR_CTL						0x1a2c
#define CSR_CFG						0x1a34
#define CSR_CIPUCTL					0x280
#define DSM_STATUS_TEST_COMPLETE	0x40
#define CSR_AFU_DSM_BASEL			0x1a00
#define CSR_AFU_DSM_BASEH			0x1a04
#define CSR_ADDR_RESET				0x1a80
#define CSR_READ_OFFSET				0x1a84
#define CSR_WRITE_OFFSET			0x1a88

// Application specific registers
#define CSR_NUM_LINES				0x1a28
#define CSR_MY_CONFIG1				0x1a94
#define CSR_MY_CONFIG2				0x1a8c
#define CSR_MY_CONFIG3				0x1a90
#define CSR_MY_CONFIG4				0x1a98
#define CSR_MY_CONFIG5				0x1a9c

static double get_time()
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec + t.tv_usec*1e-6;
}

static uint64_t ipow(uint64_t base, uint64_t exponent)
{
	uint64_t result = 1;
	for(int i = 0; i < exponent; i++)
	{
		result *= base;
	}
	return result;
}

class iFPGA: public CAASBase, public IServiceClient, public ICCIClient
{
public:
	iFPGA(RuntimeClient * rtc);
	~iFPGA();

	void allocateWorkspace();

	void writeToMemory32(char inOrOut, uint32_t dat32, uint32_t address32);
	uint32_t readFromMemory32(char inOrOut, uint32_t address32);
	void writeToMemory64(char inOrOut, uint64_t dat64, uint32_t address64);
	uint64_t readFromMemory64(char inOrOut, uint32_t address64);
	void writeToMemoryDouble(char inOrOut, double dat, uint32_t address);
	double readFromMemoryDouble(char inOrOut, uint32_t address);
	void writeToMemoryFloat(char inOrOut, float dat, uint32_t address);
	float readFromMemoryFloat(char inOrOut, uint32_t address);
	void writeToMemory64bitBCD(char inOrOut, double dat, uint32_t address);

	void doTransaction();

	// <ICCIClient>
	virtual void OnWorkspaceAllocated(TransactionID const &TranID, btVirtAddr WkspcVirt, btPhysAddr WkspcPhys, btWSSize WkspcSize);
	virtual void OnWorkspaceAllocateFailed(const IEvent &Event);
	virtual void OnWorkspaceFreed(TransactionID const &TranID);
	virtual void OnWorkspaceFreeFailed(const IEvent &Event);
	// </ICCIClient>

	// <begin IServiceClient interface>
	void serviceAllocated(IBase *pServiceBase, TransactionID const &rTranID);
	void serviceAllocateFailed(const IEvent &rEvent);
	void serviceFreed(TransactionID const &rTranID);
	void serviceEvent(const IEvent &rEvent);
	// <end IServiceClient interface>

	static const int page_count = 8;
	int page_size_in_cache_lines;

	IBase			*m_pAALService;		// The generic AAL Service interface for the AFU.
	RuntimeClient 	*m_runtimeClient;
	ICCIAFU			*m_AFUService;
	CSemaphore		m_Sem;				// For synchronizing with the AAL runtime.

	// Workspace info
	btVirtAddr 		m_DSMVirt;        ///< DSM workspace virtual address.
	btPhysAddr 		m_DSMPhys;        ///< DSM workspace physical address.
	btWSSize 		m_DSMSize;        ///< DSM workspace size in bytes.
	btVirtAddr 		m_InputVirt[page_count];      ///< Input workspace virtual address.
	btPhysAddr 		m_InputPhys[page_count];      ///< Input workspace physical address.
	btWSSize 		m_InputSize[page_count];      ///< Input workspace size in bytes.
	btVirtAddr 		m_OutputVirt[page_count];     ///< Output workspace virtual address.
	btPhysAddr 		m_OutputPhys[page_count];     ///< Output workspace physical address.
	btWSSize 		m_OutputSize[page_count];     ///< Output workspace size in bytes.
};

#endif