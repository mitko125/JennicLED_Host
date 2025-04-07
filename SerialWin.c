/****************************************************************************
 *
 * MODULE:             Linux 6LoWPAN Routing daemon
 *
 * COMPONENT:          Serial interface
 *
 * REVISION:           $Revision: 43420 $
 *
 * DATED:              $Date: 2012-06-18 15:13:17 +0100 (Mon, 18 Jun 2012) $
 *
 * AUTHOR:             Matt Redfearn
 *
 ****************************************************************************
 *
 * This software is owned by NXP B.V. and/or its supplier and is protected
 * under applicable copyright laws. All rights are reserved. We grant You,
 * and any third parties, a license to use this software solely and
 * exclusively on NXP products [NXP Microcontrollers such as JN5148, JN5142, JN5139]. 
 * You, and any third parties must reproduce the copyright and warranty notice
 * and any other legend of ownership on each copy or partial copy of the 
 * software.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.

 * Copyright NXP B.V. 2012. All rights reserved
 *
 ***************************************************************************/

#include "Serial.h"


#include <windows.h>
#include <string.h>
#include <conio.h>


#include <winbase.h>



//#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
//#include <unistd.h>
#include <fcntl.h>
//#include <sys/signal.h>
#include <sys/types.h>
//#include <errno.h>
#include <signal.h>


//#include <libdaemon/daemon.h>



#include "log.h"


#define DEBUG 0

int verbosity;

extern volatile sig_atomic_t bRunning;

int serial_fd;

int serial_open(int port, uint32_t baud)
{
  int fd;
  char device[ 80];
	DCB dcb;
	

	sprintf_s( device,80, "\\\\.\\COM%d", port);

	daemon_log(LOG_INFO, "Opening serial device '%s' at baud rate %ubps", device, baud);
    
		
	fd=(int)CreateFile(device, 
			GENERIC_READ|GENERIC_WRITE, 
			0, 
			0, 
			OPEN_EXISTING, 
			0,//FILE_FLAG_OVERLAPPED, 
			0);

	if( (HANDLE)fd == INVALID_HANDLE_VALUE)	 {
    daemon_log(LOG_ERR, "Couldn't open serial device %s", device);
		printf("Press any key\n\r");
		_getch();
    return -1;
	}



	dcb.DCBlength=sizeof(DCB);
	if (!GetCommState((HANDLE)fd,&dcb)){
		CloseHandle((HANDLE)fd);
		return -1;			
	}
	dcb.BaudRate=baud;

	dcb.ByteSize = 8;
	if( (dcb.Parity   = NOPARITY) != NOPARITY)
		dcb.fParity  = TRUE;
	else
		dcb.fParity  = FALSE;
	dcb.StopBits = ONESTOPBIT;


	dcb.fDsrSensitivity=FALSE;
	dcb.fAbortOnError=FALSE;
	dcb.fOutxCtsFlow=FALSE;
	dcb.fOutX=FALSE;
	dcb.fInX=FALSE;

	if (!SetCommState((HANDLE)fd,&dcb)) {
        daemon_log(LOG_ERR, "Error setting port settings ");
        return -1;
    }
    
 
    serial_fd = fd;
    return fd;
}


int serial_read(unsigned char *data)
{
	COMSTAT comstat;
	DWORD   err = 0;
	int res= 0;
	int bytes= 0;


	ClearCommError((HANDLE)serial_fd,&err,&comstat);
	if(comstat.cbInQue){
		;
	}else{
		  //printf("Serial read: %d\n", res);
        if (res == 0)
        {
            //daemon_log(LOG_ERR, "Serial connection to module interrupted");
            //bRunning = 0;
        }
		return 0;
	}

	ReadFile((HANDLE)serial_fd,data,1,&res,NULL);	

	if (res > 0) {
#if DEBUG
    if (verbosity >= LOG_DEBUG) daemon_log(LOG_DEBUG, "RX %02x", *data);
#endif /* DEBUG */
  }
  
  return res;
}

int serial_write(const unsigned char data)
{
    int err, attempts = 0;

		DWORD wLength;

#if DEBUG
    if (verbosity >= LOG_DEBUG) daemon_log(LOG_DEBUG, "TX %02x", data);
#endif /* DEBUG */
    
		
    err = WriteFile((HANDLE)serial_fd,&data,1,&wLength,NULL);
    if (err < 0)
    {
        if (errno == EAGAIN)
        {
            for (attempts = 0; attempts <= 5; attempts++)
            {
                Sleep(1000);
                err = WriteFile((HANDLE)serial_fd,&data,1,&wLength,NULL);
                if (err < 0) 
                {
                    if ((errno == EAGAIN) && (attempts == 5))
                    {
                        daemon_log(LOG_ERR, "Error writing to module after %d", attempts);
                        exit(-1);
                    }
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            daemon_log(LOG_ERR, "Error writing to module");
            exit(-1);
        }
    }
    return 0;
}
/*
int main(int argc, char *argv[])
{

	verbosity = LOG_DEBUG;
	if( serial_open(15,1000000L) >= 0 ){
		while(1){
			unsigned char i;
			for( i = ' '; i < 'z' ; i++){
				unsigned char data[10];
				serial_write(i);
				Sleep(100);
				serial_read(data);
			}
		}
	}
	

	return 0;
}
*/