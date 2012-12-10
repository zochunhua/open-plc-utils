/*====================================================================*
 *   
 *   Copyright (c) 2011 by Qualcomm Atheros.
 *   
 *   Permission to use, copy, modify, and/or distribute this software 
 *   for any purpose with or without fee is hereby granted, provided 
 *   that the above copyright notice and this permission notice appear 
 *   in all copies.
 *   
 *   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL 
 *   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED 
 *   WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL  
 *   THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR 
 *   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 *   LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, 
 *   NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 *   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *   
 *--------------------------------------------------------------------*/

/*====================================================================*
 *
 *   plcrate.c - Atheros INT6x00 PHY Rate monitor
 *
 *.  Qualcomm Atheros HomePlug AV Powerline Toolkit
 *:  Published 2009-2011 by Qualcomm Atheros. ALL RIGHTS RESERVED
 *;  For demonstration and evaluation only. Not for production use
 *
 *   Contributor(s):
 *      Charles Maier <cmaier@qualcomm.com>
 *
 *--------------------------------------------------------------------*/


/*====================================================================*"
 *   system header files;
 *--------------------------------------------------------------------*/

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

/*====================================================================*
 *   custom header files;
 *--------------------------------------------------------------------*/

#include "../tools/getoptv.h"
#include "../tools/putoptv.h"
#include "../tools/memory.h"
#include "../tools/number.h"
#include "../tools/symbol.h"
#include "../tools/types.h"
#include "../tools/flags.h"
#include "../tools/files.h"
#include "../tools/error.h"
#include "../plc/plc.h"

/*====================================================================*
 *   custom source files;
 *--------------------------------------------------------------------*/

#ifndef MAKEFILE
#include "../plc/chipset.c"
#include "../plc/Devices.c"
#include "../plc/Confirm.c"
#include "../plc/Display.c"
#include "../plc/Failure.c"
#include "../plc/Request.c"
#include "../plc/ReadMME.c"
#include "../plc/SendMME.c"
#include "../plc/Antiphon.c"
#include "../plc/LocalDevices.c"
#include "../plc/ResetDevice.c"
#include "../plc/VersionInfo1.c"
#include "../plc/StationRole.c"
#include "../plc/PhyRates1.c"
#include "../plc/Traffic1.c"
#include "../plc/Transmit.c"
#include "../plc/NetworkTraffic1.c"
#include "../plc/WaitForStart.c"
#endif

#ifndef MAKEFILE
#include "../tools/getoptv.c"
#include "../tools/putoptv.c"
#include "../tools/version.c"
#include "../tools/uintspec.c"
#include "../tools/hexdump.c"
#include "../tools/hexencode.c"
#include "../tools/hexdecode.c"
#include "../tools/todigit.c"
#include "../tools/checkfilename.c"
#include "../tools/checksum32.c"
#include "../tools/error.c"
#include "../tools/fdchecksum32.c"
#include "../tools/hexstring.c"
#include "../tools/synonym.c"
#include "../tools/typename.c"
#endif

#ifndef MAKEFILE
#include "../ether/openchannel.c"
#include "../ether/closechannel.c"
#include "../ether/readpacket.c"
#include "../ether/sendpacket.c"
#include "../ether/channel.c"
#endif

#ifndef MAKEFILE
#include "../mme/EthernetHeader.c"
#include "../mme/EthernetHeader.c"
#include "../mme/QualcommHeader.c"
#include "../mme/FragmentHeader.c"
#include "../mme/UnwantedMessage.c"
#include "../mme/MMECode.c"
#endif

/*====================================================================*
 *   program constants;
 *--------------------------------------------------------------------*/

#define INT6KRATE_WAIT 0
#define INT6KRATE_LOOP 1

/*====================================================================*
 *   
 *   void manager (struct plc * plc, signed count, signed pause);
 *   
 *   perform operations in logical order despite any order specfied 
 *   on the command line; for example, read PIB before writing PIB; 
 *
 *   operation order is controlled by the order of "if" statements 
 *   shown here; the entire operation sequence can be repeated with
 *   an optional pause between each iteration;
 * 
 *.  Qualcomm Atheros HomePlug AV Powerline Toolkit
 *:  Published 2009-2011 by Qualcomm Atheros. ALL RIGHTS RESERVED
 *;  For demonstration and evaluation only. Not for production use
 *
 *--------------------------------------------------------------------*/

void manager (struct plc * plc, signed count, signed pause) 

{
	while (count--) 
	{
		if (_anyset (plc->flags, PLC_VERSION)) 
		{
			VersionInfo1 (plc);
		}
		if (_anyset (plc->flags, PLC_LOCAL_TRAFFIC)) 
		{
			Traffic1 (plc);
		}
		if (_anyset (plc->flags, PLC_NETWORK_TRAFFIC)) 
		{
			NetworkTraffic1 (plc);
		}
		if (_anyset (plc->flags, PLC_NETWORK)) 
		{
			PhyRates1 (plc);
		}
		if (_anyset (plc->flags, PLC_RESET_DEVICE)) 
		{
			ResetDevice (plc);
		}
		sleep (pause);
	}
	return;
}


/*====================================================================*
 *   
 *   int main (int argc, char const * argv[]);
 *   
 *   parse command line, populate plc structure and perform selected 
 *   operations; show help summary if asked; see getoptv and putoptv
 *   to understand command line parsing and help summary display; see
 *   plc.h for the definition of struct plc; 
 *
 *   the command line accepts multiple MAC addresses and the program 
 *   performs the specified operations on each address, in turn; the
 *   address order is significant but the option order is not; the
 *   default address is a local broadcast that causes all devices on
 *   the local H1 interface to respond but not those at the remote
 *   end of the powerline;
 *
 *   the default address is 00:B0:52:00:00:01; omitting the address
 *   will automatically address the local device; some options will
 *   cancel themselves if this makes no sense;
 *
 *   the default interface is eth1 because most people use eth0 as 
 *   their principle network connection; you can specify another 
 *   interface with -i or define environment string PLC to make
 *   that the default interface and save typing;
 *   
 *.  Qualcomm Atheros HomePlug AV Powerline Toolkit
 *:  Published 2009-2011 by Qualcomm Atheros. ALL RIGHTS RESERVED
 *;  For demonstration and evaluation only. Not for production use
 *
 *--------------------------------------------------------------------*/

int main (int argc, char const * argv []) 

{
	extern struct channel channel;
	static char const * optv [] = 
	{
		"cd:ei:l:o:nqrRtTuvw:x",
		"device [device] [...]",
		"Qualcomm Atheros INT6x00 PHY Rate Monitor",
		"c\tdisplay coded PHY rates",
		"d n\ttraffic duration is (n) seconds per leg [" LITERAL (PLC_ECHOTIME) "]",
		"e\tredirect stderr to stdout",

#if defined (WINPCAP) || defined (LIBPCAP)

		"i n\thost interface is (n) [" LITERAL (CHANNEL_ETHNUMBER) "]",

#else

		"i s\thost interface is (s) [" LITERAL (CHANNEL_ETHDEVICE) "]",

#endif

		"l n\tloop (n) times [" LITERAL (INT6KRATE_LOOP) "]",
		"n\tnetwork TX/RX information",
		"o n\tread timeout is (n) milliseconds [" LITERAL (CHANNEL_TIMER) "]",
		"q\tquiet mode",
		"r\trequest device information",
		"R\treset device with VS_RS_DEV",
		"t\tgenerate network traffic (one-to-many)",
		"T\tgenerate network traffic (many-to-many)",
		"u\tdisplay uncoded PHY rates",
		"v\tverbose mode",
		"w n\twait (n) seconds [" LITERAL (INT6KRATE_WAIT) "]",
		"x\texit on error",
		(char const *) (0)
	};

#include "../plc/plc.c"

	signed loop = INT6KRATE_LOOP;
	signed wait = INT6KRATE_WAIT;
	signed c;
	optind = 1;
	if (getenv (PLCDEVICE)) 
	{

#if defined (WINPCAP) || defined (LIBPCAP)

		channel.ifindex = atoi (getenv (PLCDEVICE));

#else

		channel.ifname = strdup (getenv (PLCDEVICE));

#endif

	}
	plc.timer = PLC_ECHOTIME;
	while ((c = getoptv (argc, argv, optv)) != -1) 
	{
		switch (c) 
		{
		case 'c':
			_clrbits (plc.flags, PLC_UNCODED_RATES);
			break;
		case 'd':
			plc.timer = (unsigned)(uintspec (optarg, 1, 60));
		case 'e':
			dup2 (STDOUT_FILENO, STDERR_FILENO);
			break;
		case 'i':

#if defined (WINPCAP) || defined (LIBPCAP)

			channel.ifindex = atoi (optarg);

#else

			channel.ifname = optarg;

#endif

			break;
		case 'l':
			loop = (unsigned)(uintspec (optarg, 0, UINT_MAX));
			break;
		case 'n':
			_setbits (plc.flags, PLC_NETWORK);
			break;
		case 'o':
			channel.timer = (signed)(uintspec (optarg, 0, UINT_MAX));
			break;
		case 'q':
			_setbits (plc.flags, PLC_SILENCE);
			break;
		case 'r':
			_setbits (plc.flags, PLC_VERSION);
			break;
		case 'R':
			_setbits (plc.flags, PLC_RESET_DEVICE);
			break;
		case 't':
			_setbits (plc.flags, PLC_LOCAL_TRAFFIC);
			break;
		case 'T':
			_setbits (plc.flags, PLC_NETWORK_TRAFFIC);
			break;
		case 'u':
			_setbits (plc.flags, PLC_UNCODED_RATES);
			break;
		case 'v':
			_setbits (channel.flags, CHANNEL_VERBOSE);
			_setbits (plc.flags, PLC_VERBOSE);
			break;
		case 'w':
			wait = (unsigned)(uintspec (optarg, 0, 3600));
			break;
		case 'x':
			_setbits (plc.flags, PLC_BAILOUT);
			break;
		default:
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (_allclr (plc.flags, (PLC_VERSION | PLC_LOCAL_TRAFFIC | PLC_NETWORK_TRAFFIC | PLC_RESET_DEVICE))) 
	{
		_setbits (plc.flags, PLC_NETWORK);
	}
	openchannel (&channel);
	if (!(plc.message = malloc (sizeof (* plc.message)))) 
	{
		error (1, errno, PLC_NOMEMORY);
	}
	if (!argc) 
	{
		manager (&plc, loop, wait);
	}
	while ((argc) && (* argv)) 
	{
		if (!hexencode (channel.peer, sizeof (channel.peer), synonym (* argv, devices, SIZEOF (devices)))) 
		{
			error (1, errno, PLC_BAD_MAC, * argv);
		}
		manager (&plc, loop, wait);
		argv++;
		argc--;
	}
	free (plc.message);
	closechannel (&channel);
	exit (0);
}
