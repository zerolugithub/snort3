/*
** Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
 * Copyright (C) 2002-2013 Sourcefire, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 2 as
 * published by the Free Software Foundation.  You may not use, modify or
 * distribute this program under any other version of the GNU General
 * Public License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
// pps_stream_tcp.cc author Josh Rosenbaum <jrosenba@cisco.com>

#include <sstream>
#include <vector>
#include <string>

#include "conversion_state.h"
#include "utils/s2l_util.h"
#include "preprocessor_states/pps_binder.h"


typedef void (Binder::*binder_func)(std::string);

namespace preprocessors
{

namespace {

class StreamTcp : public ConversionState
{
public:
    StreamTcp(Converter* cv, LuaData* ld);
    virtual ~StreamTcp() {};
    virtual bool convert(std::istringstream& data_stream);

private:
    Binder* bind_client;
    Binder* bind_server;
    Binder* bind_any;
    bool binding_chosen;
    std::vector<std::string> client_protocols;
    std::vector<std::string> server_protocols;
    std::vector<std::string> any_protocols;

    bool parse_small_segments(std::istringstream& data_stream);
    bool parse_ports(std::istringstream& data_stream);
    bool parse_protocol(std::istringstream& data_stream);
    void add_to_bindings(binder_func, std::string param);
};

} // namespace

StreamTcp::StreamTcp(Converter* cv, LuaData* ld) : ConversionState(cv, ld)
{
    bind_client = nullptr;
    bind_server = nullptr;
    bind_any = nullptr;
    binding_chosen = false;
}

void StreamTcp::add_to_bindings(binder_func func, std::string param)
{
    (bind_client->*func)(param);
    (bind_server->*func)(param);
    (bind_any->*func)(param);
}

bool StreamTcp::parse_small_segments(std::istringstream& stream)
{
    int consec_segs;
    std::string bytes;
    int min_bytes;
    std::string ignore_ports;

    if (!(stream >> consec_segs) ||
        !(stream >> bytes) ||
        bytes.compare("bytes") ||
        !(stream >> min_bytes))
        return false;



    ld->open_table("small_segments");
    ld->add_option_to_table("count", consec_segs);
    ld->add_option_to_table("maximum_size", min_bytes);
    ld->close_table();


    if (!(stream >> ignore_ports))
        return true;

    // otherwise the next argument MUST be ignore_ports
    if (ignore_ports.compare("ignore_ports"))
        return false;


    ld->open_table("small_segments");
    long long port;

    // extracting into an int since thats what they should be!
    while(stream >> port)
        ld->add_list_to_table("ignore_ports", std::to_string(port));

    ld->close_table();

    if (!stream.eof())
        return false;
    return true;
}


bool StreamTcp::parse_ports(std::istringstream& arg_stream)
{
    std::string port;
    std::string dir;
    Binder* bind;

    if(!(arg_stream >> dir))
        return false;

    if( !dir.compare("client"))
    {
        ld->add_diff_option_comment("stream_tcp: ports", "binder.when.ports; binder.when.role = client");
        bind = bind_client;
    }

    else if( !dir.compare("server"))
    {
        ld->add_diff_option_comment("stream_tcp: ports", "binder.when.ports; binder.when.role = server");
        bind = bind_server;
    }

    else if( !dir.compare("both"))
    {
        ld->add_diff_option_comment("stream_tcp: ports", "binder.when.ports; binder.when.role = any");
        bind = bind_any;
    }

    else
    {
        return false;
    }

    // Ensure we only print the chosen bindings
    if (!binding_chosen)
    {
        binding_chosen = true;
        bind_client->print_binding(false);
        bind_client->set_when_role("client");
    }
    bind->print_binding(true);

    // do nothing if no ports provided
    if (arg_stream >> port )
    {
        // for all, don't set the ports variable
        if (!port.compare("all"))
            void(0);

        // for none, don't print the binding
        else if (!port.compare("none"))
            bind->print_binding(false);

        else
        {
            do
            {
                bind->add_when_port(port);
            } while (arg_stream >> port);
        }
    }

    return true;
}

bool StreamTcp::parse_protocol(std::istringstream& arg_stream)
{
    std::string dir;
    std::string lua_dir;
    std::string protocol;
    std::vector<std::string>* protocols;
    Binder* bind;

    // this may seem idiotic, but Snort does not actually require
    // any keywords for the 'protocol' keyword.  So, this is
    // still technically correct.
    if (!(arg_stream >> dir))
        return true;


    if (!dir.compare("client"))
    {
        ld->add_diff_option_comment("stream_tcp: protocol", "binder.when.proto; binder.when.role = client");
        bind = bind_client;
        protocols = &client_protocols;
    }

    else if (!dir.compare("server"))
    {
        ld->add_diff_option_comment("stream_tcp: protocol", "binder.when.proto; binder.when.role = server");
        bind = bind_server;
        protocols = &server_protocols;
    }

    else if (!dir.compare("both"))
    {
        ld->add_diff_option_comment("stream_tcp: protocol", "binder.when.proto; binder.when.role = any");
        bind = bind_any;
        protocols = &any_protocols;
    }

    else
        return false;


    // Ensure we only print the chosen bindings
    if (!binding_chosen)
    {
        binding_chosen = true;
        bind_client->print_binding(false);
        bind_client->set_when_role("client");
    }
    bind->print_binding(true);

    // do nothing if no ports provided
    if (arg_stream >> protocol )
    {
        // for all, don't set the ports variable
        if (!protocol.compare("all"))
            void(0);

        // for none, don't print the binding
        else if (!protocol.compare("none"))
            bind->print_binding(false);

        else
        {
            do
            {
                // yes, I agree this may appear odd that I am
                // adding the value to a vector rather than creating
                // new binder.  The reasons is each binder may still
                // change while parsing stream_tcp.  Since I don't want
                // to create and save a new Binder for each protocol,
                // lets save the different protocols and create new
                // Binders at the very end of the convert() functions.
                protocols->push_back(protocol);
            } while (arg_stream >> protocol);
        }
    }

    return true;
}

bool StreamTcp::convert(std::istringstream& data_stream)
{
    std::string keyword;
    bool retval = true;

    Binder client(ld);
    Binder server(ld);
    Binder any(ld);

    // by default, only print one binding
    client.print_binding(true);
    server.print_binding(false);
    any.print_binding(false);
    binding_chosen = false;

    // Only set client if specified in ports or protocol.
    // For now, client is the general binding for stream_tcp.
//    client.set_when_role("client");
    server.set_when_role("server");
    any.set_when_role("any");


    // create pointers so other member functinos can access binders
    bind_client = &client;
    bind_server = &server;
    bind_any = &any;

    add_to_bindings(&Binder::set_when_proto, "tcp");
    add_to_bindings(&Binder::set_use_type, "stream_tcp");

    ld->open_table("stream_tcp");


    while(util::get_string(data_stream, keyword, ","))
    {
        bool tmpval = true;
        std::istringstream arg_stream(keyword);

        // should be gauranteed to happen.  Checking for error just cause
        if (!(arg_stream >> keyword))
            tmpval = false;


        if (!keyword.compare("policy"))
            tmpval = parse_string_option("policy", arg_stream);

        else if (!keyword.compare("overlap_limit"))
            tmpval = parse_int_option("overlap_limit", arg_stream);

        else if (!keyword.compare("max_window"))
            tmpval = parse_int_option("max_window", arg_stream);

        else if (!keyword.compare("require_3whs"))
            tmpval = parse_int_option("require_3whs", arg_stream, false);

        else if (!keyword.compare("small_segments"))
            tmpval = parse_small_segments(arg_stream);

        else if (!keyword.compare("ignore_any_rules"))
            tmpval = ld->add_option_to_table("ignore_any_rules", true);

        else if (!keyword.compare("ports"))
            tmpval = parse_ports(arg_stream);

        else if (!keyword.compare("detect_anomalies"))
            ld->add_deleted_comment("detect_anomalies");

        else if (!keyword.compare("dont_store_large_packets"))
            ld->add_deleted_comment("dont_store_large_packets");

        else if (!keyword.compare("check_session_hijacking"))
            ld->add_deleted_comment("check_session_hijacking");

        else if (!keyword.compare("flush_factor"))
            tmpval = parse_int_option("flush_factor", arg_stream);

        else if(!keyword.compare("protocol"))
            tmpval = parse_protocol(arg_stream);

        else if (!keyword.compare("bind_to"))
        {
            ld->add_diff_option_comment("bind_to", "bindings");

            std::string addr;
            if (arg_stream >> addr)
                add_to_bindings(&Binder::add_when_net, addr);
            else
                tmpval = false;
        }

        else if (!keyword.compare("dont_reassemble_async"))
        {
            ld->add_diff_option_comment("dont_reassemble_async", "reassemble_async");
            tmpval = ld->add_option_to_table("reassemble_async", false);
        }

        else if (!keyword.compare("use_static_footprint_sizes"))
        {
            ld->add_diff_option_comment("footprint", "use_static_footprint_sizes");
            tmpval = ld->add_option_to_table("footprint", true);
        }

        else if (!keyword.compare("timeout"))
        {
            ld->add_diff_option_comment("timeout", "session_timeout");
            tmpval = parse_int_option("session_timeout", arg_stream);
        }

        else if (!keyword.compare("max_queued_segs"))
        {
            ld->add_diff_option_comment("max_queued_segs", "queue_limit.max_segments");
            ld->open_table("queue_limit");
            tmpval = parse_int_option("max_segments", arg_stream);
            ld->close_table();
        }

        else if (!keyword.compare("max_queued_bytes"))
        {
            ld->add_diff_option_comment("max_queued_bytes", "queue_limit.max_bytes");
            ld->open_table("queue_limit");
            tmpval = parse_int_option("max_bytes", arg_stream);
            ld->close_table();
        }

        else
        {
            tmpval = false;
        }

        if (!tmpval)
        {
            ld->failed_conversion(data_stream, keyword);
            retval = false;
        }
    }



    if (!client_protocols.empty())
    {
        for (std::string s : client_protocols)
        {
            Binder b = client;
            b.set_when_service(s);
            b.add_to_configuration();
        }
        client.print_binding(false); // we just printed
    }

    if (!server_protocols.empty())
    {
        for (std::string s : server_protocols)
        {
            Binder b = server;
            b.set_when_service(s);
            b.add_to_configuration();
        }
        server.print_binding(false); // we just printed
    }

    if (!any_protocols.empty())
    {
        for (std::string s : any_protocols)
        {
            Binder b = any;
            b.set_when_service(s);
            b.add_to_configuration();
        }
        any.print_binding(false); // we just printed
    }

    ld->close_table(); // "tcp_stream"
    return retval;
}

/**************************
 *******  A P I ***********
 **************************/

static ConversionState* ctor(Converter* cv, LuaData* ld)
{
    return new StreamTcp(cv, ld);
}

static const ConvertMap preprocessor_stream_tcp = 
{
    "stream5_tcp",
    ctor,
};

const ConvertMap* stream_tcp_map = &preprocessor_stream_tcp;

} // namespace preprocessors
