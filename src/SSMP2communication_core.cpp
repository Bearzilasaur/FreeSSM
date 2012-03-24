/*
 * SSMP2communication_core.cpp - Core functions (services) of the new SSM-protocol
 *
 * Copyright (C) 2008-2010 Comer352l
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "SSMP2communication_core.h"

#ifdef __WIN32__
    #include <windows.h>
    #define waitms(x) Sleep(x)
#elif defined __linux__
    #define waitms(x) usleep(1000*x)
#else
    #error "Operating system not supported !"
#endif


SSMP2communication_core::SSMP2communication_core(AbstractDiagInterface *diagInterface)
{
	_diagInterface = diagInterface;
}



bool SSMP2communication_core::ReadDataBlock(char ecuaddr, char padaddr, unsigned int dataaddr, unsigned char nrofbytes, char *data)
{
	// Send: 80 (ECUADR) F0 06  A0  00  00 01 21  (n-1)  (CS)
	// Rcv:  80 F0 (ECUADR) (1+n)  E0  data_1 data_2 ... data_n  (CS)
	if ((dataaddr > 0xffffff) || (nrofbytes > 254)) return false;	// protocol limit (length byte): max. 254 per reply message possible !
	char indata[255] = {0,};
	unsigned char indatalen = 0;
	char querymsg[6] = {0,};
	unsigned int k;
	// SETUP MESSAGE (without Header+Checksum):
	querymsg[0] = '\xA0';
	querymsg[1] = padaddr;
	querymsg[2] = (dataaddr & 0xffffff) >> 16;
	querymsg[3] = (dataaddr & 0xffff) >> 8;
	querymsg[4] = dataaddr & 0xff;
	querymsg[5] = nrofbytes - 1;
	// SEND MESSAGE + RECIEVE ANSWER:
	if (SndRcvMessage(ecuaddr, querymsg, 6, indata, &indatalen))
	{
		// CHECK DATA:
		if (indatalen == (nrofbytes+1))
		{
			if (indata[0] == '\xE0')
			{
				// EXTRACT DATA:
				for (k=0; k<nrofbytes; k++)
					data[k] = indata[1+k];
				return true;
			}
		}
	}
	return false;
}



bool SSMP2communication_core::ReadMultipleDatabytes(char ecuaddr, char padaddr, unsigned int dataaddr[256], unsigned char datalen, char *data)
{
	// 80 (ECUADR) F0 (2+3*n)  A8  (PADADDR)  a1_b2 a1_b1 a1_b0  a2_b2 a2_b1 a2_b0 ... an_b2 an_b1 an_b0  (CS)
	// 80 F0 (ECUADR) (1+n)   E8  data_1 data_2 ... data_n  (CS)
	if (datalen > 84) return false;	// protocol limit (length byte): (255-2)/3 = 84 addresses per message
	// NOTE: most CUs seem to have a limit of 33 addresses per query message !
	char indata[255] = {0,};
	unsigned char indatalen = 0;
	char querymsg[255] = {0,};
	unsigned int k = 0;
	// SETUP MESSAGE:
	querymsg[0] = '\xA8';
	querymsg[1] = padaddr;
	for (k=0; k<datalen; k++)
	{
		querymsg[2+k*3] = (dataaddr[k] & 0xffffff) >> 16;
		querymsg[3+k*3] = (dataaddr[k] & 0xffff) >> 8;
		querymsg[4+k*3] = dataaddr[k] & 0xff;
	}
	// SEND MESSAGE + RECIEVE ANSWER:
	if (SndRcvMessage(ecuaddr, querymsg, (2+3*datalen), indata, &indatalen))
	{
		// CHECK DATA:
		if (indatalen == (datalen+1))
		{
			if (indata[0] == '\xE8')
			{
				// EXTRACT DATA:
				for (k=0; k<datalen; k++)
					data[k] = indata[1+k];
				return true;
			}
		}
	}
	return false;
}



bool SSMP2communication_core::WriteDataBlock(char ecuaddr, unsigned int dataaddr, char *data, unsigned char datalen, char *datawritten)
{
	// Send: 80 (ECUADR) F0 (4+n)  B0  00 01 21  data_1 data_2 ... data_n  (CS)
	// Rcv:  80 F0 (ECUADR) (1+n)  F0  data_1 data_2 ...  (CS)
	if ((dataaddr > 0xffffff) || (datalen > 251)) return false;	// protocol limit (lengthy byte): 255-4 = 251
	char indata[252] = {0,};
	unsigned char indatalen = 0;
	char writemsg[255] = {0,};
	unsigned int k = 0;
	// SETUP MESSAGE:
	writemsg[0] = '\xB0';
	writemsg[1] = (dataaddr & 0xffffff) >> 16;
	writemsg[2] = (dataaddr & 0xffff) >> 8;
	writemsg[3] = dataaddr & 0xff;
	for (k=0; k<datalen; k++)
		writemsg[4+k] = data[k];
	// SEND MESSAGE + RECIEVE ANSWER:
	if (SndRcvMessage(ecuaddr, writemsg, (4+datalen), indata, &indatalen))
	{
		// CHECK DATA:
		if (indatalen == (datalen+1))
		{
			if (indata[0] == '\xF0')
			{
				if (datawritten == NULL)
				{
					// CHECK IF ACTUALLY WRITTEN DATA IS EQUAL TO THE DATA SENT OUT:
					for (k=0; k<datalen; k++)
					{
						if (data[k] != indata[1+k])
						return false;
					}
					return true;
				}
				else
				{
					// EXTRACT AND RETURN WRITTEN DATA:
					for (k=0; k<datalen; k++)
						datawritten[k] = indata[1+k];
					return true;
					// NOTE: NECESSARY FOR SOME OPERATIONS, WHERE THE ACTUALLY WRITTEN DATA IS DIFFERENT TO THE DATA SENT OUT
				}
			}
		}
	}
	return false;
}



bool SSMP2communication_core::WriteDatabyte(char ecuaddr, unsigned int dataaddr, char databyte, char *databytewritten)
{
	// Send: 80 (ECUADR) F0 (05)  B8  00 01 21  databyte  (CS)
	// Rcv:  80 F0 (ECUADR) (02)  F8  databyte  (CS)
	if (dataaddr > 0xffffff) return false;
	char indata[2] = {0,};
	unsigned char indatalen = 0;
	char writemsg[5] = {0,};
	// SETUP MESSAGE (without Header+Checksum):
	writemsg[0] = '\xB8';
	writemsg[1] = (dataaddr & 0xffffff) >> 16;
	writemsg[2] = (dataaddr & 0xffff) >> 8;
	writemsg[3] = dataaddr & 0xff;
	writemsg[4] = databyte;
	// SEND MESSAGE + RECIEVE ANSWER:
	if (SndRcvMessage(ecuaddr, writemsg, 5, indata, &indatalen))
	{
		// CHECK DATA:
		if (indatalen == 2)
		{
			if (indata[0] == '\xF8')
			{
				if (databytewritten == NULL)
				{
					// CHECK IF ACTUALLY WRITTEN DATA IS EQAUL TO THE DATA SENT OUT:
					if (indata[1] == databyte)
						return true;
					else
						return false;
				}
				else
				{
					// EXTRACT AND RETURN WRITTEN DATA:
					*databytewritten = indata[1];
					return true;
					// NOTE: NECESSARY FOR SOME OPERATIONS, WHERE THE ACTUALLY WRITTEN DATA IS DIFFERENT TO THE DATA SENT OUT
				}
			}
		}
	}
	return false;
}



bool SSMP2communication_core::GetCUdata(char ecuaddr, char *SYS_ID, char *ROM_ID, char *flagbytes, unsigned char *nrofflagbytes)
{
	// Send: 80 (ECUADR) F0 01 BF (CS)
	// Rcv:  80 F0 (ECUADR) (9+n)  FF  sysID_1 sysID_2 sysID_3 romID_1 ... romID_5 flagbyte_1 flagbyte_2 ... flagbyte_n  CS
	//       n = nr. of  flagbytes (32,48 or 96)
	*nrofflagbytes=0;
	char indata[255] = {0,};
	unsigned char indatalen = 0;
	char initmsg = '\xBF';
	unsigned char k = 0;
	// SEND MESSAGE + RECIEVE ANSWER:
	if (SndRcvMessage(ecuaddr, &initmsg, 1, indata, &indatalen))
	{
		// CHECK MESSAGE LENGTH:
		if ((indatalen == 41) || (indatalen == 57) || (indatalen == 105))
		{
			// CHECK DATA:
			if (indata[0] == '\xFF')
			{
				// EXTRACT CU DATA:
				SYS_ID[0] = indata[1];
				SYS_ID[1] = indata[2];
				SYS_ID[2] = indata[3];
				ROM_ID[0] = indata[4];
				ROM_ID[1] = indata[5];
				ROM_ID[2] = indata[6];
				ROM_ID[3] = indata[7];
				ROM_ID[4] = indata[8];
				*nrofflagbytes = indatalen-9;
				for (k=0; k<*nrofflagbytes; k++)
					flagbytes[k] = indata[9+k];
				return true;
			}
		}
	}
	return false;
}



bool SSMP2communication_core::SndRcvMessage(char ecuaddr, char *outdata, unsigned char outdatalen, char *indata, unsigned char *indatalen)
{
	if (_diagInterface == NULL) return false;
	if (outdatalen < 1) return false;
	std::vector<char> msg_buffer;
	unsigned int k = 0;
	// SETUP COMPLETE MESSAGE:
	// Protocol-header
	msg_buffer.push_back('\x80');
	msg_buffer.push_back(ecuaddr);
	msg_buffer.push_back('\xF0');
	msg_buffer.push_back(static_cast<char>(outdatalen));
	// Message:
	for (k=0; k<outdatalen; k++)
		msg_buffer.push_back(outdata[k]);
	// Checksum:
	msg_buffer.push_back( calcchecksum(&msg_buffer.at(0), 4 + outdatalen) );
#ifdef __FSSM_DEBUG__
	// DEBUG-OUTPUT:
	std::cout << "SSMPcore::SndRcvMessage(...):   Sending message:\n";
	for (unsigned int k=0; k<=(msg_buffer.size()/16); k++)
	{
		if (16*(k+1) <= msg_buffer.size())
			std::cout << "   " << libFSSM::StrToHexstr(&msg_buffer.at(k*16), 16) << '\n';
		else if (msg_buffer.size()%16)
			std::cout << "   " << libFSSM::StrToHexstr(&msg_buffer.at(k*16), (msg_buffer.size()%16)) << '\n';
	}
#endif
	// SEND MESSAGE:
	if (!_diagInterface->write(msg_buffer))
	{
#ifdef __FSSM_DEBUG__
		std::cout << "SSMPcore::SndRcvMessage(...):   Write failed\n";
#endif
		return false;
	}
	msg_buffer.clear();
	// PREPARE FOR ANSWER:
	std::vector<char> read_buffer;
	unsigned char count = 1;	// used for timeout
	unsigned short int inmsglen = 0;
	// WAIT FOR HEADER OF ANSWER (+ ECHO LENGTH):
	while ((msg_buffer.size() < (4u + outdatalen + 1u + 4u)) && (count < 81))	// timout: 80*20ms = 1600ms
	{
		waitms(20);
		if (!_diagInterface->read(&read_buffer))
		{
#ifdef __FSSM_DEBUG__
			std::cout << "SSMPcore::SndRcvMessage(...):   Read 1 failed\n";
#endif
			// NOTE: fail silent
		}	
		else if (read_buffer.size())
		{
			msg_buffer.insert(msg_buffer.end(), read_buffer.begin(), read_buffer.end());
		}
		count++;
	}
	// CHECK IF TIMEOUT:
	if (msg_buffer.size() < (4u + outdatalen + 1u + 4u))
	{
	#ifdef __FSSM_DEBUG__
		std::cout << "SSMPcore::SndRcvMessage(...):   Timeout 1\n";
	#endif
		return false;
	}
	// ELIMINATE ECHO :
	msg_buffer.erase(msg_buffer.begin(), msg_buffer.begin() + 4 + outdatalen + 1);
	// CHECK IF PROTOCOL HEADER IS CORRECT:
	if ((msg_buffer.at(0) != '\x80') || (msg_buffer.at(1) != '\xF0') || (msg_buffer.at(2) != ecuaddr))
	{
#ifdef __FSSM_DEBUG__
		std::cout << "SSMPcore::SndRcvMessage(...):   Invalid Protocol Header\n";
#endif
		return false;
	}
	// CALCULATE LENGTH OF COMPLETE ANSWER MESSAGE (using length byte of header):
	inmsglen = 4 + static_cast<unsigned char>(msg_buffer.at(3)) + 1;
	// READ REST OF THE MESSAGE:
	if (msg_buffer.size() < inmsglen)	// IF ANSWER IS INCOMPLETE
	{
		// WAIT FOR REST OF THE INCOMING MESSAGE:
		count = 0;	// reset timout counter
		while ( (msg_buffer.size() < inmsglen) && (count < 27) )	// 260-4=256Bytes=533.3ms => 540ms=27*20
		{
			waitms(20);
			if (!_diagInterface->read(&read_buffer))
			{
#ifdef __FSSM_DEBUG__
				std::cout << "SSMPcore::SndRcvMessage(...):   Read 2 failed\n";
#endif
				// NOTE: fail silent
			}
			else if (read_buffer.size())
			{
				msg_buffer.insert(msg_buffer.end(), read_buffer.begin(), read_buffer.end());
			}
			count++;
		}
		// CHECK IF MESSAGE COMPLETE:
		if (msg_buffer.size() != inmsglen)
		{
#ifdef __FSSM_DEBUG__
			std::cout << "SSMPcore::SndRcvMessage(...):   Timeout 2\n";
#endif
			return false;
		}
	}
	else if (msg_buffer.size() > inmsglen)	// CHECK IF ANSWER IS TOO LONG
	{
#ifdef __FSSM_DEBUG__
		std::cout << "SSMPcore::SndRcvMessage(...):   Too many bytes read\n";
#endif
		return false;
	}
	// CHECK CHECKSUM:
	if (msg_buffer.back() != calcchecksum(&msg_buffer.at(0), (msg_buffer.size() - 1)))
	{
#ifdef __FSSM_DEBUG__
		std::cout << "SSMPcore::SndRcvMessage(...):   Checksum Error\n";
#endif
		return false;
	}
	// EXTRACT AND RETURN DATA:
	*indatalen = inmsglen - 4 - 1;
	for (k=0; k<*indatalen; k++)
		indata[k] = msg_buffer.at(4+k);
#ifdef __FSSM_DEBUG__
	// DEBUG-OUTPUT:
	std::cout << "SSMPcore::SndRcvMessage(...):   Recieved message:\n";
	for (unsigned int k=0; k<=(inmsglen/16); k++)
	{
		if (16*(k+1) <= inmsglen)
			std::cout << "   " << libFSSM::StrToHexstr(&msg_buffer.at(k*16), 16) << '\n';
		else if (inmsglen%16)
			std::cout << "   " << libFSSM::StrToHexstr(&msg_buffer.at(k*16), (inmsglen%16)) << '\n';
	}
#endif
	return true;
}



char SSMP2communication_core::calcchecksum(char *message, unsigned int nrofbytes)
{
	unsigned short int cs = 0;
	unsigned int k;
	for (k=0; k<nrofbytes; k++)
		cs = (cs + message[k]) & 0xff;
	return static_cast<char>(cs);
}



bool SSMP2communication_core::charcmp(char *chararray_a, char *chararray_b, unsigned int len)
{
	unsigned int k;
	for (k=0; k<len; k++)
	{
		if (chararray_a[k] != chararray_b[k])
		return false;
	}
	return true;
}
