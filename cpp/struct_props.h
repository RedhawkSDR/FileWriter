#ifndef STRUCTPROPS_H
#define STRUCTPROPS_H

/*******************************************************************************************

    AUTO-GENERATED CODE. DO NOT MODIFY

*******************************************************************************************/

#include <ossie/CorbaUtils.h>

struct advanced_properties_struct {
    advanced_properties_struct ()
    {
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
    };

    static std::string getId() {
        return std::string("advanced_properties");
    };

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
};

inline bool operator>>= (const CORBA::Any& a, advanced_properties_struct& s) {
    CF::Properties* temp;
    if (!(a >>= temp)) return false;
    CF::Properties& props = *temp;
    for (unsigned int idx = 0; idx < props.length(); idx++) {
        if (!strcmp("advanced_properties::force_flush", props[idx].id)) {
            if (!(props[idx].value >>= s.force_flush)) return false;
        }
        else if (!strcmp("advanced_properties::max_file_size", props[idx].id)) {
            if (!(props[idx].value >>= s.max_file_size)) return false;
        }
        else if (!strcmp("advanced_properties::max_file_time", props[idx].id)) {
            if (!(props[idx].value >>= s.max_file_time)) return false;
        }
        else if (!strcmp("advanced_properties::existing_file", props[idx].id)) {
            if (!(props[idx].value >>= s.existing_file)) return false;
        }
        else if (!strcmp("advanced_properties::create_destination_dir", props[idx].id)) {
            if (!(props[idx].value >>= s.create_destination_dir)) return false;
        }
        else if (!strcmp("advanced_properties::enable_metadata_file", props[idx].id)) {
            if (!(props[idx].value >>= s.enable_metadata_file)) return false;
        }
        else if (!strcmp("advanced_properties::reset_on_max_file", props[idx].id)) {
            if (!(props[idx].value >>= s.reset_on_max_file)) return false;
        }
        else if (!strcmp("advanced_properties::reset_on_retune", props[idx].id)) {
            if (!(props[idx].value >>= s.reset_on_retune)) return false;
        }
        else if (!strcmp("advanced_properties::use_hidden_files", props[idx].id)) {
            if (!(props[idx].value >>= s.use_hidden_files)) return false;
        }
        else if (!strcmp("advanced_properties::open_file_extension", props[idx].id)) {
            if (!(props[idx].value >>= s.open_file_extension)) return false;
        }
        else if (!strcmp("advanced_properties::open_metadata_file_extension", props[idx].id)) {
            if (!(props[idx].value >>= s.open_metadata_file_extension)) return false;
        }
    }
    return true;
};

inline void operator<<= (CORBA::Any& a, const advanced_properties_struct& s) {
    CF::Properties props;
    props.length(11);
    props[0].id = CORBA::string_dup("advanced_properties::force_flush");
    props[0].value <<= s.force_flush;
    props[1].id = CORBA::string_dup("advanced_properties::max_file_size");
    props[1].value <<= s.max_file_size;
    props[2].id = CORBA::string_dup("advanced_properties::max_file_time");
    props[2].value <<= s.max_file_time;
    props[3].id = CORBA::string_dup("advanced_properties::existing_file");
    props[3].value <<= s.existing_file;
    props[4].id = CORBA::string_dup("advanced_properties::create_destination_dir");
    props[4].value <<= s.create_destination_dir;
    props[5].id = CORBA::string_dup("advanced_properties::enable_metadata_file");
    props[5].value <<= s.enable_metadata_file;
    props[6].id = CORBA::string_dup("advanced_properties::reset_on_max_file");
    props[6].value <<= s.reset_on_max_file;
    props[7].id = CORBA::string_dup("advanced_properties::reset_on_retune");
    props[7].value <<= s.reset_on_retune;
    props[8].id = CORBA::string_dup("advanced_properties::use_hidden_files");
    props[8].value <<= s.use_hidden_files;
    props[9].id = CORBA::string_dup("advanced_properties::open_file_extension");
    props[9].value <<= s.open_file_extension;
    props[10].id = CORBA::string_dup("advanced_properties::open_metadata_file_extension");
    props[10].value <<= s.open_metadata_file_extension;
    a <<= props;
};

inline bool operator== (const advanced_properties_struct& s1, const advanced_properties_struct& s2) {
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
    return true;
};

inline bool operator!= (const advanced_properties_struct& s1, const advanced_properties_struct& s2) {
    return !(s1==s2);
};

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
    CF::Properties& props = *temp;
    for (unsigned int idx = 0; idx < props.length(); idx++) {
        if (!strcmp("file_io_message::file_operation", props[idx].id)) {
            if (!(props[idx].value >>= s.file_operation)) return false;
        }
        else if (!strcmp("file_io_message::stream_id", props[idx].id)) {
            if (!(props[idx].value >>= s.stream_id)) return false;
        }
        else if (!strcmp("file_io_message::filename", props[idx].id)) {
            if (!(props[idx].value >>= s.filename)) return false;
        }
    }
    return true;
};

inline void operator<<= (CORBA::Any& a, const file_io_message_struct& s) {
    CF::Properties props;
    props.length(3);
    props[0].id = CORBA::string_dup("file_io_message::file_operation");
    props[0].value <<= s.file_operation;
    props[1].id = CORBA::string_dup("file_io_message::stream_id");
    props[1].value <<= s.stream_id;
    props[2].id = CORBA::string_dup("file_io_message::filename");
    props[2].value <<= s.filename;
    a <<= props;
};

inline bool operator== (const file_io_message_struct& s1, const file_io_message_struct& s2) {
    if (s1.file_operation!=s2.file_operation)
        return false;
    if (s1.stream_id!=s2.stream_id)
        return false;
    if (s1.filename!=s2.filename)
        return false;
    return true;
};

inline bool operator!= (const file_io_message_struct& s1, const file_io_message_struct& s2) {
    return !(s1==s2);
};

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
    CF::Properties& props = *temp;
    for (unsigned int idx = 0; idx < props.length(); idx++) {
        if (!strcmp("recording_timer::timer_struct::recording_enable", props[idx].id)) {
            if (!(props[idx].value >>= s.recording_enable)) return false;
        }
        else if (!strcmp("recording_timer::timer_struct::use_pkt_timestamp", props[idx].id)) {
            if (!(props[idx].value >>= s.use_pkt_timestamp)) return false;
        }
        else if (!strcmp("recording_timer::timer_struct::twsec", props[idx].id)) {
            if (!(props[idx].value >>= s.twsec)) return false;
        }
        else if (!strcmp("recording_timer::timer_struct::tfsec", props[idx].id)) {
            if (!(props[idx].value >>= s.tfsec)) return false;
        }
    }
    return true;
};

inline void operator<<= (CORBA::Any& a, const timer_struct_struct& s) {
    CF::Properties props;
    props.length(4);
    props[0].id = CORBA::string_dup("recording_timer::timer_struct::recording_enable");
    props[0].value <<= s.recording_enable;
    props[1].id = CORBA::string_dup("recording_timer::timer_struct::use_pkt_timestamp");
    props[1].value <<= s.use_pkt_timestamp;
    props[2].id = CORBA::string_dup("recording_timer::timer_struct::twsec");
    props[2].value <<= s.twsec;
    props[3].id = CORBA::string_dup("recording_timer::timer_struct::tfsec");
    props[3].value <<= s.tfsec;
    a <<= props;
};

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
};

inline bool operator!= (const timer_struct_struct& s1, const timer_struct_struct& s2) {
    return !(s1==s2);
};

#endif // STRUCTPROPS_H
