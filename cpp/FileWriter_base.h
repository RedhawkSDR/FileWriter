#ifndef FILEWRITER_IMPL_BASE_H
#define FILEWRITER_IMPL_BASE_H

#include <boost/thread.hpp>
#include <ossie/Resource_impl.h>
#include <ossie/ThreadedComponent.h>

#include <bulkio/bulkio.h>
#include "port_impl.h"
#include <ossie/MessageInterface.h>
#include "struct_props.h"

class FileWriter_base : public Resource_impl, protected ThreadedComponent
{
    friend class CF_DomainManager_Out_i;

    public:
        FileWriter_base(const char *uuid, const char *label);
        ~FileWriter_base();

        void start() throw (CF::Resource::StartError, CORBA::SystemException);

        void stop() throw (CF::Resource::StopError, CORBA::SystemException);

        void releaseObject() throw (CF::LifeCycle::ReleaseError, CORBA::SystemException);

        void loadProperties();

    protected:
        // Member variables exposed as properties
        bool debug_output;
        std::string destination_uri;
        std::string destination_uri_suffix;
        std::string file_format;
        bool swap_bytes;
        bool recording_enabled;
        advanced_properties_struct advanced_properties;
        file_io_message_struct file_io_message;
        std::vector<timer_struct_struct> recording_timer;

        // Ports
        bulkio::InCharPort *dataChar_in;
        bulkio::InOctetPort *dataOctet_in;
        bulkio::InShortPort *dataShort_in;
        bulkio::InUShortPort *dataUshort_in;
        bulkio::InFloatPort *dataFloat_in;
        bulkio::InDoublePort *dataDouble_in;
        bulkio::InXMLPort *dataXML_in;
        CF_DomainManager_Out_i *DomainManager_out;
        MessageSupplierPort *MessageEvent_out;
        bulkio::OutFilePort *dataFile_out;

    private:
};
#endif // FILEWRITER_IMPL_BASE_H
