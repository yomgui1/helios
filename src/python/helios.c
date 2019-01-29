/* Copyright 2008-2013,2019 Guillaume Roguez

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
** Python module to bind the Helios IEEE-1394 library.
**
*/

//#define NDEBUG

#include <Python.h>

#include "libraries/helios.h"
#include "devices/helios.h"

#include "proto/helios.h"

#include <clib/macros.h>
#include <proto/exec.h>
#include <proto/alib.h>


/*
** Private Macros and Definitions
*/

#ifndef MODNAME
#define MODNAME "helios"
#endif

#ifndef INITFUNC
#define INITFUNC inithelios
#endif

extern void kprintf(char *, ...);
extern void vkprintf(char *, void *);

#ifndef NDEBUG

#define DB(x, a...) kprintf("HELIOS: "x , ## a)
#define DB_Raw(x, a...) kprintf(x , ## a)
#define DB_NotImplemented() DB("%s: %s() not implemented\n", __FILE__, __FUNCTION__)
#define DB_NotFinished() DB("%s: %s() not finished\n", __FILE__, __FUNCTION__)
#define DB_IN(fmt, a...) DB("+ "__FUNCTION__" + "fmt"\n", ## a)
#define DB_OUT() DB("- "__FUNCTION__" -\n")
#define log_Debug(m, a...) kprintf("HELIOS-Debug: "m"\n" ,## a)
#else

#define DB(x, a...)
#define DB_Raw(x, a...)
#define DB_NotImplemented()
#define DB_NotFinished()
#define DB_IN(fmt, a...)
#define DB_OUT()
#define log_Debug(x,a...)

#endif

#define log_APIError(m) _log_APIError(__FUNCTION__, m)
static inline void _log_APIError(CONST_STRPTR fname, CONST_STRPTR msg)
{
    DB("  Function '%s' failed: \"%s\"\n", fname, msg);
}

#define log_Error(m, a...) kprintf("HELIOS-Error: "m"\n" ,## a)

/* Python useful macros */
#define ADD_TYPE(m, s, t) {Py_INCREF(t); PyModule_AddObject(m, s, (PyObject *)(t));}
#define INSI(m, s, v) if (PyModule_AddIntConstant(m, s, v)) return -1
#define INSS(m, s, v) if (PyModule_AddStringConstant(m, s, v)) return -1

#define ADDVAFUNC(name, func, doc...) {name, (PyCFunction) func, METH_VARARGS ,## doc}
#define ADD0FUNC(name, func, doc...) {name, (PyCFunction) func, METH_NOARGS ,## doc}

#define SIMPLE0FUNC(fname, func) static PyObject * fname(PyObject *self){ func(); Py_RETURN_NONE; }
#define SIMPLE0FUNC_bx(fname, func, x) static PyObject * fname(PyObject *self){ return Py_BuildValue(x, func()); }
#define SIMPLE0FUNC_fx(fname, func, x) static PyObject * fname(PyObject *self){ return x(func()); }

#define OBJ_TNAME(o) (((PyObject *)(o))->ob_type->tp_name)
#define OBJ_TNAME_SAFE(o) ({                                \
            PyObject *_o = (PyObject *)(o);                 \
            NULL != _o ? _o->ob_type->tp_name : "nil"; })

/*
** Private Types and Structures
*/

typedef struct PyHeliosOHCIDevice {
    PyObject_HEAD

    struct MsgPort * Port;
    IOHeliosHWReq *  IOReq;
} PyHeliosOHCIDevice;


/*
** Public Variables
*/

struct Library *HeliosBase = NULL;


/*
** Private Variables
*/

static PyTypeObject PyHeliosOHCIDevice_Type;


/*
** Private Functions
*/

//+ all_ins
static int
all_ins(PyObject *m) {
    /* Bus speeds */
    INSI(m, "S100", S100);
    INSI(m, "S200", S200);
    INSI(m, "S400", S400);

    /* Lock extented TCode's */
    INSI(m, "EXTCODE_MASK_SWAP",    EXTCODE_MASK_SWAP);
    INSI(m, "EXTCODE_COMPARE_SWAP", EXTCODE_COMPARE_SWAP);
    INSI(m, "EXTCODE_FETCH_ADD",    EXTCODE_FETCH_ADD);
    INSI(m, "EXTCODE_LITTLE_ADD",   EXTCODE_LITTLE_ADD);
    INSI(m, "EXTCODE_BOUNDED_ADD",  EXTCODE_BOUNDED_ADD);
    INSI(m, "EXTCODE_WRAP_ADD",     EXTCODE_WRAP_ADD);

    /* Device types */
    INSI(m, "NODE_TYPE_UNKNOWN", HELIOS_NODE_TYPE_UNKNOWN);
    INSI(m, "NODE_TYPE_DC",      HELIOS_NODE_TYPE_DC);
    INSI(m, "NODE_TYPE_AVC",     HELIOS_NODE_TYPE_AVC);
    INSI(m, "NODE_TYPE_SBP2",    HELIOS_NODE_TYPE_SBP2);
    INSI(m, "NODE_TYPE_CPU",     HELIOS_NODE_TYPE_CPU);

    return 0;
}
//- all_ins

//+ ReadCPUClock
static void
ReadCPUClock(UQUAD *v)
{
    register unsigned long tbu, tb, tbu2;

loop:
    asm volatile ("mftbu %0" : "=r" (tbu) );
    asm volatile ("mftb  %0" : "=r" (tb)  );
    asm volatile ("mftbu %0" : "=r" (tbu2));
    if (tbu != tbu2) goto loop;

    /* The slightly peculiar way of writing the next lines is
       compiled better by GCC than any other way I tried. */
    ((long*)(v))[0] = tbu;
    ((long*)(v))[1] = tb;
}
//-
//+ copy_ioreq
void copy_ioreq(IOHeliosHWReq *base, IOHeliosHWReq *dest, ULONG dstsize)
{
    CopyMem(base, dest, sizeof(IOHeliosHWReq));
    dest->iohh_Req.io_Message.mn_Length = dstsize;
}
//-
//+ do_ioreq
static LONG do_ioreq(struct IORequest *ioreq)
{
    struct MsgPort *port = ioreq->io_Message.mn_ReplyPort;
    ULONG sigs, psig;

    psig = 1ul << port->mp_SigBit;

    Py_BEGIN_ALLOW_THREADS;
    SendIO(ioreq);
    sigs = Wait(SIGBREAKF_CTRL_C | psig);
    Py_END_ALLOW_THREADS;

    if (sigs & psig)
    {
        GetMsg(port);
        return 0;
    }
    
    if (!CheckIO(ioreq))
    {
        AbortIO(ioreq);
        Py_BEGIN_ALLOW_THREADS;
        WaitIO(ioreq);
        Py_END_ALLOW_THREADS;
        //GetMsg(port); /* XXX: needed ? */
    }

    return -1;
}
//-


/*******************************************************************************************
 ** PyHeliosDevice_Type
 */

//+ ohcidev_new
static PyObject *
ohcidev_new(PyTypeObject *type, PyObject *args)
{
    PyHeliosOHCIDevice *self;
    ULONG unit=0;
    struct MsgPort *port;
    IOHeliosHWReq *ioreq;

    if (!PyArg_ParseTuple(args, "|I", &unit))
        return NULL;

    port = CreateMsgPort();
    if (NULL != port)
    {
        ioreq = (IOHeliosHWReq *) CreateExtIO(port, sizeof(IOHeliosHWReq));
        if (NULL != ioreq)
        {
            DB("MPort: %p, IOReq: %p\n", port, ioreq);

            if (!OpenDevice("Helios/ohci1394_pci.device", unit, (struct IORequest *)ioreq, 0))
            {
                self = (PyHeliosOHCIDevice *) type->tp_alloc(type, 0);
                if (NULL != self)
                {
                    self->Port = port;
                    self->IOReq = ioreq;

                    return (PyObject *)self;
                }
            }
            else
                PyErr_Format(PyExc_SystemError, "OpenDevice(\"Helios/ohci1394_pci.device\", %lu) failed", unit);

            DeleteExtIO((struct IORequest *)ioreq);
        }
        else
            PyErr_SetString(PyExc_SystemError, "CreateExtIO() failed");

        DeleteMsgPort(port);
    }
    else
        PyErr_SetString(PyExc_SystemError, "CreateMsgPort() failed");

    return NULL;
}
//-
//+ ohcidev_dealloc
static void
ohcidev_dealloc(PyHeliosOHCIDevice *self)
{
    struct Message *oldm, *msg;

    DB("Closing device...\n");
    CloseDevice(&self->IOReq->iohh_Req);

    DB("Delete IOReq...\n");
    DeleteExtIO(&self->IOReq->iohh_Req);

    DB("Flushing and delete MPort...\n");
    oldm = NULL;
    while (NULL != (msg = GetMsg(self->Port)))
    {
        if (msg == oldm)
        {
            DB("BUG: msg linked on itself!\n");
            break;
        }
        
        oldm = msg;
    }
    DeleteMsgPort(self->Port);

    self->ob_type->tp_free((PyObject *) self);
}
//-

//+ ohcidev_BusReset
static PyObject *
ohcidev_BusReset(PyHeliosOHCIDevice *self, PyObject *args)
{
    IOHeliosHWReq ioreq;
    ULONG isshort = TRUE;
    
    if (!PyArg_ParseTuple(args, "|i", &isshort))
        return NULL;
    
    copy_ioreq(self->IOReq, &ioreq, sizeof(ioreq));

    ioreq.iohh_Req.io_Command = HHIOCMD_BUSRESET;
    ioreq.iohh_Data = (APTR)isshort;

    if (do_ioreq(&ioreq.iohh_Req))
    {
        PyErr_SetInterrupt();
        return NULL;
    }

    DB("err=%d\n", ioreq.iohh_Req.io_Error);
    if (HHIOERR_NO_ERROR != ioreq.iohh_Req.io_Error)
        return PyErr_Format(PyExc_IOError, "BusReset failed");

    Py_RETURN_NONE;
}
//-
//+ ohcidev_WritePHY
static PyObject *
ohcidev_WritePHY(PyHeliosOHCIDevice *self, PyObject *args)
{
    QUADLET data;
    IOHeliosHWReq ioreq;
    
    if (!PyArg_ParseTuple(args, "k", &data))
        return NULL;
    
    copy_ioreq(self->IOReq, &ioreq, sizeof(ioreq));

    ioreq.iohh_Req.io_Command = HHIOCMD_SENDPHY;
    ioreq.iohh_Data = (APTR)data;

    if (do_ioreq(&ioreq.iohh_Req))
    {
        PyErr_SetInterrupt();
        return NULL;
    }

    DB("err=%d\n", ioreq.iohh_Req.io_Error);
    if (HHIOERR_NO_ERROR != ioreq.iohh_Req.io_Error)
        return PyErr_Format(PyExc_IOError, "WritePHY failed, err=%d, ack=%ld",
                            ioreq.iohh_Req.io_Error, ioreq.iohh_Actual);

    Py_RETURN_NONE;
}
//-
//+ ohcidev_ReadQ
static PyObject *
ohcidev_ReadQ(PyHeliosOHCIDevice *self, PyObject *args)
{
    IOHeliosHWSendRequest ioreq;
    HeliosAPacket *p;
    UWORD destid;
    HeliosOffset offset;
    QUADLET q = 0;
    int speed = S100;
    
    if (!PyArg_ParseTuple(args, "HK|", &destid, &offset, &speed))
        return NULL;

    p = &ioreq.iohhe_Transaction.htr_Packet;
    Helios_FillReadQuadletPacket(p, speed & 3, offset);
    p->DestID = (destid & 0x3f) | HELIOS_LOCAL_BUS;
    
    copy_ioreq(self->IOReq, &ioreq.iohhe_Req, sizeof(ioreq));
    ioreq.iohhe_Req.iohh_Req.io_Command = HHIOCMD_SENDREQUEST;
    ioreq.iohhe_Device = NULL; /* use direct call */
    ioreq.iohhe_Req.iohh_Data = &q;
    ioreq.iohhe_Req.iohh_Length = 4;

    if (do_ioreq(&ioreq.iohhe_Req.iohh_Req))
    {
        PyErr_SetInterrupt();
        return NULL;
    }

    DB("err=%d\n", ioreq.iohhe_Req.iohh_Req.io_Error);
    return Py_BuildValue("kbi", q, p->RCode, ioreq.iohhe_Req.iohh_Req.io_Error);
}
//-
//+ ohcidev_Read
static PyObject *
ohcidev_Read(PyHeliosOHCIDevice *self, PyObject *args)
{
    IOHeliosHWSendRequest ioreq;
    HeliosAPacket *p;
    UWORD destid, len;
    HeliosOffset offset;
    PyObject *buffer;
    int speed = S100;
    
    if (!PyArg_ParseTuple(args, "HKH|i", &destid, &offset, &len, &speed))
        return NULL;

    buffer = PyString_FromStringAndSize(NULL, len);
    if (NULL == buffer)
        return NULL;

    memset(PyString_AS_STRING(buffer), 0, len);

    p = &ioreq.iohhe_Transaction.htr_Packet;
    Helios_FillReadBlockPacket(p, speed & 3, offset, len);
    p->DestID = (destid & 0x3f) | HELIOS_LOCAL_BUS;
    
    copy_ioreq(self->IOReq, &ioreq.iohhe_Req, sizeof(ioreq));
    ioreq.iohhe_Req.iohh_Req.io_Command = HHIOCMD_SENDREQUEST;
    ioreq.iohhe_Device = NULL; /* use direct call */
    ioreq.iohhe_Req.iohh_Data = PyString_AS_STRING(buffer);
    ioreq.iohhe_Req.iohh_Length = len;

    if (do_ioreq(&ioreq.iohhe_Req.iohh_Req))
    {
        PyErr_SetInterrupt();
        return NULL;
    }

    DB("err=%d\n", ioreq.iohhe_Req.iohh_Req.io_Error);
    return Py_BuildValue("Nbi", buffer, p->RCode, ioreq.iohhe_Req.iohh_Req.io_Error);
}
//-
//+ ohcidev_WriteQ
static PyObject *
ohcidev_WriteQ(PyHeliosOHCIDevice *self, PyObject *args)
{
    IOHeliosHWSendRequest ioreq;
    HeliosAPacket *p;
    UWORD destid;
    HeliosOffset offset;
    QUADLET q;
    int speed = S100;

    if (!PyArg_ParseTuple(args, "HKk|i", &destid, &offset, &q, &speed))
        return NULL;

    p = &ioreq.iohhe_Transaction.htr_Packet;
    Helios_FillWriteQuadletPacket(p, speed & 3, offset, q);
    p->DestID = (destid & 0x3f) | HELIOS_LOCAL_BUS;
    
    copy_ioreq(self->IOReq, &ioreq.iohhe_Req, sizeof(ioreq));
    ioreq.iohhe_Req.iohh_Req.io_Command = HHIOCMD_SENDREQUEST;
    ioreq.iohhe_Device = NULL; /* use direct call */
    ioreq.iohhe_Req.iohh_Data = NULL;

    if (do_ioreq(&ioreq.iohhe_Req.iohh_Req))
    {
        PyErr_SetInterrupt();
        return NULL;
    }

    DB("err=%d\n", ioreq.iohhe_Req.iohh_Req.io_Error);
    return Py_BuildValue("bi", p->RCode, ioreq.iohhe_Req.iohh_Req.io_Error);
}
//-

PyDoc_STRVAR(PyHeliosOHCIDevice_Type__doc__,
             "Helios OHCI Device Object."
             "\nThis is direct Python wrapper type on a Helios OHCI-PCI device."
             "\nGive a unit number as first parameter.");

//+ ohcidev_methods
static struct PyMethodDef ohcidev_methods[] =
{
    ADDVAFUNC("BusReset", ohcidev_BusReset, "BusReset(short=True) -> None"
              "\n\nCause a BusReset on the bus, short if 'short' is True else long."),
    ADDVAFUNC("WritePHY", ohcidev_WritePHY, "WritePHY(phy_data) -> status."
              "\n\nSend a PHY packet on the bus."
              "\nReturn the ack status."),
    ADDVAFUNC("ReadQ", ohcidev_ReadQ, "ReadQ(destid, offset, speed=S100) -> (value, rcode, err)"),
    ADDVAFUNC("Read", ohcidev_Read, "Read(destid, offset, length, speed=S100) -> (data, rcode, err)"),
    ADDVAFUNC("WriteQ", ohcidev_WriteQ, "WriteQ(destid, offset, quadlet, speed=S100) -> (rcode, err)"),
    {NULL} /* Sentinel */
};
//-

//+ PyHeliosDevice_Type
static PyTypeObject PyHeliosOHCIDevice_Type =
{
    PyObject_HEAD_INIT(NULL)

    tp_name         : "helios.OHCIDevice",
    tp_basicsize    : sizeof(PyHeliosOHCIDevice_Type),
    tp_flags        : Py_TPFLAGS_DEFAULT,
    tp_doc          : PyHeliosOHCIDevice_Type__doc__,

    tp_new          : (newfunc) ohcidev_new,
    tp_dealloc      : (destructor) ohcidev_dealloc,

    tp_methods      : ohcidev_methods,
};
//- 

/*
** Module Functions
**
** List of functions exported by this module reside here
*/

static PyMethodDef module_methods[] = {
    {NULL, NULL} /* Sentinel */
};

PyDoc_STRVAR(module__doc__, "This module provides a complete binding to the IEEE1394 MorphOS stack, Helios.");


/*
** Public Functions
*/

//+ PyMorphOS_CloseModule
void
PyMorphOS_TermModule(void) {
    DB("Closing module...\n");

    if (NULL != HeliosBase)
    {
        CloseLibrary(HeliosBase);
        HeliosBase = NULL;
    }

    DB("Bye\n");
}
//- PyMorphOS_CloseModule
//+ INITFUNC()
PyMODINIT_FUNC
INITFUNC(void)
{
    PyObject *m;

    HeliosBase = OpenLibrary("helios.library", 0);
    if (!HeliosBase) return;

    if (PyType_Ready(&PyHeliosOHCIDevice_Type) < 0) return;

    /* Module creation/initialization */
    m = Py_InitModule3(MODNAME, module_methods, module__doc__);

    if (all_ins(m)) return;

    ADD_TYPE(m, "OHCIDev", &PyHeliosOHCIDevice_Type);
}
//-
