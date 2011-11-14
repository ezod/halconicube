/** \file hAcqIcube.c
 * \brief Linux HALCON acquisition interface for NET iCube.
 * \author Aaron Mavrinac <mavrin1@uwindsor.ca>
 */

#define INTERFACE_REVISION "4.0"

#include <time.h>
#include <stdio.h>
#include <strings.h>
#include <pthread.h>

#include <Halcon.h>
#include <hlib/CIOFrameGrab.h>

#include "NETUSBCAM_API.h"

/* Use this macro to display error messages                               */
#define MY_PRINT_ERROR_MESSAGE(ERR) { \
    if (HDoLowError) IOPrintErrorMessage(ERR); }
static char errMsg[MAX_STRING];

#define STR_CASE_CMP(S1,S2)   strcasecmp(S1,S2)

extern HUserExport Herror FGInit(Hproc_handle proc_id, FGClass * fg);
extern HLibExport Herror IOPrintErrorMessage(char * err);

typedef struct
{
    INT index;
} TFGInstance;

static FGClass * fgClass;

static TFGInstance FGInst[FG_MAX_INST];
static INT num_instances = 0;
static INT num_devices = 0;

#define NUM_MODES 9
static int modelist[NUM_MODES][2] = {
    {320, 240},
    {640, 480},
    {752, 480},
    {800, 600},
    {1024, 768},
    {1280, 1024},
    {1600, 1200},
    {2048, 1536},
    {2592, 1944}
};

static pthread_mutex_t image_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t image_ready = PTHREAD_COND_INITIALIZER;

static INT ImageComplete(void * buffer, unsigned int bsize, void * context)
{
    pthread_mutex_lock(&image_mutex);

    memcpy(context, buffer, bsize);

    pthread_cond_signal(&image_ready);
    pthread_mutex_unlock(&image_mutex);

    return 0;
}

static Herror FGOpen(Hproc_handle proc_id, FGInstance * fginst)
{
    TFGInstance * currInst = (TFGInstance *)fginst->gen_pointer;
    INT i;
    unsigned int nmodes = NUM_MODES;
    unsigned int modes[NUM_MODES];

    if(fginst->port >= num_devices)
    {
        MY_PRINT_ERROR_MESSAGE("invalid device index")
        return H_ERR_FGNI;
    }
    currInst->index = fginst->port;
    if(NETUSBCAM_Open(currInst->index) != 0)
    {
        MY_PRINT_ERROR_MESSAGE("open camera failed")
        return H_ERR_FGNI;
    }
    num_instances++;

    if(fginst->horizontal_resolution > 0 && fginst->vertical_resolution > 0)
    {
        NETUSBCAM_GetModeList(currInst->index, &nmodes, modes);
        for(i = 0; i < nmodes; i++)
        {
            if(fginst->horizontal_resolution == modelist[modes[i]][0] && fginst->vertical_resolution == modelist[modes[i]][1])
            {
                NETUSBCAM_SetMode(currInst->index, modes[i]);
                break;
            }
        }
    }

    if(fginst->image_width && fginst->image_height)
    {
        if(NETUSBCAM_SetResolution(currInst->index, fginst->image_width, fginst->image_height, fginst->start_col, fginst->start_row) != 0)
        {
            MY_PRINT_ERROR_MESSAGE("setting ROI failed")
        }
    }

    return H_MSG_OK;
}

static Herror FGClose (Hproc_handle proc_id, FGInstance * fginst)
{
    TFGInstance * currInst = (TFGInstance *)fginst->gen_pointer;

    if(NETUSBCAM_Close(currInst->index) != 0)
    {
        MY_PRINT_ERROR_MESSAGE("close camera failed")
        return H_ERR_FGCLOSE;
    }
    num_instances--;

    return H_MSG_OK;
}

static Herror FGGrabStartAsync(Hproc_handle proc_id, FGInstance * fginst, double maxDelay)
{
    return H_ERR_FGASYNC;
}

static Herror FGGrab(Hproc_handle proc_id, FGInstance * fginst, Himage * image, INT * num_image)
{
    TFGInstance * currInst = (TFGInstance *)fginst->gen_pointer;
    Herror err;
    INT save, xres, yres, xpos, ypos;

    NETUSBCAM_GetResolution(currInst->index, &xres, &yres, &xpos, &ypos);

    pthread_mutex_lock(&image_mutex);

    HReadSysComInfo(proc_id, HGInitNewImage, &save);
    HWriteSysComInfo(proc_id, HGInitNewImage, FALSE);
    *num_image = 1;
    err = HNewImage(proc_id, &image[0], BYTE_IMAGE, xres, yres);
    if(err != H_MSG_OK)
    {
        HWriteSysComInfo(proc_id, HGInitNewImage, save);
        return err;
    }

    NETUSBCAM_SetCallback(currInst->index, CALLBACK_RAW, &ImageComplete, (void *)image[0].pixel.b);

    if(NETUSBCAM_Start(currInst->index) != 0)
    {
        MY_PRINT_ERROR_MESSAGE("start camera failed")
        return H_ERR_FGF;
    }

    pthread_cond_wait(&image_ready, &image_mutex);

    if(NETUSBCAM_Stop(currInst->index) != 0)
    {
        MY_PRINT_ERROR_MESSAGE("stop camera failed")
        return H_ERR_FGF;
    }

    pthread_mutex_unlock(&image_mutex);

    return H_MSG_OK;
}

static Herror FGGrabAsync(Hproc_handle proc_id, FGInstance * fginst, double maxDelay, Himage * image, INT * num_image)
{
    return H_ERR_FGASYNC;
}

static Herror FGInfo(Hproc_handle proc_id, INT queryType, char ** info, Hcpar ** values, INT * numValues)
{
    Hcpar *val;
    INT i;

    switch(queryType)
    {
        case FG_QUERY_GENERAL:
            *info = "HALCON image acquisition interface for NET iCube.";
            *values = NULL;
            *numValues = 0;
            break;
        case FG_QUERY_DEFAULTS:
            *info = "Default values (as used for open_framegrabber).";
            HCkP(HFgGetDefaults(proc_id, fgClass, values, numValues));
            break;
        case FG_QUERY_PARAMETERS:
            *info = "Additional parameters for this image acquisition interface.";
            /* TODO: specify all parameter names and types */
            /*HCkP(HAlloc(proc_id, (size_t)(0 * sizeof(Hcpar)), &val));*/
            *values = NULL;
            *numValues = 0;
            break;
        case FG_QUERY_PARAMETERS_RO:
            *info = "Additional read-only parameters for this interface.";
            /* TODO: specify all parameter names and types */
            /*HCkP(HAlloc(proc_id, (size_t)(0 * sizeof(Hcpar)), &val));*/
            *values = NULL;
            *numValues = 0;
            break;
        case FG_QUERY_PARAMETERS_WO:
            *info = "Additional write-only parameters for this interface.";
            /* TODO: specify all parameter names and types */
            /*HCkP(HAlloc(proc_id, (size_t)(0 * sizeof(Hcpar)), &val));*/
            *values = NULL;
            *numValues = 0;
            break;
        case FG_QUERY_REVISION:
            *info = "Current interface revision.";
            HCkP(HAlloc(proc_id, (size_t)(sizeof(Hcpar)), &val));
            val[0].par.s = INTERFACE_REVISION;
            val[0].type = STRING_PAR;
            *values = val;
            *numValues = 1;
            break;
        case FG_QUERY_CAMERA_TYPE:
            *info = "Name of the camera description file or 'default'.";
            *values = NULL;
            *numValues = 0;
            break;
        case FG_QUERY_PORT:
            *info = "List of available camera device indices.";
            HCkP(HAlloc(proc_id, (size_t)(num_devices * sizeof(Hcpar)), &val));
            for(i = 0; i < num_devices; i++)
            {
                val[i].par.l = i;
                val[i].type = LONG_PAR;
            }
            *values = val;
            *numValues = num_devices;
            break;
        case FG_QUERY_INFO_BOARDS:
        case FG_QUERY_BITS_PER_CHANNEL:
        case FG_QUERY_COLOR_SPACE:
        case FG_QUERY_DEVICE:
        case FG_QUERY_EXT_TRIGGER:
        case FG_QUERY_FIELD:
        case FG_QUERY_GENERIC:
        case FG_QUERY_HORIZONTAL_RESOLUTION:
        case FG_QUERY_VERTICAL_RESOLUTION:
            *info = "Unused.";
            *values = NULL;
            *numValues = 0;
            break;
        default:
            *info = "Unsupported query!";
            *values = NULL;
            *numValues = 0;
    }

    return H_MSG_OK;
}

static Herror FGSetParam(Hproc_handle proc_id, FGInstance * fginst, char * param, Hcpar * value, INT num)
{
    TFGInstance * currInst = (TFGInstance *)fginst->gen_pointer;

    /* TODO: set parameters */
    /* else
        return H_ERR_FGPARAM; */

    return H_MSG_OK;
}

static Herror FGGetParam(Hproc_handle proc_id, FGInstance * fginst, char * param, Hcpar * value, INT * num)
{
    TFGInstance * currInst = (TFGInstance *)fginst->gen_pointer;

    *num = 1;

    /* TODO: set parameters */
    /* else
        return H_ERR_FGPARAM; */

    return H_MSG_OK;
}

static FGInstance ** FGOpenRequest(Hproc_handle proc_id, FGInstance * fginst)
{
    /* support only a single instance */
    fginst->gen_pointer = ( void * )&FGInst;
    return &( fgClass->instance[ 0 ] );
}

Herror FGInit(Hproc_handle proc_id, FGClass * fg)
{
    INT i;

    fg->interface_version = FG_INTERFACE_VERSION;

    fg->available = TRUE;
    fg->instances_num = 0;
    fg->instances_max = FG_MAX_INST;

    /* interface-specific functions */
    fg->OpenRequest = FGOpenRequest;
    fg->Open = FGOpen;
    fg->Close = FGClose;
    fg->Info = FGInfo;
    fg->Grab = FGGrab;
    fg->GrabStartAsync = FGGrabStartAsync;
    fg->GrabAsync = FGGrabAsync;
    fg->SetParam = FGSetParam;
    fg->GetParam = FGGetParam;

    fgClass = fg;

    for(i = 0; i < FG_MAX_INST; i++)
        FGInst[i].index = i;

    num_devices = NETUSBCAM_Init();
    if(num_devices <= 0)
    {
        MY_PRINT_ERROR_MESSAGE("no camera detected")
        return H_ERR_FGNI;
    }

    return H_MSG_OK;
}
