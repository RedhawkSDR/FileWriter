/*
 * This file is protected by Copyright. Please refer to the COPYRIGHT file
 * distributed with this source distribution.
 *
 * This file is part of REDHAWK Basic Components FileWriter.
 *
 * REDHAWK Basic Components FileWriter is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * REDHAWK Basic Components FileWriter is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/.
 */

#ifndef STRUCTPROPS_H
#define STRUCTPROPS_H

/*******************************************************************************************

    AUTO-GENERATED CODE. DO NOT MODIFY

*******************************************************************************************/

#include <ossie/CorbaUtils.h>
#include <CF/cf.h>
#include <ossie/PropertyMap.h>

struct advanced_properties_struct {
    advanced_properties_struct ()
    {
        debug_output = false;
        force_flush = false;
        max_file_size = "UNLIMITED";
        max_file_time = -1;
        existing_file = "RENAME";
        create_destination_dir = true;
        enable_metadata_file = false;
        reset_on_max_file = true;
        reset_on_retune = true;
        use_hidden_files = true;
        open_file_extension = "inProgress";
        open_metadata_file_extension = "inProgress";
        use_tc_prec = true;
        output_filename_case = 0;
    };

    static std::string getId() {
        return std::string("advanced_properties");
    };

    bool debug_output;
    bool force_flush;
    std::string max_file_size;
    CORBA::Long max_file_time;
    std::string existing_file;
    bool create_destination_dir;
    bool enable_metadata_file;
    bool reset_on_max_file;
    bool reset_on_retune;
    bool use_hidden_files;
    std::string open_file_extension;
    std::string open_metadata_file_extension;
    bool use_tc_prec;
    short output_filename_case;
};

inline bool operator>>= (const CORBA::Any& a, advanced_properties_struct& s) {
    CF::Properties* temp;
    if (!(a >>= temp)) return false;
    const redhawk::PropertyMap& props = redhawk::PropertyMap::cast(*temp);
    if (props.contains("advanced_properties::debug_output")) {
        if (!(props["advanced_properties::debug_output"] >>= s.debug_output)) return false;
    }
    if (props.contains("advanced_properties::force_flush")) {
        if (!(props["advanced_properties::force_flush"] >>= s.force_flush)) return false;
    }
    if (props.contains("advanced_properties::max_file_size")) {
        if (!(props["advanced_properties::max_file_size"] >>= s.max_file_size)) return false;
    }
    if (props.contains("advanced_properties::max_file_time")) {
        if (!(props["advanced_properties::max_file_time"] >>= s.max_file_time)) return false;
    }
    if (props.contains("advanced_properties::existing_file")) {
        if (!(props["advanced_properties::existing_file"] >>= s.existing_file)) return false;
    }
    if (props.contains("advanced_properties::create_destination_dir")) {
        if (!(props["advanced_properties::create_destination_dir"] >>= s.create_destination_dir)) return false;
    }
    if (props.contains("advanced_properties::enable_metadata_file")) {
        if (!(props["advanced_properties::enable_metadata_file"] >>= s.enable_metadata_file)) return false;
    }
    if (props.contains("advanced_properties::reset_on_max_file")) {
        if (!(props["advanced_properties::reset_on_max_file"] >>= s.reset_on_max_file)) return false;
    }
    if (props.contains("advanced_properties::reset_on_retune")) {
        if (!(props["advanced_properties::reset_on_retune"] >>= s.reset_on_retune)) return false;
    }
    if (props.contains("advanced_properties::use_hidden_files")) {
        if (!(props["advanced_properties::use_hidden_files"] >>= s.use_hidden_files)) return false;
    }
    if (props.contains("advanced_properties::open_file_extension")) {
        if (!(props["advanced_properties::open_file_extension"] >>= s.open_file_extension)) return false;
    }
    if (props.contains("advanced_properties::open_metadata_file_extension")) {
        if (!(props["advanced_properties::open_metadata_file_extension"] >>= s.open_metadata_file_extension)) return false;
    }
    if (props.contains("advanced_properties::use_tc_prec")) {
        if (!(props["advanced_properties::use_tc_prec"] >>= s.use_tc_prec)) return false;
    }
    if (props.contains("advanced_properties::output_filename_case")) {
        if (!(props["advanced_properties::output_filename_case"] >>= s.output_filename_case)) return false;
    }
    return true;
}

inline void operator<<= (CORBA::Any& a, const advanced_properties_struct& s) {
    redhawk::PropertyMap props;
 
    props["advanced_properties::debug_output"] = s.debug_output;
 
    props["advanced_properties::force_flush"] = s.force_flush;
 
    props["advanced_properties::max_file_size"] = s.max_file_size;
 
    props["advanced_properties::max_file_time"] = s.max_file_time;
 
    props["advanced_properties::existing_file"] = s.existing_file;
 
    props["advanced_properties::create_destination_dir"] = s.create_destination_dir;
 
    props["advanced_properties::enable_metadata_file"] = s.enable_metadata_file;
 
    props["advanced_properties::reset_on_max_file"] = s.reset_on_max_file;
 
    props["advanced_properties::reset_on_retune"] = s.reset_on_retune;
 
    props["advanced_properties::use_hidden_files"] = s.use_hidden_files;
 
    props["advanced_properties::open_file_extension"] = s.open_file_extension;
 
    props["advanced_properties::open_metadata_file_extension"] = s.open_metadata_file_extension;
 
    props["advanced_properties::use_tc_prec"] = s.use_tc_prec;
 
    props["advanced_properties::output_filename_case"] = s.output_filename_case;
    a <<= props;
}

inline bool operator== (const advanced_properties_struct& s1, const advanced_properties_struct& s2) {
    if (s1.debug_output!=s2.debug_output)
        return false;
    if (s1.force_flush!=s2.force_flush)
        return false;
    if (s1.max_file_size!=s2.max_file_size)
        return false;
    if (s1.max_file_time!=s2.max_file_time)
        return false;
    if (s1.existing_file!=s2.existing_file)
        return false;
    if (s1.create_destination_dir!=s2.create_destination_dir)
        return false;
    if (s1.enable_metadata_file!=s2.enable_metadata_file)
        return false;
    if (s1.reset_on_max_file!=s2.reset_on_max_file)
        return false;
    if (s1.reset_on_retune!=s2.reset_on_retune)
        return false;
    if (s1.use_hidden_files!=s2.use_hidden_files)
        return false;
    if (s1.open_file_extension!=s2.open_file_extension)
        return false;
    if (s1.open_metadata_file_extension!=s2.open_metadata_file_extension)
        return false;
    if (s1.use_tc_prec!=s2.use_tc_prec)
        return false;
    if (s1.output_filename_case!=s2.output_filename_case)
        return false;
    return true;
}

inline bool operator!= (const advanced_properties_struct& s1, const advanced_properties_struct& s2) {
    return !(s1==s2);
}

struct file_io_message_struct {
    file_io_message_struct ()
    {
        file_operation = "OPEN";
    };

    static std::string getId() {
        return std::string("file_io_message");
    };

    std::string file_operation;
    std::string stream_id;
    std::string filename;
};

inline bool operator>>= (const CORBA::Any& a, file_io_message_struct& s) {
    CF::Properties* temp;
    if (!(a >>= temp)) return false;
    const redhawk::PropertyMap& props = redhawk::PropertyMap::cast(*temp);
    if (props.contains("file_io_message::file_operation")) {
        if (!(props["file_io_message::file_operation"] >>= s.file_operation)) return false;
    }
    if (props.contains("file_io_message::stream_id")) {
        if (!(props["file_io_message::stream_id"] >>= s.stream_id)) return false;
    }
    if (props.contains("file_io_message::filename")) {
        if (!(props["file_io_message::filename"] >>= s.filename)) return false;
    }
    return true;
}

inline void operator<<= (CORBA::Any& a, const file_io_message_struct& s) {
    redhawk::PropertyMap props;
 
    props["file_io_message::file_operation"] = s.file_operation;
 
    props["file_io_message::stream_id"] = s.stream_id;
 
    props["file_io_message::filename"] = s.filename;
    a <<= props;
}

inline bool operator== (const file_io_message_struct& s1, const file_io_message_struct& s2) {
    if (s1.file_operation!=s2.file_operation)
        return false;
    if (s1.stream_id!=s2.stream_id)
        return false;
    if (s1.filename!=s2.filename)
        return false;
    return true;
}

inline bool operator!= (const file_io_message_struct& s1, const file_io_message_struct& s2) {
    return !(s1==s2);
}

struct component_status_struct {
    component_status_struct ()
    {
        domain_name = "(domainless)";
    };

    static std::string getId() {
        return std::string("component_status");
    };

    std::string domain_name;
};

inline bool operator>>= (const CORBA::Any& a, component_status_struct& s) {
    CF::Properties* temp;
    if (!(a >>= temp)) return false;
    const redhawk::PropertyMap& props = redhawk::PropertyMap::cast(*temp);
    if (props.contains("component_status::domain_name")) {
        if (!(props["component_status::domain_name"] >>= s.domain_name)) return false;
    }
    return true;
}

inline void operator<<= (CORBA::Any& a, const component_status_struct& s) {
    redhawk::PropertyMap props;
 
    props["component_status::domain_name"] = s.domain_name;
    a <<= props;
}

inline bool operator== (const component_status_struct& s1, const component_status_struct& s2) {
    if (s1.domain_name!=s2.domain_name)
        return false;
    return true;
}

inline bool operator!= (const component_status_struct& s1, const component_status_struct& s2) {
    return !(s1==s2);
}

struct timer_struct_struct {
    timer_struct_struct ()
    {
        recording_enable = true;
        use_pkt_timestamp = false;
        twsec = 0;
        tfsec = 0;
    };

    static std::string getId() {
        return std::string("recording_timer::timer_struct");
    };

    bool recording_enable;
    bool use_pkt_timestamp;
    double twsec;
    double tfsec;
};

inline bool operator>>= (const CORBA::Any& a, timer_struct_struct& s) {
    CF::Properties* temp;
    if (!(a >>= temp)) return false;
    const redhawk::PropertyMap& props = redhawk::PropertyMap::cast(*temp);
    if (props.contains("recording_timer::recording_enable")) {
        if (!(props["recording_timer::recording_enable"] >>= s.recording_enable)) return false;
    }
    if (props.contains("recording_timer::use_pkt_timestamp")) {
        if (!(props["recording_timer::use_pkt_timestamp"] >>= s.use_pkt_timestamp)) return false;
    }
    if (props.contains("recording_timer::twsec")) {
        if (!(props["recording_timer::twsec"] >>= s.twsec)) return false;
    }
    if (props.contains("recording_timer::tfsec")) {
        if (!(props["recording_timer::tfsec"] >>= s.tfsec)) return false;
    }
    return true;
}

inline void operator<<= (CORBA::Any& a, const timer_struct_struct& s) {
    redhawk::PropertyMap props;
 
    props["recording_timer::recording_enable"] = s.recording_enable;
 
    props["recording_timer::use_pkt_timestamp"] = s.use_pkt_timestamp;
 
    props["recording_timer::twsec"] = s.twsec;
 
    props["recording_timer::tfsec"] = s.tfsec;
    a <<= props;
}

inline bool operator== (const timer_struct_struct& s1, const timer_struct_struct& s2) {
    if (s1.recording_enable!=s2.recording_enable)
        return false;
    if (s1.use_pkt_timestamp!=s2.use_pkt_timestamp)
        return false;
    if (s1.twsec!=s2.twsec)
        return false;
    if (s1.tfsec!=s2.tfsec)
        return false;
    return true;
}

inline bool operator!= (const timer_struct_struct& s1, const timer_struct_struct& s2) {
    return !(s1==s2);
}

#endif // STRUCTPROPS_H
