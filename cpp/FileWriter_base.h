/*
 * This file is protected by Copyright. Please refer to the COPYRIGHT file
 * distributed with this source distribution.
 *
 * This file is part of REDHAWK Basic Components FileWriter.
 *
 * REDHAWK Basic Components FileWriter is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * REDHAWK Basic Components FileWriter is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/.
 */

#ifndef FILEWRITER_BASE_IMPL_BASE_H
#define FILEWRITER_BASE_IMPL_BASE_H

#include <boost/thread.hpp>
#include <ossie/Component.h>
#include <ossie/ThreadedComponent.h>

#include <bulkio/bulkio.h>
#include <ossie/MessageInterface.h>
#include "struct_props.h"

class FileWriter_base : public Component, protected ThreadedComponent
{
    public:
        FileWriter_base(const char *uuid, const char *label);
        ~FileWriter_base();

        void start() throw (CF::Resource::StartError, CORBA::SystemException);

        void stop() throw (CF::Resource::StopError, CORBA::SystemException);

        void releaseObject() throw (CF::LifeCycle::ReleaseError, CORBA::SystemException);

        void loadProperties();

    protected:
        // Member variables exposed as properties
        std::string destination_uri;
        std::string destination_uri_suffix;
        std::string file_format;
        bool swap_bytes;
        bool recording_enabled;
        advanced_properties_struct advanced_properties;
        file_io_message_struct file_io_message;
        component_status_struct component_status;
        std::vector<timer_struct_struct> recording_timer;

        // Ports
        bulkio::InCharPort *dataChar_in;
        bulkio::InOctetPort *dataOctet_in;
        bulkio::InShortPort *dataShort_in;
        bulkio::InUShortPort *dataUshort_in;
        bulkio::InFloatPort *dataFloat_in;
        bulkio::InDoublePort *dataDouble_in;
        bulkio::InXMLPort *dataXML_in;
        MessageSupplierPort *MessageEvent_out;
        bulkio::OutFilePort *dataFile_out;

    private:
};
#endif // FILEWRITER_BASE_IMPL_BASE_H
