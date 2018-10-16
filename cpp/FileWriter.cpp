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

/**************************************************************************

    This is the component code. This file contains the child class where
    custom functionality can be added to the component. Custom
    functionality to the base class can be extended here. Access to
    the ports can also be done from this class

 **************************************************************************/

#include "FileWriter.h"

PREPARE_LOGGING(FileWriter_i)

FileWriter_i::FileWriter_i(const char *uuid, const char *label) :
FileWriter_base(uuid, label) {
    //properties are not updated until callback function is called and they are explicitly set.
    addPropertyListener(destination_uri, this, &FileWriter_i::destination_uriChanged);
    addPropertyListener(destination_uri_suffix, this, &FileWriter_i::destination_uri_suffixChanged);
    addPropertyListener(file_format, this, &FileWriter_i::file_formatChanged);
    addPropertyListener(advanced_properties, this, &FileWriter_i::advanced_propertiesChanged);
    addPropertyListener(recording_timer, this, &FileWriter_i::recording_timerChanged);
}

FileWriter_i::~FileWriter_i() {
}

/*
* REDHAWK constructor. All properties are initialized before this constructor is called
*/
void FileWriter_i::constructor() {

    // accounts for initial values of properties since callbacks are not called
    construct_recording_timer(recording_timer);
    if (file_format == "BLUEFILE")
        current_writer_type = BLUEFILE;
    else
        current_writer_type = RAW;
    maxSize = sizeString_to_longBytes(advanced_properties.max_file_size);
    change_uri();
}

void FileWriter_i::start() throw (CF::Resource::StartError, CORBA::SystemException) {
    FileWriter_base::start();
}

void FileWriter_i::stop() throw (CF::Resource::StopError, CORBA::SystemException) {
    FileWriter_base::stop();

    exclusive_lock lock(service_thread_lock);

    for (std::map<std::string, std::string>::iterator curFileIter = stream_to_file_mapping.begin(); curFileIter != stream_to_file_mapping.end(); curFileIter++) {
        close_file(curFileIter->second, getSystemTimestamp());
    }
    stream_to_file_mapping.clear();
    timer_set_iter = timer_set.end();
}

void FileWriter_i::destination_uriChanged(std::string oldValue, std::string newValue) {
    exclusive_lock lock(service_thread_lock);
    if (oldValue != newValue) {
        change_uri();
    }
}

void FileWriter_i::destination_uri_suffixChanged(std::string oldValue, std::string newValue) {
    exclusive_lock lock(service_thread_lock);
    if (oldValue != newValue) {
        change_uri();
    }
}

void FileWriter_i::change_uri() {
    std::string destination_uri_full = destination_uri + destination_uri_suffix;
    std::string normFile = filesystem.normalize_uri_path(destination_uri_full);
    std::string prefix, dir, base;
    ABSTRACTED_FILE_IO::FILESYSTEM_TYPE type;
    filesystem.uri_path_extraction(normFile, dir, base, type);
    prefix = ABSTRACTED_FILE_IO::local_uri_prefix;
    if (type == ABSTRACTED_FILE_IO::SCA_FILESYSTEM) {
        prefix = ABSTRACTED_FILE_IO::sca_uri_prefix;
        try {
            if(!filesystem.is_sca_file_manager_valid()){
                if (getDomainManager() && !CORBA::is_nil(getDomainManager()->getRef())) {
                    std::string dom_id = ossie::corba::returnString(getDomainManager()->getRef()->identifier());
                    CF::DomainManager_var dm = FILE_WRITER_DOMAIN_MGR_HELPERS::domainManager_id_to_var(dom_id);
                    if (!CORBA::is_nil(dm)){
                        filesystem.update_sca_file_manager(dm->fileMgr());
                        component_status.domain_name = ossie::corba::returnString(dm->name());
                    } else {
                        LOG_DEBUG(FileWriter_i,"Domain Manager var is nil, throwing logic_error...");
                        throw std::logic_error("The Domain Manager var is nil");
                    }
                } else {
                    LOG_DEBUG(FileWriter_i,"Domain Manager pointer is nil, throwing logic_error...");
                    throw std::logic_error("The Domain Manager pointer is nil");
                }
            }

        } catch (...) {
            LOG_DEBUG(FileWriter_i,"Exception caught while attempting to update sca file manager");
            //component_status.domain_name = "(domainless)"; // leave as default value
            LOG_INFO(FileWriter_i, "Cannot determine domain, defaulting to local $SDRROOT filesystem");
            char* sdr_env = getenv("SDRROOT");
            std::string sdr_root(sdr_env);
            if (sdr_env == NULL) {
                //size_t temp = destination_uri_full.find("sca:://");
                destination_uri_full.replace(0,7,"file://");
                destination_uri_full.insert(7,"/tmp/");
            }else{
                destination_uri_full.replace(0,7,"file://");
                sdr_root.append("/dom/");
                destination_uri_full.insert(7,sdr_root);
            }
            normFile= filesystem.normalize_uri_path(destination_uri_full);
            filesystem.uri_path_extraction(normFile, dir, base, type);
            prefix = ABSTRACTED_FILE_IO::local_uri_prefix;
        };
    }
    prop_dirname = prefix + dir;
    prop_basename = base;
    prop_full_filename = prop_dirname + base;
    if (!filesystem.exists(prop_dirname)) {
    
        if (!advanced_properties.create_destination_dir) {

            LOG_ERROR(FileWriter_i, "Error: destination directory does not exist!\n");
            throw CF::PropertySet::InvalidConfiguration(); //"Error: destination directory does not exist!");
        }
    
    if (!filesystem.make_dir(prop_dirname)) {
    
        LOG_ERROR(FileWriter_i, "Error: could not create destination directory!\n");
            throw CF::PropertySet::InvalidConfiguration(); //"Error: could not create destination directory!",advanced_properties);
        }
    }
}

void FileWriter_i::file_formatChanged(std::string oldValue, std::string newValue){
    exclusive_lock lock(service_thread_lock);
    if (oldValue != newValue) {
        if (newValue == "BLUEFILE")
            current_writer_type = BLUEFILE;
        else
            current_writer_type = RAW;
    }
}

void FileWriter_i::advanced_propertiesChanged(const advanced_properties_struct &oldValue, const advanced_properties_struct &newValue) {
    exclusive_lock lock(service_thread_lock);
    if (oldValue.max_file_size != newValue.max_file_size) {
        maxSize = sizeString_to_longBytes(newValue.max_file_size);
    }
}

void FileWriter_i::recording_timerChanged(const std::vector<timer_struct_struct> &oldValue, const std::vector<timer_struct_struct> &newValue) {
    construct_recording_timer(newValue);
}

void FileWriter_i::construct_recording_timer(const std::vector<timer_struct_struct> &timers) {
    exclusive_lock lock(service_thread_lock);
    timer_set.clear();

    for (unsigned int i=0; i<timers.size(); ++i){
        BULKIO::PrecisionUTCTime timer_element;
        //modified the interpretation of tcmode and tcstatus
        timer_element.tcmode = timers.at(i).use_pkt_timestamp;
        timer_element.tcstatus = timers.at(i).recording_enable;
        timer_element.toff = 0;
        timer_element.twsec = timers.at(i).twsec;
        timer_element.tfsec = timers.at(i).tfsec;
        timer_set.insert(timer_element);
    }
    timer_set_iter = timer_set.begin();
}

/***********************************************************************************************

    Basic functionality:

        The service function is called by the serviceThread object (of type ProcessThread).
        This call happens immediately after the previous call if the return value for
        the previous call was NORMAL.
        If the return value for the previous call was NOOP, then the serviceThread waits
        an amount of time defined in the serviceThread's constructor.

    SRI:
        To create a StreamSRI object, use the following code:
                std::string stream_id = "testStream";
                BULKIO::StreamSRI sri = bulkio::sri::create(stream_id);

        Time:
            To create a PrecisionUTCTime object, use the following code:
                BULKIO::PrecisionUTCTime tstamp = bulkio::time::utils::now();


    Ports:

        Data is passed to the serviceFunction through the getPacket call (BULKIO only).
        The dataTransfer class is a port-specific class, so each port implementing the
        BULKIO interface will have its own type-specific dataTransfer.

        The argument to the getPacket function is a floating point number that specifies
        the time to wait in seconds. A zero value is non-blocking. A negative value
        is blocking.  Constants have been defined for these values, bulkio::Const::BLOCKING and
        bulkio::Const::NON_BLOCKING.

        Each received dataTransfer is owned by serviceFunction and *MUST* be
        explicitly deallocated.

        To send data using a BULKIO interface, a convenience interface has been added
        that takes a std::vector as the data input

        NOTE: If you have a BULKIO dataSDDS port, you must manually call
              "port->updateStats()" to update the port statistics when appropriate.

        Example:
            // this example assumes that the component has two ports:
            //  A provides (input) port of type bulkio::InShortPort called short_in
            //  A uses (output) port of type bulkio::OutFloatPort called float_out
            // The mapping between the port and the class is found
            // in the component base class header file

            bulkio::InShortPort::dataTransfer *tmp = short_in->getPacket(bulkio::Const::BLOCKING);
            if (not tmp) { // No data is available
                return NOOP;
            }

            std::vector<float> outputData;
            outputData.resize(tmp->dataBuffer.size());
            for (unsigned int i=0; i<tmp->dataBuffer.size(); i++) {
                outputData[i] = (float)tmp->dataBuffer[i];
            }

            // NOTE: You must make at least one valid pushSRI call
            if (tmp->sriChanged) {
                float_out->pushSRI(tmp->SRI);
            }
            float_out->pushPacket(outputData, tmp->T, tmp->EOS, tmp->streamID);

            delete tmp; // IMPORTANT: MUST RELEASE THE RECEIVED DATA BLOCK
            return NORMAL;

        If working with complex data (i.e., the "mode" on the SRI is set to
        true), the std::vector passed from/to BulkIO can be typecast to/from
        std::vector< std::complex<dataType> >.  For example, for short data:

            bulkio::InShortPort::dataTransfer *tmp = myInput->getPacket(bulkio::Const::BLOCKING);
            std::vector<std::complex<short> >* intermediate = (std::vector<std::complex<short> >*) &(tmp->dataBuffer);
            // do work here
            std::vector<short>* output = (std::vector<short>*) intermediate;
            myOutput->pushPacket(*output, tmp->T, tmp->EOS, tmp->streamID);

        Interactions with non-BULKIO ports are left up to the component developer's discretion

    Properties:

        Properties are accessed directly as member variables. For example, if the
        property name is "baudRate", it may be accessed within member functions as
        "baudRate". Unnamed properties are given a generated name of the form
        "prop_n", where "n" is the ordinal number of the property in the PRF file.
        Property types are mapped to the nearest C++ type, (e.g. "string" becomes
        "std::string"). All generated properties are declared in the base class
        (FileWriter_base).

        Simple sequence properties are mapped to "std::vector" of the simple type.
        Struct properties, if used, are mapped to C++ structs defined in the
        generated file "struct_props.h". Field names are taken from the name in
        the properties file; if no name is given, a generated name of the form
        "field_n" is used, where "n" is the ordinal number of the field.

        Example:
            // This example makes use of the following Properties:
            //  - A float value called scaleValue
            //  - A boolean called scaleInput

            if (scaleInput) {
                dataOut[i] = dataIn[i] * scaleValue;
            } else {
                dataOut[i] = dataIn[i];
            }

        Callback methods can be associated with a property so that the methods are
        called each time the property value changes.  This is done by calling
        addPropertyChangeListener(<property name>, this, &FileWriter_i::<callback method>)
        in the constructor.

        Callback methods should take two arguments, both const pointers to the value
        type (e.g., "const float *"), and return void.

        Example:
            // This example makes use of the following Properties:
            //  - A float value called scaleValue

        //Add to FileWriter.cpp
        FileWriter_i::FileWriter_i(const char *uuid, const char *label) :
            FileWriter_base(uuid, label)
        {
            addPropertyChangeListener("scaleValue", this, &FileWriter_i::scaleChanged);
        }

        void FileWriter_i::scaleChanged(const float *oldValue, const float *newValue)
        {
            std::cout << "scaleValue changed from" << *oldValue << " to " << *newValue
                      << std::endl;
        }

        //Add to FileWriter.h
        void scaleChanged(const float* oldValue, const float* newValue);


 ************************************************************************************************/
int FileWriter_i::serviceFunction() {
    exclusive_lock lock(service_thread_lock);
    // Service each port individually. Does not account for multiple ports connected at once
    bool retService = singleService(dataChar_in, "8t");
    retService = retService || singleService(dataOctet_in, "8o");
    retService = retService || singleService(dataShort_in, "16tr");
    retService = retService || singleService(dataUshort_in, "16or");
    retService = retService || singleService(dataFloat_in, "32fr");
    retService = retService || singleService(dataDouble_in, "64fr");
    retService = retService || singleService(dataXML_in, "8t");

    if (retService) // If retService is true, then at least 1 packet was received and processed
        return NORMAL;

    return NOOP;
}

/**
 * A templated service function that is generic between data types.
 */
template <class IN_PORT_TYPE> bool FileWriter_i::singleService(IN_PORT_TYPE * dataIn, const std::string & dt) {
    typename IN_PORT_TYPE::dataTransfer *packet = dataIn->getPacket(0);
    if (packet == NULL)
        return false;
    if (packet->inputQueueFlushed){
        LOG_WARN(FileWriter_i, "WARNING: FILEWRITER INPUT QUEUE HAS BEEN FLUSHED!\n");
    }

    //Sets basic data type. IE- float for float port, short for short port
    typedef typeof (packet->dataBuffer[0]) PACKET_ELEMENT_TYPE;
    size_t packet_pos = 0;
    std::string existing_file = advanced_properties.existing_file;

    // STREAM ID
    std::string stream_id = packet->streamID;
    updateIfFound_KeywordValueByID<std::string>(& packet->SRI, "STREAM_GROUP",stream_id);

    // Initial Search for file struct
    std::string destination_filename = "";
    std::map<std::string, std::string>::iterator curFileIter = stream_to_file_mapping.find(stream_id);
    std::map<std::string, file_struct>::iterator curFileDescIter = file_to_struct_mapping.end();
    if (curFileIter != stream_to_file_mapping.end()) {
        destination_filename = curFileIter->second;
        curFileDescIter = file_to_struct_mapping.find(destination_filename);
    }

    // RECORDING TIMER
     // modified the interpretation of tcmode and tcstatus
     //   tcmode stores (bool) use_pkt_timestamp
     //   tcstatus stores (bool) recording_enable
    bool next_recording_enable = recording_enabled;
    while (timer_set_iter != timer_set.end()) {
        timer_timestamp = getSystemTimestamp();
        if (timer_set_iter->tcmode == 1) // if use_pkt_timestamp
            timer_timestamp = packet->T;
        if (compare_utc_time(timer_timestamp, *timer_set_iter))
            break;
        next_recording_enable = (timer_set_iter->tcstatus == 1);
        timer_set_iter++;
    }

    // If recording is disabled, make sure files are closed and check timer
    if (!recording_enabled){
        if (!destination_filename.empty()) {
            LOG_DEBUG(FileWriter_i, "CLOSING FILE: " << destination_filename << " DUE TO RECORDING BEING DISABLED!");
            close_file(destination_filename, packet->T, stream_id);
            stream_to_file_mapping.erase(stream_id);
            curFileIter = stream_to_file_mapping.end();
            destination_filename.clear();
        }

        // If still disabled, clean up and return
        if (!next_recording_enable){
            delete packet;
            return true;
        }
    }
    recording_enabled = next_recording_enable; // this takes effect next iteration

    // Do not open a file handle that will be of size 0
    if (packet->dataBuffer.empty() && packet->EOS && destination_filename.empty()){
        delete packet;
        return true;
    }

    // Check for state where we've reached our max file size (or time)
    if (curFileIter != stream_to_file_mapping.end() && curFileDescIter == file_to_struct_mapping.end()){
        delete packet;
        return true;
    }

    // RESET ON RETUNE
    if (advanced_properties.reset_on_retune && curFileDescIter != file_to_struct_mapping.end()) {
        bool close = false;
        close |= (curFileDescIter->second.lastSRI.xdelta != packet->SRI.xdelta);
        close |= (curFileDescIter->second.lastSRI.mode != packet->SRI.mode);

        double old_cf = 0;
        double new_cf = 0;
        updateIfFound_KeywordValueByID<double>(& curFileDescIter->second.lastSRI, "COL_RF", old_cf);
        updateIfFound_KeywordValueByID<double>(& packet->SRI, "COL_RF", new_cf);
        close |= (old_cf != new_cf);

        old_cf = 0;
        new_cf = 0;
        updateIfFound_KeywordValueByID<double>(& curFileDescIter->second.lastSRI, "CHAN_RF", old_cf);
        updateIfFound_KeywordValueByID<double>(& packet->SRI, "CHAN_RF", new_cf);
        close |= (old_cf != new_cf);

        if (close) {
            close_file(destination_filename, packet->T, stream_id);
            stream_to_file_mapping.erase(stream_id);
            curFileIter = stream_to_file_mapping.end();
            destination_filename.clear();
        }
    }

    // BYTE SWAP
    if (swap_bytes) {
        if (dt.find("16") != std::string::npos) {
            LOG_DEBUG(FileWriter_i, "SWAP_BYTES - swapping 16");
            std::vector<uint16_t> *svp = (std::vector<uint16_t> *) & packet->dataBuffer;
            std::transform(svp->begin(), svp->end(), svp->begin(), Byte_Swap16<uint16_t>);
        } else if (dt.find("32") != std::string::npos) {
            LOG_DEBUG(FileWriter_i, "SWAP_BYTES - swapping 32");
            std::vector<uint32_t> *svp = (std::vector<uint32_t> *) & packet->dataBuffer;
            std::transform(svp->begin(), svp->end(), svp->begin(), Byte_Swap32<uint32_t>);
        } else if (dt.find("64") != std::string::npos) {
            LOG_DEBUG(FileWriter_i, "SWAP_BYTES - swapping 64");
            std::vector<uint64_t> *svp = (std::vector<uint64_t> *) & packet->dataBuffer;
            std::transform(svp->begin(), svp->end(), svp->begin(), Byte_Swap64<uint64_t>);
        }
    }
    do {
        try {

            // Is File New
            bool new_file = destination_filename.empty();
            if (new_file) {
                std::string basename = stream_to_basename(stream_id, packet->SRI, packet->T, file_format, dt);
                destination_filename = prop_dirname + basename;
                bool append = false;
                // Unless the file is appending, do something if the file already exists
                if (filesystem.exists(destination_filename)) {
                    if (existing_file == "DROP")
                        throw std::logic_error("File Exists. Dropping Packet!");
                    else if (existing_file == "TRUNCATE") {
                        curFileDescIter = file_to_struct_mapping.find(destination_filename);
                        if (curFileDescIter != file_to_struct_mapping.end()) {
                            throw std::logic_error("Cannot truncate a file currently being written to by this File Writer. Dropping Packet!");
                        }
                    } else if (existing_file == "RENAME") {
                        int counter = 1;
                        std::string tmpFN = destination_filename;
                        do {
                            tmpFN = destination_filename + "-" + boost::lexical_cast<std::string>(counter);
                            counter++;
                        } while (filesystem.exists(tmpFN) && counter <= 1024);
                        destination_filename = tmpFN;
                        if (filesystem.exists(destination_filename))
                            throw std::logic_error("Cannot rename file to an available name. Dropping Reset of Packet!");
                    } else if (existing_file == "APPEND") {
                        append = true;
                    }
                }

                // Correlates stream ID to file
                curFileIter = stream_to_file_mapping.insert(std::make_pair(stream_id, destination_filename)).first;
                curFileDescIter = file_to_struct_mapping.find(destination_filename);
                if (curFileDescIter == file_to_struct_mapping.end()) {
                    double tmp_ws = packet->T.twsec + floor(packet->T.toff);
                    double tmp_fs = packet->T.tfsec + (packet->T.toff-floor(packet->T.toff));
                    if (tmp_fs >= 1.0) {
                        tmp_fs -= 1.0;
                        tmp_ws += 1.0;
                    }
                    else if (tmp_fs < 0) {
                        tmp_fs += 1.0;
                        tmp_ws -= 1.0;
                    }
                    file_struct fs(destination_filename, current_writer_type, tmp_ws, tmp_fs, advanced_properties.enable_metadata_file, advanced_properties.use_hidden_files, advanced_properties.open_file_extension, advanced_properties.open_metadata_file_extension, stream_id);
                    curFileDescIter = file_to_struct_mapping.insert(std::make_pair(destination_filename, fs)).first;
                    curFileDescIter->second.file_size_internal = filesystem.file_size(curFileDescIter->second.in_process_uri_filename);

                    bool open_success = filesystem.open_file(curFileDescIter->second.in_process_uri_filename, true, append);
                    if (curFileDescIter->second.metdata_file_enabled())
                        open_success |= filesystem.open_file(curFileDescIter->second.in_process_uri_metadata_filename, true, append);
                    if (!open_success) {
                        close_file(destination_filename, packet->T, stream_id);
                        stream_to_file_mapping.erase(stream_id);
                        LOG_ERROR(FileWriter_i, "ERROR OPENING FILE: " << curFileDescIter->second.in_process_uri_filename);
                        throw std::logic_error("ERROR OPENING FILE: " + curFileDescIter->second.in_process_uri_filename);
                    }

                    if (advanced_properties.debug_output)
                        std::cout << "DEBUG (" << __PRETTY_FUNCTION__ << "): OPENED STREAM: " << packet->streamID
                            << " (" << stream_id << ") " << " OR FILE (TMP): " << fs.in_process_uri_filename
                            << " OR FILE (TMP): " << fs.uri_filename << std::endl;
                    LOG_INFO(FileWriter_i, "OPENED STREAM: " << packet->streamID << " (" << stream_id << ") " << " OR FILE (TMP): " << fs.in_process_uri_filename << " OR FILE (TMP): " << fs.uri_filename);

                    file_io_message_struct file_event = create_file_io_message("OPEN", stream_id, filesystem.uri_to_file(fs.in_process_uri_filename));
                    MessageEvent_out->sendMessage(file_event);

                    curFileDescIter->second.lastSRI = packet->SRI;

                    // Initialize Metadata File
                    if (curFileDescIter->second.metdata_file_enabled()){
                    	std::ostringstream openXML;
                    	openXML << "<FileWriter_metadata datafile=\""<< curFileDescIter->second.basename<<"\">";
                        std::string openXML_str = openXML.str();
                    	filesystem.write(curFileDescIter->second.in_process_uri_metadata_filename, &openXML_str, advanced_properties.force_flush);
                    }

                    // BLUEFILE
                    if (curFileDescIter->second.file_type == BLUEFILE) {
                        size_t pos = filesystem.file_tell(curFileDescIter->second.in_process_uri_filename);
                        curFileDescIter->second.lastSRI = packet->SRI;
                        curFileDescIter->second.midas_type = midas_type<PACKET_ELEMENT_TYPE > ((curFileDescIter->second.lastSRI.mode == 0));
                        if (pos == 0) {
                            std::string empty_string; // Write 512 block of 0s as place holder for bluefile header
                            empty_string.resize(BLUEFILE_BLOCK_SIZE, '0');
                            filesystem.write(fs.in_process_uri_filename, (char*) empty_string.c_str(), BLUEFILE_BLOCK_SIZE, advanced_properties.force_flush);
                            filesystem.file_seek(fs.in_process_uri_filename, BLUEFILE_BLOCK_SIZE);
                        } else if (curFileDescIter->second.num_writers == 1) {
                            std::vector<char> buff(BLUEFILE_BLOCK_SIZE);
                            filesystem.file_seek(fs.in_process_uri_filename, 0);
                            filesystem.read(fs.in_process_uri_filename, &buff, BLUEFILE_BLOCK_SIZE);
                            blue::HeaderControlBlock hcb = blue::HeaderControlBlock((const blue::hcb_s *) & buff[0]);
                            if (hcb.validate(false) != 0) {
                                LOG_WARN(FileWriter_i, "CAN NOT READ BLUEHEADER FOR APPENDING DATA!");
                            }
                            filesystem.file_seek(fs.in_process_uri_filename, hcb.getDataStart() + hcb.getDataSize());
                        }
                    }

                } else{
                    curFileDescIter->second.num_writers++;
                }
            }

            curFileDescIter = file_to_struct_mapping.find(destination_filename);
            if (curFileDescIter == file_to_struct_mapping.end()){
                throw std::logic_error("ERROR. SHOULD HAVE CORRESPONDING FILE STRUCTURE OBJECT");
            }


            bool eos = (packet->EOS && (stream_id == packet->streamID));
            size_t write_bytes = packet->dataBuffer.size() * sizeof (packet->dataBuffer[0]) - packet_pos;

            long maxSize_time_size = maxSize;
            if (advanced_properties.max_file_time > 0) {
                // seconds                    *    samples/second     * B/sample
                long maxSize_time = advanced_properties.max_file_time * 1 / packet->SRI.xdelta * sizeof (packet->dataBuffer[0]) * (packet->SRI.mode + 1);
                if (maxSize_time > 0 && (maxSize_time_size <= 0 || maxSize_time < maxSize_time_size))
                    maxSize_time_size = maxSize_time;

            }

            bool reached_max_size = false;
            if (maxSize_time_size > 0) {
                size_t avail_in_file = maxSize_time_size - curFileDescIter->second.file_size_internal;
                if (avail_in_file <= write_bytes) {
                    write_bytes = avail_in_file;
                    reached_max_size = true;
                    LOG_DEBUG(FileWriter_i, "Reached max file size: write_bytes="<<write_bytes);
                }
            }

            // Output Data To File
            if (advanced_properties.debug_output) {
                std::cout << "DEBUG (" << __PRETTY_FUNCTION__ << "): WRITING: " << write_bytes << " BYTES TO FILE: " << curFileDescIter->second.in_process_uri_filename << std::endl;
                LOG_DEBUG(FileWriter_i,"WRITING: " << write_bytes << " BYTES TO FILE: " << curFileDescIter->second.in_process_uri_filename );
            }
            filesystem.write(curFileDescIter->second.in_process_uri_filename, (char*) &packet->dataBuffer[0] + packet_pos, write_bytes, advanced_properties.force_flush);
            curFileDescIter->second.file_size_internal += write_bytes;
            packet_pos += write_bytes;


            if (packet->sriChanged || new_file) {
                curFileDescIter->second.lastSRI = packet->SRI;
                curFileDescIter->second.midas_type = midas_type<PACKET_ELEMENT_TYPE > ((curFileDescIter->second.lastSRI.mode == 0));
                packet->SRI.streamID = stream_id.c_str();
                dataFile_out->pushSRI(packet->SRI);
                if (curFileDescIter->second.metdata_file_enabled()) {
                    std::string metadata = sri_to_XMLstring(packet->SRI,packet->sriChanged);
                    filesystem.write(curFileDescIter->second.in_process_uri_metadata_filename, &metadata, advanced_properties.force_flush);
                }
                if (curFileDescIter->second.file_type == BLUEFILE) {
                	mergeKeywords(curFileDescIter->second.lastSRI.keywords);
                	curFileDescIter->second.lastSRI.keywords = allKeywords;
                }
            }

            //Write packet metadata to file
            if (curFileDescIter->second.metdata_file_enabled()) {
            	std::string packetmetadata = packet_to_XMLstring(write_bytes,packet->SRI,packet->T,packet->EOS,packet_pos-write_bytes,sizeof(packet->dataBuffer[0]));
                filesystem.write(curFileDescIter->second.in_process_uri_metadata_filename, &packetmetadata, advanced_properties.force_flush);

            }

            // Close File
            if (eos || reached_max_size) {
                LOG_DEBUG(FileWriter_i, " *** PROCESSING EOS FOR STREAM ID : " << stream_id);
                if (eos) {

					allKeywords.length(0);
                }
                close_file(destination_filename, packet->T, stream_id);
                if (reached_max_size && advanced_properties.reset_on_max_file) {
                    LOG_DEBUG(FileWriter_i, "Reseting on max file size...");
                    stream_to_file_mapping.erase(stream_id);
                    curFileIter = stream_to_file_mapping.end();
                    destination_filename.clear();
                } else if (reached_max_size){
                    LOG_DEBUG(FileWriter_i, "Not reseting on max file size...");
                    break;
                }
            }
        }
        catch (const std::logic_error & error) {
            LOG_DEBUG(FileWriter_i, error.what());
        }
        catch (...) {
            LOG_DEBUG(FileWriter_i, "Caught unknown exception in service function loop");
            break;
        };
    } while (packet_pos < packet->dataBuffer.size() * sizeof (packet->dataBuffer[0]));

    //Delete Memory
    if (packet->EOS && stream_id == std::string(packet->streamID))
        stream_to_file_mapping.erase(stream_id);
    delete packet;

    return true;
}

bool FileWriter_i::close_file(const std::string& filename, const BULKIO::PrecisionUTCTime & timestamp, std::string streamId) {
    std::map<std::string, file_struct>::iterator curFileDescIter = file_to_struct_mapping.find(filename);
    if (curFileDescIter == file_to_struct_mapping.end())
        return true;


    std::string stream_id = streamId;
    updateIfFound_KeywordValueByID<std::string>(&curFileDescIter->second.lastSRI, "STREAM_GROUP",stream_id);


    curFileDescIter->second.num_writers--;
    if (curFileDescIter->second.num_writers <= 0) {
        if (curFileDescIter->second.file_type == BLUEFILE) {
            size_t curPos = filesystem.file_tell(curFileDescIter->second.in_process_uri_filename);
            std::pair<blue::HeaderControlBlock, std::vector<char> > bheaders = createBluefilesHeaders(curFileDescIter->second.lastSRI,
                            curPos, curFileDescIter->second.midas_type, curFileDescIter->second.start_time_ws, curFileDescIter->second.start_time_fs);
            filesystem.file_seek(curFileDescIter->second.in_process_uri_filename, 0);
            blue::hcb_s tmp_hcb = bheaders.first.getHCB();
            filesystem.write(curFileDescIter->second.in_process_uri_filename, (char*) & tmp_hcb, BLUEFILE_BLOCK_SIZE, advanced_properties.force_flush);
            filesystem.file_seek(curFileDescIter->second.in_process_uri_filename, curPos);
            filesystem.write(curFileDescIter->second.in_process_uri_filename, (char*) &bheaders.second[0], bheaders.second.size(), advanced_properties.force_flush);
        }
        filesystem.close_file(curFileDescIter->second.in_process_uri_filename);
        if(curFileDescIter->second.in_process_uri_filename != curFileDescIter->second.uri_filename){
            filesystem.move_file(curFileDescIter->second.in_process_uri_filename,curFileDescIter->second.uri_filename);
        }

        if (curFileDescIter->second.metdata_file_enabled()) {
            std::string closeXML = "</FileWriter_metadata>";
            filesystem.write(curFileDescIter->second.in_process_uri_metadata_filename, &closeXML, advanced_properties.force_flush);
            filesystem.close_file(curFileDescIter->second.in_process_uri_metadata_filename);
            if(curFileDescIter->second.in_process_uri_metadata_filename != curFileDescIter->second.uri_metadata_filename){
                filesystem.move_file(curFileDescIter->second.in_process_uri_metadata_filename,curFileDescIter->second.uri_metadata_filename);
            }
        }

        if (advanced_properties.debug_output)
            std::cout << "DEBUG (" << __PRETTY_FUNCTION__ << "): CLOSED FILE: " << curFileDescIter->second.uri_filename << std::endl;
        LOG_INFO(FileWriter_i, "CLOSED FILE: " << curFileDescIter->second.uri_filename );

        std::string sca_filename = filesystem.uri_to_file(curFileDescIter->second.uri_filename);
        file_io_message_struct file_event = create_file_io_message("CLOSE", stream_id, sca_filename);
        BULKIO::PrecisionUTCTime tstamp = bulkio::time::utils::now();
        MessageEvent_out->sendMessage(file_event);
        dataFile_out->pushPacket(sca_filename.c_str(), tstamp, true, stream_id.c_str());

        file_to_struct_mapping.erase(curFileDescIter);
        curFileDescIter = file_to_struct_mapping.end();
        return true;
    }
    return false;
}

std::string FileWriter_i::stream_to_basename(const std::string & stream_id, const BULKIO::StreamSRI& sri, const BULKIO::PrecisionUTCTime &_T, const std::string & extension, const std::string & dt) {

    // Create Timestamp String
    BULKIO::PrecisionUTCTime tstamp = _T;
    if (tstamp.tcstatus == BULKIO::TCS_INVALID) {
        struct timeval tmp_time;
        struct timezone tmp_tz;
        gettimeofday(&tmp_time, &tmp_tz);
        double wsec = tmp_time.tv_sec;
        double fsec = tmp_time.tv_usec / 1e6;

        tstamp = BULKIO::PrecisionUTCTime();
        tstamp.tcmode = BULKIO::TCM_CPU;
        tstamp.tcstatus = (short) 1;
        tstamp.toff = 0.0;
        tstamp.twsec = wsec;
        tstamp.tfsec = fsec;
    }

    std::string time_string_no_fract = time_to_string(tstamp, false, true);
    std::string time_string = time_to_string(tstamp, true, true);
    BULKIO::PrecisionUTCTime tstamp_system = getSystemTimestamp();
    std::string system_time_string_no_fract = time_to_string(tstamp_system, false, true);
    std::string system_time_string = time_to_string(tstamp_system, true, true);

    // Ensure extension is lowercase
    std::string ext = extension;
    toLower(ext);

    // Mode
    std::string mode = "real";
    if (sri.mode == 1)
        mode = "cplx";

    // Sample Rate
    double sampleRate = 1.0 / sri.xdelta;
    char sr[25] = "";
    sprintf(sr, "%.0f", sampleRate);


    // Perform Replacements
    std::string bn = prop_basename;
    bn = replace_string(bn, "%STREAMID%", stream_id);
    bn = replace_string(bn, "%TIMESTAMP%", time_string);
    bn = replace_string(bn, "%TIMESTAMP_NO_FRACT%", time_string_no_fract);
    bn = replace_string(bn, "%SYSTEM_TIMESTAMP%", system_time_string);
    bn = replace_string(bn, "%SYSTEM_TIMESTAMP_NO_FRACT%", system_time_string_no_fract);
    bn = replace_string(bn, "%COMP_NS_NAME%", naming_service_name);
    bn = replace_string(bn, "%EXTENSION%", ext);
    bn = replace_string(bn, "%MODE%", mode);
    bn = replace_string(bn, "%SR%", std::string(sr));
    bn = replace_string(bn, "%DT%", std::string(dt));

    std::string cf_hz_str = "Hz";
    std::string colrf_hz_str = "";
    std::string chanrf_hz_str = "";

    for (size_t i = 0; i < sri.keywords.length(); i++) {
        std::string search = "%" + std::string(sri.keywords[i].id) + "%";
        std::string replace = ossie::any_to_string(sri.keywords[i].value);
        bn = replace_string(bn, search, replace);
        if (std::string(sri.keywords[i].id) == "COL_RF") {
            std::ostringstream result;
            CORBA::Double tmp;
            sri.keywords[i].value >>= tmp;
            CORBA::LongLong tmpLL = CORBA::LongLong(tmp);
            result << tmpLL;
            colrf_hz_str = std::string(result.str()) + "Hz";
        }
        if (std::string(sri.keywords[i].id) == "CHAN_RF") {
            std::ostringstream result;
            CORBA::Double tmp;
            sri.keywords[i].value >>= tmp;
            CORBA::LongLong tmpLL = CORBA::LongLong(tmp);
            result << tmpLL;
            chanrf_hz_str = std::string(result.str()) + "Hz";
        }
    }

    if (!chanrf_hz_str.empty())
        cf_hz_str = chanrf_hz_str;
    else if (!colrf_hz_str.empty())
        cf_hz_str = colrf_hz_str;
    bn = replace_string(bn, "%CF_HZ%", std::string(cf_hz_str));
    bn = replace_string(bn, "%COLRF_HZ%", std::string(colrf_hz_str));
    bn = replace_string(bn, "%CHANRF_HZ%", std::string(chanrf_hz_str));

    // Force to lower/upper case here if needed
    //advanced_properties.output_filename_case: 0 mixed; 1 lower; 2 upper
    switch( advanced_properties.output_filename_case ) {
    //case 0: // mixed case permitted - do nothing, same as default case
    //    break;
    case 1: // tolower
        std::transform(bn.begin(), bn.end(), bn.begin(), ::tolower);
        break;
    case 2: // toupper
        std::transform(bn.begin(), bn.end(), bn.begin(), ::toupper);
        break;
    default:
        // do nothing
        break;
    }

    return bn;
}

std::string FileWriter_i::sri_to_XMLstring(const BULKIO::StreamSRI& sri,const bool newsri) {
    std::ostringstream sri_string;
    if (newsri) {
    	sri_string << "<sri new=\"true\">";
    } else {
    	sri_string << "<sri new=\"false\">";
    }
    sri_string << "<streamID>" << sri.streamID << "</streamID>";
    sri_string << "<hversion>" << sri.hversion << "</hversion>";
    sri_string << "<xstart>" << sri.xstart << "</xstart>";
    sri_string << "<xdelta>" <<std::setprecision (15) <<sri.xdelta << "</xdelta>";
    sri_string << "<xunits>" << sri.xunits << "</xunits>";
    sri_string << "<subsize>" << sri.subsize << "</subsize>";
    sri_string << "<ystart>" << sri.ystart << "</ystart>";
    sri_string << "<ydelta>" <<std::setprecision (15)<< sri.ydelta << "</ydelta>";
    sri_string << "<yunits>" << sri.yunits << "</yunits>";
    sri_string << "<mode>" << sri.mode << "</mode>";
    for (unsigned int i = 0; i < sri.keywords.length(); i++) {
    	unsigned int typecode_name = sri.keywords[i].value.type()->kind();
        sri_string << "<keyword id=\"" << sri.keywords[i].id << "\" type=\"" <<typecode_name<<"\">"<<ossie::any_to_string(sri.keywords[i].value)<<"</keyword>";
    }
    sri_string << "</sri>";

    return std::string(sri_string.str());
}

std::string FileWriter_i::packet_to_XMLstring(const int packetSize, const BULKIO::StreamSRI& sri,const BULKIO::PrecisionUTCTime& timecode, const bool& eos,const size_t packetPosition,const size_t elementSize) {
    std::ostringstream packet_string;
    packet_string << "<packet>";
    packet_string << "<streamID>" << sri.streamID << "</streamID>";
    packet_string << "<datalength>" << packetSize << "</datalength>";
    packet_string << "<EOS>" <<eos << "</EOS>";
    packet_string << "<timecode>";
    packet_string << "<tcmode>" << timecode.tcmode << "</tcmode>";
	packet_string << "<tcstatus>" << timecode.tcstatus << "</tcstatus>";
    if (packetPosition==0) {

    	packet_string << "<tfsec>" <<std::setprecision (15) << timecode.tfsec << "</tfsec>";
    	packet_string << "<toff>" << timecode.toff << "</toff>";
    	packet_string << "<twsec>" <<std::setprecision (15)<< timecode.twsec << "</twsec>";
    } else {
    	// Create an adjusted timecode if this packet was split between two files
    	double timeoffset = sri.xdelta*packetPosition/elementSize;
    	double correctedtfsec  = timecode.tfsec +timeoffset;
    	double correctedtwsec = 0.0;
    	correctedtfsec = modf(correctedtfsec, &correctedtwsec);
    	correctedtwsec +=timecode.twsec;
		packet_string << "<tfsec>" <<std::setprecision (15) << correctedtfsec << "</tfsec>";
    	packet_string << "<toff>" << timecode.toff << "</toff>";
    	packet_string << "<twsec>" <<std::setprecision (15)<< correctedtwsec << "</twsec>";
    }


    	packet_string << "</timecode>";
    	packet_string << "</packet>";
    return std::string(packet_string.str());

}

std::pair<blue::HeaderControlBlock, std::vector<char> > FileWriter_i::createBluefilesHeaders(const BULKIO::StreamSRI& sri, size_t datasize, std::string midasType, double start_ws, double start_fs) {
    blue::HeaderControlBlock hcb;
    blue::ExtendedHeader ecb;


    if ( sri.subsize == 0) {
        hcb.setTypeCode(1000);
        hcb.setXstart(sri.xstart);
        hcb.setXdelta(sri.xdelta);
        hcb.setXunits(sri.xunits);
    } else {
        hcb.setTypeCode(2000);
        hcb.setXstart(sri.xstart);
        hcb.setXdelta(sri.xdelta);
        hcb.setXunits(sri.xunits);
        hcb.setColRecs(sri.subsize);
        hcb.setColStart(sri.ystart);
        hcb.setColDelta(sri.ydelta);
        hcb.setColUnits(sri.yunits);
    }

    hcb.setFormatCode(midasType);
    if ( !advanced_properties.use_tc_prec ) {
        hcb.setTimeCode(start_ws + start_fs + long(631152000));
    } else {
        LOG_DEBUG(FileWriter_i, "Using TC_PREC keyword in BLUE file header for extra timecode precision.");
        double start_fs_us = floor(start_fs*1.0e6)*1.0e-6;
        double start_fs_prec = floor( (start_fs-start_fs_us)*1.0e12 ) * 1.0e-12;
        hcb.setTimeCode(start_ws + start_fs_us + long(631152000));
        if ( start_fs_prec != 0 ) {
            LOG_DEBUG(FileWriter_i, "TC_PREC keyword is (double) " << start_fs_prec << ", adding to BLUE file header.");
            // add TC_PREC keyword with value from start_fs_prec as ascii string
            std::stringstream ss_p;
            ss_p <<std::uppercase << start_fs_prec; // << std::nouppercase;
            LOG_DEBUG(FileWriter_i, "TC_PREC keyword is (string) " << ss_p.str() << ", adding to BLUE file header.");
            if ( !hcb.addKeyword("TC_PREC", ss_p.str()) ) {
                LOG_WARN(FileWriter_i, "No room in BLUE file header for extra timecode precision (TC_PREC keyword).");
            }
        } else {
            // no TC_PREC keyword, delete if necessary
            LOG_DEBUG(FileWriter_i, "TC_PREC keyword would be zero, not including in BLUE file header.");
            hcb.removeKeyword("TC_PREC");
        }
    }

    hcb.setDataSize(datasize - BLUEFILE_BLOCK_SIZE);
    hcb.setHeaderRep(blue::IEEE); // Note: avoid EEEI headers (EEEI datasets are ok)

    if (swap_bytes) {
        hcb.setDataRep(blue::EEEI);
    } else {
        hcb.setDataRep(blue::IEEE);
    }

    // Turn SRI keywords to Midas Keywords
    std::set<blue::Keyword> midasKeywords;
    for (int i = 0; i < (int) sri.keywords.length(); ++i) {
        std::string id = std::string(sri.keywords[i].id);
        if (id == "")
            continue;
        CORBA::TCKind val_kind = sri.keywords[i].value.type()->kind();
        if (val_kind == CORBA::tk_short) {
            CORBA::Short tmp;
            sri.keywords[i].value >>= tmp;
            blue::Keyword kw(blue::INTEGER, id.c_str());
            kw.push();
            kw.setValue(tmp, 0);
            kw.setUnits(blue::NOT_APPLICABLE);
            midasKeywords.insert(kw);
        } else if (val_kind == CORBA::tk_long) {
            CORBA::Long tmp;
            sri.keywords[i].value >>= tmp;
            blue::Keyword kw(blue::LONG, id.c_str());
            kw.push();
            kw.setValue(tmp, 0);
            kw.setUnits(blue::NOT_APPLICABLE);
            midasKeywords.insert(kw);
        } else if (val_kind == CORBA::tk_char) {
            CORBA::Char tmp;
            sri.keywords[i].value >>= CORBA::Any::to_char(tmp);
            blue::Keyword kw(blue::BYTE, id.c_str());
            kw.push();
            kw.setValue(tmp, 0);
            kw.setUnits(blue::NOT_APPLICABLE);
            midasKeywords.insert(kw);
        } else if (val_kind == CORBA::tk_octet) {
            CORBA::Octet tmp;
            sri.keywords[i].value >>= CORBA::Any::to_octet(tmp);
            ;
            blue::Keyword kw(blue::OFFSET, id.c_str());
            kw.push();
            kw.setValue(tmp, 0);
            kw.setUnits(blue::NOT_APPLICABLE);
            midasKeywords.insert(kw);
        } else if (val_kind == CORBA::tk_longlong) {
            CORBA::LongLong tmp;
            sri.keywords[i].value >>= tmp;
            blue::Keyword kw(blue::XTENDED, id.c_str());
            kw.push();
            kw.setValue((long long) (tmp), 0);
            kw.setUnits(blue::NOT_APPLICABLE);
            midasKeywords.insert(kw);
        } else if (val_kind == CORBA::tk_float) {
            float tmp;
            sri.keywords[i].value >>= tmp;
            blue::Keyword kw(blue::FLOAT, id.c_str());
            kw.push();
            kw.setValue(tmp, 0);
            kw.setUnits(blue::NOT_APPLICABLE);
            midasKeywords.insert(kw);
        } else if (val_kind == CORBA::tk_double) {
            double tmp;
            sri.keywords[i].value >>= tmp;
            blue::Keyword kw(blue::DOUBLE, id.c_str());
            kw.push();
            kw.setValue(tmp, 0);
            kw.setUnits(blue::NOT_APPLICABLE);
            midasKeywords.insert(kw);
        } else {
            std::string tmp = ossie::any_to_string(sri.keywords[i].value);
            if (tmp == "" || tmp.size() >= 80)
                continue;

            blue::Keyword kw(blue::ASCII, id.c_str());
            kw.push();
            kw.setValue(tmp.c_str(), 0);
            kw.setUnits(blue::NOT_APPLICABLE);
            midasKeywords.insert(kw);
        }
    }



    // Add to buffer
    std::vector<char> buff;
    std::set<blue::Keyword>::iterator it = midasKeywords.begin();
    for (; it != midasKeywords.end(); ++it) {
        //std::cout << "Snapper_cpp_impl1_i::writeMidasExtendedHeader keyword " << it->getName() << " value " << it->getRawData() << " format " << it->getFormat() << " units " << it->getUnits() << " isIndexKeyword " << it->isIndexKeyword() << "  ===================" << std::endl;
        if (it->getFormat() == blue::ASCII) {
            std::string data = it->getString();
            blue::MidasKey::PackMemory(it->getName(), data, &buff);
        } else {
            blue::MidasKey::PackMemory(it->getName(), it->getRawData(), it->size(), it->getFormat(), &buff);
        }

        blue::UnitCodeEnum unit = it->getUnits();
        if (unit != blue::NOT_APPLICABLE && unit != blue::NOT_APPLICABLE) {
            int16_t iunit = static_cast<int16_t> (unit);
            blue::MidasKey::PackMemory(it->getName() + ".UNITS", &iunit, 1, blue::INTEGER, &buff);
        }

        if (it->isIndexKeyword()) {
            if (it->getIndexFormat() == blue::ASCII) {
                std::string data = it->getIndexString();
                blue::MidasKey::PackMemory(it->getName() + ".INDEX", data, &buff);
            } else {
                std::vector<char> data;
                it->getRawIndexData(&data);
                blue::MidasKey::PackMemory(
                        it->getName() + ".INDEX", &data[0],
                        it->size(), it->getIndexFormat(), &buff);
            }

            unit = it->getIndexUnits();
            if (unit != blue::NOT_APPLICABLE && unit != blue::NOT_APPLICABLE) {
                int16_t iunit = static_cast<int16_t> (unit);
                blue::MidasKey::PackMemory(
                        it->getName() + ".INDEX_UNITS", &iunit, 1, blue::INTEGER, &buff);
            }
        } // if indexKey

    }

    //ExtendedHeader::close_()
    //char *bb = &(buff[0]);
    const int NN = static_cast<int> (buff.size());
    // Roundup(NN, 512); // New buffer size padded to a 512 boundary.
    int newsize = NN % 512 ? (NN / 512 + 1)*512 : NN;
    int newpad = newsize - NN; // Number of bytes to be added.

    if (newpad > 0) {
        //blue::midaskeystruct *mks = reinterpret_cast<blue::midaskeystruct*> (bb + lastkeyword);
        //mks->next_offset += newpad;
        //mks->non_data_len += newpad;

        buff.insert(buff.end(), newpad, 0);
    }
    size_t startBlock = size_t(ceil(double(datasize) / double(BLUEFILE_BLOCK_SIZE)));
    size_t numZeros = startBlock * BLUEFILE_BLOCK_SIZE - datasize;
    hcb.setExtStart(startBlock);
    //hcb.setExtSize(static_cast<int> (buff.size()));
    hcb.setExtSize(static_cast<int> (NN));
    buff.insert(buff.begin(), numZeros, 0);
    return std::make_pair(hcb, buff);

}

size_t FileWriter_i::sizeString_to_longBytes(std::string size) {
    toUpper(size);

    long prefix_multiplier = 1;
    size_t pos;
    if ((pos = size.find("KB")) != std::string::npos) {
        prefix_multiplier = 1024;
        size.erase(pos);
    } else if ((pos = size.find("MB")) != std::string::npos) {
        prefix_multiplier = 1024 * 1024;
        size.erase(pos);
    } else if ((pos = size.find("GB")) != std::string::npos) {
        prefix_multiplier = 1024 * 1024 * 1024;
        size.erase(pos);
    } else if ((pos = size.find("B")) != std::string::npos) {
        prefix_multiplier = 1;
        size.erase(pos);
    }

    return size_t(prefix_multiplier * std::atof(size.c_str()));
};

void FileWriter_i::mergeKeywords(BULKIO::StreamSRI::_keywords_seq newkeywords) {

	for (unsigned int i = 0; i<newkeywords.length(); i++) {
		bool found = false;
		for (unsigned int j = 0; j<allKeywords.length(); j++) {
			if (!strcmp(newkeywords[i].id,allKeywords[j].id)) {
				//Keywords already in List, update value
				LOG_DEBUG(FileWriter_i, "  mergeKeywords: Keywords already in List, update value " << newkeywords[i].id)
				allKeywords[j].value = newkeywords[i].value;
				found=true;
				break;
			}
		}
		if (!found) {
			//Keyword not already in list of all keywords so add it
			LOG_DEBUG(FileWriter_i, "  mergeKeywords: Adding Keyword " << newkeywords[i].id)
			allKeywords.length(allKeywords.length()+1);
			allKeywords[allKeywords.length()-1] = newkeywords[i];

		}

	}

};


