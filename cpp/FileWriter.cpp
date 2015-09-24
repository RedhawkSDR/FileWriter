/*
 * This file is protected by Copyright. Please refer to the COPYRIGHT file
 * distributed with this source distribution.
 *
 * This file is part of FilterDecimate.
 *
 * FilterDecimate is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * FilterDecimate is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
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
    maxSize = 0;
    timer_set_iter = timer_set.end();
    filesystem = new ABSTRACTED_FILE_IO::abstracted_file_io();
    //properties are not updated until callback function is called and they are explicitly set.
    addPropertyChangeListener("destination_uri", this, &FileWriter_i::destination_uriChanged);
    addPropertyChangeListener("destination_uri_suffix", this, &FileWriter_i::destination_uri_suffixChanged);
    addPropertyChangeListener("file_format", this, &FileWriter_i::file_formatChanged);
    addPropertyChangeListener("advanced_properties", this, &FileWriter_i::advanced_propertiesChanged);
    addPropertyChangeListener("recording_timer", this, &FileWriter_i::recording_timerChanged);
    change_uri();
}

FileWriter_i::~FileWriter_i() {
    if (filesystem)
        delete filesystem;
    filesystem = NULL;

}

void FileWriter_i::initialize() throw (CF::LifeCycle::InitializeError, CORBA::SystemException) {
    FileWriter_base::initialize();
    try {
        CF::DomainManager_var dm = FILE_READER_DOMAIN_MGR_HELPERS::domainManager_id_to_var(DomainManager_out->identifier());
        filesystem->update_sca_file_manager(dm->fileMgr());

    } catch (...) {
    }
}

void FileWriter_i::start() throw (CF::Resource::StartError, CORBA::SystemException) {
    FileWriter_base::start();
   /* try{
         change_uri();
    }
    catch(...){
        LOG_WARN(FileWriter_i, "No Domain was found, preceeding with local filesystem");
    }
     */       
}

void FileWriter_i::stop() throw (CF::Resource::StopError, CORBA::SystemException) {
    FileWriter_base::stop();

    if (filesystem != NULL) {
        exclusive_lock lock(service_thread_lock);

        for (std::map<std::string, std::string>::iterator curFileIter = stream_to_file_mapping.begin(); curFileIter != stream_to_file_mapping.end(); curFileIter++) {
        	close_file(curFileIter->second, getSystemTimestamp());
        }
        stream_to_file_mapping.clear();
        timer_set_iter = timer_set.end();

    }

}

void FileWriter_i::destination_uriChanged(const std::string *oldValue, const std::string *newValue) {
    exclusive_lock lock(service_thread_lock);
    if (*oldValue != *newValue) {
        change_uri();
    }
}

void FileWriter_i::destination_uri_suffixChanged(const std::string *oldValue, const std::string *newValue) {
    exclusive_lock lock(service_thread_lock);
    if (*oldValue != *newValue) {
        change_uri();
    }
}

void FileWriter_i::change_uri() {
    std::string destination_uri_full = destination_uri + destination_uri_suffix;
    std::string normFile = filesystem->normalize_uri_path(destination_uri_full);
    //std::cout << normFile << std::endl;
    std::string prefix, dir, base;
    ABSTRACTED_FILE_IO::FILESYSTEM_TYPE type;
    filesystem->uri_path_extraction(normFile, dir, base, type);
    prefix = ABSTRACTED_FILE_IO::local_uri_prefix;
    if (type == ABSTRACTED_FILE_IO::SCA_FILESYSTEM) {
        prefix = ABSTRACTED_FILE_IO::sca_uri_prefix;
        //do{
        try {
            CF::DomainManager_var dm = FILE_READER_DOMAIN_MGR_HELPERS::domainManager_id_to_var(DomainManager_out->identifier());
            filesystem->update_sca_file_manager(dm->fileMgr());
        } catch (...) {
            LOG_WARN(FileWriter_i, "Error: can not determine domain!\n");
            LOG_WARN(FileWriter_i, "Defaulting to local $SDRROOT filesystem");       
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
            normFile= filesystem->normalize_uri_path(destination_uri_full);
            filesystem->uri_path_extraction(normFile, dir, base, type);
            prefix = ABSTRACTED_FILE_IO::local_uri_prefix;
        };
    }
    prop_dirname = prefix + dir;
    prop_basename = base;
    prop_full_filename = prop_dirname + base;
    if (!filesystem->exists(prop_dirname)) {
    
        if (!advanced_properties.create_destination_dir) {
    
            LOG_ERROR(FileWriter_i, "Error: destination directory does not exist!\n");
            throw CF::PropertySet::InvalidConfiguration(); //"Error: destination directory does not exist!");
        }
    
    if (!filesystem->make_dir(prop_dirname)) {
    
        LOG_ERROR(FileWriter_i, "Error: could not create destination directory!\n");
            throw CF::PropertySet::InvalidConfiguration(); //"Error: could not create destination directory!",advanced_properties);
        }
    }
}

void FileWriter_i::file_formatChanged(const std::string *oldValue, const std::string *newValue) {
    exclusive_lock lock(service_thread_lock);
    if (*oldValue != *newValue) {
        if (file_format == "BLUEFILE")
            current_writer_type = BLUEFILE;
        else
            current_writer_type = RAW;
    }
}

void FileWriter_i::advanced_propertiesChanged(const advanced_properties_struct *oldValue, const advanced_properties_struct *newValue) {
    exclusive_lock lock(service_thread_lock);
    //if (oldValue->max_file_size != newValue->max_file_size)
    //
    if (oldValue->max_file_size != newValue->max_file_size) {
        maxSize = sizeString_to_longBytes(advanced_properties.max_file_size);
    }
}

void FileWriter_i::recording_timerChanged(const std::vector<timer_struct_struct> *oldValue, const std::vector<timer_struct_struct> *newValue) {
    exclusive_lock lock(service_thread_lock);
    timer_set.clear();
    
    for (unsigned int i=0; i<newValue->size(); ++i){
        if (newValue->at(i) != oldValue->at(i)){
        
        	if (newValue->at(i).recording_enable){
        		BULKIO::PrecisionUTCTime timer_element;
        		//modified the interpretation of tcmode and tcstatus
        		timer_element.tcmode = newValue->at(i).use_pkt_timestamp;
        		timer_element.tcstatus = newValue->at(i).recording_enable;
        		timer_element.toff = 0;
        		timer_element.twsec = newValue->at(i).twsec;
        		timer_element.tfsec = newValue->at(i).tfsec;
        		timer_set.insert(timer_element);
        		timer_set_iter = timer_set.begin();
        	}
        }
    }
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
    if (packet->inputQueueFlushed)
        LOG_WARN(FileWriter_i, "WARNING: FILEWRITER INPUT QUEUE HAS BEEN FLUSHED!\n");


    std::string stream_id = packet->streamID;
    try {
        std::string tmp = getKeywordValueByID<std::string>(& packet->SRI, "STREAM_GROUP");
        stream_id = tmp;
    } catch (...) {
    };

    //Sets basic data type. IE- float for float port, short for short port
    typedef typeof (packet->dataBuffer[0]) PACKET_ELEMENT_TYPE;
    size_t packet_pos = 0;
    std::string existing_file = advanced_properties.existing_file;
    while (timer_set_iter != timer_set.end()) {
        timer_timestamp = getSystemTimestamp();
        if (timer_set_iter->tcmode == 1)
            timer_timestamp = packet->T;
        if (utc_time_comp_obj.operator() (timer_timestamp, *timer_set_iter))
            break;
        recording_enabled = (timer_set_iter->tcstatus == 1);
        timer_set_iter++;
    }


    std::map<std::string, std::string>::iterator curFileIter = stream_to_file_mapping.find(stream_id);
    if (curFileIter != stream_to_file_mapping.end() && advanced_properties.reset_on_retune){
    	std::string filename = curFileIter->second;
    	std::map<std::string, file_struct>::iterator cur_file_desc_iter = file_to_struct_mapping.find(filename);
    	if(cur_file_desc_iter != file_to_struct_mapping.end()){
    		bool close = false;
    		close |=  (cur_file_desc_iter->second.lastSRI.xdelta !=  packet->SRI.xdelta);
    		close |=  (cur_file_desc_iter->second.lastSRI.mode !=  packet->SRI.mode);
    		double old_cf = 0;
    		double new_cf = 0;
    		try{
    			old_cf = getKeywordValueByID<double>(& cur_file_desc_iter->second.lastSRI, "COL_RF");
    		}catch(...){};
    		try{
    			new_cf = getKeywordValueByID<double>(& packet->SRI, "COL_RF");
    		}catch(...){};
    		close |=  (old_cf != new_cf);

    		old_cf = 0;
    		new_cf = 0;
			try {
				old_cf = getKeywordValueByID<double> (&cur_file_desc_iter->second.lastSRI, "CHAN_RF");
			} catch (...) {
			};
			try {
				new_cf = getKeywordValueByID<double> (&packet->SRI, "CHAN_RF");
			} catch (...) {
			};
			close |= (old_cf != new_cf);

			if(close){
				close_file(filename, packet->T, stream_id);
				stream_to_file_mapping.erase(stream_id);
			}
    	}
    }
    std::map<std::string, file_struct>::iterator curFileDescIter = file_to_struct_mapping.end();
    
    if (advanced_properties.swap_bytes) {
        if (dt.find("16") != std::string::npos) {
            //std::cout << "FileWriter_i::DEBUG - swap_bytes - swapping 16\n";
            std::vector<uint16_t> *svp = (std::vector<uint16_t> *) & packet->dataBuffer;
            std::transform(svp->begin(), svp->end(), svp->begin(), Byte_Swap16<uint16_t>);
        } else if (dt.find("32") != std::string::npos) {
            //std::cout << "FileWriter_i::DEBUG - swap_bytes - swapping 32\n";
            std::vector<uint32_t> *svp = (std::vector<uint32_t> *) & packet->dataBuffer;
            std::transform(svp->begin(), svp->end(), svp->begin(), Byte_Swap32<uint32_t>);
        } else if (dt.find("64") != std::string::npos) {
            //std::cout << "FileWriter_i::DEBUG - swap_bytes - swapping 64\n";
            std::vector<uint64_t> *svp = (std::vector<uint64_t> *) & packet->dataBuffer;
            std::transform(svp->begin(), svp->end(), svp->begin(), Byte_Swap64<uint64_t>);
        }
    }
    do {
        try {
            // Ensure that recording is enabled
            if (!recording_enabled)
                throw std::logic_error("Recording Disabled. Dropping Packet!");
            // Ensure File is Open
            bool new_metadata_file = false;
            if (curFileIter == stream_to_file_mapping.end()) {

                std::string basename = stream_to_basename(stream_id, packet->SRI, packet->T, file_format, dt);
                std::string filename = prop_dirname + "/" + basename;
                
                // Unless the file is appending, do something if the file already exists
                if (filesystem->exists(filename)) {
                    if (existing_file == "DROP")
                        throw std::logic_error("File Exists. Dropping Packet!");
                    else if (existing_file == "TRUNCATE") {
                        curFileDescIter = file_to_struct_mapping.find(filename);
                        if (curFileDescIter != file_to_struct_mapping.end())
                            throw std::logic_error("Cannot truncate a file currently being written to by this File Writer. Dropping Packet!");
                        filesystem->delete_file(filename);
                    } else if (existing_file == "RENAME") {
                        char countBuf[5];
                        int counter = 1;
                        std::string tmpFN = filename;
                        do {
                            sprintf(countBuf, "%d", counter);
                            tmpFN = filename + "-" + std::string(countBuf);
                            counter++;
                        } while (filesystem->exists(tmpFN) && counter <= 1024);
                        filename = tmpFN;
                        if (filesystem->exists(filename))
                            throw std::logic_error("Cannot rename file to an available name. Dropping Reset of Packet!");
                    } else if (existing_file == "APPEND") {
                    }
                }
                // Attempt to open the file
                if (filesystem->open_file(filename, true, true)) {
                    LOG_DEBUG(FileWriter_i, "DEBUG OPENED STREAM: " << packet->streamID << " (" << stream_id << ") " << " OR FILE: " << filename);

                    file_io_message_struct file_event;
                    file_event.file_operation = "OPEN";
                    file_event.filename = filesystem->uri_to_file(filename);
                    file_event.stream_id = stream_id;
                    MessageEvent_out->sendMessage(file_event);

                    // Correlates stream ID to file
                    curFileIter = stream_to_file_mapping.insert(std::make_pair(stream_id, filename)).first;

                    // Keeps track of file information
                    curFileDescIter = file_to_struct_mapping.find(filename);
                    if (curFileDescIter == file_to_struct_mapping.end()) {
                        double curTime = packet->T.toff + packet->T.twsec + packet->T.tfsec;
                        curFileDescIter = file_to_struct_mapping.insert(std::make_pair(filename, file_struct(filename, current_writer_type, curTime))).first;
                        curFileDescIter->second.stream_id = stream_id;
                        curFileDescIter->second.file_size_internal = filesystem->file_size(filename);
                    } else
                        curFileDescIter->second.num_writers++;
                    curFileDescIter->second.lastSRI = packet->SRI;

                    size_t pos = filesystem->file_tell(filename);
                    if (curFileDescIter->second.file_type == BLUEFILE) {
                        curFileDescIter->second.lastSRI = packet->SRI;
                        curFileDescIter->second.midas_type = midas_type<PACKET_ELEMENT_TYPE > ((curFileDescIter->second.lastSRI.mode == 0));
                        //std::cout << " BLUEFILE WITH POS: " << pos << " AND NUM WRITER: " << curFileDescIter->second.num_writers << std::endl;
                        if (pos == 0) {
                            filesystem->file_seek(filename, BLUEFILE_BLOCK_SIZE);
                        } else if (curFileDescIter->second.num_writers == 1) {
                            std::vector<char> buff(BLUEFILE_BLOCK_SIZE);
                            filesystem->file_seek(filename, 0);
                            filesystem->read(filename, &buff, BLUEFILE_BLOCK_SIZE);
                            blue::HeaderControlBlock hcb = blue::HeaderControlBlock((const blue::hcb_s *) & buff[0]);
                            if (hcb.validate(false) != 0) {
                                LOG_WARN(FileWriter_i, "CAN NOT READ BLUEHEADER FOR APPENDING DATA!\n");
                            }
                            filesystem->file_seek(filename, hcb.getDataStart() + hcb.getDataSize());
                        }
                    }


                    // Opens metadata file
                    if (advanced_properties.enable_metadata_file && curFileDescIter->second.uri_metadata_filename.empty()) {
                        curFileDescIter->second.uri_metadata_filename = filename + INPROCESS;
                        if (!filesystem->open_file(curFileDescIter->second.uri_metadata_filename, true, true)) {
                            LOG_ERROR(FileWriter_i, "ERROR (" << __PRETTY_FUNCTION__ << "): COULD NOT OPEN METADATA FILE: " << curFileDescIter->second.uri_metadata_filename);
                            curFileDescIter->second.uri_metadata_filename.clear();
                            throw std::logic_error("COULD NOT OPEN METADATA FILE!");
                        }
                        std::string openXML = "<FileWriter_metadata>";
                        filesystem->write(curFileDescIter->second.uri_metadata_filename, &openXML, advanced_properties.force_flush);
                        new_metadata_file = true;
                    }


                }
                // Throw error if file wasnt opened
                if (curFileIter == stream_to_file_mapping.end()) {
                    LOG_ERROR(FileWriter_i, "ERROR (" << __PRETTY_FUNCTION__ << "): COULD NOT OPEN FILE: " << filename);
                    throw std::logic_error("COULD NOT OPEN FILE!");
                }
            }

            curFileDescIter = file_to_struct_mapping.find(curFileIter->second);
            if (curFileDescIter == file_to_struct_mapping.end())
                break;

            bool eos = (packet->EOS && (stream_id == packet->streamID));
            size_t write_bytes = packet->dataBuffer.size() * sizeof (packet->dataBuffer[0]) - packet_pos;

            long maxSize_time_size = maxSize;
            if (advanced_properties.max_file_time > 0) {
                // seconds					*    samples/second     * B/sample
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
                }
            }

            // Output Data To File
            LOG_DEBUG(FileWriter_i, " WRITING: " << write_bytes << " BYTES TO FILE: " << curFileIter->second);

            filesystem->write(curFileIter->second, (char*) &packet->dataBuffer[0] + packet_pos, write_bytes, advanced_properties.force_flush);
            curFileDescIter->second.file_size_internal += write_bytes;
            packet_pos += write_bytes;

            // Output Metadata To File
            if (new_metadata_file || (packet->sriChanged && !curFileDescIter->second.uri_metadata_filename.empty())) {
                std::string metadata = sri_to_XMLstring(packet->SRI);
                filesystem->write(curFileDescIter->second.uri_metadata_filename, &metadata, advanced_properties.force_flush);
                new_metadata_file = false;
            }
            if (packet->sriChanged) {
                curFileDescIter->second.lastSRI = packet->SRI;
                curFileDescIter->second.midas_type = midas_type<PACKET_ELEMENT_TYPE > ((curFileDescIter->second.lastSRI.mode == 0));
                packet->SRI.streamID = stream_id.c_str();
                dataFile_out->pushSRI(packet->SRI);
            }

            // Close File
    		if (eos || reached_max_size) {
    			LOG_DEBUG(FileWriter_i, " *** PROCESSING EOS FOR STREAM ID : " << stream_id );
    			if (eos && !curFileDescIter->second.uri_metadata_filename.empty()) {
                    std::string metadata = eos_to_XMLstring(packet->SRI);
                    filesystem->write(curFileDescIter->second.uri_metadata_filename, &metadata, advanced_properties.force_flush);
                }

    			close_file(curFileIter->second, packet->T, stream_id);
    			if(reached_max_size && advanced_properties.reset_on_max_file){
    				stream_to_file_mapping.erase(stream_id);
                }
            }

        } catch (...) {
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
	if(stream_id.empty()){
		stream_id = curFileDescIter->second.lastSRI.streamID;
		try {
			std::string tmp = getKeywordValueByID<std::string> ( &curFileDescIter->second.lastSRI, "STREAM_GROUP");
			stream_id = tmp;
		} catch (...) {
		};
	}


	curFileDescIter->second.num_writers--;
	if (curFileDescIter->second.num_writers <= 0) {
		if (curFileDescIter->second.file_type == BLUEFILE) {
			size_t curPos = filesystem->file_tell(curFileDescIter->second.uri_filename);
			std::pair<blue::HeaderControlBlock, std::vector<char> > bheaders = createBluefilesHeaders(curFileDescIter->second.lastSRI,
							curPos, curFileDescIter->second.midas_type, curFileDescIter->second.start_time);
			filesystem->file_seek(curFileDescIter->second.uri_filename, 0);
			blue::hcb_s tmp_hcb = bheaders.first.getHCB();
			filesystem->write(curFileDescIter->second.uri_filename, (char*) & tmp_hcb, BLUEFILE_BLOCK_SIZE, advanced_properties.force_flush);
			filesystem->file_seek(curFileDescIter->second.uri_filename, curPos);
			filesystem->write(curFileDescIter->second.uri_filename, (char*) &bheaders.second[0], bheaders.second.size(), advanced_properties.force_flush);
		}
		filesystem->close_file(filename);

		file_io_message_struct file_event;
		file_event.file_operation = "CLOSE";
		file_event.filename = filesystem->uri_to_file(filename);
		file_event.stream_id = stream_id;
		BULKIO::PrecisionUTCTime tstamp = bulkio::time::utils::now();
		MessageEvent_out->sendMessage(file_event);
		dataFile_out->pushPacket(file_event.filename.c_str(), tstamp, true, stream_id.c_str());
		LOG_DEBUG(FileWriter_i, "CLOSED FILE: " << filename);
		if (!curFileDescIter->second.uri_metadata_filename.empty()) {
			std::string closeXML = "</FileWriter_metadata>";
			filesystem->write(curFileDescIter->second.uri_metadata_filename, &closeXML, advanced_properties.force_flush);
			filesystem->close_file( curFileDescIter->second.uri_metadata_filename);
			filesystem->move_file( curFileDescIter->second.uri_metadata_filename, curFileDescIter->second.uri_filename + COMPLETE);
		}
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

    return bn;
}

std::string FileWriter_i::sri_to_XMLstring(const BULKIO::StreamSRI& sri) {
    std::ostringstream sri_string;
    sri_string << "<sri>";
    sri_string << "<streamID>" << sri.streamID << "</streamID>";
    sri_string << "<hversion>" << sri.hversion << "</hversion>";
    sri_string << "<xstart>" << sri.xstart << "</xstart>";
    sri_string << "<xdelta>" << sri.xdelta << "</xdelta>";
    sri_string << "<xunits>" << sri.xunits << "</xunits>";
    sri_string << "<subsize>" << sri.subsize << "</subsize>";
    sri_string << "<ystart>" << sri.ystart << "</ystart>";
    sri_string << "<ydelta>" << sri.ydelta << "</ydelta>";
    sri_string << "<yunits>" << sri.yunits << "</yunits>";
    sri_string << "<mode>" << sri.mode << "</mode>";
    for (unsigned int i = 0; i < sri.keywords.length(); i++) {
        sri_string << "<keyword><id>" << sri.keywords[i].id << "</id>";
        sri_string << "<value>" << ossie::any_to_string(sri.keywords[i].value) << "</value></keyword>";
    }
    sri_string << "</sri>";

    return std::string(sri_string.str());
}

std::string FileWriter_i::eos_to_XMLstring(const BULKIO::StreamSRI& sri) {
    std::ostringstream eos_string;
    eos_string << "<eos>";
    eos_string << "<streamID>" << sri.streamID << "</streamID>";
    eos_string << "</eos>";
    return std::string(eos_string.str());
}

std::pair<blue::HeaderControlBlock, std::vector<char> > FileWriter_i::createBluefilesHeaders(const BULKIO::StreamSRI& sri, size_t datasize, std::string midasType, double start_time) {
    blue::HeaderControlBlock hcb;
    blue::ExtendedHeader ecb;


    hcb.setTypeCode(1000);
    hcb.setXdelta(sri.xdelta);
    hcb.setXunits(sri.xunits);
    hcb.setXstart(sri.xstart);
    hcb.setFormatCode(midasType);
    hcb.setTimeCode(start_time + long(631152000));
    hcb.setDataSize(datasize - BLUEFILE_BLOCK_SIZE);
    hcb.setHeaderRep(blue::IEEE); // Note: WVT cannot parse EEEI headers (EEEI datasets are ok)

    if (advanced_properties.swap_bytes) {
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
            short tmp;
            sri.keywords[i].value >>= tmp;
            blue::Keyword kw(blue::INTEGER, id.c_str());
            kw.push();
            kw.setValue(tmp, 0);
            kw.setUnits(blue::NOT_APPLICABLE);
            midasKeywords.insert(kw);
        } else if (val_kind == CORBA::tk_long) {
            long tmp;
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
    int lastkeyword = -1;
    for (; it != midasKeywords.end(); ++it) {
        //        cout << "Snapper_cpp_impl1_i::writeMidasExtendedHeader keyword " << it->getName() << " value " << it->getRawData() << " format " << it->getFormat() << " units " << it->getUnits() << " isIndexKeyword " << it->isIndexKeyword() << "  ===================" << endl;
        int savedSize = buff.size();
        int newKwSize = 0;
        if (it->getFormat() == blue::ASCII) {
            std::string data = it->getString();
            newKwSize = blue::MidasKey::PackMemory(it->getName(), data, &buff);
        } else {
            newKwSize = blue::MidasKey::PackMemory(it->getName(), it->getRawData(), it->size(), it->getFormat(), &buff);
        }

        if (newKwSize > 0)
            lastkeyword = savedSize;

        blue::UnitCodeEnum unit = it->getUnits();
        if (unit != blue::NOT_APPLICABLE && unit != blue::NOT_APPLICABLE) {
            savedSize = buff.size();
            int16_t iunit = static_cast<int16_t> (unit);
            newKwSize = blue::MidasKey::PackMemory(it->getName() + ".UNITS", &iunit, 1, blue::INTEGER, &buff);
        }

        if (it->isIndexKeyword()) {
            savedSize = buff.size();
            if (it->getIndexFormat() == blue::ASCII) {
                std::string data = it->getIndexString();
                newKwSize = blue::MidasKey::PackMemory(it->getName() + ".INDEX", data, &buff);
            } else {
                std::vector<char> data;
                it->getRawIndexData(&data);
                newKwSize = blue::MidasKey::PackMemory(
                        it->getName() + ".INDEX", &data[0],
                        it->size(), it->getIndexFormat(), &buff);
            }

            unit = it->getIndexUnits();
            if (unit != blue::NOT_APPLICABLE && unit != blue::NOT_APPLICABLE) {
                savedSize = buff.size();
                int16_t iunit = static_cast<int16_t> (unit);
                newKwSize = blue::MidasKey::PackMemory(
                        it->getName() + ".INDEX_UNITS", &iunit, 1, blue::INTEGER, &buff);
            }
        } // if indexKey

        if (newKwSize > 0)
            lastkeyword = savedSize;
    }

    //ExtendedHeader::close_()
    char *bb = &(buff[0]);
    const int NN = static_cast<int> (buff.size());
    // Roundup(NN, 512); // New buffer size padded to a 512 boundary.
    int newsize = NN % 512 ? (NN / 512 + 1)*512 : NN;
    int newpad = newsize - NN; // Number of bytes to be added.

    if (newpad > 0) {
        blue::midaskeystruct *mks = reinterpret_cast<blue::midaskeystruct*> (bb + lastkeyword);
        mks->next_offset += newpad;
        mks->non_data_len += newpad;

        buff.insert(buff.end(), newpad, 0);
    }
    size_t startBlock = size_t(ceil(double(datasize) / double(BLUEFILE_BLOCK_SIZE)));
    size_t numZeros = startBlock * BLUEFILE_BLOCK_SIZE - datasize;
    hcb.setExtStart(startBlock);
    hcb.setExtSize(static_cast<int> (buff.size()));
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

