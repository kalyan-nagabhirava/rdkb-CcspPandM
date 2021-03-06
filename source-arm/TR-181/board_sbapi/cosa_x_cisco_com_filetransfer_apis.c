/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2015 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**********************************************************************
   Copyright [2014] [Cisco Systems, Inc.]
 
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
 
       http://www.apache.org/licenses/LICENSE-2.0
 
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
**********************************************************************/

/**************************************************************************

    module: cosa_FileTransfer_api.c

        For COSA Data Model Library Development

    -------------------------------------------------------------------

    copyright:

        Cisco Systems, Inc.
        All Rights Reserved.

    -------------------------------------------------------------------

    description:

        This file implementes back-end apis for the COSA Data Model Library

        *  CosaDmlDiInit
    -------------------------------------------------------------------

    environment:

        platform independent

    -------------------------------------------------------------------

    author:

        COSA XML TOOL CODE GENERATOR 1.0

    -------------------------------------------------------------------

    revision:

        01/11/2011    initial revision.

**************************************************************************/

#include "cosa_x_cisco_com_filetransfer_apis.h"
#include "cosa_x_cisco_com_filetransfer_internal.h"
#include "dmsb_tr181_psm_definitions.h"
#include <curl/curl.h>

/* Set up default tftp server table */
typedef struct
{
    COSA_DML_FILETRANSFER_SERVER    mStatus;
    char                  *mServer;
    char                  *mServerName;
} FILETRANSFER_SERVER_item;


FILETRANSFER_SERVER_item FileTransfer_TFTPServers[] = {
    {COSA_DML_FILETRANSFER_SERVER_NONE, "", ""},
    {COSA_DML_FILETRANSFER_SERVER_TFTP1, "tftpwest01.comcast.net", "TFTP Server 1"},
    {COSA_DML_FILETRANSFER_SERVER_TFTP2, "tftpwest02.comcast.net", "TFTP Server 2"}
};


ANSC_STATUS
CosaDmlFileTransferInit
    (
        ANSC_HANDLE                 hDml,
        PANSC_HANDLE                phContext
)
{
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
VerifyFileFormat (char* configFilePath)
{
    int formatVerifyStatus = FORMAT_VERIFY_SUCCESS;
    FILE* refFile = NULL;
    FILE* configFile = NULL;
    int i = 0;
    int totalRefLines = 0;
    int foundMatch = 0;
    char refStrings [MAX_STRING_COUNT][MAX_LINE_SIZE] = {{'\0'}};;
    char configLine [MAX_LINE_SIZE];
    char* configField;

    /* Reading reference file and populating array of refrence strings */

    refFile = fopen ( TRUE_STATIC_IP_CONFIG_REFERENCE_FILE, "r" );
    if ( refFile != NULL )
    {
        i = 0;
        while ((fgets (refStrings[i], sizeof refStrings[i], refFile) != NULL) && (i < MAX_STRING_COUNT))
        {
            refStrings[i][strcspn(refStrings[i], "\n")] = 0;
            i++;
        }
        totalRefLines = i;

        fclose(refFile);
    }
    else
    {
        AnscTraceWarning(("Cannot open reference file\n"));
        perror ( TRUE_STATIC_IP_CONFIG_REFERENCE_FILE );
        formatVerifyStatus = FORMAT_VERIFY_FAILURE;
    }

    /* Reading downloaded configuration file and verifying configuration field with refrence strings */
    configFile = fopen (configFilePath, "r");
    if ((configFile != NULL) && (formatVerifyStatus != FORMAT_VERIFY_FAILURE))
    {
        while (fgets (configLine, sizeof configLine, configFile) != NULL )
        {
            configField = strtok (configLine, "  \t");
            foundMatch = 0;

            for (i = 0; ((i < totalRefLines) && (foundMatch == 0)); i++)
            {
                if ((strcmp (refStrings[i], configField)) == 0 )
                {
                    foundMatch = 1;
                }
            }

            if (foundMatch == 0)
            {
                formatVerifyStatus = FORMAT_VERIFY_FAILURE;
                break;
            }
        }

        fclose (configFile);
    }
    else
    {
        AnscTraceWarning(("Cannot open config file\n"));
        formatVerifyStatus = FORMAT_VERIFY_FAILURE;
    }

    return formatVerifyStatus;
}

static
ANSC_STATUS
FileTransferTask
    (
        ANSC_HANDLE     hTaskContext
    )
{
    PCOSA_DATAMODEL_FILETRANSFER    pMyObject       = (PCOSA_DATAMODEL_FILETRANSFER)hTaskContext;
    PCOSA_DML_FILETRANSFER_CFG      pCfg            = (PCOSA_DML_FILETRANSFER_CFG  )&pMyObject->Cfg;
    char                            URL[64]         = {0};
    char                            Path[64]        = {0};
    FILE*                           fp              = NULL;
    CURL*                           curl            = NULL;
    CURLcode                        ret             = CURLE_OK;
    int                             formatVerifyStatus = FORMAT_VERIFY_FAILURE;

    ret  = curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if ( !curl ) 
    {
        AnscTraceWarning(("Curl library initialization error!\n"));
        return ANSC_STATUS_FAILURE;
    }

    if (pCfg->Server == COSA_DML_FILETRANSFER_SERVER_NONE)
    {
        AnscTraceWarning(("TFTP Server not set!\n"));
        return ANSC_STATUS_FAILURE;
    }

    _ansc_sprintf(URL, "tftp://%s/%s", FileTransfer_TFTPServers[pCfg->Server].mServer, pCfg->FileName);
    ret = curl_easy_setopt(curl, CURLOPT_URL, URL);

    _ansc_sprintf(Path, "%s%s", TRUE_STATIC_IP_CONFIG_PATH, TRUE_STATIC_IP_CONFIG_FILE);
    if ((fp = fopen(Path,"w+"))== NULL )
    {
        curl_easy_cleanup(curl);
        return ANSC_STATUS_FAILURE;
    }

    ret = curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    ret = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60);
    pMyObject->Status = COSA_DML_FILETRANSFER_STATUS_InProgress;
    ret = curl_easy_perform(curl);
    fclose(fp);

    if ( ret == CURLE_OK )
    {
        formatVerifyStatus = VerifyFileFormat (Path);
        if (formatVerifyStatus == FORMAT_VERIFY_SUCCESS)
        {
            pMyObject->Status = COSA_DML_FILETRANSFER_STATUS_Complete;
            AnscTraceWarning(("TFTP server %s[%s] responded and file [%s] downloaded\n", FileTransfer_TFTPServers[pCfg->Server].mServerName, FileTransfer_TFTPServers[pCfg->Server].mServer, pCfg->FileName));
        }
        else
        {
            pMyObject->Status = COSA_DML_FILETRANSFER_STATUS_IncorrectFileFormat;
            AnscTraceWarning(("TFTP server %s[%s] responded and file [%s] was found but the formatting in this file is not correct. File rejected\n", FileTransfer_TFTPServers[pCfg->Server].mServerName, FileTransfer_TFTPServers[pCfg->Server].mServer, pCfg->FileName));
            return ANSC_STATUS_FAILURE;
        }
    }
    else
    {
        printf("File download failure, error code = %d, error : %s\n", ret, curl_easy_strerror(ret));

        if ( ret == CURLE_OPERATION_TIMEDOUT )
        {
            AnscTraceWarning(("TFTP server at %s[%s] was not found or is not responding\n", FileTransfer_TFTPServers[pCfg->Server].mServerName, FileTransfer_TFTPServers[pCfg->Server].mServer));
            pMyObject->Status = COSA_DML_FILETRANSFER_STATUS_ServerNotFound;
        }
        else if ( ret == CURLE_TFTP_NOTFOUND )
        {
            AnscTraceWarning(("TFTP server %s[%s] responded but the requested file [%s] was not found on this server\n", FileTransfer_TFTPServers[pCfg->Server].mServerName, FileTransfer_TFTPServers[pCfg->Server].mServer, pCfg->FileName));
            pMyObject->Status = COSA_DML_FILETRANSFER_STATUS_FileNotFound;
        }
        else
        {
            AnscTraceWarning(("File [%s] transfer failed due to unknown reason from TFTP server %s[%s]\n",pCfg->FileName, FileTransfer_TFTPServers[pCfg->Server].mServerName, FileTransfer_TFTPServers[pCfg->Server].mServer));
            pMyObject->Status = COSA_DML_FILETRANSFER_STATUS_Failed;
        }
    }
 
    curl_easy_cleanup(curl);

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlFileTransferSetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_FILETRANSFER_CFG  pCfg
    )
{
    PCOSA_DATAMODEL_FILETRANSFER    pMyObject       = (PCOSA_DATAMODEL_FILETRANSFER)hContext;
    int                             iReturnValue    = CCSP_SUCCESS;
    char                            pParamPath[64]  = {0};
    unsigned int                    RecordType      = ccsp_string;
    char                            RecordValue[64] = {0};

    /* Only support TFTP download for now */

    if ( TRUE )     /* Server */
    {
        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_ft_Root DMSB_TR181_PSM_ft_Server
            );

        RecordType = ccsp_unsignedInt;
        _ansc_sprintf(RecordValue, "%d", pCfg->Server);

        iReturnValue =
            PSM_Set_Record_Value2
                (
                    g_MessageBusHandle,
                    g_SubsystemPrefix,
                    pParamPath,
                    RecordType,
                    RecordValue
                );

        if ( iReturnValue != CCSP_SUCCESS )
        {
            AnscTraceWarning
                ((
                    "%s -- failed to set the parameter %s, error code %d, type %d, value %s.\n",
                    __FUNCTION__,
                    pParamPath,
                    iReturnValue,
                    RecordType,
                    RecordValue
                ));
        }
    }

    if ( TRUE )     /* Protocol */
    {
        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_ft_Root DMSB_TR181_PSM_ft_Protocol
            );

        RecordType = ccsp_unsignedInt;
        _ansc_sprintf(RecordValue, "%d", pCfg->Protocol);

        iReturnValue =
            PSM_Set_Record_Value2
                (
                    g_MessageBusHandle,
                    g_SubsystemPrefix,
                    pParamPath,
                    RecordType,
                    RecordValue
                );

        if ( iReturnValue != CCSP_SUCCESS )
        {
            AnscTraceWarning
                ((
                    "%s -- failed to set the parameter %s, error code %d, type %d, value %s.\n",
                    __FUNCTION__,
                    pParamPath,
                    iReturnValue,
                    RecordType,
                    RecordValue
                ));
        }
    }

    if ( TRUE )     /* FileName */
    {
        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_ft_Root DMSB_TR181_PSM_ft_FileName
            );

        RecordType = ccsp_string;
        AnscCopyString(RecordValue, pCfg->FileName);

        iReturnValue =
            PSM_Set_Record_Value2
                (
                    g_MessageBusHandle,
                    g_SubsystemPrefix,
                    pParamPath,
                    RecordType,
                    RecordValue
                );

        if ( iReturnValue != CCSP_SUCCESS )
        {
            AnscTraceWarning
                ((
                    "%s -- failed to set the parameter %s, error code %d, type %d, value %s.\n",
                    __FUNCTION__,
                    pParamPath,
                    iReturnValue,
                    RecordType,
                    RecordValue
                ));
        }
    }

    if ( pCfg->Protocol == COSA_DML_FILETRANSFER_PROTOCOL_TFTP && pCfg->Action == COSA_DML_FILETRANSFER_ACTION_Download )
    {
        if ( pMyObject->Status == COSA_DML_FILETRANSFER_STATUS_InProgress )
        {
            AnscTraceWarning(("Another download session is running, please try again later...\n"));
            return ANSC_STATUS_FAILURE;
        }

        AnscSpawnTask
            (
                FileTransferTask,
                (ANSC_HANDLE)pMyObject,
                "FileTransferTask"
            );
    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlFileTransferGetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_FILETRANSFER_CFG  pCfg
    )
{
    int                             iReturnValue    = CCSP_SUCCESS;
    char                            pParamPath[64]  = {0};
    unsigned int                    RecordType      = 0;
    SLAP_VARIABLE                   SlapValue       = {0};

    /* Fetch Cfg, we only support tftp download for now */

    if ( TRUE )     /* Server */
    {
        SlapInitVariable(&SlapValue);

        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_ft_Root DMSB_TR181_PSM_ft_Server
            );

        iReturnValue =
            PSM_Get_Record_Value
                (
                    g_MessageBusHandle,
                    g_SubsystemPrefix,
                    pParamPath,
                    &RecordType,
                    &SlapValue
                );

        if ( (iReturnValue != CCSP_SUCCESS) || (RecordType != ccsp_unsignedInt))
        {
            AnscTraceWarning
                ((
                    "%s -- failed to retrieve the parameter %s, error code %d, type %d\n",
                    __FUNCTION__,
                    pParamPath,
                    iReturnValue,
                    RecordType
                ));
        }
        else
        {
            pCfg->Server, SlapValue.Variant.varUint32;
        }

        SlapCleanVariable(&SlapValue);
    }

    if ( TRUE )     /* Protocol */
    {
        SlapInitVariable(&SlapValue);

        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_ft_Root DMSB_TR181_PSM_ft_Protocol
            );

        iReturnValue =
            PSM_Get_Record_Value
                (
                    g_MessageBusHandle,
                    g_SubsystemPrefix,
                    pParamPath,
                    &RecordType,
                    &SlapValue
                );

        if ( (iReturnValue != CCSP_SUCCESS) || (RecordType != ccsp_unsignedInt))
        {
            AnscTraceWarning
                ((
                    "%s -- failed to retrieve the parameter %s, error code %d, type %d\n",
                    __FUNCTION__,
                    pParamPath,
                    iReturnValue,
                    RecordType
                ));
        }
        else
        {
            pCfg->Protocol = SlapValue.Variant.varUint32;
        }

        SlapCleanVariable(&SlapValue);
    }

    if ( TRUE )     /* FileName */
    {
        SlapInitVariable(&SlapValue);

        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_ft_Root DMSB_TR181_PSM_ft_FileName
            );

        iReturnValue =
            PSM_Get_Record_Value
                (
                    g_MessageBusHandle,
                    g_SubsystemPrefix,
                    pParamPath,
                    &RecordType,
                    &SlapValue
                );

        if ( (iReturnValue != CCSP_SUCCESS) || (RecordType != ccsp_string))
        {
            AnscTraceWarning
                ((
                    "%s -- failed to retrieve the parameter %s, error code %d, type %d\n",
                    __FUNCTION__,
                    pParamPath,
                    iReturnValue,
                    RecordType
                ));
        }
        else
        {
            AnscCopyString(pCfg->FileName, SlapValue.Variant.varString);
        }

        SlapCleanVariable(&SlapValue);
    }

    return ANSC_STATUS_SUCCESS;
}

ULONG
CosaDmlFileTransferGetStatus
    (
        ANSC_HANDLE                 hContext
    )
{
    PCOSA_DATAMODEL_FILETRANSFER    pMyObject = (PCOSA_DATAMODEL_FILETRANSFER)hContext;

    return pMyObject->Status;
}

