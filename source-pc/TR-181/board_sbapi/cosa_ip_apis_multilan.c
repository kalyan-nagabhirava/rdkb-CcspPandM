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

/**********************************************************************

    File: cosa_ip_apis_multilan.c

    For Data Model SBAPI Implementation,
    Common Component Software Platform (CCSP)

    ---------------------------------------------------------------

    description:

        The APIs servicing TR-181 data model IP Interface SBAPI
        integration are implemented in this file.

    ---------------------------------------------------------------

    environment:

        Platform Independent, but with multi-LAN support

    ---------------------------------------------------------------

    author:

        Ding Hua

    ---------------------------------------------------------------

    revision:

        02/28/2013  initial revision.

**********************************************************************/


#include "cosa_apis.h"
#include "cosa_ip_apis_multilan.h"
#include "ccsp_psm_helper.h"

#include "linux/if.h"
#include "linux/sockios.h"
#include <sys/ioctl.h>

//RDKB-EMU
extern ANSC_HANDLE bus_handle;
extern char g_Subsystem[32];

/********************************************************************n
                            HELPER FUNCTIONS
**********************************************************************/
COSA_DML_IF_STATUS getIfStatus(const PUCHAR name, struct ifreq *pIfr);

BOOLEAN getIfEnable(const PUCHAR name)
{
	char buf[512] = {0},status[512] = {0};
	FILE *fp = NULL;
	int count = 0;
	sprintf(buf,"%s%s%s%s%s","ifconfig ",name," | grep ",name," | wc -l > /tmp/Interface_EnablingStatus.txt");
	system(buf);
	fp = popen("cat /tmp/Interface_EnablingStatus.txt","r");
	if(fp == NULL)
	{
		printf("Failed to run command in Function %s\n",__FUNCTION__);
                return 0;
        }
	if(fgets(buf, sizeof(buf)-1, fp) != NULL)
        {
        for(count=0;buf[count]!='\n';count++)
                status[count]=buf[count];
        status[count]='\0';
        }
	pclose(fp);
	if(strcmp(status,"1") == 0)
		return TRUE;
	else
		return FALSE;

}
/**********************************************************************
                            MAIN ROUTINES
**********************************************************************/

ANSC_STATUS
CosaDmlIpMlanInit
    (
        ANSC_HANDLE                 hDml,
        PANSC_HANDLE                phContext
    )
{
    PDMSB_TR181_IP_CONTEXT          pIpContext     = NULL;

    AnscTraceWarning(("CosaDmlIpMlanInit...\n"));

    pIpContext = (PDMSB_TR181_IP_CONTEXT)AnscAllocateMemory(sizeof(DMSB_TR181_IP_CONTEXT));

    if ( pIpContext )
    {
        DMSB_TR181_IP_CONTEXT_Init(pIpContext);

        *phContext = (ANSC_HANDLE)pIpContext;
        return  ANSC_STATUS_SUCCESS;
    }
    else
    {
        return  ANSC_STATUS_RESOURCES;
    }

    /* Need the Unload function to free allocated context */
}

/*
 *  Utility routines
 */
PDMSB_TR181_IP_IF   
CosaDmlIpIfMlanFindByInstNum
    (
        PDMSB_TR181_IP_CONTEXT      pIpContext,
        ULONG                       ulInstNum
    )
{
    PSINGLE_LINK_ENTRY              pSLinkEntry;
    PDMSB_TR181_IP_IF               pIpIf;

    pSLinkEntry = AnscSListGetFirstEntry(&pIpContext->IpIfList);

    while ( pSLinkEntry )
    {
        pIpIf    = ACCESS_DMSB_TR181_IP_IF(pSLinkEntry);
        pSLinkEntry = AnscSListGetNextEntry(pSLinkEntry);

        if ( pIpIf->Cfg.InstanceNumber == ulInstNum )
        {
            return  pIpIf;
        }
    }

    AnscTraceWarning(("%s -- failed to find %d!\n", __FUNCTION__, ulInstNum));

    return  NULL;
}


ANSC_STATUS
CosaDmlIpIfMlanLoadPsm
    (
        PDMSB_TR181_IP_CONTEXT      pIpContext
    )
{
    int                             iReturnValue    = CCSP_SUCCESS;
    int                             iNumInst        = 0;
    int*                            pInstArray      = NULL;
    ULONG                           ulIndex;
    PDMSB_TR181_IP_IF               pIpIf           = NULL;
    char                            pParamPath[64]  = {0};
    unsigned int                    RecordType      = 0;
    SLAP_VARIABLE                   SlapValue       = {0};
    ULONG                           ulL2netInst     = 0;

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    iReturnValue = 
        PsmGetNextLevelInstances
            (
                g_MessageBusHandle,
                g_SubsystemPrefix,
                DMSB_TR181_PSM_l3net_Root,
                &iNumInst,
                &pInstArray
            );

    if ( iReturnValue != CCSP_SUCCESS )
    {
        AnscTraceWarning(("%s -- PsmGetNextLevelInstances failed, error code = %d!\n", __FUNCTION__, iReturnValue));
        goto  EXIT;
    }

    for ( ulIndex = 0+1; ulIndex < iNumInst; ulIndex++ )  //RDKB-EMU
    {
        pIpIf = (PDMSB_TR181_IP_IF)AnscAllocateMemory(sizeof(DMSB_TR181_IP_IF));

        if ( !pIpIf )
        {
            AnscTraceWarning(("%s -- insufficient resources, IpIf instance %d\n", __FUNCTION__, ulIndex));
            return  ANSC_STATUS_RESOURCES;
        }
        else
        {
            //pIpIf->Cfg.InstanceNumber = pInstArray[ulIndex];
            pIpIf->Cfg.InstanceNumber = ulIndex;
        }

        /* Fetch Cfg */

        if ( TRUE )     /* Enable */
        {
            SlapInitVariable(&SlapValue);

            /*_ansc_sprintf
                (
                    pParamPath,
                    DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_Enable,
                    pInstArray[ulIndex]
                );*/
             _ansc_sprintf
                (
                    pParamPath,
                    DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_Enable,
                    ulIndex
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

            if ( (iReturnValue != CCSP_SUCCESS) )
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
            else if (RecordType == ccsp_boolean)
            {
                pIpIf->Cfg.bEnabled = SlapValue.Variant.varBool;
            }
            else if (RecordType == ccsp_string)
            {
                pIpIf->Cfg.bEnabled = AnscEqualString(SlapValue.Variant.varString, "true", TRUE);
            }

            SlapCleanVariable(&SlapValue);
        }

        if ( TRUE )     /* Alias */
        {
            SlapInitVariable(&SlapValue);

            /*_ansc_sprintf
                (
                    pParamPath,
                    DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_Alias,
                    pInstArray[ulIndex]
                );*/
            _ansc_sprintf
                (
                    pParamPath,
                    DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_Alias,
                    ulIndex
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
                _ansc_strcpy(pIpIf->Cfg.Alias, SlapValue.Variant.varString);
            }

            SlapCleanVariable(&SlapValue);
        }
        if ( TRUE )     /* MaxMTU */
        {
            SlapInitVariable(&SlapValue);

            /*_ansc_sprintf
                (
                    pParamPath,
                    DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_MaxMTU,
                    pInstArray[ulIndex]
                );*/
            _ansc_sprintf
                (
                    pParamPath,
                    DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_MaxMTU,
                    ulIndex
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

            if ( (iReturnValue != CCSP_SUCCESS) )
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
            else if (RecordType == ccsp_unsignedInt)
            {
                pIpIf->Cfg.MaxMTUSize = SlapValue.Variant.varUint32;
            }
            else if (RecordType == ccsp_string)
            {
                pIpIf->Cfg.MaxMTUSize = _ansc_atoi(SlapValue.Variant.varString);
            }

            SlapCleanVariable(&SlapValue);
        }
        if ( TRUE )     /* AutoIPEnable */
        {
            SlapInitVariable(&SlapValue);

            /*_ansc_sprintf
                (
                    pParamPath,
                    DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_AutoIPEnable,
                    pInstArray[ulIndex]
                );*/
            _ansc_sprintf
                (
                    pParamPath,
                    DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_AutoIPEnable,
                    ulIndex
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

            if ( (iReturnValue != CCSP_SUCCESS) )
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
            else if (RecordType == ccsp_boolean)
            {
                pIpIf->Cfg.AutoIPEnable = SlapValue.Variant.varBool;
            }
            else if (RecordType == ccsp_string)
            {
                pIpIf->Cfg.AutoIPEnable = AnscEqualString(SlapValue.Variant.varString, "true", TRUE);
            }

            SlapCleanVariable(&SlapValue);
        }

        if ( TRUE )     /* ArpCacheTimeout */
        {
            SlapInitVariable(&SlapValue);

            /*_ansc_sprintf
                (
                    pParamPath,
                    DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_ArpCacheTimeout,
                    pInstArray[ulIndex]
                );*/
            _ansc_sprintf
                (
                    pParamPath,
                    DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_ArpCacheTimeout,
                    ulIndex
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

            if ( (iReturnValue != CCSP_SUCCESS) )
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
            else if (RecordType == ccsp_unsignedInt)
            {
                pIpIf->Cfg.ArpCacheTimeout = SlapValue.Variant.varUint32;
            }
            else if (RecordType == ccsp_string)
            {
                pIpIf->Cfg.ArpCacheTimeout = _ansc_atoi(SlapValue.Variant.varString);
            }

            SlapCleanVariable(&SlapValue);
        }

        if ( TRUE )     /* UPnPIGDEnabled */
        {
            SlapInitVariable(&SlapValue);

           /* _ansc_sprintf
                (
                    pParamPath,
                    DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_UpnpIgdEnabled,
                    pInstArray[ulIndex]
                );*/
             _ansc_sprintf
                (
                    pParamPath,
                    DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_UpnpIgdEnabled,
                    ulIndex
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

            if ( (iReturnValue != CCSP_SUCCESS) )
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
            else if (RecordType == ccsp_boolean)
            {
                pIpIf->Cfg.UpnpIgdEnabled = SlapValue.Variant.varBool;
            }
            else if (RecordType == ccsp_string)
            {
                pIpIf->Cfg.UpnpIgdEnabled = AnscEqualString(SlapValue.Variant.varString, "true", TRUE);
            }

            SlapCleanVariable(&SlapValue);
        }

        if ( TRUE )     /* EthLink */
        {
            SlapInitVariable(&SlapValue);

            /*_ansc_sprintf
                (
                    pParamPath,
                    DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_EthLink,
                    pInstArray[ulIndex]
                );*/
             _ansc_sprintf
                (
                    pParamPath,
                    DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_EthLink,
                    ulIndex
                );

		printf(" EthLink Value is %s \n", pParamPath);
            iReturnValue =
                PSM_Get_Record_Value
                    (
                        g_MessageBusHandle,
                        g_SubsystemPrefix,
                        pParamPath,
                        &RecordType,
                        &SlapValue
                    );

            if ( (iReturnValue != CCSP_SUCCESS) )
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
            else if (RecordType == ccsp_unsignedInt)
            {
                pIpIf->Cfg.LinkInstNum = SlapValue.Variant.varUint32;
            }
            else if (RecordType == ccsp_string)
            {
                pIpIf->Cfg.LinkInstNum = _ansc_atoi(SlapValue.Variant.varString);
            }

            SlapCleanVariable(&SlapValue);

            /* LinkType is always EthLink for now */
            pIpIf->Cfg.LinkType = COSA_DML_LINK_TYPE_EthLink;
        }

        /*
         *  Fetch the "l2net" parameter from l3net PSM object
         */
        if ( pIpIf->Cfg.LinkInstNum )
        {
            SlapInitVariable(&SlapValue);

            _ansc_sprintf
                (
                    pParamPath,
                    DMSB_TR181_PSM_EthLink_Root DMSB_TR181_PSM_EthLink_i DMSB_TR181_PSM_EthLink_l2net,
                    pIpIf->Cfg.LinkInstNum
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

            if ( (iReturnValue != CCSP_SUCCESS) )
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
            else if (RecordType == ccsp_unsignedInt)
            {
                ulL2netInst = SlapValue.Variant.varUint32;
            }
            else if (RecordType == ccsp_string)
            {
                ulL2netInst = _ansc_atoi(SlapValue.Variant.varString);
            }

            SlapCleanVariable(&SlapValue);
        }

        /*
         *  Fetch the "name" parameter from l2net PSM object
         */
        if ( ulL2netInst )
        {
            SlapInitVariable(&SlapValue);

            _ansc_sprintf
                (
                    pParamPath,
                    DMSB_TR181_PSM_l2net_Root DMSB_TR181_PSM_l2net_i DMSB_TR181_PSM_l2net_name,
                    ulL2netInst
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
                _ansc_strcpy(pIpIf->Info.Name, SlapValue.Variant.varString);
                _ansc_strcpy(pIpIf->Cfg.LinkName, pIpIf->Info.Name);
            }

            SlapCleanVariable(&SlapValue);
        }

        /* Fetch Info */
        pIpIf->Info.LastChange = AnscGetTickInSeconds();

        /* Fill in other predetermined info */
        pIpIf->Cfg.IfType = COSA_DML_IP_IF_TYPE_Normal;

        AnscSListPushEntryAtBack(&pIpContext->IpIfList, &pIpIf->Linkage);
    }

EXIT:
    if ( pInstArray )
    {
        AnscFreeMemory(pInstArray);
        pInstArray = NULL;
    }

    return  iReturnValue;
}


ANSC_STATUS
CosaDmlIpIfMlanSavePsm
    (
        PDMSB_TR181_IP_CONTEXT      pIpContext,
        PCOSA_DML_IP_IF_CFG         pCfg
    )
{
    int                             iReturnValue    = CCSP_SUCCESS;
    char                            pParamPath[64]  = {0};
    unsigned int                    RecordType      = ccsp_string;
    char                            RecordValue[64] = {0};

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    if ( TRUE )     /* Enable */
    {
        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_Enable,
                pCfg->InstanceNumber
            );

        RecordType = ccsp_boolean;
        _ansc_strcpy(RecordValue, pCfg->bEnabled ? PSM_TRUE : PSM_FALSE);

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

    if ( TRUE )     /* Alias */
    {
        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_Alias,
                pCfg->InstanceNumber
            );

        RecordType = ccsp_string;
        _ansc_strcpy(RecordValue, pCfg->Alias);

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

    if ( TRUE )     /* MaxMTU */
    {
        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_MaxMTU,
                pCfg->InstanceNumber
            );

        RecordType = ccsp_unsignedInt;
        _ansc_sprintf(RecordValue, "%d", pCfg->MaxMTUSize);

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

    if ( TRUE )     /* AutoIPEnable */
    {
        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_AutoIPEnable,
                pCfg->InstanceNumber
            );

        RecordType = ccsp_boolean;
        _ansc_strcpy(RecordValue, pCfg->AutoIPEnable ? PSM_TRUE : PSM_FALSE);

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

    if ( TRUE )     /* ArpCacheTimeout */
    {
        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_ArpCacheTimeout,
                pCfg->InstanceNumber
            );

        RecordType = ccsp_unsignedInt;
        _ansc_sprintf(RecordValue, "%d", pCfg->ArpCacheTimeout);

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

    if ( TRUE )     /* UPnPIGDEnabled */
    {
        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_UpnpIgdEnabled,
                pCfg->InstanceNumber
            );

        RecordType = ccsp_boolean;
        _ansc_strcpy(RecordValue, pCfg->UpnpIgdEnabled ? PSM_TRUE : PSM_FALSE);

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

    if ( TRUE )     /* EthLink */
    {
        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_EthLink,
                pCfg->InstanceNumber
            );

        RecordType = ccsp_unsignedInt;
        _ansc_sprintf(RecordValue, "%d", pCfg->LinkInstNum);

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

    return  CCSP_SUCCESS;
}


ANSC_STATUS
CosaDmlIpIfMlanDelPsm
    (
        PDMSB_TR181_IP_CONTEXT      pIpContext,
        PCOSA_DML_IP_IF_CFG         pCfg
    )
{
    int                             iReturnValue    = CCSP_SUCCESS;
    char                            pParamPath[64]  = {0};

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    if ( TRUE )     /* Enable */
    {
        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i,
                pCfg->InstanceNumber
            );

        iReturnValue =
            PSM_Del_Record
                (
                    g_MessageBusHandle,
                    g_SubsystemPrefix,
                    pParamPath
                );

        if ( iReturnValue != CCSP_SUCCESS )
        {
            AnscTraceWarning(("%s -- failed to delete PSM records, error code %d", __FUNCTION__, iReturnValue));
        }
    }

    return  CCSP_SUCCESS;
}


ULONG
CosaDmlIpIfMlanGetNumberOfEntries
    (
        ANSC_HANDLE                 hContext
    )
{
    PDMSB_TR181_IP_CONTEXT          pIpContext      = (PDMSB_TR181_IP_CONTEXT )hContext;

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    if ( !pIpContext->IpIfInit )
    {
        CosaDmlIpIfMlanLoadPsm(pIpContext);
        pIpContext->IpIfInit = TRUE;
    }

    return  AnscSListQueryDepth(&pIpContext->IpIfList);
}


ANSC_STATUS
CosaDmlIpIfMlanGetEntry
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIndex,
        PCOSA_DML_IP_IF_FULL        pEntry
    )
{
    PDMSB_TR181_IP_CONTEXT          pIpContext     = (PDMSB_TR181_IP_CONTEXT)hContext;
    PSINGLE_LINK_ENTRY              pSLinkEntry;
    PDMSB_TR181_IP_IF               pIpIf;

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    if ( !pEntry )
    {
        return ANSC_STATUS_FAILURE;
    }

    if ( ulIndex < AnscSListQueryDepth(&pIpContext->IpIfList) )
    {
        pSLinkEntry = AnscSListGetEntryByIndex(&pIpContext->IpIfList, ulIndex);//RDKB-EMU

        if ( pSLinkEntry )
        {
            pIpIf = ACCESS_DMSB_TR181_IP_IF(pSLinkEntry);

            AnscCopyMemory(&pEntry->Cfg, &pIpIf->Cfg, sizeof(pIpIf->Cfg));
            AnscCopyMemory(&pEntry->Info, &pIpIf->Info, sizeof(pIpIf->Info));
	    if(pEntry->Cfg.InstanceNumber == 2)
            {
            AnscCopyString(pIpIf->Info.Name,"eth2");//RDKB-EMU
            AnscCopyString(pEntry->Info.Name,pIpIf->Info.Name);//RDKB-EMU
            AnscCopyString(pEntry->Cfg.LinkName,pIpIf->Info.Name);//RDKB-EMU
            }
	    if(pEntry->Cfg.InstanceNumber == 3)
            {
            AnscCopyString(pIpIf->Info.Name,"lo");//RDKB-EMU
            AnscCopyString(pEntry->Info.Name,pIpIf->Info.Name);//RDKB-EMU
            AnscCopyString(pEntry->Cfg.LinkName,pIpIf->Info.Name);//RDKB-EMU
            }
	    if(pEntry->Cfg.InstanceNumber == 6)
            {
            AnscCopyString(pIpIf->Info.Name,"brlan2");//RDKB-EMU
            AnscCopyString(pEntry->Info.Name,pIpIf->Info.Name);//RDKB-EMU
            AnscCopyString(pEntry->Cfg.LinkName,pIpIf->Info.Name);//RDKB-EMU
            }
	    if((pEntry->Cfg.InstanceNumber != 6) && (pEntry->Cfg.InstanceNumber != 5))
	    			pEntry->Cfg.bEnabled = getIfEnable(pIpIf->Info.Name); //RDKB-EMU
	    if((pEntry->Cfg.InstanceNumber != 6) && (pEntry->Cfg.InstanceNumber != 5))
	    			pEntry->Cfg.MaxMTUSize = CosaUtilIoctlXXX(pIpIf->Info.Name,"mtu",NULL);
            return  ANSC_STATUS_SUCCESS;
        }
        else
        {
            return  ANSC_STATUS_CANT_FIND;
        }
    }
    else
    {
        return ANSC_STATUS_FAILURE;
    }
}


ANSC_STATUS
CosaDmlIpIfMlanSetValues
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIndex,
        ULONG                       ulInstanceNumber,
        char*                       pAlias
    )
{
    PDMSB_TR181_IP_CONTEXT          pIpContext     = (PDMSB_TR181_IP_CONTEXT)hContext;
    PSINGLE_LINK_ENTRY              pSLinkEntry;
    PDMSB_TR181_IP_IF               pIpIf;

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    pSLinkEntry = AnscSListGetEntryByIndex(&pIpContext->IpIfList, ulIndex);

    if ( pSLinkEntry )
    {
        pIpIf = ACCESS_DMSB_TR181_IP_IF(pSLinkEntry);

        /* Instance number cannot be changed! */
        _ansc_strcpy(pIpIf->Cfg.Alias, pAlias);

        CosaDmlIpIfMlanSavePsm(pIpContext, &pIpIf->Cfg);
    }
    else
    {
        AnscTraceWarning
            ((
                "%s -- cannot find the interface, index = %d, InstanceNumber = %d, alias = ''\n",
                __FUNCTION__,
                ulIndex,
                ulInstanceNumber,
                pAlias
            ));

        return  ANSC_STATUS_CANT_FIND;
    }
}


ANSC_STATUS
CosaDmlIpIfMlanAddEntry
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_IP_IF_FULL        pEntry
    )
{
    int                             iReturnValue    = CCSP_SUCCESS;
    PDMSB_TR181_IP_CONTEXT          pIpContext     = (PDMSB_TR181_IP_CONTEXT)hContext;
    PSINGLE_LINK_ENTRY              pSLinkEntry;
    PDMSB_TR181_IP_IF               pIpIf;

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    if ( !pEntry )
    {
        return ANSC_STATUS_FAILURE;
    }
    else
    {
        pIpIf = (PDMSB_TR181_IP_IF)AnscAllocateMemory(sizeof(DMSB_TR181_IP_IF));

        if ( !pIpIf )
        {
            AnscTraceWarning(("%s -- insufficient resources, IpIf instance %d\n", __FUNCTION__, pEntry->Cfg.InstanceNumber));
            return  ANSC_STATUS_RESOURCES;
        }
        else
        {
            AnscCopyMemory(&pIpIf->Cfg,        &pEntry->Cfg,        sizeof(pIpIf->Cfg));
            AnscCopyMemory(&pIpIf->Info, &pEntry->Info, sizeof(pIpIf->Info));

            if ( pIpIf->Cfg.LinkInstNum )
            {
                char                            pParamPath[64]  = {0};
                unsigned int                    RecordType      = 0;
                SLAP_VARIABLE                   SlapValue       = {0};

                /*
                 *  Fetch the "name" parameter from l2net PSM object
                 */
                SlapInitVariable(&SlapValue);

                _ansc_sprintf
                    (
                        pParamPath,
                        DMSB_TR181_PSM_l2net_Root DMSB_TR181_PSM_l2net_i DMSB_TR181_PSM_l2net_name,
                        pIpIf->Cfg.LinkInstNum
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
                    AnscTraceWarning(("%s -- failed to retrieve 'l2net.name' parameter, error code %d, type %d\n", __FUNCTION__, iReturnValue, RecordType));
                }
                else
                {
                    _ansc_strcpy(pIpIf->Info.Name, SlapValue.Variant.varString);
                }

                SlapCleanVariable(&SlapValue);
            }

            pEntry->Info.Status    = getIfStatus(pIpIf->Info.Name, NULL);
            pIpIf->Info.LastChange = AnscGetTickInSeconds();

            AnscSListPushEntryAtBack(&pIpContext->IpIfList, &pIpIf->Linkage);

            CosaDmlIpIfMlanSavePsm(pIpContext, &pIpIf->Cfg);

            TR181_Mlan_Sysevent_ResyncAll();
        }
    }
}

ANSC_STATUS
CosaDmlIpIfMlanDelEntry
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber
    )
{
    PDMSB_TR181_IP_CONTEXT          pIpContext     = (PDMSB_TR181_IP_CONTEXT)hContext;
    PDMSB_TR181_IP_IF               pIpIf;

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    pIpIf = CosaDmlIpIfMlanFindByInstNum(pIpContext, ulInstanceNumber);

    if ( !pIpIf )
    {
        return  ANSC_STATUS_CANT_FIND;
    }
    else
    {
        CosaDmlIpIfMlanDelPsm(pIpContext, &pIpIf->Cfg);

        TR181_Mlan_Sysevent_ResyncAll();

        AnscSListPopEntryByLink(&pIpContext->IpIfList, &pIpIf->Linkage);
        AnscFreeMemory(pIpIf);

        return  ANSC_STATUS_SUCCESS;
    }
}


ANSC_STATUS
CosaDmlIpIfMlanSetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_IP_IF_CFG         pCfg
    )
{
    PDMSB_TR181_IP_CONTEXT          pIpContext     = (PDMSB_TR181_IP_CONTEXT)hContext;
    PDMSB_TR181_IP_IF               pIpIf;

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    pIpIf = CosaDmlIpIfMlanFindByInstNum(pIpContext, pCfg->InstanceNumber);

    if ( !pIpIf )
    {
        return  ANSC_STATUS_CANT_FIND;
    }
    else
    {
        struct ifreq                ifr;

        /* Save the configuraton */
        AnscCopyMemory(&pIpIf->Cfg, pCfg, sizeof(pIpIf->Cfg));
        CosaDmlIpIfMlanSavePsm(pIpContext, &pIpIf->Cfg);

        TR181_Mlan_Sysevent_Resync(pIpIf->Cfg.InstanceNumber);

        /* Update the name -- just copy LinkName to Name field */
        _ansc_strcpy(pIpIf->Info.Name, pCfg->LinkName);

        /*
         *  Reset stats
         */
        if ( getIfStatus(pIpIf->Info.Name, &ifr) == COSA_DML_IF_STATUS_Unknown )
        {
            return ANSC_STATUS_FAILURE;
        }

        if ( pCfg->bEnabled && !(pIpIf->Cfg.bEnabled) && !(ifr.ifr_flags & IFF_UP) )
        {
            ifr.ifr_flags |= IFF_UP;

            /*
             *  Comment out the set function for now
             *
            if ( setIfStatus(&ifr) )
            {
                return ANSC_STATUS_FAILURE;
            }
             */

            pIpIf->Info.LastChange = AnscGetTickInSeconds();
            CosaUtilGetIfStats(pIpIf->Info.Name, &pIpIf->LastStats);
        }
        else if ( !(pCfg->bEnabled) && pIpIf->Cfg.bEnabled )
        {
            ifr.ifr_flags &= ~IFF_UP;

            /*
             *  Comment out the set function for now
             *
            if ( setIfStatus(&ifr) )
            {
                return ANSC_STATUS_FAILURE;
            }
             */

            pIpIf->Info.LastChange = AnscGetTickInSeconds();
            CosaUtilGetIfStats(pIpIf->Info.Name, &pIpIf->LastStats);
        }

        return  ANSC_STATUS_SUCCESS;
    }
}


ANSC_STATUS
CosaDmlIpIfMlanGetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_IP_IF_CFG         pCfg
    )
{
    PDMSB_TR181_IP_CONTEXT          pIpContext     = (PDMSB_TR181_IP_CONTEXT)hContext;
    PDMSB_TR181_IP_IF               pIpIf;

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    pIpIf = CosaDmlIpIfMlanFindByInstNum(pIpContext, pCfg->InstanceNumber);

    if ( !pIpIf )
    {
        return  ANSC_STATUS_CANT_FIND;
    }
    else
    {
        AnscCopyMemory(pCfg, &pIpIf->Cfg, sizeof(pIpIf->Cfg));

        return  ANSC_STATUS_SUCCESS;
    }
}


ANSC_STATUS
CosaDmlIpIfMlanGetInfo
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_IP_IF_INFO        pInfo
    )
{
    PDMSB_TR181_IP_CONTEXT          pIpContext     = (PDMSB_TR181_IP_CONTEXT)hContext;
    PDMSB_TR181_IP_IF               pIpIf;

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    pIpIf = CosaDmlIpIfMlanFindByInstNum(pIpContext, ulInstanceNumber);

    if ( !pIpIf )
    {
        return  ANSC_STATUS_CANT_FIND;
    }
    else
    {
        pInfo->Status       = getIfStatus(pIpIf->Info.Name, NULL);

        return  ANSC_STATUS_SUCCESS;
    }
}


ANSC_STATUS
CosaDmlIpIfMlanReset
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber
    )
{
    PDMSB_TR181_IP_CONTEXT          pIpContext     = (PDMSB_TR181_IP_CONTEXT)hContext;
    PDMSB_TR181_IP_IF               pIpIf;

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    pIpIf = CosaDmlIpIfMlanFindByInstNum(pIpContext, ulInstanceNumber);

    if ( !pIpIf )
    {
        return  ANSC_STATUS_CANT_FIND;
    }
    else
    {
        TR181_Mlan_Sysevent_Resync(ulInstanceNumber);

        return  ANSC_STATUS_SUCCESS;
    }
}


/*
 *  IP Interface IPv4Address
 */
ULONG
CosaDmlIpIfMlanGetNumberOfV4Addrs
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIpIfInstanceNumber
    )
{
    /* Always 1 */
    return  1;
}

ANSC_STATUS
CosaDmlIpIfMlanGetIPv4Addr
    (
        ULONG                       ulIpIfInstanceNumber,
        PCOSA_DML_IP_V4ADDR         pEntry
    )
{
    int                             iReturnValue    = CCSP_SUCCESS;
    char                            pParamPath[64]  = {0};
    unsigned int                    RecordType      = 0;
    SLAP_VARIABLE                   SlapValue       = {0};

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    /*
     *  Retrieve the IPv4Addr
     */
    if ( ulIpIfInstanceNumber )
    {
        SlapInitVariable(&SlapValue);

        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_V4Addr,
                ulIpIfInstanceNumber
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
            AnscTraceWarning(("%s -- failed to retrieve 'V4Addr' parameter, error code %d, type %d\n", __FUNCTION__, iReturnValue, RecordType));
        }
        else
        {
            pEntry->IPAddress.Value = _ansc_inet_addr(SlapValue.Variant.varString);
        }

        SlapCleanVariable(&SlapValue);
    }

    return  ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlIpIfMlanGetSubnetMask
    (
        ULONG                       ulIpIfInstanceNumber,
        PCOSA_DML_IP_V4ADDR         pEntry
    )
{
    int                             iReturnValue    = CCSP_SUCCESS;
    char                            pParamPath[64]  = {0};
    unsigned int                    RecordType      = 0;
    SLAP_VARIABLE                   SlapValue       = {0};

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    /*
     *  Retrieve the Subnet Mask
     */
    if ( ulIpIfInstanceNumber )
    {
        SlapInitVariable(&SlapValue);

        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_V4SubnetMask,
                ulIpIfInstanceNumber
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
            AnscTraceWarning(("%s -- failed to retrieve 'V4SubnetMask' parameter, error code %d, type %d\n", __FUNCTION__, iReturnValue, RecordType));
        }
        else
        {
            pEntry->SubnetMask.Value = _ansc_inet_addr(SlapValue.Variant.varString);
        }

        SlapCleanVariable(&SlapValue);
    }

    return  ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlIpIfMlanGetV4Addr
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIpIfInstanceNumber,
        ULONG                       ulIndex,
        PCOSA_DML_IP_V4ADDR         pEntry
    )
{
    int                             iReturnValue    = CCSP_SUCCESS;
    char                            pParamPath[64]  = {0};
    unsigned int                    RecordType      = 0;
    SLAP_VARIABLE                   SlapValue       = {0};
    char 			    lan_ip[256]     = {0};
    char 			    lan_subnet[256] = {0};
    uint32_t  			    ip_integer;
    uint32_t                        netmask;

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    if ( ulIndex != 0 )
    {
        /* 1 instance only -- index has to be zero */
        return  ANSC_STATUS_UNAPPLICABLE;
    }

    /*
     *  Retrieve the IPv4Addr
     */
    if ( ulIpIfInstanceNumber )
    {
        SlapInitVariable(&SlapValue);

        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_V4Addr,
                ulIpIfInstanceNumber
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
            AnscTraceWarning(("%s -- failed to retrieve 'V4Addr' parameter, error code %d, type %d\n", __FUNCTION__, iReturnValue, RecordType));
        }
        else
        {
            pEntry->IPAddress.Value = _ansc_inet_addr(SlapValue.Variant.varString);
        }

        SlapCleanVariable(&SlapValue);
    }

    /*
     *  Retrieve the Subnet Mask
     */
    if ( ulIpIfInstanceNumber )
    {
        SlapInitVariable(&SlapValue);

        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_V4SubnetMask,
                ulIpIfInstanceNumber
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
            AnscTraceWarning(("%s -- failed to retrieve 'V4SubnetMask' parameter, error code %d, type %d\n", __FUNCTION__, iReturnValue, RecordType));
        }
        else
        {
            pEntry->SubnetMask.Value = _ansc_inet_addr(SlapValue.Variant.varString);
        }

        SlapCleanVariable(&SlapValue);
    }

    /*
     *  Fill other fields
     */
    if ( TRUE )
    {
        pEntry->InstanceNumber  = 1;
        _ansc_sprintf(pEntry->Alias, "Interface_%d", pEntry->InstanceNumber);
        pEntry->bEnabled        = TRUE;
        pEntry->Status          = COSA_DML_IP_ADDR_STATUS_Enabled;
        pEntry->AddressingType  = COSA_DML_IP_ADDR_TYPE_Static;

    }
   if(ulIpIfInstanceNumber == 1)
   {
	pEntry->bEnabled  = getIfEnable("eth0");
        pEntry->AddressingType  = COSA_DML_IP_ADDR_TYPE_DHCP;
	ip_integer = CosaUtilGetIfAddr("eth0");
	netmask=CosaUtilIoctlXXX("eth0","netmask",NULL);
	*(uint32_t*)pEntry->IPAddress.Dot = ip_integer;
        *(uint32_t*)pEntry->SubnetMask.Dot = netmask;
	 _ansc_sprintf(lan_ip, "%d.%d.%d.%d",
            pEntry->IPAddress.Dot[0], pEntry->IPAddress.Dot[1], pEntry->IPAddress.Dot[2], pEntry->IPAddress.Dot[3]);
        _ansc_sprintf(lan_subnet, "%d.%d.%d.%d",
            pEntry->SubnetMask.Dot[0], pEntry->SubnetMask.Dot[1],pEntry->SubnetMask.Dot[2], pEntry->SubnetMask.Dot[3]);
	PSM_Set_Record_Value2(bus_handle,g_Subsystem,"dmsb.l3net.1.V4Addr", ccsp_string, lan_ip);
	PSM_Set_Record_Value2(bus_handle,g_Subsystem,"dmsb.l3net.1.V4SubnetMask", ccsp_string, lan_subnet);

   }
   else if(ulIpIfInstanceNumber == 2)
   {
	pEntry->bEnabled = getIfEnable("eth2");
        pEntry->AddressingType  = COSA_DML_IP_ADDR_TYPE_DHCP;
	ip_integer = CosaUtilGetIfAddr("eth2");
        netmask=CosaUtilIoctlXXX("eth2","netmask",NULL);
        *(uint32_t*)pEntry->IPAddress.Dot = ip_integer;
        *(uint32_t*)pEntry->SubnetMask.Dot = netmask;
         _ansc_sprintf(lan_ip, "%d.%d.%d.%d",
            pEntry->IPAddress.Dot[0], pEntry->IPAddress.Dot[1], pEntry->IPAddress.Dot[2], pEntry->IPAddress.Dot[3]);
        _ansc_sprintf(lan_subnet, "%d.%d.%d.%d",
            pEntry->SubnetMask.Dot[0], pEntry->SubnetMask.Dot[1],pEntry->SubnetMask.Dot[2], pEntry->SubnetMask.Dot[3]);
        PSM_Set_Record_Value2(bus_handle,g_Subsystem,"dmsb.l3net.2.V4Addr", ccsp_string, lan_ip);
        PSM_Set_Record_Value2(bus_handle,g_Subsystem,"dmsb.l3net.2.V4SubnetMask", ccsp_string, lan_subnet);
   }
        return  ANSC_STATUS_SUCCESS;
}


ANSC_STATUS
CosaDmlIpIfMlanSetV4AddrValues
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIpIfInstanceNumber,
        ULONG                       ulIndex,
        ULONG                       ulInstanceNumber,
        char*                       pAlias
    )
{
    /*
     *  We don't need to set the instance number
     *  Alias cannot be changed, so do nothing here
     */
    return  ANSC_STATUS_SUCCESS;
}


ANSC_STATUS
CosaDmlIpIfMlanAddV4Addr
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIpIfInstanceNumber,
        PCOSA_DML_IP_V4ADDR         pEntry
    )
{
    /* Add/Del V4/V6 address is not supported */
    return  ANSC_STATUS_UNAPPLICABLE;    
}

ANSC_STATUS
CosaDmlIpIfMlanDelV4Addr
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIpIfInstanceNumber,
        PCOSA_DML_IP_V4ADDR         pEntry
    )
{
    /* Add/Del V4/V6 address is not supported */
    return  ANSC_STATUS_UNAPPLICABLE;    
}

ANSC_STATUS
CosaDmlIpIfMlanSetV4Addr
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIpIfInstanceNumber,
        PCOSA_DML_IP_V4ADDR         pEntry
    )
{
    int                             iReturnValue    = CCSP_SUCCESS;
    char                            pParamPath[64]  = {0};
    unsigned int                    RecordType      = ccsp_string;
    char                            RecordValue[64] = {0};

    if ( pEntry->InstanceNumber != 1 )
    {
        /* 1 instance only -- index has to be zero */
        return  ANSC_STATUS_UNAPPLICABLE;
    }

    AnscTraceFlow
        ((
            "%s -- instance number %d, address %u.%u.%u.%u, subnet mask %u.%u.%u.%u\n",
            __FUNCTION__,
            ulIpIfInstanceNumber,
            pEntry->IPAddress.Dot[0], pEntry->IPAddress.Dot[1],
            pEntry->IPAddress.Dot[2], pEntry->IPAddress.Dot[3],
            pEntry->SubnetMask.Dot[0], pEntry->SubnetMask.Dot[1],
            pEntry->SubnetMask.Dot[2], pEntry->SubnetMask.Dot[3]
        ));

    /*
     *  Only saves the parameters below
     */
    if ( TRUE )     /* V4Addr */
    {
        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_V4Addr,
                ulIpIfInstanceNumber
            );

        RecordType = ccsp_string;
        _ansc_strcpy(RecordValue, _ansc_inet_ntoa(*((struct in_addr*)pEntry->IPAddress.Dot)));

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
            AnscTraceWarning(("%s -- failed to set 'V4Addr' parameter, error code %d, type %d\n", __FUNCTION__, iReturnValue, RecordType));
        }
    }


    if ( TRUE )     /* V4SubnetMask */
    {
        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_V4SubnetMask,
                ulIpIfInstanceNumber
            );

        RecordType = ccsp_string;
        _ansc_strcpy(RecordValue, _ansc_inet_ntoa(*((struct in_addr*)pEntry->SubnetMask.Dot)));

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
            AnscTraceWarning(("%s -- failed to set 'V4Addr' parameter, error code %d, type %d\n", __FUNCTION__, iReturnValue, RecordType));
        }
    }

    TR181_Mlan_Sysevent_Resync(ulIpIfInstanceNumber);

    return  ANSC_STATUS_SUCCESS;
}


ANSC_STATUS
CosaDmlIpIfMlanGetV4Addr2
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIpIfInstanceNumber,
        PCOSA_DML_IP_V4ADDR         pEntry         
    )
{
    int                             iReturnValue    = CCSP_SUCCESS;
    char                            pParamPath[64]  = {0};
    unsigned int                    RecordType      = 0;
    SLAP_VARIABLE                   SlapValue       = {0};

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    /*
     *  Retrieve the IPv4Addr
     */
    if ( ulIpIfInstanceNumber )
    {
        SlapInitVariable(&SlapValue);

        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_V4Addr,
                ulIpIfInstanceNumber
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
            AnscTraceWarning(("%s -- failed to retrieve 'V4Addr' parameter, error code %d, type %d\n", __FUNCTION__, iReturnValue, RecordType));
        }
        else
        {
            pEntry->IPAddress.Value = _ansc_inet_addr(SlapValue.Variant.varString);
        }

        SlapCleanVariable(&SlapValue);
    }

    /*
     *  Retrieve the Subnet Mask
     */
    if ( ulIpIfInstanceNumber )
    {
        SlapInitVariable(&SlapValue);

        _ansc_sprintf
            (
                pParamPath,
                DMSB_TR181_PSM_l3net_Root DMSB_TR181_PSM_l3net_i DMSB_TR181_PSM_l3net_V4SubnetMask,
                ulIpIfInstanceNumber
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
            AnscTraceWarning(("%s -- failed to retrieve 'V4SubnetMask' parameter, error code %d, type %d\n", __FUNCTION__, iReturnValue, RecordType));
        }
        else
        {
            pEntry->SubnetMask.Value = _ansc_inet_addr(SlapValue.Variant.varString);
        }

        SlapCleanVariable(&SlapValue);
    }

    /*
     *  Fill other fields
     */
    if ( TRUE )
    {
        pEntry->InstanceNumber  = 1;
        _ansc_sprintf(pEntry->Alias, "%d", pEntry->InstanceNumber);
        pEntry->bEnabled        = TRUE;
        pEntry->Status          = COSA_DML_IP_ADDR_STATUS_Enabled;
        pEntry->AddressingType  = COSA_DML_IP_ADDR_TYPE_Static;

        return  ANSC_STATUS_SUCCESS;
    }
}

/*
 *  IP Interface IPv6Address
 */
ANSC_STATUS
CosaDmlIpIfMlanAddV6Addr
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIpIfInstanceNumber,
        PCOSA_DML_IP_V6ADDR         pEntry
    )
{
    /* Add/Del V4/V6 address is not supported */
    return  ANSC_STATUS_UNAPPLICABLE;    
}


ANSC_STATUS
CosaDmlIpIfMlanDelV6Addr
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIpIfInstanceNumber,
        PCOSA_DML_IP_V6ADDR         pEntry
    )
{
    /* Add/Del V4/V6 address is not supported */
    return  ANSC_STATUS_FAILURE;
}


ANSC_STATUS
CosaDmlIpIfMlanSetV6Addr
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIpIfInstanceNumber,
        PCOSA_DML_IP_V6ADDR         pEntry
    )
{
    /*
     *  No IPv6 support for Multi-LAN at the moment
     *  It should not have got here at all!
     */
    return  ANSC_STATUS_UNAPPLICABLE;    
}


ANSC_STATUS
CosaDmlIpIfMlanGetV6Addr2
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIpIfInstanceNumber,
        PCOSA_DML_IP_V6ADDR         pEntry         
    )
{
    /*
     *  No IPv6 support for Multi-LAN at the moment
     *  It should not have got here at all!
     */
    return  ANSC_STATUS_UNAPPLICABLE;    
}


ANSC_STATUS
CosaDmlIpIfMlanAddV6Prefix
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIpIfInstanceNumber,
        PCOSA_DML_IP_V6PREFIX       pEntry
    )
{
    /*
     *  No IPv6 support for Multi-LAN at the moment
     *  It should not have got here at all!
     */
    return  ANSC_STATUS_UNAPPLICABLE;    
}


ANSC_STATUS
CosaDmlIpIfMlanDelV6Prefix
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIpIfInstanceNumber,
        PCOSA_DML_IP_V6PREFIX       pEntry
    )
{
    /*
     *  No IPv6 support for Multi-LAN at the moment
     *  It should not have got here at all!
     */
    return  ANSC_STATUS_UNAPPLICABLE;    
}


ANSC_STATUS
CosaDmlIpIfMlanSetV6Prefix
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIpIfInstanceNumber,
        PCOSA_DML_IP_V6PREFIX       pEntry
    )
{
    /*
     *  No IPv6 support for Multi-LAN at the moment
     *  It should not have got here at all!
     */
    return  ANSC_STATUS_UNAPPLICABLE;    
}


ANSC_STATUS
CosaDmlIpIfMlanGetV6Prefix2 
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIpIfInstanceNumber,
        PCOSA_DML_IP_V6PREFIX       pEntry         
    )
{
    /*
     *  No IPv6 support for Multi-LAN at the moment
     *  It should not have got here at all!
     */
    return  ANSC_STATUS_UNAPPLICABLE;    
}

/*
 *  IP Interface statistics
 */
ANSC_STATUS
CosaDmlIpIfMlanGetStats
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIpIfInstanceNumber,
        PCOSA_DML_IP_STATS          pStats
    )
{
    PDMSB_TR181_IP_CONTEXT          pIpContext     = (PDMSB_TR181_IP_CONTEXT)hContext;
    PDMSB_TR181_IP_IF               pIpIf;

    AnscTraceFlow(("%s...\n", __FUNCTION__));

    pIpIf = CosaDmlIpIfMlanFindByInstNum(pIpContext, ulIpIfInstanceNumber);

    if ( !pIpIf )
    {
        return  ANSC_STATUS_CANT_FIND;
    }
    else
    {
        _ansc_memset(pStats, 0, sizeof(*pStats));

        CosaUtilGetIfStats(pIpIf->Info.Name, pStats);
        
        pStats->BroadcastPacketsReceived    -= pIpIf->LastStats.BroadcastPacketsReceived;
        pStats->BroadcastPacketsSent        -= pIpIf->LastStats.BroadcastPacketsSent;
        pStats->BytesReceived               -= pIpIf->LastStats.BytesReceived;
        pStats->BytesSent                   -= pIpIf->LastStats.BytesSent;
        pStats->DiscardPacketsReceived      -= pIpIf->LastStats.DiscardPacketsReceived;
        pStats->DiscardPacketsSent          -= pIpIf->LastStats.DiscardPacketsSent;
        pStats->ErrorsReceived              -= pIpIf->LastStats.ErrorsReceived;
        pStats->ErrorsSent                  -= pIpIf->LastStats.ErrorsSent;
        pStats->MulticastPacketsReceived    -= pIpIf->LastStats.MulticastPacketsReceived;
        pStats->MulticastPacketsSent        -= pIpIf->LastStats.MulticastPacketsSent;
        pStats->PacketsReceived             -= pIpIf->LastStats.PacketsReceived;
        pStats->PacketsSent                 -= pIpIf->LastStats.PacketsSent;
        pStats->UnicastPacketsReceived      -= pIpIf->LastStats.UnicastPacketsReceived;
        pStats->UnicastPacketsSent          -= pIpIf->LastStats.UnicastPacketsSent;
        pStats->UnknownProtoPacketsReceived -= pIpIf->LastStats.UnknownProtoPacketsReceived;

        AnscTraceFlow(("%s done!\n", __FUNCTION__));

        return ANSC_STATUS_SUCCESS;
    }
}

