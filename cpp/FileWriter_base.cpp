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

#include "FileWriter_base.h"

/*******************************************************************************************

    AUTO-GENERATED CODE. DO NOT MODIFY

    The following class functions are for the base class for the component class. To
    customize any of these functions, do not modify them here. Instead, overload them
    on the child class

******************************************************************************************/

FileWriter_base::FileWriter_base(const char *uuid, const char *label) :
    Component(uuid, label),
    ThreadedComponent()
{
    loadProperties();

    dataChar_in = new bulkio::InCharPort("dataChar_in");
    addPort("dataChar_in", "Char input port for data. ", dataChar_in);
    dataOctet_in = new bulkio::InOctetPort("dataOctet_in");
    addPort("dataOctet_in", "Octet input port for data. ", dataOctet_in);
    dataShort_in = new bulkio::InShortPort("dataShort_in");
    addPort("dataShort_in", "Short input port for data. ", dataShort_in);
    dataUshort_in = new bulkio::InUShortPort("dataUshort_in");
    addPort("dataUshort_in", "Unsigned short input port for data. ", dataUshort_in);
    dataFloat_in = new bulkio::InFloatPort("dataFloat_in");
    addPort("dataFloat_in", "Float input port for data. ", dataFloat_in);
    dataDouble_in = new bulkio::InDoublePort("dataDouble_in");
    addPort("dataDouble_in", "Double input port for data. ", dataDouble_in);
    dataXML_in = new bulkio::InXMLPort("dataXML_in");
    addPort("dataXML_in", "XML input port for data. ", dataXML_in);
    MessageEvent_out = new MessageSupplierPort("MessageEvent_out");
    addPort("MessageEvent_out", "MessageEvent output port. Messages sent for the opening and closing of files. ", MessageEvent_out);
    dataFile_out = new bulkio::OutFilePort("dataFile_out");
    addPort("dataFile_out", "Output port that provided the file URL for writen file. ", dataFile_out);
}

FileWriter_base::~FileWriter_base()
{
    delete dataChar_in;
    dataChar_in = 0;
    delete dataOctet_in;
    dataOctet_in = 0;
    delete dataShort_in;
    dataShort_in = 0;
    delete dataUshort_in;
    dataUshort_in = 0;
    delete dataFloat_in;
    dataFloat_in = 0;
    delete dataDouble_in;
    dataDouble_in = 0;
    delete dataXML_in;
    dataXML_in = 0;
    delete MessageEvent_out;
    MessageEvent_out = 0;
    delete dataFile_out;
    dataFile_out = 0;
}

/*******************************************************************************************
    Framework-level functions
    These functions are generally called by the framework to perform housekeeping.
*******************************************************************************************/
void FileWriter_base::start() throw (CORBA::SystemException, CF::Resource::StartError)
{
    Component::start();
    ThreadedComponent::startThread();
}

void FileWriter_base::stop() throw (CORBA::SystemException, CF::Resource::StopError)
{
    Component::stop();
    if (!ThreadedComponent::stopThread()) {
        throw CF::Resource::StopError(CF::CF_NOTSET, "Processing thread did not die");
    }
}

void FileWriter_base::releaseObject() throw (CORBA::SystemException, CF::LifeCycle::ReleaseError)
{
    // This function clears the component running condition so main shuts down everything
    try {
        stop();
    } catch (CF::Resource::StopError& ex) {
        // TODO - this should probably be logged instead of ignored
    }

    Component::releaseObject();
}

void FileWriter_base::loadProperties()
{
    addProperty(destination_uri,
                "sca:///data/%STREAMID%.%TIMESTAMP%.%MODE%.%SR%.%DT%",
                "destination_uri",
                "destination_uri",
                "readwrite",
                "",
                "external",
                "configure");

    addProperty(destination_uri_suffix,
                "",
                "destination_uri_suffix",
                "destination_uri_suffix",
                "readwrite",
                "",
                "external",
                "configure");

    addProperty(file_format,
                "RAW",
                "file_format",
                "file_format",
                "readwrite",
                "",
                "external",
                "configure");

    addProperty(swap_bytes,
                false,
                "swap_bytes",
                "swap_bytes",
                "readwrite",
                "",
                "external",
                "configure");

    addProperty(recording_enabled,
                true,
                "recording_enabled",
                "recording_enabled",
                "readwrite",
                "",
                "external",
                "configure");

    addProperty(advanced_properties,
                advanced_properties_struct(),
                "advanced_properties",
                "advanced_properties",
                "readwrite",
                "",
                "external",
                "configure");

    addProperty(file_io_message,
                file_io_message_struct(),
                "file_io_message",
                "",
                "readwrite",
                "",
                "external",
                "message");

    addProperty(component_status,
                component_status_struct(),
                "component_status",
                "component_status",
                "readonly",
                "",
                "external",
                "configure");

    addProperty(recording_timer,
                "recording_timer",
                "recording_timer",
                "readwrite",
                "",
                "external",
                "configure");

}


