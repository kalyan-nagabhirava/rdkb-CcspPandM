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

    module: cosa_firewall_internal.h

        For COSA Data Model Library Development

    -------------------------------------------------------------------

    copyright:

        Cisco Systems, Inc.
        All Rights Reserved.

    -------------------------------------------------------------------

    description:

        This file defines the internal apis for objects to support Data Model Library.

    -------------------------------------------------------------------


    author:

        Tom Chang

    -------------------------------------------------------------------

    revision:

        01/11/2011    initial revision.

**************************************************************************/

#ifndef  _COSA_FIREWALL_INTERNAL_H
#define  _COSA_FIREWALL_INTERNAL_H


#include "cosa_firewall_apis.h"


#define  COSA_DATAMODEL_FIREWALL_CLASS_CONTENT                                              \
    /* duplication of the base object class content */                                      \
    COSA_BASE_CONTENT                                                                       \
    /* start of FIREWALL object class content */                                            \
    COSA_DML_FIREWALL_CFG           FirewallConfig;                                         \


typedef  struct
_COSA_DATAMODEL_FIREWALL
{
	COSA_DATAMODEL_FIREWALL_CLASS_CONTENT
}
COSA_DATAMODEL_FIREWALL,  *PCOSA_DATAMODEL_FIREWALL;


/*
 *  Standard function declaration 
 */

ANSC_HANDLE
CosaFirewallCreate
    (
        VOID
    );

ANSC_STATUS
CosaFirewallInitialize
    (
        ANSC_HANDLE                 hThisObject
    );

ANSC_STATUS
CosaFirewallRemove
    (
        ANSC_HANDLE                 hThisObject
    );


#endif