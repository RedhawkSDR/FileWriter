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

#ifndef FILEWRITER__IMPL_H
#define FILEWRITER__IMPL_H

#include "FileWriter_base.h"
#include <vector>
#include <queue>
#include <complex>
#include <set>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <stdexcept>
#include <errno.h>
#include <string.h>
#include <uuid/uuid.h>
#include <ossie/prop_helpers.h>
#include <ossie/Resource_impl.h>
#include <omniORB4/CORBA.h>
#include <omniORB4/omniURI.h>
#include <omniORB4/omniORB.h>
#include <bulkio/bulkio.h>
#include <iostream>
#include <ctime>
#include <sys/types.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <HeaderControlBlock.h>
#include <ExtendedHeader.h>
#include <MidasKey.h>
#include <abstracted_file_io.h>
#include <byte_swap.h>
#include <boost_compat.h>
class FileWriter_i;

#define METADATA_EXTENSION ".metadata.xml"
#define BLUEFILE_BLOCK_SIZE 512   // Exact size of fixed header

namespace FILE_WRITER_DOMAIN_MGR_HELPERS {

    inline CF::DomainManager_var domainManager_id_to_var(std::string id) {
        CF::DomainManager_var domainMgr_var = CF::DomainManager::_nil();
        CosNaming::BindingIterator_var it;
        CosNaming::BindingList_var bl;
        CosNaming::NamingContext_var context = CosNaming::NamingContext::_narrow(ossie::corba::InitialNamingContext());
        context->list(100, bl, it);
        for (unsigned int ii = 0; ii < bl->length(); ++ii) {
            try {
                std::string domString = std::string(bl[ii].binding_name[0].id) + "/" + std::string(bl[ii].binding_name[0].id);
                CosNaming::Name_var cosName = omni::omniURI::stringToName(domString.c_str());
                CORBA::Object_var bobj = context->resolve(cosName);
                domainMgr_var = CF::DomainManager::_narrow(bobj);
                if (id.empty() || id == std::string(domainMgr_var->identifier())){
                    return domainMgr_var;
                }
            } catch (...) {};
        }
        return domainMgr_var;
    }
};

enum FILE_TYPES{
    RAW = 0,
    BLUEFILE = 1
};
struct file_struct{
    file_struct(std::string uri_full_filename, FILE_TYPES type, double start_ws, double start_fs, bool enable_metadata, bool hidden_tmp_files,
                const std::string& open_file_extension, const std::string& open_metadata_file_extension, const std::string& file_stream_id)
    {
        uri_filename = uri_full_filename;
        uri_metadata_filename = uri_filename + METADATA_EXTENSION;
        in_process_uri_filename = "";
        in_process_uri_metadata_filename = "";
        file_size_internal = 0;
        num_writers = 1;
        file_type = type;
        start_time_ws = start_ws;
        start_time_fs = start_fs;
        stream_id = file_stream_id;
        midas_type = "";

        boost::filesystem::path uri_path = BOOST_FILESYSTEM_PATH(uri_filename);
        basename = BOOST_PATH_STRING(uri_path.filename());
        dirname = uri_path.parent_path().string();


        // Write to tmp file
        if(hidden_tmp_files){
            in_process_uri_filename = dirname + "/." + basename ;
            in_process_uri_metadata_filename = dirname + "/." + basename+ METADATA_EXTENSION ;
        }
        else{
            in_process_uri_filename = dirname + "/" + basename;
            in_process_uri_metadata_filename = dirname + "/" + basename+ METADATA_EXTENSION ;
        }
        if(!open_file_extension.empty())
            in_process_uri_filename += "." + open_file_extension;
        if(!open_metadata_file_extension.empty())
            in_process_uri_metadata_filename += "." + open_metadata_file_extension;

        // Disabling of the metadata file
        if(!enable_metadata){
            uri_metadata_filename = "";
            in_process_uri_metadata_filename = "";
        }


    };
    bool metdata_file_enabled(){
        return !uri_metadata_filename.empty();
    }
    std::string uri_filename;
    std::string uri_metadata_filename;

    std::string in_process_uri_filename;
    std::string in_process_uri_metadata_filename;

    std::string basename;
    std::string dirname;
    unsigned long long file_size_internal;
    size_t num_writers;
    FILE_TYPES file_type;
    double start_time_ws;
    double start_time_fs;
    std::string stream_id;
    BULKIO::StreamSRI lastSRI;
    std::string midas_type;
};


class FileWriter_i : public FileWriter_base {
    typedef boost::mutex::scoped_lock exclusive_lock;
    ENABLE_LOGGING;
public:
    FileWriter_i(const char *uuid, const char *label);
    ~FileWriter_i();
    int serviceFunction();

    void start() throw (CF::Resource::StartError, CORBA::SystemException);
    void stop() throw (CF::Resource::StopError, CORBA::SystemException);
    void constructor ();
private:
    /*property change listener methods*/
    void destination_uriChanged(std::string oldValue, std::string newValue);
    void destination_uri_suffixChanged(std::string oldValue, std::string newValue);
    void change_uri();
    void file_formatChanged(std::string oldValue, std::string newValue);
    void recording_enabledChanged(bool oldValue, const bool *newValue);
    void advanced_propertiesChanged(const advanced_properties_struct &oldValue, const advanced_properties_struct &newValue);
    void recording_timerChanged(const std::vector<timer_struct_struct> &oldValue, const std::vector<timer_struct_struct> &newValue);
    void construct_recording_timer(const std::vector<timer_struct_struct> &timers);
    void input_bulkio_byte_orderChanged(std::string oldValue, std::string newValue);
    
    long maxSize;
    std::string prop_dirname;
    std::string prop_basename;
    std::string prop_full_filename;
    std::pair<blue::HeaderControlBlock,std::vector<char> >
       createBluefilesHeaders(const BULKIO::StreamSRI& sri, size_t datasize, std::string midasType, double start_ws, double start_fs);

    std::string sri_to_XMLstring(const BULKIO::StreamSRI& sri,const bool newsri);
    std::string packet_to_XMLstring(const int packetSize, const BULKIO::StreamSRI& sri, const BULKIO::PrecisionUTCTime& timecode, const bool& eos,const size_t packetPosition,const size_t elementSize);

    template <typename IN_PORT_TYPE> std::string stream_to_basename(const std::string & stream_id,const BULKIO::StreamSRI& sri, const BULKIO::PrecisionUTCTime &_T, const std::string & extension);
    template <class IN_PORT_TYPE> bool singleService(IN_PORT_TYPE *dataIn);
    size_t sizeString_to_longBytes(std::string size);
    bool close_file(const std::string& filename, const BULKIO::PrecisionUTCTime & timestamp, std::string streamId = "");

    void mergeKeywords(BULKIO::StreamSRI::_keywords_seq newkeywords);

    // Ensure that configure() and serviceFunction() are thread safe
    boost::mutex service_thread_lock;
    ABSTRACTED_FILE_IO::abstracted_file_io filesystem;
    FILE_TYPES current_writer_type;

    BULKIO::StreamSRI::_keywords_seq allKeywords;

    std::map<std::string, std::string> stream_to_file_mapping;
    std::map<std::string, file_struct> file_to_struct_mapping;

    long BULKIO_BYTE_ORDER;

    bool remove_file_from_filesystem(const std::string& filename){
        if(!filename.empty()){
            return filesystem.delete_file(filename);
        }
        return false;
    }

    file_io_message_struct create_file_io_message(const std::string& file_operation, const std::string& stream_id, const std::string& filename){
        file_io_message_struct f;
        f.file_operation = file_operation;
        f.stream_id = stream_id;
        f.filename = filename;
        return f;
    }

    inline std::string replace_string(std::string whole_string, const std::string& cur_substr, const std::string& new_substr) {
        boost::algorithm::replace_all(whole_string,cur_substr,new_substr);
        return whole_string;
    }
    void toUpper(std::string& s) {
        for (std::string::iterator p = s.begin(); p != s.end(); ++p) {
            *p = toupper(*p);
        }
    }
    void toLower(std::string& s) {
        for (std::string::iterator p = s.begin(); p != s.end(); ++p) {
            *p = tolower(*p);
        }
    }

    inline double J1950_to_J1970(const double& _j1950){
        return _j1950 + double(631152000);
    }
    inline double J1970_to_J1950(const double& _j1970){
        return _j1970 - double(631152000);
    }

    template <typename IN_PORT_TYPE> std::string data_format_string(){

        // Add 'r' to mean "reversed" or Little Endian when:
        //   - atom size is greater than 1 Byte (8 bits)
        //   - Byte order is Little Endian and we ARE NOT byte swapping
        //   - Byte order is Big Endian and we ARE byte swapping
        std::string endian = "";
        if((BULKIO_BYTE_ORDER==LITTLE_ENDIAN) ^ swap_bytes)
            endian = "r";

        std::string format = "";
        if (typeid(IN_PORT_TYPE) == typeid(bulkio::InCharPort) || typeid(IN_PORT_TYPE) == typeid(bulkio::InXMLPort)) {
            format = "8t";
        } else if (typeid(IN_PORT_TYPE) == typeid(bulkio::InOctetPort)) {
            format = "8o";
        } else if (typeid(IN_PORT_TYPE) == typeid(bulkio::InShortPort)) {
            format = "16t" + endian;
        } else if (typeid(IN_PORT_TYPE) == typeid(bulkio::InUShortPort)) {
            format = "16o" + endian;
        } else if (typeid(IN_PORT_TYPE) == typeid(bulkio::InFloatPort)) {
            format = "32f" + endian;
        } else if (typeid(IN_PORT_TYPE) == typeid(bulkio::InDoublePort)) {
            format = "64f" + endian;
        } else {
            LOG_ERROR(FileWriter_i,"Could not determine data format string; Defaulting to 8o for unsigned bytes.");
            format = "8o";
        }

        LOG_DEBUG(FileWriter_i,"data_format_string="<<format<<"  BYTE_ORDER="<<BULKIO_BYTE_ORDER<<"  swap_bytes="<<swap_bytes);
        return format;
    }

    template <typename DATA_TYPE> std::string midas_type(bool isReal){
        // Real/Complex
        std::string format = "S";
        if(!isReal)
            format = "C";

        //Type
        if(typeid(DATA_TYPE) == typeid(unsigned char))
            format += "B";
        else if (typeid(DATA_TYPE) == typeid(char))
            format += "B";
        else if (typeid(DATA_TYPE) == typeid(unsigned short))
            format += "I";
        else if (typeid(DATA_TYPE) == typeid(short))
            format += "I";
        else if (typeid(DATA_TYPE) == typeid(unsigned long))
            format += "L";
        else if (typeid(DATA_TYPE) == typeid(long))
            format += "L";
        else if (typeid(DATA_TYPE) == typeid(unsigned long long))
            format += "X";
        else if (typeid(DATA_TYPE) == typeid(long long))
            format += "X";
        else if (typeid(DATA_TYPE) == typeid(float))
            format += "F";
        else if (typeid(DATA_TYPE) == typeid(double))
            format += "D";
        else
            format += "B";

        return format;

    }

    struct utc_time_comp {
        bool operator()(const BULKIO::PrecisionUTCTime & __x, const BULKIO::PrecisionUTCTime & __y) const {
            if (__x.twsec != __y.twsec)
                return __x.twsec < __y.twsec;
            return __x.tfsec < __y.tfsec;

        }
    };
    inline bool compare_utc_time(const BULKIO::PrecisionUTCTime & time1, const BULKIO::PrecisionUTCTime & time2) const {
        if (time1.twsec != time2.twsec)
            return time1.twsec < time2.twsec;
        return time1.tfsec < time2.tfsec;
    }
    std::set<BULKIO::PrecisionUTCTime,utc_time_comp> timer_set;
    std::set<BULKIO::PrecisionUTCTime,utc_time_comp>::iterator timer_set_iter;
    utc_time_comp utc_time_comp_obj;
    BULKIO::PrecisionUTCTime timer_timestamp;

    inline  BULKIO::PrecisionUTCTime getSystemTimestamp( double additional_time = 0.0 ) {
            double  whole;
            double  fract = modf( additional_time, &whole );
            struct timeval tmp_time;
            struct timezone tmp_tz;
            gettimeofday( &tmp_time, &tmp_tz );
            double  wsec = tmp_time.tv_sec;
            double  fsec = tmp_time.tv_usec / 1e6;
            BULKIO::PrecisionUTCTime tstamp = BULKIO::PrecisionUTCTime(  );
            tstamp.tcmode = 1;
            tstamp.tcstatus = ( short )1;
            tstamp.toff = 0.0;
            tstamp.twsec = wsec + whole;
            tstamp.tfsec = fsec + fract;
            while( tstamp.tfsec < 0 ) {
                tstamp.twsec -= 1.0;
                tstamp.tfsec += 1.0;
            }
            return tstamp;
        };

    inline bool is_ts_invalid(const BULKIO::PrecisionUTCTime & tstamp){
        bool invalid = (tstamp.tcstatus == BULKIO::TCS_INVALID);
        invalid |= (!std::isfinite(tstamp.twsec));
        invalid |= (tstamp.twsec < 0 || tstamp.twsec > 1e10);
        invalid |= (!std::isfinite(tstamp.tfsec));
        invalid |= (tstamp.tfsec < 0 || tstamp.tfsec > 1e10);
        return invalid;
    };

    inline  std::string time_to_string( const BULKIO::PrecisionUTCTime & timeTag, bool include_fractional = true, bool compressed = false ) {
            double wsec = 0.0;
            double fsec = 0.0;
            if(!is_ts_invalid(timeTag)){
                wsec = timeTag.twsec;
                fsec =  timeTag.tfsec;
            }

            time_t  _fileStartTime;
            struct tm *local;
            char    timeArray[30];
            _fileStartTime = ( time_t ) wsec;
            local = gmtime( &_fileStartTime );      //converts second since epoch to tm struct
            if( compressed )
                strftime( timeArray, 30, "%d%b%Y.%H%M%S", local );  //prints out string from tm struct
            else
                strftime( timeArray, 30, "%d-%b-%Y %H:%M:%S", local );      //prints out string from tm struct
            std::string time = std::string( timeArray );
            if( include_fractional ) {
                char    fractSec[30];
                sprintf( fractSec, "%010.0f", fsec * 1e10 );
                time += "." + std::string( fractSec );
            }
            return time;
        }

    template <typename MYTYPE> MYTYPE getKeywordValueByID(BULKIO::StreamSRI *sri, CORBA::String_member id) throw(std::logic_error){
        MYTYPE value;
        for (unsigned int i = 0; i < sri->keywords.length(); i++) {
            if (!strcmp(sri->keywords[i].id, id)) {
                sri->keywords[i].value >>= value;
                return value;
            }
        }
        throw std::logic_error("KEYWORD NOT FOUND!");
    }

    template <typename MYTYPE> bool updateIfFound_KeywordValueByID(BULKIO::StreamSRI *sri, CORBA::String_member id, MYTYPE& current) throw(std::logic_error){
        try {
            MYTYPE value = getKeywordValueByID<MYTYPE>(sri, id);
            current = value;
            return true;
        } catch (...) {
        };
        return false;
    }

};

#endif
