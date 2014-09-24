#include "FileWriter_base.h"

/*******************************************************************************************

    AUTO-GENERATED CODE. DO NOT MODIFY

    The following class functions are for the base class for the component class. To
    customize any of these functions, do not modify them here. Instead, overload them
    on the child class

******************************************************************************************/

FileWriter_base::FileWriter_base(const char *uuid, const char *label) :
    Resource_impl(uuid, label),
    ThreadedComponent()
{
    loadProperties();

    dataChar_in = new bulkio::InCharPort("dataChar_in");
    addPort("dataChar_in", dataChar_in);
    dataOctet_in = new bulkio::InOctetPort("dataOctet_in");
    addPort("dataOctet_in", dataOctet_in);
    dataShort_in = new bulkio::InShortPort("dataShort_in");
    addPort("dataShort_in", dataShort_in);
    dataUshort_in = new bulkio::InUShortPort("dataUshort_in");
    addPort("dataUshort_in", dataUshort_in);
    dataFloat_in = new bulkio::InFloatPort("dataFloat_in");
    addPort("dataFloat_in", dataFloat_in);
    dataDouble_in = new bulkio::InDoublePort("dataDouble_in");
    addPort("dataDouble_in", dataDouble_in);
    dataXML_in = new bulkio::InXMLPort("dataXML_in");
    addPort("dataXML_in", dataXML_in);
    DomainManager_out = new CF_DomainManager_Out_i("DomainManager_out", this);
    addPort("DomainManager_out", DomainManager_out);
    MessageEvent_out = new MessageSupplierPort("MessageEvent_out");
    addPort("MessageEvent_out", MessageEvent_out);
    dataFile_out = new bulkio::OutFilePort("dataFile_out");
    addPort("dataFile_out", dataFile_out);
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
    delete DomainManager_out;
    DomainManager_out = 0;
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
    Resource_impl::start();
    ThreadedComponent::startThread();
}

void FileWriter_base::stop() throw (CORBA::SystemException, CF::Resource::StopError)
{
    Resource_impl::stop();
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

    Resource_impl::releaseObject();
}

void FileWriter_base::loadProperties()
{
    addProperty(debug_output,
                false,
                "debug_output",
                "debug_output",
                "readwrite",
                "",
                "external",
                "configure");

    addProperty(destination_uri,
                "sca:///data/%STREAMID%.%TIMESTAMP%.%MODE%.%SR%.%DT%",
                "destination_uri",
                "destination_uri",
                "readwrite",
                "",
                "external",
                "configure");

    addProperty(destination_uri_suffix,
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

    addProperty(recording_timer,
                "recording_timer",
                "recording_timer",
                "readwrite",
                "",
                "external",
                "configure");

}


