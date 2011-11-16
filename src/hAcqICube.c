/** \file hAcqIcube.c
 * \brief Linux HALCON acquisition interface for NET iCube.
 * \author Aaron Mavrinac <mavrin1@uwindsor.ca>
 */

#define INTERFACE_REVISION "4.0"

#include <time.h>
#include <stdlib.h>
#include <strings.h>
#include <pthread.h>

#include <Halcon.h>
#include <hlib/CIOFrameGrab.h>

#include "netusbcamextra.h"
#include <NETUSBCAM_API.h>

#define FG_PARAM_INDEX "index"
#define FG_PARAM_GRAB_TIMEOUT "grab_timeout"
#define FG_PARAM_EXPOSURE_TIME "exposure_time"
#define FG_PARAM_EXPOSURE_AUTO "exposure_auto"
#define FG_PARAM_EXPOSURE_TARGET "exposure_target"
#define FG_PARAM_EXPOSURE_MSEC "exposure_msec"

#define FG_PARAM_GRAB_TIMEOUT_RANGE "grab_timeout_range"
#define FG_PARAM_EXPOSURE_TIME_RANGE "exposure_time_range"
#define FG_PARAM_EXPOSURE_TARGET_RANGE "exposure_target_range"

#define FG_PARAM_EXPOSURE_AUTO_VALUES "exposure_auto_values"

#define FG_PARAM_INDEX_DESCR "index_description"
#define FG_PARAM_GRAB_TIMEOUT_DESCR "grab_timeout_description"
#define FG_PARAM_EXPOSURE_TIME_DESCR "exposure_time_description"
#define FG_PARAM_EXPOSURE_AUTO_DESCR "exposure_auto_description"
#define FG_PARAM_EXPOSURE_TARGET_DESCR "exposure_target_description"
#define FG_PARAM_EXPOSURE_MSEC_DESCR "exposure_msec_description"

/* Use this macro to display error messages                               */
#define MY_PRINT_ERROR_MESSAGE(ERR) { \
    if (HDoLowError) IOPrintErrorMessage(ERR); }

extern HUserExport Herror FGInit(Hproc_handle proc_id, FGClass * fg);
extern HLibExport Herror IOPrintErrorMessage(char * err);

typedef struct
{
    INT index;
    INT grab_timeout;
    HBYTE * image;
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

#define GRAB_TIMEOUT_MAX 10000
#define GRAB_TIMEOUT_DEFAULT 1000

static pthread_mutex_t image_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t image_ready = PTHREAD_COND_INITIALIZER;

static INT ImageComplete(void * buffer, UINT bsize, void * context)
{
    pthread_mutex_lock(&image_mutex);

    memcpy(context, buffer, bsize);

    pthread_cond_signal(&image_ready);
    pthread_mutex_unlock(&image_mutex);

    return 0;
}

static INT AllocateImage(FGInstance * fginst)
{
    TFGInstance * currInst = (TFGInstance *)fginst->gen_pointer;
    void * ptr;

    NETUSBCAM_GetResolution(currInst->index, &fginst->image_width, &fginst->image_height, &fginst->start_col, &fginst->start_row);

    if(currInst->image)
    {
        ptr = realloc(currInst->image, fginst->image_width * fginst->image_height * sizeof(HBYTE));
        if(ptr)
            currInst->image = (HBYTE *)ptr;
        else
            return 1;
    }
    else
    {
        currInst->image = (HBYTE *)malloc(fginst->image_width * fginst->image_height * sizeof(HBYTE));
    }
    
    return 0;
}

static Herror FGOpen(Hproc_handle proc_id, FGInstance * fginst)
{
    TFGInstance * currInst = (TFGInstance *)fginst->gen_pointer;
    INT i;
    UINT mode, nmodes = NUM_MODES;
    UINT modes[NUM_MODES];

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
                fginst->horizontal_resolution = modelist[modes[i]][0];
                fginst->vertical_resolution = modelist[modes[i]][1];
                break;
            }
        }
    }
    else
    {
        NETUSBCAM_GetMode(currInst->index, &mode);
        fginst->horizontal_resolution = modelist[mode][0];
        fginst->vertical_resolution = modelist[mode][1];
    }

    if(fginst->image_width && fginst->image_height)
    {
        if(NETUSBCAM_SetResolution(currInst->index, fginst->image_width, fginst->image_height, fginst->start_col, fginst->start_row) != 0)
        {
            MY_PRINT_ERROR_MESSAGE("setting ROI failed")
        }
    }

    if(AllocateImage(fginst) != 0)
        return H_ERR_MEM;

    if(fginst->external_trigger)
        NETUSBCAM_SetTrigger(currInst->index, 2);
    else
    {
        NETUSBCAM_SetTrigger(currInst->index, 3);
        NETUSBCAM_SetTrigger(currInst->index, 0);
    }

    NETUSBCAM_SetCallback(currInst->index, CALLBACK_RAW, &ImageComplete, (void *)currInst->image);

    if(NETUSBCAM_Start(currInst->index) != 0)
    {
        MY_PRINT_ERROR_MESSAGE("start camera failed")
        return H_ERR_FGF;
    }

    return H_MSG_OK;
}

static Herror FGClose(Hproc_handle proc_id, FGInstance * fginst)
{
    TFGInstance * currInst = (TFGInstance *)fginst->gen_pointer;

    if(NETUSBCAM_Stop(currInst->index) != 0)
    {
        MY_PRINT_ERROR_MESSAGE("stop camera failed")
        return H_ERR_FGF;
    }

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
    INT save, to;
    struct timespec timeout;

    HReadSysComInfo(proc_id, HGInitNewImage, &save);
    HWriteSysComInfo(proc_id, HGInitNewImage, FALSE);
    *num_image = 1;
    err = HNewImage(proc_id, &image[0], BYTE_IMAGE, fginst->image_width, fginst->image_height);
    if(err != H_MSG_OK)
    {
        HWriteSysComInfo(proc_id, HGInitNewImage, save);
        return err;
    }

    pthread_mutex_lock(&image_mutex);
    NETUSBCAM_SetTrigger(currInst->index, 1);
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += currInst->grab_timeout / 1000;
    timeout.tv_nsec += (long)(currInst->grab_timeout % 1000) * 1000000L;
    timeout.tv_sec += timeout.tv_nsec / 1000000000L;
    timeout.tv_nsec %= 1000000000L;
    to = pthread_cond_timedwait(&image_ready, &image_mutex, &timeout);
    if(!to)
        memcpy((void *)image[0].pixel.b, (void *)currInst->image, fginst->image_width * fginst->image_height);
    pthread_mutex_unlock(&image_mutex);

    if(to)
        return H_ERR_FGTIMEOUT;

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
            HCkP(HAlloc(proc_id, (size_t)(6 * sizeof(Hcpar)), &val));
            val[0].par.s = FG_PARAM_INDEX;
            val[1].par.s = FG_PARAM_GRAB_TIMEOUT;
            val[2].par.s = FG_PARAM_EXPOSURE_TIME;
            val[3].par.s = FG_PARAM_EXPOSURE_AUTO;
            val[4].par.s = FG_PARAM_EXPOSURE_TARGET;
            val[5].par.s = FG_PARAM_EXPOSURE_MSEC;
            for(i = 0; i < 6; i++)
                val[i].type = STRING_PAR;
            *values = val;
            *numValues = 6;
            break;
        case FG_QUERY_PARAMETERS_RO:
            *info = "Additional read-only parameters for this interface.";
            HCkP(HAlloc(proc_id, (size_t)(2 * sizeof(Hcpar)), &val));
            val[0].par.s = FG_PARAM_INDEX;
            val[1].par.s = FG_PARAM_EXPOSURE_MSEC;
            for(i = 0; i < 2; i++)
                val[i].type = STRING_PAR;
            *values = val;
            *numValues = 2;
            break;
        case FG_QUERY_PARAMETERS_WO:
            *info = "Additional write-only parameters for this interface.";
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
        case FG_QUERY_EXT_TRIGGER:
            *info = "Value list for ExternalTrigger parameter.";
            HCkP(HAlloc(proc_id, (size_t)(2 * sizeof(*val)), &val));
            val[0].par.s = "false";
            val[0].type = STRING_PAR;
            val[1].par.s = "true";
            val[1].type = STRING_PAR;
            *values = val;
            *numValues = 2;
            break;
        case FG_QUERY_HORIZONTAL_RESOLUTION:
            *info = "Supported HorizontalResolution values.";
            HCkP(HAlloc(proc_id, (size_t)(NUM_MODES * sizeof(*val)), &val));
            for(i = 0; i < NUM_MODES; i++)
            {
                val[i].par.l = modelist[i][0];
                val[i].type = LONG_PAR;
            }
            *values = val;
            *numValues = NUM_MODES;
            break;
        case FG_QUERY_VERTICAL_RESOLUTION:
            *info = "Supported VerticalResolution values.";
            HCkP(HAlloc(proc_id, (size_t)(NUM_MODES * sizeof(*val)), &val));
            for(i = 0; i < NUM_MODES; i++)
            {
                val[i].par.l = modelist[i][1];
                val[i].type = LONG_PAR;
            }
            *values = val;
            *numValues = NUM_MODES;
            break;
        case FG_QUERY_INFO_BOARDS:
        case FG_QUERY_BITS_PER_CHANNEL:
        case FG_QUERY_COLOR_SPACE:
        case FG_QUERY_DEVICE:
        case FG_QUERY_FIELD:
        case FG_QUERY_GENERIC:
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
    HBOOL ok;
    INT i;
    UINT nmodes = NUM_MODES;
    UINT modes[NUM_MODES];
    PARAM_PROPERTY param_property;
    ROI_RANGE_PROPERTY roi_range_property;

    if(!strcasecmp(param, FG_PARAM_HORIZONTAL_RESOLUTION))
    {
        if(value->type != LONG_PAR)
            return H_ERR_FGPART;
        if(value->par.l == fginst->horizontal_resolution)
            return H_MSG_OK;
        if(NETUSBCAM_Stop(currInst->index) != 0)
        {
            MY_PRINT_ERROR_MESSAGE("stop camera failed")
            return H_ERR_FGSETPAR;
        }
        ok = FALSE;
        NETUSBCAM_GetModeList(currInst->index, &nmodes, modes);
        for(i = 0; i < nmodes; i++)
        {
            if(value->par.l == modelist[modes[i]][0])
            {
                NETUSBCAM_SetMode(currInst->index, modes[i]);
                fginst->horizontal_resolution = modelist[modes[i]][0];
                fginst->vertical_resolution = modelist[modes[i]][1];
                ok = TRUE;
                break;
            }
        }
        if(!ok)
            return H_ERR_FGPARV;
        if(AllocateImage(fginst) != 0)
            return H_ERR_MEM;
        if(NETUSBCAM_Start(currInst->index) != 0)
        {
            MY_PRINT_ERROR_MESSAGE("restart camera failed")
            return H_ERR_FGSETPAR;
        }
    }
    else if(!strcasecmp(param, FG_PARAM_VERTICAL_RESOLUTION))
    {
        if(value->type != LONG_PAR)
            return H_ERR_FGPART;
        if(value->par.l == fginst->vertical_resolution)
            return H_MSG_OK;
        if(NETUSBCAM_Stop(currInst->index) != 0)
        {
            MY_PRINT_ERROR_MESSAGE("stop camera failed")
            return H_ERR_FGSETPAR;
        }
        ok = FALSE;
        NETUSBCAM_GetModeList(currInst->index, &nmodes, modes);
        for(i = 0; i < nmodes; i++)
        {
            if(value->par.l == modelist[modes[i]][1])
            {
                NETUSBCAM_SetMode(currInst->index, modes[i]);
                fginst->horizontal_resolution = modelist[modes[i]][0];
                fginst->vertical_resolution = modelist[modes[i]][1];
                ok = TRUE;
                break;
            }
        }
        if(!ok)
            return H_ERR_FGPARV;
        if(AllocateImage(fginst) != 0)
            return H_ERR_MEM;
        if(NETUSBCAM_Start(currInst->index) != 0)
        {
            MY_PRINT_ERROR_MESSAGE("restart camera failed")
            return H_ERR_FGSETPAR;
        }
    }
    else if(!strcasecmp(param, FG_PARAM_IMAGE_WIDTH))
    {
        if(value->type != LONG_PAR)
            return H_ERR_FGPART;
        if(value->par.l == fginst->image_width)
            return H_MSG_OK;
        if(NETUSBCAM_Stop(currInst->index) != 0)
        {
            MY_PRINT_ERROR_MESSAGE("stop camera failed")
            return H_ERR_FGSETPAR;
        }
        NETUSBCAM_GetResolutionRange(currInst->index, &roi_range_property);
        if(value->par.l < roi_range_property.nXMin || value->par.l > roi_range_property.nXMax)
            return H_ERR_FGPARV;
        fginst->image_width = value->par.l;
        if(NETUSBCAM_SetResolution(currInst->index, fginst->image_width, fginst->image_height, fginst->start_col, fginst->start_row) != 0)
            return H_ERR_FGSETPAR;
        if(AllocateImage(fginst) != 0)
            return H_ERR_MEM;
        if(NETUSBCAM_Start(currInst->index) != 0)
        {
            MY_PRINT_ERROR_MESSAGE("restart camera failed")
            return H_ERR_FGSETPAR;
        }
    }
    else if(!strcasecmp(param, FG_PARAM_IMAGE_HEIGHT))
    {
        if(value->type != LONG_PAR)
            return H_ERR_FGPART;
        if(value->par.l == fginst->image_height)
            return H_MSG_OK;
        if(NETUSBCAM_Stop(currInst->index) != 0)
        {
            MY_PRINT_ERROR_MESSAGE("stop camera failed")
            return H_ERR_FGSETPAR;
        }
        NETUSBCAM_GetResolutionRange(currInst->index, &roi_range_property);
        if(value->par.l < roi_range_property.nYMin || value->par.l > roi_range_property.nYMax)
            return H_ERR_FGPARV;
        fginst->image_height = value->par.l;
        if(NETUSBCAM_SetResolution(currInst->index, fginst->image_width, fginst->image_height, fginst->start_col, fginst->start_row) != 0)
            return H_ERR_FGSETPAR;
        if(AllocateImage(fginst) != 0)
            return H_ERR_MEM;
        if(NETUSBCAM_Start(currInst->index) != 0)
        {
            MY_PRINT_ERROR_MESSAGE("restart camera failed")
            return H_ERR_FGSETPAR;
        }
    }
    else if(!strcasecmp(param, FG_PARAM_GRAB_TIMEOUT))
    {
        if(value->type != LONG_PAR)
            return H_ERR_FGPART;
        if(value->par.l < 0 || value->par.l > GRAB_TIMEOUT_MAX)
            return H_ERR_FGPARV;
        currInst->grab_timeout = value->par.l;
    }
    else if(!strcasecmp(param, FG_PARAM_EXPOSURE_TIME))
    {
        if(value->type != LONG_PAR)
            return H_ERR_FGPART;
        NETUSBCAM_GetParamAuto(currInst->index, REG_EXPOSURE_TIME, &i);
        if(i)
            return H_ERR_FGPARNA;
        NETUSBCAM_GetCamParameterRange(currInst->index, REG_EXPOSURE_TIME, &param_property);
        if(value->par.l < param_property.nMin || value->par.l > param_property.nMax)
            return H_ERR_FGPARV;
        if(NETUSBCAM_SetCamParameter(currInst->index, REG_EXPOSURE_TIME, (unsigned long)value->par.l) != 0)
            return H_ERR_FGSETPAR;
    }
    else if(!strcasecmp(param, FG_PARAM_EXPOSURE_AUTO))
    {
        if(value->type != STRING_PAR)
            return H_ERR_FGPART;
        if(!strcasecmp(value->par.s, "true"))
        {
            if(NETUSBCAM_SetParamAuto(currInst->index, REG_EXPOSURE_TIME, 1) != 0)
                return H_ERR_FGSETPAR;
        }
        else if(!strcasecmp(value->par.s, "false"))
        {
            if(NETUSBCAM_SetParamAuto(currInst->index, REG_EXPOSURE_TIME, 0) != 0)
                return H_ERR_FGSETPAR;
        }
        else
            return H_ERR_FGPARV;
    }
    else if(!strcasecmp(param, FG_PARAM_EXPOSURE_TARGET))
    {
        if(value->type != LONG_PAR)
            return H_ERR_FGPART;
        NETUSBCAM_GetCamParameterRange(currInst->index, REG_EXPOSURE_TARGET, &param_property);
        if(value->par.l < param_property.nMin || value->par.l > param_property.nMax)
            return H_ERR_FGPARV;
        if(NETUSBCAM_SetCamParameter(currInst->index, REG_EXPOSURE_TARGET, (unsigned long)value->par.l) != 0)
            return H_ERR_FGSETPAR;
    }
    else
        return H_ERR_FGPARAM;

    return H_MSG_OK;
}

static Herror FGGetParam(Hproc_handle proc_id, FGInstance * fginst, char * param, Hcpar * value, INT * num)
{
    TFGInstance * currInst = (TFGInstance *)fginst->gen_pointer;
    INT i;
    float f;
    unsigned long ul;
    PARAM_PROPERTY param_property;

    *num = 1;

    if(!strcasecmp(param, FG_PARAM_HORIZONTAL_RESOLUTION))
    {
        value->type = LONG_PAR;
        value->par.l = fginst->horizontal_resolution;
    }
    else if(!strcasecmp(param, FG_PARAM_VERTICAL_RESOLUTION))
    {
        value->type = LONG_PAR;
        value->par.l = fginst->vertical_resolution;
    }
    else if(!strcasecmp(param, FG_PARAM_IMAGE_WIDTH))
    {
        value->type = LONG_PAR;
        value->par.l = fginst->image_width;
    }
    else if(!strcasecmp(param, FG_PARAM_IMAGE_HEIGHT))
    {
        value->type = LONG_PAR;
        value->par.l = fginst->image_height;
    }
    else if(!strcasecmp(param, FG_PARAM_START_COL))
    {
        value->type = LONG_PAR;
        value->par.l = fginst->start_col;
    }
    else if(!strcasecmp(param, FG_PARAM_START_ROW))
    {
        value->type = LONG_PAR;
        value->par.l = fginst->start_row;
    }
    else if(!strcasecmp(param, FG_PARAM_INDEX))
    {
        value->type = LONG_PAR;
        value->par.l = currInst->index;
    }
    else if(!strcasecmp(param, FG_PARAM_GRAB_TIMEOUT))
    {
        value->type = LONG_PAR;
        value->par.l = currInst->grab_timeout;
    }
    else if(!strcasecmp(param, FG_PARAM_EXPOSURE_TIME))
    {
        value->type = LONG_PAR;
        if(NETUSBCAM_GetCamParameter(currInst->index, REG_EXPOSURE_TIME, &ul) != 0)
            return H_ERR_FGGETPAR;
        value->par.l = ul;
    }
    else if(!strcasecmp(param, FG_PARAM_EXPOSURE_AUTO))
    {
        value->type = STRING_PAR;
        if(NETUSBCAM_GetParamAuto(currInst->index, REG_EXPOSURE_TIME, &i) != 0)
            return H_ERR_FGGETPAR;
        value->par.s = i ? "true" : "false";
    }
    else if(!strcasecmp(param, FG_PARAM_EXPOSURE_TARGET))
    {
        value->type = LONG_PAR;
        if(NETUSBCAM_GetCamParameter(currInst->index, REG_EXPOSURE_TARGET, &ul) != 0)
            return H_ERR_FGGETPAR;
        value->par.l = ul;
    }
    else if(!strcasecmp(param, FG_PARAM_EXPOSURE_MSEC))
    {
        value->type = FLOAT_PAR;
        if(NETUSBCAM_GetExposure(currInst->index, &f) != 0)
            return H_ERR_FGGETPAR;
        value->par.f = f;
    }
    else if(!strcasecmp(param, FG_PARAM_GRAB_TIMEOUT_RANGE))
    {
        for(i = 0; i < 4; i++)
            value[i].type = LONG_PAR;
        value[0].par.l = 0;
        value[1].par.l = GRAB_TIMEOUT_MAX;
        value[2].par.l = 10;
        value[3].par.l = GRAB_TIMEOUT_DEFAULT;
        *num = 4;
    }
    else if(!strcasecmp(param, FG_PARAM_EXPOSURE_TIME_RANGE))
    {
        if(NETUSBCAM_GetCamParameterRange(currInst->index, REG_EXPOSURE_TIME, &param_property) != 0)
            return H_ERR_FGGETPAR;
        for(i = 0; i < 4; i++)
            value[i].type = LONG_PAR;
        value[0].par.l = param_property.nMin;
        value[1].par.l = param_property.nMax;
        value[2].par.l = 1;
        value[3].par.l = param_property.nDef;
        *num = 4;
    }
    else if(!strcasecmp(param, FG_PARAM_EXPOSURE_TARGET_RANGE))
    {
        if(NETUSBCAM_GetCamParameterRange(currInst->index, REG_EXPOSURE_TARGET, &param_property) != 0)
            return H_ERR_FGGETPAR;
        for(i = 0; i < 4; i++)
            value[i].type = LONG_PAR;
        value[0].par.l = param_property.nMin;
        value[1].par.l = param_property.nMax;
        value[2].par.l = 1;
        value[3].par.l = param_property.nDef;
        *num = 4;
    }
    else if(!strcasecmp(param, FG_PARAM_EXPOSURE_AUTO_VALUES))
    {
        value[0].par.s = "false";
        value[0].type = STRING_PAR;
        value[1].par.s = "true";
        value[1].type = STRING_PAR;
        *num = 2;
    }
    else if(!strcasecmp(param, FG_PARAM_INDEX_DESCR))
    {
        value->type = STRING_PAR;
        value->par.s = "Camera device index.";
    }
    else if(!strcasecmp(param, FG_PARAM_GRAB_TIMEOUT_DESCR))
    {
        value->type = STRING_PAR;
        value->par.s = "Grab timeout in milliseconds.";
    }
    else if(!strcasecmp(param, FG_PARAM_EXPOSURE_TIME_DESCR))
    {
        value->type = STRING_PAR;
        value->par.s = "Exposure time.";
    }
    else if(!strcasecmp(param, FG_PARAM_EXPOSURE_AUTO_DESCR))
    {
        value->type = STRING_PAR;
        value->par.s = "Toggle automatic exposure.";
    }
    else if(!strcasecmp(param, FG_PARAM_EXPOSURE_TARGET_DESCR))
    {
        value->type = STRING_PAR;
        value->par.s = "Target brightness for automatic exposure.";
    }
    else if(!strcasecmp(param, FG_PARAM_EXPOSURE_MSEC_DESCR))
    {
        value->type = STRING_PAR;
        value->par.s = "Exposure time in milliseconds.";
    }
    else
        return H_ERR_FGPARAM;

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
    {
        FGInst[i].index = i;
        FGInst[i].grab_timeout = GRAB_TIMEOUT_DEFAULT;
        FGInst[i].image = NULL;
    }

    num_devices = NETUSBCAM_Init();
    if(num_devices <= 0)
    {
        MY_PRINT_ERROR_MESSAGE("no camera detected")
        return H_ERR_FGNI;
    }

    return H_MSG_OK;
}
