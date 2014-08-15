/*
 ** Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License Version 2 as
 ** published by the Free Software Foundation.  You may not use, modify or
 ** distribute this program under any other version of the GNU General
 ** Public License.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program; if not, write to the Free Software
 ** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
// ips_priority.cc author Russ Combs <rucombs@cisco.com>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "snort_types.h"
#include "protocols/packet.h"
#include "snort_debug.h"
#include "snort.h"
#include "profiler.h"
#include "detection/detection_defines.h"
#include "detection_util.h"
#include "framework/cursor.h"
#include "framework/ips_option.h"
#include "framework/module.h"

static const char* s_name = "raw_data";

static THREAD_LOCAL ProfileStats rawDataPerfStats;

class RawDataOption : public IpsOption
{
public:
    RawDataOption() : IpsOption(s_name) { };

    CursorActionType get_cursor_type() const
    { return CAT_SET_RAW; };

    int eval(Cursor&, Packet*);
};

int RawDataOption::eval(Cursor& c, Packet* p)
{
    PROFILE_VARS;
    MODULE_PROFILE_START(rawDataPerfStats);

    c.set(s_name, p->data, p->dsize);

    MODULE_PROFILE_END(rawDataPerfStats);
    return DETECTION_OPTION_MATCH;
}

//-------------------------------------------------------------------------
// module
//-------------------------------------------------------------------------

class RawDataModule : public Module
{
public:
    RawDataModule() : Module(s_name) { };

    ProfileStats* get_profile() const
    { return &rawDataPerfStats; };
};

//-------------------------------------------------------------------------
// api methods
//-------------------------------------------------------------------------

static Module* mod_ctor()
{
    return new RawDataModule;
}

static void mod_dtor(Module* m)
{
    delete m;
}

static IpsOption* raw_data_ctor(Module*, OptTreeNode*)
{
    return new RawDataOption;
}

static void raw_data_dtor(IpsOption* p)
{
    delete p;
}

static const IpsApi raw_data_api =
{
    {
        PT_IPS_OPTION,
        s_name,
        IPSAPI_PLUGIN_V0,
        0,
        mod_ctor,
        mod_dtor
    },
    OPT_TYPE_DETECTION,
    0, 0,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    raw_data_ctor,
    raw_data_dtor,
    nullptr
};

#ifdef BUILDING_SO
SO_PUBLIC const BaseApi* snort_plugins[] =
{
    &raw_data_api.base,
    nullptr
};
#else
const BaseApi* ips_raw_data = &raw_data_api.base;
#endif
