/* Copyright 2008-2013, 2018 Guillaume Roguez

This file is part of Helios.

Helios is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Helios is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Helios.  If not, see <https://www.gnu.org/licenses/>.

*/

/* 
**
*/

#include <proto/helios.h>
#include <proto/exec.h>
#include <proto/dos.h>

struct Library *HeliosBase;
void __chkabort(void) {};

CONST_STRPTR type_str[] =
{
    [HRMB_FATAL]= "Fatal",
    [HRMB_ERROR]= "Error",
    [HRMB_WARN] = "Warning",
    [HRMB_INFO] = "Info",
    [HRMB_DBG]  = "Debug",
};

int main(int argc, char **argv)
{
    HeliosEventListenerList *ell=NULL;
    HeliosEventMsg listener;
    HeliosReportMsg *msg;
    struct MsgPort *evt_port;
    int ret = RETURN_ERROR;

    HeliosBase = OpenLibrary("helios.library", 52);
    if (NULL != HeliosBase)
    {
        Helios_GetAttrs(HGA_BASE, HeliosBase,
                        HA_EventListenerList, (ULONG)&ell,
                        TAG_DONE);
        if (NULL != ell)
        {
            evt_port = CreateMsgPort();
            if (NULL != evt_port)
            {
                /* Prepare our listener */
                listener.hm_Msg.mn_ReplyPort = evt_port;
                listener.hm_Msg.mn_Length = sizeof(listener);
                listener.hm_Type = HELIOS_MSGTYPE_FAST_EVENT;
                listener.hm_EventMask = HEVTF_NEW_REPORTMSG;
                
                for (;;)
                {
                    ULONG sigs;

                    /* Fetch all report messages */
                    while (NULL != (msg = Helios_GetNextReportMsg()))
                    {
                        Printf("%7s-%-12s: %s\n",
                               (ULONG)type_str[msg->hrm_TypeBit],
                               (ULONG)msg->hrm_Label,
                               (ULONG)msg->hrm_Msg);
                        
                        Helios_FreeReportMsg(msg);
                    }

                    /* (Re-)register the listener (as it's a FAST_EVENT listenera) */
                    Helios_AddEventListener(ell, &listener);

                    /* Wait for new reports or BREAK-C */
                    sigs = Wait((1ul << evt_port->mp_SigBit) | SIGBREAKF_CTRL_C);
                    if (sigs & SIGBREAKF_CTRL_C)
                        break;

                    /* I can get only one possible messages.
                     * In others cases a while loop is needed!
                     */
                    GetMsg(evt_port);
                }

                Helios_RemoveEventListener(ell, &listener);
                DeleteMsgPort(evt_port);
            }
        }

        CloseLibrary(HeliosBase);
    }

    return ret;
}
