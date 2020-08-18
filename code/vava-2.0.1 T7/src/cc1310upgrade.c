#include "basetype.h"
#include "vavahal.h"
#include "cc1310upgrade.h"

#define false			0
#define true			1
#define FALSE			0
#define TRUE			1
#define bool			int
#define HI_FAILURE      -1 
#define HI_SUCCESS		0

typedef unsigned int    HI_U32; 
typedef int 			HI_S32;
typedef unsigned char	HI_U8;

///////////////////////////1310 upgrade
#define UPGRADE_ARR_SIZE            		(128*1024)
static char *pArrBuf = NULL;

#define SBL_MAX_DEVICES             		20
#define SBL_DEFAULT_RETRY_COUNT     		1
#define SBL_DEFAULT_READ_TIMEOUT    		100 // in ms
#define SBL_DEFAULT_WRITE_TIMEOUT   		200 // in ms

#define SBL_CC2650_PAGE_ERASE_SIZE          4096
#define SBL_CC2650_FLASH_START_ADDRESS      0x00000000
#define SBL_CC2650_RAM_START_ADDRESS        0x20000000
#define SBL_CC2650_ACCESS_WIDTH_32B         1
#define SBL_CC2650_ACCESS_WIDTH_8B          0
#define SBL_CC2650_PAGE_ERASE_TIME_MS       20
#define SBL_CC2650_MAX_BYTES_PER_TRANSFER   252
#define SBL_CC2650_MAX_MEMWRITE_BYTES		247
#define SBL_CC2650_MAX_MEMWRITE_WORDS		61
#define SBL_CC2650_MAX_MEMREAD_BYTES		253
#define SBL_CC2650_MAX_MEMREAD_WORDS		63
#define SBL_CC2650_FLASH_SIZE_CFG           0x4003002C
#define SBL_CC2650_RAM_SIZE_CFG             0x40082250
#define SBL_CC2650_BL_CONFIG_PAGE_OFFSET    0xFDB
#define SBL_CC2650_BL_CONFIG_ENABLED_BM     0xC5
#define SBL_CC2650_BL_WORK_MEMORY_START		0x20000000
#define SBL_CC2650_BL_WORK_MEMORY_END		0x2000016F
#define SBL_CC2650_BL_STACK_MEMORY_START	0x20000FC0
#define SBL_CC2650_BL_STACK_MEMORY_END		0x20000FFF

typedef enum {
    SBL_SUCCESS = 0,
    SBL_ERROR,
    SBL_ARGUMENT_ERROR,
    SBL_TIMEOUT_ERROR,
    SBL_PORT_ERROR,
    SBL_ENUM_ERROR,
    SBL_UNSUPPORTED_FUNCTION,
}tSblStatus;

enum {
	CMD_PING			 = 0x20,
	CMD_DOWNLOAD		 = 0x21,
	CMD_GET_STATUS		 = 0x23,
	CMD_SEND_DATA		 = 0x24,
	CMD_RESET			 = 0x25,
	CMD_SECTOR_ERASE	 = 0x26,
	CMD_CRC32			 = 0x27,
	CMD_GET_CHIP_ID 	 = 0x28,
	CMD_MEMORY_READ 	 = 0x2A,
	CMD_MEMORY_WRITE	 = 0x2B,
	CMD_BANK_ERASE		 = 0x2C,
	CMD_SET_CCFG		 = 0x2D,
};

/* Early samples had different command IDs */
enum
{
	REV1_CMD_BANK_ERASE   = 0x2A,
	REV1_CMD_SET_CCFG	  = 0x2B,
	REV1_CMD_MEMORY_READ  = 0x2C,
	REV1_CMD_MEMORY_WRITE = 0x2D,
};

enum {
	CMD_RET_SUCCESS 	 = 0x40,
	CMD_RET_UNKNOWN_CMD  = 0x41,
	CMD_RET_INVALID_CMD  = 0x42,
	CMD_RET_INVALID_ADR  = 0x43,
	CMD_RET_FLASH_FAIL	 = 0x44,
};

#define UPGRADE_UF_START 0
#define UPGRADE_UF_LEN 1
#define UPGRADE_UF_CMD 2
#define UPGRADE_UF_DATA 3
#define UPGRADE_UF_MAX_LEN 50

/// Struct used when splitting long transfers
typedef struct {
    HI_U32 startAddr;
    HI_U32 byteCount;
    HI_U32 startOffset;
    bool     bExpectAck;
}tTransfer;

static HI_U32 m_deviceId;
static HI_U32 m_deviceRev;
static bool		m_bCommInitialized;
static int32_t 	m_lastDeviceStatus;
static HI_U32	m_flashSize;
static HI_U32	m_ramSize;

static int uartX_fd = -1;

static int rfup_FormatRate(int rate)
{
	int outrate = 0;
	
	switch(rate)
	{
		case 1200:      
			outrate = B1200; 
			break;
		case 1800:
			outrate = B1800;
			break;
		case 2400:
			outrate = B2400;
			break;
		case 4800:
			outrate = B4800;
			break;
		case 9600:
			outrate = B9600;
			break;
		case 19200:
			outrate = B19200;
			break;
		case 38400:
			outrate = B38400;
			break;
		case 115200:
			outrate = B115200;
			break;
		default:
			outrate = B115200;
			break;		
	}
	
	return outrate;
}

static int rfup_SetRate(int outrate)
{
	struct termios Opt;
	
	if(uartX_fd == -1)
	{
		return -1;
	}
	
	tcgetattr(uartX_fd, &Opt);   
	cfsetispeed(&Opt, outrate);      
	cfsetospeed(&Opt, outrate);
	tcsetattr(uartX_fd, TCSAFLUSH, &Opt);  
	
	return 0;
}

static int rfup_SetParam(unsigned int Rate, unsigned char DataBit, unsigned char StopBit, unsigned char CheckBit)
{
	struct termios options;

	if(uartX_fd == -1)
	{
		return -1;
	}

	tcgetattr(uartX_fd, &options);
	options.c_oflag &= ~OPOST;  
	options.c_cflag &= ~CSIZE;

	switch(DataBit)
	{
		case 5:
			options.c_cflag |= CS5;
			break;
		case 6:
			options.c_cflag |= CS6;
			break;
		case 7:
			options.c_cflag |= CS7;
			break;
		case 8:
			options.c_cflag |= CS8;
			break;
		default:
			RFUP_PRINT("rfup_SetParam: param invalid");
			return -1;
	}

	switch(StopBit)
	{
		case 1:
			options.c_cflag &= ~(CSTOPB);
			break;
		case 2:
			options.c_cflag |= CSTOPB;
			break;
		default:
			RFUP_PRINT("rfup_SetParam: param invalid");
			return -1;
	}

	switch(CheckBit)
	{
		case 0:
			options.c_iflag &= ~(INPCK | ISTRIP);
			options.c_cflag &= ~PARENB;
			options.c_cflag &= ~PARODD;	
			break;
		case 1:
			options.c_iflag |= (INPCK | ISTRIP);
			options.c_cflag |= PARENB;            //有校验位
			options.c_cflag |= PARODD;		      //奇校验
			break;
		case 2:
			options.c_iflag |= (INPCK | ISTRIP);  
			options.c_cflag |= PARENB; 
			options.c_cflag &= ~PARODD;           //偶校验
			break;
		default:
			RFUP_PRINT("rfup_SetParam: param invalid");
			return -1;
	}

	options.c_lflag = 0; 
	options.c_iflag = 0;
	options.c_oflag = 0;

	if(tcsetattr(uartX_fd, TCSANOW, &options) < 0)
	{
		RFUP_PRINT("rfup_SetParam: tcsetattr err\n");
		return -1;
	}
	
	if(rfup_SetRate(rfup_FormatRate(Rate)) < 0)
	{
		RFUP_PRINT("rfup_SetParam: set rate err\n");
		return -1;
	}	

	return 0;
}

static void rfup_DeInit()
{
	if(uartX_fd != -1)
	{
		close(uartX_fd);
		uartX_fd = -1;	
	}

	return;
}

static int rfup_InitUart(int num, unsigned int rate)
{
	int ret;
	char uart_name[12];

	memset(uart_name, 0, 12);
	sprintf(uart_name, "/dev/ttyS%d", num);

	uartX_fd = open(uart_name, O_RDWR | O_NOCTTY | O_NDELAY, 0);
	if(uartX_fd < 0)
	{
		return -1;
	}

	fcntl(uartX_fd, F_SETFL, 0);

	ret = rfup_SetParam(rate, 8, 1, 0);
	if(ret < 0)
	{
		rfup_DeInit();
		
		RFUP_PRINT("rfup_InitUart: rfup_SetParam fail\n");
		return -1;
	}

	return 0;
}

int min(HI_U32 a, HI_U32 b)
{
	return (a > b ? b : a);
}

int Upgrade_UART_Send_Data(HI_S32 fd, HI_U8* sbuf)
{
    int ret, i;

    //防止mcu丢失host发过来的消息。300us为mcu处理一次接受usart消息中断的最长时间
    for (i = 0; i < sbuf[0]; i++){
        ret = write(fd, &sbuf[i], 1);
        //udelay(300);
        usleep(300);
        if (ret != 1)
        {
            RFUP_PRINT("write %d fd return %d\n", fd, ret);
            return -1;
        }
    }

    //RFUP_PRINT("uart send ok!\n");
    return 0;
}

int Upgrade_UART_Send_Data_Only(HI_S32 fd, HI_U8* sbuf)
{
    int ret, i;
	//RFUP_PRINT("Upgrade_UART_Send_Data_Only %x %x\n",sbuf[0],sbuf[1]);
    //防止mcu丢失host发过来的消息。300us为mcu处理一次接受usart消息中断的最长时间
    for (i = 0/*1*/; i < 2/*sbuf[0]*/; i++){
        ret = write(fd, &sbuf[i], 1);
        //usleep(300);
        if (ret != 1)
        {
            RFUP_PRINT("write %d fd return %d\n", fd, ret);
            return -1;
        }
    }

    //RFUP_PRINT("uart send ok!\n");
    return 0;
}

HI_U32 getFlashSize() { return m_flashSize; }  
HI_U32 getRamSize() { return m_ramSize; }

static HI_U32 getDeviceRev(HI_U32 deviceId)
{
    HI_U32 tmp = deviceId >> 28;
    switch(tmp)
    {
        // Early samples (Rev 1)
    case 0:
    case 1:
        return 1;
    default: 
        return 2;
    }
}

HI_U32 convertCmdForEarlySamples(HI_U32 ui32Cmd)
{
    if(m_deviceRev != 1)
    {
        // No conversion needed, return early.
        return ui32Cmd;
    }
    switch(ui32Cmd)
    {
    case CMD_MEMORY_READ: 
        return REV1_CMD_MEMORY_READ;
    case CMD_MEMORY_WRITE:
        return REV1_CMD_MEMORY_WRITE;
    case CMD_SET_CCFG:
        return REV1_CMD_SET_CCFG;
    case CMD_BANK_ERASE:
        return REV1_CMD_BANK_ERASE;
    default: 
        // No conversion needed, return original commad.
        return ui32Cmd;
    }
}


HI_U8 generateCheckSum(HI_U32 ui32Cmd, const char *pcData, HI_U32 ui32DataLen)
{
	HI_U32 i;
    HI_U8 ui8CheckSum = (HI_U8)ui32Cmd;
    for(i = 0; i < ui32DataLen; i++)
    {
        ui8CheckSum += pcData[i];
    }
    return ui8CheckSum;
}

HI_U32 sendCmd(HI_U32 ui32Cmd, const char *pcSendData/* = NULL*/, HI_U32 ui32SendLen/* = 0*/)
{
	static int showcount = 0;;
	
	if(uartX_fd < 0)
	{
		return 1;
	}
	
	ui32Cmd = convertCmdForEarlySamples(ui32Cmd);

	unsigned char pktLen = ui32SendLen + 3;
	memset(pArrBuf, 0, UPGRADE_ARR_SIZE);
    unsigned char pktSum = generateCheckSum(ui32Cmd, pcSendData, ui32SendLen);
	
	pArrBuf[0] = pktLen;
    pArrBuf[1] = pktSum;
    pArrBuf[2] = (unsigned char)ui32Cmd;

    if(ui32SendLen)
    {
        memcpy(&pArrBuf[3], pcSendData, ui32SendLen);
    }
	
	Upgrade_UART_Send_Data(uartX_fd,(HI_U8*)pArrBuf);

	showcount++;
	if(showcount++ >= 60)
	{
		RFUP_PRINT("RF upgrading...\n");
		showcount = 0;
	}

    return SBL_SUCCESS;
}

HI_U32 getCmdResponse(bool *bAck, HI_U32 ui32MaxRetries/* = SBL_DEFAULT_RETRY_COUNT*/, bool bQuietTimeout/* = false*/)
{
    unsigned char pIn[2];
    memset(pIn, 0, 2);
    HI_U32 numBytes = 0;
    HI_U32 retry = 0;
    *bAck = false;
    HI_U32 bytesRecv = 0;

	if(uartX_fd < 0)
	{
		return 1;
	}

    //
    // Expect 2 bytes (ACK or NAK)
    //
    do 
    {
		numBytes = read(uartX_fd, pIn, 2);
		
        bytesRecv += numBytes;
		
		//setState(SBL_ERROR, "readBytes numBytes=%d bytesRecv=%d\n", numBytes,bytesRecv);
		//RFUP_PRINT("readBytes numBytes=%d\n", numBytes);
        retry++;
    }
    while((bytesRecv < 2) && (retry < ui32MaxRetries));

    if(bytesRecv < 2)
    {
        if(!bQuietTimeout) RFUP_PRINT("Timed out waiting for ACK/NAK. No response from device.\n");
        return SBL_TIMEOUT_ERROR;
    }
    else
    {
        if(pIn[0] == 0x00 && pIn[1] == 0xCC)
        {
			//RFUP_PRINT("pIn[0]=0x00  pIn[1]=0xCC\n");
            *bAck = true;
            return (SBL_SUCCESS);
        }
        else if(pIn[0] == 0x00 && pIn[1] == 0x33)
        {
			//RFUP_PRINT("pIn[0]=0x00  pIn[1]=0x33\n");
            return (SBL_SUCCESS);
        }
        else
        {
            RFUP_PRINT("ACK/NAK not received. Expected 0x00 0xCC or 0x00 0x33, received 0x%02X 0x%02X.\n", pIn[0], pIn[1]);
            return SBL_ERROR;
        }
    }
    return SBL_ERROR;
}

//-----------------------------------------------------------------------------
/** \brief Send auto baud.
 *
 * \param[out] bBaudSetOk
 *      True if response is ACK, false otherwise
 *
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
HI_U32 sendAutoBaud(bool *bBaudSetOk)
{
    *bBaudSetOk = false;
    //
    // Send 0x55 0x55 and expect ACK
    //
    char pData[2];
    memset(pData, 0x55, 2);
    //if(m_pCom->writeBytes(pData, 2) != 2)
    //if(m_pCom->writeBytes(pData, 2) != 2)
    
    RFUP_PRINT("sendAutoBaud\n");
	#if 0
    unsigned char pktLen = 3; 
	char sendBuf[pktLen];
    //unsigned char pktSum = generateCheckSum(ui32Cmd, pcSendData, ui32SendLen);

	memset(sendBuf,0,pktLen);
    sendBuf[0] = pktLen;
    //sendBuf[1] = pktSum;
    //sendBuf[2] = (unsigned char)ui32Cmd;
	
    memcpy(&sendBuf[1], pData, 2);
	#endif
	
	if(Upgrade_UART_Send_Data_Only(uartX_fd, (HI_U8*) pData))
    {
        RFUP_PRINT("Communication init failed. Failed to send data.\n");
        return SBL_PORT_ERROR;
    }

    if(getCmdResponse(bBaudSetOk, 2, true) != SBL_SUCCESS)
    {
        // No response received. Invalid baud rate?
        RFUP_PRINT("No response from device. Device may not be in bootloader mode. Reset device and try again.\nIf problem persists, check connection and baud rate.\n");
        return SBL_PORT_ERROR;
    }

    return SBL_SUCCESS;
}

HI_U32 ping()
{
    int retCode = SBL_SUCCESS;
    bool bResponse = false;
#if 0
    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }
#endif
    //
    // Send command
    //
    if((retCode = sendCmd(CMD_PING,NULL,0)) != SBL_SUCCESS)
    {
        return retCode;
    }

    //
    // Get response
    //
    if((retCode = getCmdResponse(&bResponse,SBL_DEFAULT_RETRY_COUNT,false)) != SBL_SUCCESS)
    {
        return retCode;
    }

    return (bResponse) ? SBL_SUCCESS : SBL_ERROR;
}

//-----------------------------------------------------------------------------
/** \brief This function reset the device. Communication to the device must be 
 *      reinitialized after calling this function.
 *
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
HI_U32 reset()
{
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;
	#if 0
    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }
	#endif
	RFUP_PRINT("before reset cmd\n");
    //
    // Send command
    //
    if((retCode = sendCmd(CMD_RESET,NULL,0)) != SBL_SUCCESS)
    {
        return retCode;        
    }
	RFUP_PRINT("after reset cmd\n");
    //
    // Receive command response (ACK/NAK)
    //
    if((retCode = getCmdResponse(&bSuccess,SBL_DEFAULT_RETRY_COUNT,false)) != SBL_SUCCESS)
    {
        return retCode;
    }
    if(!bSuccess)
    {
        RFUP_PRINT("Reset command NAKed by device.\n");
        return SBL_ERROR;
    }

    m_bCommInitialized = false;
    return SBL_SUCCESS;
}

//-----------------------------------------------------------------------------
/** \brief Utility function for splitting 32 bit variable into char array
 *      (4 elements). Data are converted MSB, that is, \e pcDst[0] is the
 *      most significant byte.
 *
 * \param[in] ui32Src
 *      The 32 bit variable to convert.
 *
 * \param[out] pcDst
 *      Pointer to the char array where the data will be stored.
 *
 * \return
 *      void
 */
//-----------------------------------------------------------------------------
/*static */void ulToCharArray(const HI_U32 ui32Src, char *pcDst)
{
    // MSB first
    pcDst[0] =  (HI_U8)(ui32Src >> 24);
    pcDst[1] =  (HI_U8)(ui32Src >> 16);
    pcDst[2] =  (HI_U8)(ui32Src >> 8);
    pcDst[3] =  (HI_U8)(ui32Src >> 0);
}

bool addressInBLWorkMemory(HI_U32 ui32StartAddress, HI_U32 ui32ByteCount/* = 1*/)
{
    HI_U32 ui32EndAddr = ui32StartAddress + ui32ByteCount;

	if(ui32StartAddress <= SBL_CC2650_BL_WORK_MEMORY_END)
	{
		return true;
	}
	if((ui32StartAddress >= SBL_CC2650_BL_STACK_MEMORY_START) && 
	   (ui32StartAddress <= SBL_CC2650_BL_STACK_MEMORY_END))
	{
		return true;
	}
	if((ui32EndAddr >= SBL_CC2650_BL_STACK_MEMORY_START) && 
	   (ui32EndAddr <= SBL_CC2650_BL_STACK_MEMORY_END))
	{
		return true;
	}
	return false;
}

HI_U32 writeMemory32(HI_U32 ui32StartAddress, HI_U32 ui32UnitCount, const HI_U32 *pui32Data)
{
	HI_U32 i;
	HI_U32 j;
    HI_U32 retCode = SBL_SUCCESS;
    bool bSuccess = false;
    
    //
    // Check input arguments
    //
    if((ui32StartAddress & 0x03))
    {
        RFUP_PRINT("writeMemory32(): Start address (0x%08X) must 4 byte aligned.\n", ui32StartAddress);
        return SBL_ARGUMENT_ERROR;
    }
	if(addressInBLWorkMemory(ui32StartAddress, ui32UnitCount * 4))
	{
		// Issue warning
		RFUP_PRINT("writeMemory32(): Writing to bootloader work memory/stack:\n(0x%08X-0x%08X, 0x%08X-0x%08X)\n",
			         SBL_CC2650_BL_WORK_MEMORY_START,SBL_CC2650_BL_WORK_MEMORY_END, SBL_CC2650_BL_STACK_MEMORY_START,SBL_CC2650_BL_STACK_MEMORY_END);
		return SBL_ARGUMENT_ERROR;
	}
	#if 0
    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }
	#endif
	HI_U32 chunkCount = (ui32UnitCount / SBL_CC2650_MAX_MEMWRITE_WORDS);
	if(ui32UnitCount % SBL_CC2650_MAX_MEMWRITE_WORDS) chunkCount++;
	HI_U32 remainingCount = ui32UnitCount;
	//char* pcPayload = new char[5 + (SBL_CC2650_MAX_MEMWRITE_WORDS*4)];
	char* pcPayload = (char*)malloc((5 + (SBL_CC2650_MAX_MEMWRITE_WORDS*4))*sizeof(char));
	if(pcPayload == NULL)
	{
		
        return SBL_ARGUMENT_ERROR;
	}

	for(i = 0; i < chunkCount; i++)
	{
		HI_U32 chunkOffset = i * SBL_CC2650_MAX_MEMWRITE_WORDS;
		HI_U32 chunkStart  = ui32StartAddress + (chunkOffset * 4);
		HI_U32 chunkSize   = min(remainingCount, SBL_CC2650_MAX_MEMWRITE_WORDS);
		remainingCount -= chunkSize;

		//
		// Build payload
		// - 4B address (MSB first)
		// - 1B access width
		// - 1-SBL_CC2650_MAX_MEMWRITE_WORDS data (MSB first)
		//
		ulToCharArray(chunkStart, &pcPayload[0]);
		pcPayload[4] = SBL_CC2650_ACCESS_WIDTH_32B;
		for(j = 0; j < chunkSize; j++)
		{
			ulToCharArray(pui32Data[j + chunkOffset], &pcPayload[5 + j*4]);
		}

		//
		// Set progress
		//
		//setProgress( ((i * 100) / chunkCount) );

		//
		// Send command
		//
		if((retCode = sendCmd(CMD_MEMORY_WRITE, pcPayload, 5 + chunkSize*4)) != SBL_SUCCESS)
		{
			return retCode;        
		}

		//
		// Receive command response (ACK/NAK)
		//
		//if((retCode = getCmdResponse(bSuccess, 5)) != SBL_SUCCESS)
		if((retCode = getCmdResponse(&bSuccess, 5,false)) != SBL_SUCCESS)
		{
			return retCode;
		}
		if(!bSuccess)
		{
			RFUP_PRINT("writeMemory32(): Device NAKed command for address 0x%08X.\n", chunkStart);
			return SBL_ERROR;
		}
	}
    
    //
    // Set progress
    //
	//setProgress(100);

	//
	// Cleanup
	//
    //delete [] pcPayload;

    free(pcPayload);
    return SBL_SUCCESS;
}

bool addressInFlash(HI_U32 ui32StartAddress, HI_U32 ui32ByteCount/* = 1*/)
{
    HI_U32 ui32EndAddr = ui32StartAddress + ui32ByteCount;

    if(ui32StartAddress < SBL_CC2650_FLASH_START_ADDRESS)
    {
        return false;
    }
    if(ui32EndAddr > (SBL_CC2650_FLASH_START_ADDRESS + getFlashSize()))
    {
        return false;
    }
    
    return true;
}

//-----------------------------------------------------------------------------
/** \brief Utility function for converting 4 elements in char array into
 *      32 bit variable. Data are converted MSB, that is. \e pcSrc[0] is the
 *      most significant byte.
 *
 * \param pcSrc[in]
 *      A pointer to the source array.
 *
 * \return
 *      Returns the 32 bit variable.
 */
//-----------------------------------------------------------------------------
/*static */HI_U32 charArrayToUL(const char *pcSrc)
{
    HI_U32 ui32Val = (unsigned char)pcSrc[3];
    ui32Val += (((unsigned long)pcSrc[2]) & 0xFF) << 8;
    ui32Val += (((unsigned long)pcSrc[1]) & 0xFF) << 16;
    ui32Val += (((unsigned long)pcSrc[0]) & 0xFF) << 24;
    return (ui32Val);
}

//-----------------------------------------------------------------------------
/** \brief Send command response (ACK/NAK).
 *
 * \param[in] bAck
 *      True if response is ACK, false if response is NAK.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
HI_U32 sendCmdResponse(bool bAck)
{

    char pData[2];
    pData[0] = 0x00;
    pData[1] = (bAck) ? 0xCC : 0x33;

	/////////////////////////
	#if 0
    unsigned char pktLen = 3; 
	char sendBuf[pktLen];//={0};
    //unsigned char pktSum = generateCheckSum(ui32Cmd, pcSendData, ui32SendLen);

	memset(sendBuf,0,pktLen);
    sendBuf[0] = pktLen;
    //sendBuf[1] = pktSum;
    //sendBuf[2] = (unsigned char)ui32Cmd;
	
    memcpy(&sendBuf[1], pData, 2);
	#endif
	
	if(Upgrade_UART_Send_Data_Only(uartX_fd, (HI_U8*) pData))
    {
        RFUP_PRINT("Failed to send ACK/NAK response over\n");
        return SBL_PORT_ERROR;
    }


	/////////////////////////
    #if 0
    if(m_pCom->writeBytes(pData, 2) != 2)
    {
        RFUP_PRINT("Failed to send ACK/NAK response over\n");
        return SBL_PORT_ERROR;
    }
	#endif
    return SBL_SUCCESS;
}

bool addressInRam(HI_U32 ui32StartAddress, HI_U32 ui32ByteCount/* = 1*/)
{
    HI_U32 ui32EndAddr = ui32StartAddress + ui32ByteCount;

    if(ui32StartAddress < SBL_CC2650_RAM_START_ADDRESS)
    {
        return false;
    }
    if(ui32EndAddr > (SBL_CC2650_RAM_START_ADDRESS + getRamSize()))
    {
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
/** \brief Get response data from device.
 *
 * \param[out] pcData
 *      Pointer to where received data will be stored.
 * \param[in|out] ui32MaxLen
 *      Max number of bytes that can be received. Is populated with the actual
 *      number of bytes received.
 * \param[in] ui32MaxRetries (optional)
 *      How many times ComPort::readBytes() can time out before fail is issued.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
unsigned int getResponseData(char *pcData, unsigned int *ui32MaxLen)
{
    unsigned int retry = 0;
    unsigned char pcHdr[2];
    unsigned int numPayloadBytes;
    HI_U8 hdrChecksum, dataChecksum;
    unsigned int bytesRecv = 0;
	unsigned int ui32MaxLenCalcu = *ui32MaxLen;
	int ui32MaxRetries =1;

    
    //
    // Read length and checksum
    //
    memset(pcHdr, 0, 2);
    do
    {
        //bytesRecv += m_pCom->readBytes(&pcHdr[bytesRecv], (2-bytesRecv));
		bytesRecv += read(uartX_fd, &pcHdr[bytesRecv], (2-bytesRecv));
        retry ++;
    }
    while((bytesRecv < 2) && retry < ui32MaxRetries);

    //
    // Check that we've received 2 bytes
    //
    if(bytesRecv < 2)
    {
        RFUP_PRINT("Timed out waiting for data header from device.\n");
        return SBL_TIMEOUT_ERROR;
    }
    numPayloadBytes = pcHdr[0]-2;
    hdrChecksum = pcHdr[1];

    //
    // Check if length byte is too long.
    //
    if(numPayloadBytes > ui32MaxLenCalcu/*ui32MaxLen*/)
    {
        RFUP_PRINT("Error: Device sending more data than expected. \nMax expected was %d, sent was %d.\n", (HI_U32)ui32MaxLenCalcu, (numPayloadBytes+2));
        //m_pCom->flushBuffers();
        return SBL_ERROR;
    }

    //
    // Read the payload data
    //
    bytesRecv = 0;
    do 
    {
        //bytesRecv += m_pCom->readBytes(&pcData[bytesRecv], (numPayloadBytes-bytesRecv));

		bytesRecv += read(uartX_fd, &pcData[bytesRecv], (numPayloadBytes-bytesRecv));
        retry++;
    }
    while(bytesRecv < numPayloadBytes && retry < ui32MaxRetries);

    //
    // Have we received what we expected?
    //
    if(bytesRecv < numPayloadBytes)
    {
        *ui32MaxLen = bytesRecv;
        RFUP_PRINT("Timed out waiting for data from device.\n");
        return SBL_TIMEOUT_ERROR;
    }

    //
    // Verify data checksum
    //
    dataChecksum = generateCheckSum(0, pcData, numPayloadBytes);
    if(dataChecksum != hdrChecksum)
    {
        RFUP_PRINT("Checksum verification error. Expected 0x%02X, got 0x%02X.\n", hdrChecksum, dataChecksum);
        return SBL_ERROR;
    }

    *ui32MaxLen = bytesRecv;
    return SBL_SUCCESS;
}

HI_U32 calculateCrc32(HI_U32 ui32StartAddress, HI_U32 ui32ByteCount, HI_U32 *pui32Crc)
{
    HI_U32 retCode = SBL_SUCCESS;
    bool bSuccess = false;
    char pcPayload[12];
    HI_U32 ui32RecvCount = 0;

    //
    // Check input arguments
    //
    if(!addressInFlash(ui32StartAddress, ui32ByteCount) &&
       !addressInRam(ui32StartAddress, ui32ByteCount))
    {
        RFUP_PRINT("Specified address range (0x%08X + %d bytes) is not in device FLASH nor RAM.\n", ui32StartAddress, ui32ByteCount);
        return SBL_ARGUMENT_ERROR;
    }
#if 0
    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }
#endif

    //
    // Set progress
    //
    //setProgress(0);

    //
    // Build payload
    // - 4B address (MSB first)
    // - 4B byte count(MSB first)
    //
    ulToCharArray(ui32StartAddress, &pcPayload[0]);
    ulToCharArray(ui32ByteCount, &pcPayload[4]);
    pcPayload[8] = 0x00;
    pcPayload[9] = 0x00;
    pcPayload[10] = 0x00;
    pcPayload[11] = 0x00;
    //
    // Send command
    //
    if((retCode = sendCmd(CMD_CRC32, pcPayload, 12)) != SBL_SUCCESS)
    {
        return retCode;        
    }

    //
    // Receive command response (ACK/NAK)
    //
    //if((retCode = getCmdResponse(bSuccess, 5)) != SBL_SUCCESS)
	if((retCode = getCmdResponse(&bSuccess, 5, false)) != SBL_SUCCESS)
    {
        return retCode;
    }
    if(!bSuccess)
    {
        RFUP_PRINT("Device NAKed CRC32 command.\n");
        return SBL_ERROR;
    }

    //
    // Get data response
    //
    ui32RecvCount = 4;
    if((retCode = getResponseData(pcPayload, &ui32RecvCount)) != SBL_SUCCESS)
    {
        sendCmdResponse(false);
        return retCode;
    }
    *pui32Crc = charArrayToUL(pcPayload);

    //
    // Send ACK/NAK to command
    //
    bool bAck = (ui32RecvCount == 4) ? true : false;
    sendCmdResponse(bAck);

    //
    // Set progress
    //
    //setProgress(100);

    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief Erases all customer accessible flash sectors not protected by FCFG1
 *
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
HI_U32 eraseFlashBank()
{
    int retCode = SBL_SUCCESS;
    bool bResponse = false;
#if 0
    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }
#endif

    //
    // Send command
    //
    if((retCode = sendCmd(CMD_BANK_ERASE,NULL,0)) != SBL_SUCCESS)
    {
        return retCode;
    }

    //
    // Get response
    //
    if((retCode = getCmdResponse(&bResponse,SBL_DEFAULT_RETRY_COUNT,false)) != SBL_SUCCESS)
    {
        return retCode;
    }

    return (bResponse) ? SBL_SUCCESS : SBL_ERROR;
}


//-----------------------------------------------------------------------------
/** \brief Writes the CC26xx defined CCFG fields to the flash CCFG area with
 *      the values received in the data bytes of this command.
 *
 * \param[in] ui32Field
 *      CCFG Field ID which identifies the CCFG parameter to be written.
 * \param[in] ui32FieldValue
 *      Field value to be programmed.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------

HI_U32 setCCFG(HI_U32 ui32Field, HI_U32 ui32FieldValue){
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;
#if 0
    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }
#endif

    //
    // Generate payload
    // - 4B Field ID
    // - 4B Field value
    //
    char pcPayload[8];
    ulToCharArray(ui32Field, &pcPayload[0]);
    ulToCharArray(ui32FieldValue, &pcPayload[4]);

    //
    // Send command
    //
    if((retCode = sendCmd(CMD_SET_CCFG, pcPayload, 8)) != SBL_SUCCESS)
    {
        return retCode;        
    }

    //
    // Receive command response (ACK/NAK)
    //
    //if((retCode = getCmdResponse(bSuccess)) != SBL_SUCCESS)
		
	if((retCode = getCmdResponse(&bSuccess,SBL_DEFAULT_RETRY_COUNT,false)) != SBL_SUCCESS)
    {
        return retCode;
    }
    if(!bSuccess)
    {
        RFUP_PRINT("Set CCFG command NAKed by device.\n");
        return SBL_ERROR;
    }

    
    return SBL_SUCCESS;
}

HI_U32 cmdDownload(HI_U32 ui32Address, HI_U32 ui32Size)
{
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;


    //
    // Check input arguments
    //
    if(!addressInFlash(ui32Address, ui32Size))
    {
        RFUP_PRINT("Flash download: Address range (0x%08X + %d bytes) is not in device FLASH nor RAM.\n", ui32Address, ui32Size);
        return SBL_ARGUMENT_ERROR;
    }
    if(ui32Size & 0x03)
    {
        RFUP_PRINT("Flash download: Byte count must be a multiple of 4\n");
        return SBL_ARGUMENT_ERROR;
    }

    //
    // Generate payload
    // - 4B Program address
    // - 4B Program size
    //
    char pcPayload[8];
    ulToCharArray(ui32Address, &pcPayload[0]);
    ulToCharArray(ui32Size, &pcPayload[4]);

    //
    // Send command
    //
    if((retCode = sendCmd(CMD_DOWNLOAD, pcPayload, 8)) != SBL_SUCCESS)
    {
        return retCode;        
    }

    //
    // Receive command response (ACK/NAK)
    //
    //if((retCode = getCmdResponse(bSuccess)) != SBL_SUCCESS)
    if((retCode = getCmdResponse(&bSuccess,SBL_DEFAULT_RETRY_COUNT,false)) != SBL_SUCCESS)
    {
        return retCode;
    }

    //
    // Return command response
    //
    return (bSuccess) ? SBL_SUCCESS : SBL_ERROR;
}


//-----------------------------------------------------------------------------
/** \brief This function sends the CC2650 SendData command and handles the
 *      device response.
 *
 * \param[in] pcData
 *      Pointer to the data to send.
 * \param[in] ui32ByteCount
 *      The number of bytes to send.
 *
 * \return
 *      Returns SBL_SUCCESS if command and response was successful.
 */
//-----------------------------------------------------------------------------
HI_U32 cmdSendData(const char *pcData, HI_U32 ui32ByteCount)
{   
    HI_U32 retCode = SBL_SUCCESS;
    bool bSuccess = false;

    //
    // Check input arguments
    //
    if(ui32ByteCount > SBL_CC2650_MAX_BYTES_PER_TRANSFER)
    {
        RFUP_PRINT("Error: Byte count (%d) exceeds maximum transfer size %d.\n", ui32ByteCount, SBL_CC2650_MAX_BYTES_PER_TRANSFER);
        return SBL_ERROR;
    }

    //
    // Send command
    //
    if((retCode = sendCmd(CMD_SEND_DATA, pcData, ui32ByteCount)) != SBL_SUCCESS)
    {
        return retCode;        
    }

    //
    // Receive command response (ACK/NAK)
    //
    //if((retCode = getCmdResponse(bSuccess, 3)) != SBL_SUCCESS)
    if((retCode = getCmdResponse(&bSuccess, 3,false)) != SBL_SUCCESS)
    {
        return retCode;
    }
    if(!bSuccess)
    {
        return SBL_ERROR;
    }

    return SBL_SUCCESS;
}

//-----------------------------------------------------------------------------
/** \brief This function returns the page within which address \e ui32Address
 *      is located.
 *
 * \param[in] ui32Address
 *      The address.
 *
 * \return
 *      Returns the flash page within which an address is located.
 */
//-----------------------------------------------------------------------------
HI_U32 addressToPage(HI_U32 ui32Address)
{
    return ((ui32Address - SBL_CC2650_FLASH_START_ADDRESS) / SBL_CC2650_PAGE_ERASE_SIZE);
}

//-----------------------------------------------------------------------------
/** \brief This function initializes connection to the CC2538 device. 
 *
 * \param[in] bSetXosc
 *      If true, try to enable device XOSC.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
HI_U32 initCommunication(bool bSetXosc)
{
    bool bSuccess, bBaudSetOk;
    int retCode = SBL_ERROR;
	
	if(uartX_fd < 0)
	{
		return 1;
	}

	sendAutoBaud(&bBaudSetOk);

    if(sendCmd(0, NULL, 0) != SBL_SUCCESS)
    {
        return SBL_ERROR;
    }

    //
    // Do we get a response (ACK/NAK)?
    //
    bSuccess = false;
    if(getCmdResponse(&bSuccess, SBL_DEFAULT_RETRY_COUNT, true) != SBL_SUCCESS)
    {
		retCode = sendAutoBaud(&bBaudSetOk);
        if(retCode != SBL_SUCCESS)
        {
            return retCode;
        }
    }

    return SBL_SUCCESS;
}

//-----------------------------------------------------------------------------
/** \brief This function reads device ID.
 *
 * \param[out] pui32DeviceId
 *      Pointer to where device ID is stored.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
HI_U32 readDeviceId(HI_U32 *pui32DeviceId)
{
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;

 
    //
    // Send command
    //
    if((retCode = sendCmd(CMD_GET_CHIP_ID,NULL,0)) != SBL_SUCCESS)
    {
        return retCode;        
    }

    //
    // Receive command response (ACK/NAK)
    //
    //if((retCode = getCmdResponse(bSuccess)) != SBL_SUCCESS)
	if((retCode = getCmdResponse(&bSuccess,SBL_DEFAULT_RETRY_COUNT,false)) != SBL_SUCCESS)
	{
        return retCode;
    }
    if(!bSuccess)
    {
        return SBL_ERROR;
    }

    //
    // Receive response data
    //
    char pId[4];
    memset(pId, 0, 4);
    HI_U32 numBytes = 4;
    if((retCode = getResponseData(pId, &numBytes)) != SBL_SUCCESS)
    {
        //
        // Respond with NAK
        //
        sendCmdResponse(false);
        return retCode;
    }

    if(numBytes != 4)
        {
            //
            // Respond with NAK
            //
            sendCmdResponse(false);
            RFUP_PRINT("Didn't receive 4 B.\n");
            return SBL_ERROR;
        }

    //
    // Respond with ACK
    //
    sendCmdResponse(true);

    //
    // Store retrieved ID and report success
    //
    *pui32DeviceId = charArrayToUL(pId);
    m_deviceId = *pui32DeviceId;

    //
    // Store device revision (used internally, see sbl_device_cc2650.h)
    //
    m_deviceRev = getDeviceRev(m_deviceId);

    return SBL_SUCCESS;
}

//-----------------------------------------------------------------------------
/** \brief This function reads \e ui32UnitCount (32 bit) words of data from 
 *      device. Destination array is 32 bit wide. The start address must be 4 
 *      byte aligned.
 *
 * \param[in] ui32StartAddress
 *      Start address in device (must be 4 byte aligned).
 * \param[in] ui32UnitCount
 *      Number of data words to read.
 * \param[out] pcData
 *      Pointer to where read data is stored.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
HI_U32 readMemory32(HI_U32 ui32StartAddress, HI_U32 ui32UnitCount, HI_U32 *pui32Data)
{
	HI_U32 i;
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;

    //
    // Check input arguments
    //
    if((ui32StartAddress & 0x03))
    {
        RFUP_PRINT("readMemory32(): Start address (0x%08X) must be a multiple of 4.\n", ui32StartAddress);
        return SBL_ARGUMENT_ERROR;
    }

#if 0
    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }
#endif
    unsigned char pcPayload[6];
	HI_U32 responseData[SBL_CC2650_MAX_MEMREAD_WORDS];
	HI_U32 chunkCount = ui32UnitCount / SBL_CC2650_MAX_MEMREAD_WORDS;
	if(ui32UnitCount % SBL_CC2650_MAX_MEMREAD_WORDS) chunkCount++;
	HI_U32 remainingCount = ui32UnitCount;

	for(i = 0; i < chunkCount; i++)
	{
		HI_U32 dataOffset = (i * SBL_CC2650_MAX_MEMREAD_WORDS);
		HI_U32 chunkStart = ui32StartAddress + dataOffset;
		HI_U32 chunkSize  = min(remainingCount, SBL_CC2650_MAX_MEMREAD_WORDS);
		remainingCount -= chunkSize;
    
        //
        // Build payload
        // - 4B address (MSB first)
        // - 1B access width
		// - 1B Number of accesses (in words)
        //
        ulToCharArray(chunkStart, (char *)&pcPayload[0]);
        pcPayload[4] = SBL_CC2650_ACCESS_WIDTH_32B;
        pcPayload[5] = chunkSize;
		//
        // Set progress
        //
        //setProgress(((i * 100) / chunkCount));

        //
        // Send command
        //
        if((retCode = sendCmd(CMD_MEMORY_READ, (char *)pcPayload, 6)) != SBL_SUCCESS)
        {
            return retCode;
        }

        //
        // Receive command response (ACK/NAK)
        //
        //if((retCode = getCmdResponse(bSuccess)) != SBL_SUCCESS)
		if((retCode = getCmdResponse(&bSuccess, SBL_DEFAULT_RETRY_COUNT,false)) != SBL_SUCCESS)
        {
            return retCode;
        }
        if(!bSuccess)
        {
            return SBL_ERROR;
        }

        //
        // Receive 4B response
        //
		HI_U32 expectedBytes = chunkSize * 4;
		HI_U32 recvBytes = expectedBytes;
        if((retCode = getResponseData((char*)responseData, &recvBytes)) != SBL_SUCCESS)
        {
            //
            // Respond with NAK
            //
            sendCmdResponse(false);
            return retCode;
        }
	
		if(recvBytes != expectedBytes)
        {
            //
            // Respond with NAK
            //
            sendCmdResponse(false);
            RFUP_PRINT("Didn't receive 4 B.\n");
            return SBL_ERROR;
        }

		memcpy(&pui32Data[dataOffset], responseData, expectedBytes);
		//delete [] responseData;
        //
        // Respond with ACK
        //
        sendCmdResponse(true);
	}

    //
    // Set progress
    //
    //setProgress(100);

    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function reads device FLASH size in bytes.
 *
 * \param[out] pui32FlashSize
 *      Pointer to where FLASH size is stored.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
HI_U32 readFlashSize(HI_U32 *pui32FlashSize)
{
    HI_U32 retCode = SBL_SUCCESS;

    //
    // Read CC2650 DIECFG0 (contains FLASH size information)
    //
    HI_U32 addr = SBL_CC2650_FLASH_SIZE_CFG;
    HI_U32 value;
	retCode = readMemory32(addr, 1, &value);
    if(retCode != SBL_SUCCESS)
    {
        RFUP_PRINT("Failed to read device FLASH size %d\n", retCode);
        return retCode;
    }
	
    //
    // Calculate flash size (The number of flash sectors are at bits [7:0])
    //
    value &= 0xFF;
    *pui32FlashSize = value*SBL_CC2650_PAGE_ERASE_SIZE;

    m_flashSize = *pui32FlashSize;
	RFUP_PRINT("m_flashSize = %d\n",m_flashSize);
    return SBL_SUCCESS;
}



//-----------------------------------------------------------------------------
/** \brief This function reads device RAM size in bytes.
 *
 * \param[out] pui32RamSize
 *      Pointer to where RAM size is stored.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
HI_U32 readRamSize(HI_U32 *pui32RamSize)
{
    int retCode = SBL_SUCCESS;

    //
    // Read CC2650 DIECFG0 (contains RAM size information
    //
    HI_U32 addr = SBL_CC2650_RAM_SIZE_CFG;
    HI_U32 value;
    if((retCode = readMemory32(addr, 1, &value)) != SBL_SUCCESS)
    {
        RFUP_PRINT("Failed to read device RAM size");
        return retCode;
    }

    //
    // Calculate RAM size in bytes (Ram size bits are at bits [1:0])
    //
    value &= 0x03;
    if(m_deviceRev == 1)
    {
        // Early samples has less RAM
        switch(value)
        {
        case 3: *pui32RamSize = 0x4000; break;    // 16 KB
        case 2: *pui32RamSize = 0x2000; break;    // 8 KB
        case 1: *pui32RamSize = 0x1000; break;    // 4 KB
        case 0:                                   // 2 KB
        default:*pui32RamSize = 0x0800; break;    // All invalid values are interpreted as 2 KB
        }
    }
    else
    {
        switch(value)
        {
        case 3: *pui32RamSize = 0x5000; break;    // 20 KB
        case 2: *pui32RamSize = 0x4000; break;    // 16 KB
        case 1: *pui32RamSize = 0x2800; break;    // 10 KB
        case 0:                                   // 4 KB
        default:*pui32RamSize = 0x1000; break;    // All invalid values are interpreted as 4 KB
        }
    }
    
    //
    // Save RAM size internally
    //
    m_ramSize = *pui32RamSize;
	RFUP_PRINT("m_ramSize=%d\n",m_ramSize);
    return retCode;
}


//-----------------------------------------------------------------------------
/** \brief Connect to given port number at specified baud rate.
 *
 * \param[in] csPortNum
 *      String containing the COM port to use
 * \param[in] ui32BaudRate
 *      Baudrate to use for talking to the device.
 * \param[in] bEnableXosc (optional)
 *      If true, try to enable device XOSC. Defaults to false. This option is
 *      not available for all device types.
 *
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
HI_U32 upgrade_connect(int csPortNum, HI_U32 ui32BaudRate, bool bEnableXosc/* = false*/)
{
    int retCode = SBL_SUCCESS;
   

    // Check if device is responding at the given baud rate
    if((retCode = initCommunication(bEnableXosc)) != SBL_SUCCESS)
    {
        return retCode;        
    }

    //
    // Read device ID
    //
    HI_U32 tmp;
    if((retCode = readDeviceId(&tmp)) != SBL_SUCCESS)
    {
        RFUP_PRINT("Failed to read device ID during initial connect.\n");
        return retCode;
    }
    //
    // Read device flash size
    //   
    if((retCode = readFlashSize(&tmp)) != SBL_SUCCESS)
    {
        RFUP_PRINT("Failed to read flash size during initial connect.\n");
        return retCode;
    }
    //
    // Read device ram size
    //
    if((retCode = readRamSize(&tmp)) != SBL_SUCCESS)
    {
        RFUP_PRINT("Failed to read RAM size during initial connect.\n");
        return retCode;
    }

    return SBL_SUCCESS;
}

// Calculate crc32 checksum the way CC2538 and CC2650 does it.
int calcCrcLikeChip(const unsigned char *pData, unsigned long ulByteCount)
{
    unsigned long d, ind;
    unsigned long acc = 0xFFFFFFFF;
    const unsigned long ulCrcRand32Lut[] =
    {
        0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 
        0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C, 
        0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C, 
        0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
    };

    while ( ulByteCount-- )
    {
        d = *pData++;
        ind = (acc & 0x0F) ^ (d & 0x0F);
        acc = (acc >> 4) ^ ulCrcRand32Lut[ind];
        ind = (acc & 0x0F) ^ (d >> 4);
        acc = (acc >> 4) ^ ulCrcRand32Lut[ind];
    }

    return (acc ^ 0xFFFFFFFF);
}

//-----------------------------------------------------------------------------
/** \brief This function gets status from device.
 *
 * \param[out] pStatus
 *      Pointer to where status is stored.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
HI_U32 readStatus(HI_U32 *pui32Status)
{
    HI_U32 retCode = SBL_SUCCESS;
    bool bSuccess = false;

    //
    // Send command
    //
    if((retCode = sendCmd(CMD_GET_STATUS,NULL,0)) != SBL_SUCCESS)
    {
        return retCode;        
    }

    //
    // Receive command response
    //
    //if((retCode = getCmdResponse(bSuccess)) != SBL_SUCCESS)
    if((retCode = getCmdResponse(&bSuccess,SBL_DEFAULT_RETRY_COUNT,false)) != SBL_SUCCESS)
    {
        return retCode;
    }

    if(!bSuccess)
    {
        return SBL_ERROR;
    }

    //
    // Receive command response data
    //
    char status = 0;
    HI_U32 ui32NumBytes = 1;
    if((retCode = getResponseData(&status, &ui32NumBytes)) != SBL_SUCCESS)
    {
        //
        // Respond with NAK
        //
        sendCmdResponse(false);
        return retCode;
    }

    //
    // Respond with ACK
    //
    sendCmdResponse(true);

    m_lastDeviceStatus = status;
    *pui32Status = status;
    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function erases device flash pages. Starting page is the page 
 *      that includes the address in \e startAddress. Ending page is the page 
 *      that includes the address <startAddress + byteCount>. CC13/CC26xx erase 
 *      size is 4KB.
 *
 * \param[in] ui32StartAddress
 *      The start address in flash.
 * \param[in] ui32ByteCount
 *      The number of bytes to erase.
 *
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
HI_U32 eraseFlashRange(HI_U32 ui32StartAddress, HI_U32 ui32ByteCount)
{
    HI_U32 retCode = SBL_SUCCESS;
    bool bSuccess = false;
    char pcPayload[4];
    HI_U32 devStatus;
	HI_U32 i;

    //
    // Calculate retry count
    //
    HI_U32 ui32PageCount = ui32ByteCount / SBL_CC2650_PAGE_ERASE_SIZE;
    if( ui32ByteCount % SBL_CC2650_PAGE_ERASE_SIZE) ui32PageCount ++;
    //setProgress( 0 );
    for(i = 0; i < ui32PageCount; i++)
    {

        //
        // Build payload
        // - 4B address (MSB first)
        //
        ulToCharArray(ui32StartAddress + i*(4096), &pcPayload[0]);

        //
        // Send command
        //
        if((retCode = sendCmd(CMD_SECTOR_ERASE, pcPayload, 4)) != SBL_SUCCESS)
        {
            return retCode;        
        }
		
        //
        // Receive command response (ACK/NAK)
        //
        //if((retCode = getCmdResponse(bSuccess)) != SBL_SUCCESS)
        if((retCode = getCmdResponse(&bSuccess,SBL_DEFAULT_RETRY_COUNT,false)) != SBL_SUCCESS)
        {
            return retCode;
        }
        if(!bSuccess)
        {
            return SBL_ERROR;
        }

        //
        // Check device status (Flash failed if page(s) locked)
        //
        readStatus(&devStatus);
        if(devStatus != CMD_RET_SUCCESS)
        {
            RFUP_PRINT("Flash erase failed. (Status 0x%02X). Flash pages may be locked.\n", devStatus);
            return SBL_ERROR;
        }

        //setProgress( 100*(i+1)/ui32PageCount );
    }
  

    return SBL_SUCCESS;
}

HI_U32 writeFlashRange(HI_U32 ui32StartAddress, HI_U32 ui32ByteCount, const char *pcData)
{
    HI_U32 devStatus = CMD_RET_UNKNOWN_CMD;
    HI_U32 retCode = SBL_SUCCESS;
    HI_U32 bytesLeft, dataIdx, bytesInTransfer;
    HI_U32 transferNumber = 1;
    bool bIsRetry = false;
    bool bBlToBeDisabled = false;
	HI_U32 i;
    //std::vector<tTransfer> pvTransfer;

	int s32TransNum = 0;
	tTransfer pvTransfer[2];
	memset(pvTransfer,0,sizeof(tTransfer)*2);
	
    HI_U32 ui32TotChunks = (ui32ByteCount / SBL_CC2650_MAX_BYTES_PER_TRANSFER);
    if(ui32ByteCount % SBL_CC2650_MAX_BYTES_PER_TRANSFER) ui32TotChunks++;

    //
    // Calculate BL configuration address (depends on flash size)
    //
    HI_U32 ui32BlCfgAddr = SBL_CC2650_FLASH_START_ADDRESS +                 \
                             getFlashSize() -                               \
                             SBL_CC2650_PAGE_ERASE_SIZE +                   \
                             SBL_CC2650_BL_CONFIG_PAGE_OFFSET;

    //
    // Calculate BL configuration buffer index
    //
    HI_U32 ui32BlCfgDataIdx = ui32BlCfgAddr - ui32StartAddress;

    //
    // Is BL configuration part of buffer?
    //
    if(ui32BlCfgDataIdx <= ui32ByteCount)
    {
        if(((pcData[ui32BlCfgDataIdx]) & 0xFF) != SBL_CC2650_BL_CONFIG_ENABLED_BM)
        {
            bBlToBeDisabled = false;
            RFUP_PRINT("Warning: CC2650 bootloader will be disabled.\n");
        }
    }

    if(bBlToBeDisabled)
    {
        //
        // Split into two transfers
        //
        //pvTransfer.resize(2);
        s32TransNum = 2;

        //
        // Main transfer (before lock bit)
        //
        pvTransfer[0].bExpectAck  = true;
        pvTransfer[0].startAddr   = ui32StartAddress;
        pvTransfer[0].byteCount   = (ui32BlCfgAddr - ui32StartAddress) & (~0x03);
        pvTransfer[0].startOffset = 0;

        //
        // The transfer locking the backdoor
        //
        pvTransfer[1].bExpectAck  = false;
        pvTransfer[1].startAddr   = ui32BlCfgAddr - (ui32BlCfgAddr % 4);
        pvTransfer[1].byteCount   = ui32ByteCount - pvTransfer[0].byteCount;
        pvTransfer[1].startOffset = ui32BlCfgDataIdx - (ui32BlCfgDataIdx % 4);

    }
    else
    {
        //pvTransfer.resize(1);
        s32TransNum = 1;
        pvTransfer[0].bExpectAck  = true;
        pvTransfer[0].byteCount = ui32ByteCount;
        pvTransfer[0].startAddr = ui32StartAddress;
        pvTransfer[0].startOffset = 0;
    }

    //
    // For each transfer
    //
    for(i = 0; i < s32TransNum /*pvTransfer.size()*/; i++)
    {
        //
        // Sanity check
        //
        if(pvTransfer[i].byteCount == 0)
        {
            continue;
        }
        
        //
        // Set progress
        //
        //setProgress(addressToPage(pvTransfer[i].startAddr));

        //
        // Send download command
        //
        if((retCode = cmdDownload(pvTransfer[i].startAddr, 
                                  pvTransfer[i].byteCount)) != SBL_SUCCESS)
        {
            return retCode;
        }

        //
        // Check status after download command
        //
        retCode = readStatus(&devStatus);
        if(retCode != SBL_SUCCESS)
        {
            RFUP_PRINT("Error during download initialization. Failed to read device status after sending download command.\n");
            return retCode;
        }
        if(devStatus != CMD_RET_SUCCESS)
        {
            //setState(SBL_ERROR, "Error during download initialization. Device returned status %d (%s).\n", devStatus, getCmdStatusString(devStatus).c_str());
			RFUP_PRINT("read status error 11\n");
			return SBL_ERROR;
        }

        //
        // Send data in chunks
        //
        bytesLeft = pvTransfer[i].byteCount;
        dataIdx   = pvTransfer[i].startOffset;
        while(bytesLeft)
        {
            //
            // Set progress
            //
            //setProgress(addressToPage(ui32StartAddress + dataIdx));
            //setProgress( ((100*(++ui32CurrChunk))/ui32TotChunks) );

            //
            // Limit transfer count
            //
            bytesInTransfer = min(SBL_CC2650_MAX_BYTES_PER_TRANSFER, bytesLeft);

            //
            // Send Data command
            //
            retCode = cmdSendData(&pcData[dataIdx], bytesInTransfer);
            if(retCode != SBL_SUCCESS)
            {
            	#if 0
                setState(retCode, "Error during flash download. \n- Start address 0x%08X (page %d). \n- Tried to transfer %d bytes. \n- This was transfer %d.\n", 
                         (ui32StartAddress+dataIdx), 
                         addressToPage(ui32StartAddress+dataIdx),
                         bytesInTransfer, 
                         (transferNumber));
				#endif
				RFUP_PRINT("cmdSendData failed 11\n");
                return retCode;
            }

            if(pvTransfer[i].bExpectAck)
            {
                //
                // Check status after send data command
                //
                devStatus = 0;
                retCode = readStatus(&devStatus);
                if(retCode != SBL_SUCCESS)
                {
                	#if 0
                    setState(retCode, "Error during flash download. Failed to read device status.\n- Start address 0x%08X (page %d). \n- Tried to transfer %d bytes. \n- This was transfer %d in chunk %d.\n", 
                                 (ui32StartAddress+dataIdx), 
                                 addressToPage(ui32StartAddress + dataIdx),
                                 (bytesInTransfer), (transferNumber), 
                                 (i));
					#endif
					RFUP_PRINT("retCode != SBL_SUCCESS\n");
                    return retCode;
                }
                if(devStatus != CMD_RET_SUCCESS)
                {
                    //setState(SBL_SUCCESS, "Device returned status %s\n", getCmdStatusString(devStatus).c_str());
                    RFUP_PRINT("devStatus != CMD_RET_SUCCESS\n");
                    if(bIsRetry)
                    {
                        //
                        // We have failed a second time. Aborting.
                        #if 0
                        setState(SBL_ERROR, "Error retrying flash download.\n- Start address 0x%08X (page %d). \n- Tried to transfer %d bytes. \n- This was transfer %d in chunk %d.\n", 
                                 (ui32StartAddress+dataIdx), 
                                 addressToPage(ui32StartAddress + dataIdx),
                                 (bytesInTransfer), (transferNumber), 
                                 (i));
						#endif
						RFUP_PRINT("devStatus != CMD_RET_SUCCESS 11\n");
                        return SBL_ERROR;
                    }

                    //
                    // Retry to send data one more time.
                    //
                    bIsRetry = true;
                    continue;
                }
            }
            else
            {
                //
                // We're locking device and will lose access
                //
                m_bCommInitialized = false;
            }

            //
            // Update index and bytesLeft
            //
            bytesLeft -= bytesInTransfer;
            dataIdx += bytesInTransfer;
            transferNumber++;
            bIsRetry = false;
        }
    }

    return SBL_SUCCESS;
}

int vStartCC1310Upgrade()
{
	int s32Ret = 0;

	FILE *pFd = NULL;
	char *path = NULL;
	
	HI_U32 byteCount = 0;			// File size in bytes
    HI_U32 fileCrc, devCrc;		// Variables to save CRC checksum
	HI_U32 devFlashBase = 0;	    // Flash start address

	s32Ret = rfup_InitUart(1, 115200);
	if(s32Ret != 0)
	{
		RFUP_PRINT("vStartCC1310Upgrade: rfup_inituart fail\n");
		return -1;
	}

	pArrBuf = (char *)malloc(UPGRADE_ARR_SIZE * sizeof(char));
	if(pArrBuf == NULL)
	{
		RFUP_PRINT("malloc pArrBuf failed\n");
		return -1;
	}

	if(g_fw1->fwtype == VAVA_UPDATE_TYPE_RF)
	{
		path = UPDATE_FILE1;
	}
	else if(g_fw2->fwtype == VAVA_UPDATE_TYPE_RF)
	{
		path = UPDATE_FILE2;
	}

	if(path == NULL)
	{
		RFUP_PRINT("vStartCC1310Upgrade: [WARING] check type err\n");
		return -1;
	}

	pFd = fopen(path, "rb");
	if(pFd == NULL)
	{
		RFUP_PRINT("fopen %s failed\n", path);
		free(pArrBuf);
		return -1;
	}

	fseek(pFd, 0L, SEEK_END);  
    byteCount = ftell(pFd); 	
	fseek(pFd, 0L, SEEK_SET);
	
	char *buf = (char *)malloc(byteCount*sizeof(char));
	if(buf == NULL)
	{
		RFUP_PRINT("malloc buf failed\n");
		fclose(pFd);
		free(pArrBuf);
		return -1;
	}

	s32Ret = fread(buf,1,byteCount,pFd);
	if(s32Ret != byteCount)
	{
		free(buf);
		fclose(pFd);
		free(pArrBuf);
		return -1;
	}

	fclose(pFd);
	
    if(upgrade_connect(0, 115200, false) != SBL_SUCCESS) 
    {
    	free(buf);
		free(pArrBuf);
        return -1;
    }

	fileCrc = calcCrcLikeChip((unsigned char *)buf, 27*4096/*byteCount*/);
	RFUP_PRINT("Erasing flash ... devFlashBase = %d\n", devFlashBase);

	if(eraseFlashRange(devFlashBase, 27*4096/*byteCount*/) != SBL_SUCCESS)
    {
    	free(buf);
		RFUP_PRINT("Erasing flash Error\n");
		free(pArrBuf);
        return -1;
    }

    RFUP_PRINT("Writing flash ...\n");
    if(writeFlashRange(devFlashBase, 27*4096/*byteCount*/, buf) != SBL_SUCCESS)
    {
    	free(buf);
		RFUP_PRINT("Writing flash Error\n");
		free(pArrBuf);
        return -1;
    }

    RFUP_PRINT("Calculating CRC on device ...\n");
    if(calculateCrc32(devFlashBase, 27*4096/*byteCount*/, &devCrc) != SBL_SUCCESS)
    {
    	free(buf);
		RFUP_PRINT("Calculating CRC on device Error\n");
		free(pArrBuf);
		return -1;
    }

    RFUP_PRINT("Comparing CRC ...\n");
    if(fileCrc == devCrc) RFUP_PRINT("CRC OK\n");
    else RFUP_PRINT("CRC Mismatch!\n");

	//触发IO口接低 
	VAVAHAL_SystemCmd("echo 0 > /sys/devices/gpio-leds/leds/vava:orange:boot/brightness");

	sleep(2);

    RFUP_PRINT("Resetting device ...\n");
    if(reset() != SBL_SUCCESS)
    {
    	free(buf);
		RFUP_PRINT("Resetting device Error\n");
		free(pArrBuf);
        return -1;
    }
    RFUP_PRINT("Reset OK Upgrade finish\n");

	if(buf != NULL)
	{
		free(buf);
		buf = NULL;
	}
	
	free(pArrBuf);

	return 0;
}

