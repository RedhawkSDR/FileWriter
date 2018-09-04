#!/usr/bin/env python
#
# This file is protected by Copyright. Please refer to the COPYRIGHT file distributed with this 
# source distribution.
# 
# This file is part of REDHAWK Basic Components FileWriter.
# 
# REDHAWK Basic Components FileWriter is free software: you can redistribute it and/or modify it under the terms of 
# the GNU Lesser General Public License as published by the Free Software Foundation, either 
# version 3 of the License, or (at your option) any later version.
# 
# REDHAWK Basic Components FileWriter is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
# PURPOSE.  See the GNU Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public License along with this 
# program.  If not, see http://www.gnu.org/licenses/.
#

import ossie.utils.testing
import os
import time
from omniORB import any
from ossie.utils import sb, bulkio
from ossie.cf import CF
import filecmp
import struct
from ossie.properties import props_from_dict, props_to_dict
from ossie.utils.bluefile import bluefile, bluefile_helpers
from ossie.utils.bulkio import bulkio_helpers
from bulkio.bulkioInterfaces import BULKIO, BULKIO__POA
from bulkio.sri import create as createSri
from bulkio.timestamp import create as createTs
import bulkio as bio
import numpy


###############################################################
# FIXES TO bluefile_helpers.py
###############################################################

def hdr_to_sri(hdr, stream_id):
    """
    Generates a StreamSRI object based on the information contained in the
    X-Midas header file.
    
    Inputs:
        <hdr>    The X-Midas header file
        <stream_id>    The stream id
    
    Output:
        Returns a BULKIO.StreamSRI object 
    """
    hversion = 1  
    xstart = hdr['xstart']
    xdelta = hdr['xdelta']
    xunits = hdr['xunits']
    data_type = hdr['type']
    data_format = hdr['format']

    
    # The subsize needs to be 0 if one-dimensional or > 0 otherwise so 
    # using the data type to find out
    if data_type == 1000:
        subsize = 0
        ystart = 0
        ydelta = 0
        yunits = 0  # FIXME added yunits
    else:
        #subsize = str(data_type)[0] # FIXME replaced this with line below
        subsize = hdr['subsize']
        ystart = hdr['ystart']  
        ydelta = hdr['ydelta']  
        yunits = hdr['yunits']  # FIXME added yunits
    
    # The mode is based on the data type: 0 if is Scalar or 1 if it is 
    # Complex.  Setting it to -1 for any other type
    if data_format.startswith('S'):
        mode = 0
    elif data_format.startswith('C'):
        mode = 1
    else:
        mode = -1            
    
    kwds = []        
    
    # Getting all the items in the extended header
    if hdr.has_key('ext_header'):
        ext_hdr = hdr['ext_header']
        if isinstance(ext_hdr, dict):            
            for key, value in ext_hdr.iteritems():
                # WARNING: CORBA types are hard-coded through here
                dt = CF.DataType(key, ossie.properties.to_tc_value(long(value), 'long')) # FIXME - cast to long from numpy (or other) type (in 32-bit, the value is np.int32 and causes CORBA wrong python type error when SRI is pushed)
                kwds.append(dt)
        elif isinstance(ext_hdr, list):
            for item in ext_hdr:
                try:
                    dt = CF.DataType(item[0], ossie.properties.to_tc_value(long(item[1]), 'long')) #FIXME - cast to long from numpy (or other) type
                    kwds.append(dt)
                except:
                    continue
            
    return BULKIO.StreamSRI(hversion, xstart, xdelta, xunits, 
                            subsize, ystart, ydelta, yunits, # FIXME - changed this to return yunits
                            mode, stream_id, True, kwds)
    
bluefile_helpers.hdr_to_sri = hdr_to_sri
    
def sri_to_hdr(sri, data_type, data_format):
    """
    Generates an X-Midas header file from the SRI information.
    
    Inputs:
        <sri>          The BULKIO.StreamSRI object
        <data_type>    The X-Midas file type (1000, 2000, etc)
        <data_format>  The X-Midas data format (SD, SF, CF, etc)
    
    Output:
        Returns an X-Midas header file 
    """
    kwds = {}
    
    kwds['timecode'] = 631152000.0 # Default to RH epoch of Jan 1 1970
    
    kwds['xstart'] = sri.xstart
    kwds['xdelta'] = sri.xdelta
    kwds['xunits'] = sri.xunits
    
    # FIXME: Slight hack - bluefile.py T1000 adjunct header has a few additional fields.
    # One field is 'fill3' and has a default value of 1.0. BLUE 1.1 doesn't have
    # that field defined, and the memory at that offset (32) is zero filled (padded).
    # Here we tell the input file to default to 0.0 such that it will match FW output.
    kwds['fill3'] = 0.0

    kwds['subsize'] = sri.subsize # FIXME - THIS is added to fix function

    kwds['ystart'] = sri.ystart
    kwds['ydelta'] = sri.ydelta
    kwds['yunits'] = sri.yunits
    
    kwds['format'] = data_format
    kwds['type'] = data_type
    
    ext_hdr = sri.keywords
    if len(ext_hdr) > 0:
        items = []
        for item in ext_hdr:
            items.append((item.id, item.value.value()))
        
        kwds['ext_header'] = items
        
    return bluefile.header(**kwds)

bluefile_helpers.sri_to_hdr = sri_to_hdr


def bluefile_helpers_BlueFileWriter_pushPacket(self, data, ts, EOS, stream_id):
        """
        Pushes data to the file.
        
        Input:
            <data>        The actual data to write to the file
            <ts>          The timestamp
            <EOS>         Flag indicating if this is the End Of the Stream
            <stream_id>   The name of the file
        """
        self.port_lock.acquire()
        if EOS:
            self.gotEOS = True
            self.eos_cond.notifyAll()
        else:
            self.gotEOS = False
        try:
            #if self.header and not self.gotTimecode:
            if self.header and self.header['timecode'] <= 631152000.0:
                self.header['timecode'] = ts.twsec+ts.tfsec + long(631152000)
                bluefile.writeheader(self.outFile, self.header, keepopen=0, ext_header_type=list)

            if self.header and self.header['format'][1] == 'B':
                # convert back from string to array of 8-bit integers
                try: # FIXME - added try/except to handle when data is NOT a string of bytes
                    data = numpy.fromstring(data, numpy.int8)
                except TypeError:
                    data = numpy.array(data, numpy.int8)

            # If complex data, need to convert data back from array of scalar values
            # to some form of complex values
            if self.header and self.header['format'][0] == 'C':
                # float and double are handled by numpy
                # each array element is a single complex value
                if self.header['format'][1] in ('F', 'D'):
                    data = bulkio_helpers.bulkioComplexToPythonComplexList(data)
                # other data types are not handled by numpy
                # each element is two value array representing real and imaginary values
                # if data is also framed, wait to reshape everything at once
                elif self.header['subsize'] == 0: # FIXME - this is changed
                    # Need to rehape the data into complex value pairs
                    data = numpy.reshape(data,(-1,2))

            # FIXME - the entire subsize framing section is new
            # If framed data, need to frame the data according to subsize
            if self.header and self.header['subsize'] != 0:
                # If scalar or single complex values, just frame it
                if self.header['format'][0] != 'C' or  self.header['format'][1] in ('F', 'D'):
                    data = numpy.reshape(data,(-1, int(self.header['subsize'])))
                # otherwise, frame and pair as complex values
                else:
                    data = numpy.reshape(data,(-1, int(self.header['subsize']), 2))

            bluefile.write(self.outFile, hdr=None, data=data, 
                       append=1)     
        finally:
            self.port_lock.release()
            
bluefile_helpers.BlueFileWriter.pushPacket = bluefile_helpers_BlueFileWriter_pushPacket

def bluefile_helpers_BlueFileReader_run(self, infile, pktsize=1024, streamID=None):
        """
        Pushes the data through the connected port.  Each packet of data 
        contains no more than pktsize elements.  Once all the elements have 
        been sent, the method sends an empty list with the EOS set to True to 
        indicate the end of the stream.
        
        Inputs:
            <infile>     The name of the X-Midas file containing the data 
                         to push
            <pktsize>    The maximum number of elements to send on each push
            <streamID>   The stream ID to be used, if None, then it defaults to filename 
        """        
        hdr, data = bluefile.read(infile, list)
        # generates a new SRI based on the header of the file
        path, stream_id = os.path.split(infile)
        if streamID == None:
            sri = hdr_to_sri(hdr, stream_id)
        else:
            sri = hdr_to_sri(hdr, streamID)
        self.pushSRI(sri)
        
        start = 0           # stores the start of the packet
        end = start         # stores the end of the packet

        if hdr['format'].startswith('C'):
            #data = data.flatten() # FIXME: replaced this line with below
            data = numpy.reshape(data,(-1,)) # flatten data
            if hdr['format'].endswith('F'):
                data = data.view(numpy.float32)
            elif hdr['format'].endswith('D'):
                data = data.view(numpy.float64)
        
        # FIXME: added this section
        if 'subsize' in hdr and hdr['subsize'] != 0:
            data = numpy.reshape(data,(-1,)) # flatten data

        sz = len(data)      
        self.done = False
        
        # Use midas header timecode to set time of first sample
        # NOTE: midas time is seconds since Jan. 1 1950
        #       Redhawk time is seconds since Jan. 1 1970
        currentSampleTime = 0.0
        if hdr.has_key('timecode'):
            # Set sample time to seconds since Jan. 1 1970 
            currentSampleTime = hdr['timecode'] - long(631152000)
            if currentSampleTime < 0:
                currentSampleTime = 0.0
      
        while not self.done:
            chunk = start + pktsize
            # if the next chunk is greater than the file, then grab remaining
            # only, otherwise grab a whole packet size
            if chunk > sz:
                end = sz
                self.done = True
            else:
                end = chunk
            
            dataset = data[start:end]
            
            # X-Midas returns an array, so we need to generate a list
            if hdr['format'].endswith('B'):
                d = dataset.tostring()
            else:
                d = dataset.tolist()
            start = end
            
            T = BULKIO.PrecisionUTCTime(BULKIO.TCM_CPU, BULKIO.TCS_VALID, 0.0, int(currentSampleTime), currentSampleTime - int(currentSampleTime))
            self.pushPacket(d, T, False, sri.streamID)
            dataSize = len(d)
            # TODO FIXME - this only works for TYPE 1000 data. Workaround (for now) is to size pktsize large to avoid chunks
            sampleRate = 1.0/sri.xdelta
            currentSampleTime = currentSampleTime + dataSize/sampleRate
        T = BULKIO.PrecisionUTCTime(BULKIO.TCM_CPU, BULKIO.TCS_VALID, 0.0, int(currentSampleTime), currentSampleTime - int(currentSampleTime))
        if hdr['format'].endswith('B'):
            self.pushPacket('', T, True, sri.streamID)
        else: 
            self.pushPacket([], T, True, sri.streamID)

bluefile_helpers.BlueFileReader.run = bluefile_helpers_BlueFileReader_run

class ResourceTests(ossie.utils.testing.ScaComponentTestCase):
    """Test for all resource implementations in FileWriter"""

    def testScaBasicBehavior(self):
        #######################################################################
        # Launch the resource with the default execparams
        execparams = self.getPropertySet(kinds=("execparam",), modes=("readwrite", "writeonly"), includeNil=False)
        execparams = dict([(x.id, any.from_any(x.value)) for x in execparams])
        self.launch(execparams)

        #######################################################################
        # Verify the basic state of the resource
        self.assertNotEqual(self.comp, None)
        self.assertEqual(self.comp.ref._non_existent(), False)

        self.assertEqual(self.comp.ref._is_a("IDL:CF/Resource:1.0"), True)

        #######################################################################
        # Validate that query returns all expected parameters
        # Query of '[]' should return the following set of properties
        expectedProps = []
        expectedProps.extend(self.getPropertySet(kinds=("configure", "execparam"), modes=("readwrite", "readonly"), includeNil=True))
        expectedProps.extend(self.getPropertySet(kinds=("allocate",), action="external", includeNil=True))
        props = self.comp.query([])
        props = dict((x.id, any.from_any(x.value)) for x in props)
        # Query may return more than expected, but not less
        for expectedProp in expectedProps:
            self.assertEquals(props.has_key(expectedProp.id), True)

        #######################################################################
        # Verify that all expected ports are available
        for port in self.scd.get_componentfeatures().get_ports().get_uses():
            port_obj = self.comp.getPort(str(port.get_usesname()))
            self.assertNotEqual(port_obj, None)
            self.assertEqual(port_obj._non_existent(), False)
            self.assertEqual(port_obj._is_a("IDL:CF/Port:1.0"),  True)

        for port in self.scd.get_componentfeatures().get_ports().get_provides():
            port_obj = self.comp.getPort(str(port.get_providesname()))
            self.assertNotEqual(port_obj, None)
            self.assertEqual(port_obj._non_existent(), False)
            self.assertEqual(port_obj._is_a(port.get_repid()),  True)

        #######################################################################
        # Make sure start and stop can be called without throwing exceptions
        self.comp.start()
        self.comp.stop()

        #######################################################################
        # Simulate regular resource shutdown
        self.comp.releaseObject()
    # TODO Add additional tests here
    #
    # See:
    #   ossie.utils.bulkio.bulkio_helpers,
    #   ossie.utils.bluefile.bluefile_helpers
    # for modules that will assist with testing resource with BULKIO ports
    
    def testCharPort(self):
        #######################################################################
        # Test Char Functionality
        print "\n**TESTING CHAR PORT"
        
        #Define test files
        dataFileIn = './data.in'
        dataFileOut = './data.out'
        
        #Create Test Data File if it doesn't exist
        if not os.path.isfile(dataFileIn):
            with open(dataFileIn, 'wb') as dataIn:
                dataIn.write(os.urandom(1024))
        
        #Read in Data from Test File
        size = os.path.getsize(dataFileIn)
        with open (dataFileIn, 'rb') as dataIn:
            data = list(struct.unpack('b'*size, dataIn.read(size)))
            
        #Create Components and Connections
        comp = sb.launch('../FileWriter.spd.xml')
        comp.destination_uri = dataFileOut
        comp.advanced_properties.existing_file = "TRUNCATE"
        
        source = sb.DataSource(bytesPerPush=64, dataFormat='8t')
        source.connect(comp,providesPortName='dataChar_in')
        
        #Start Components & Push Data
        sb.start()
        source.push(data)
        time.sleep(2)
        sb.stop()
        
        #Check that the input and output files are the same          
        try:
            self.assertEqual(filecmp.cmp(dataFileIn, dataFileOut), True)
        except self.failureException as e:
            comp.releaseObject()
            source.releaseObject()
            os.remove(dataFileIn)
            os.remove(dataFileOut)
            raise e
        
        #Release the components and remove the generated files
        comp.releaseObject()
        source.releaseObject()
        os.remove(dataFileIn)
        os.remove(dataFileOut)
        
        print "........ PASSED\n"
        return
    
    def testBlue1000CharPort(self):
        #######################################################################
        # Test Bluefile Type 1000 Char Functionality
        sid = 'bluefileChar'
        port_poa = BULKIO__POA.dataChar
        port_name = 'dataChar_in'
        print "\n**TESTING TYPE 1000 BLUEFILE + CHAR PORT"
        return self.blue1000PortTests(sid, port_poa, port_name)
    
    def testBlue2000CharPort(self):
        #######################################################################
        # Test Bluefile Type 2000 Char Functionality
        sid = 'bluefileChar'
        port_poa = BULKIO__POA.dataChar
        port_name = 'dataChar_in'
        print "\n**TESTING TYPE 2000 BLUEFILE + CHAR PORT"
        return self.blue2000PortTests(sid, port_poa, port_name)
    
    def testBlue1000CharPortCx(self):
        #######################################################################
        # Test Bluefile Type 1000 Char Cx Functionality
        sid = 'bluefileCharCx'
        port_poa = BULKIO__POA.dataChar
        port_name = 'dataChar_in'
        print "\n**TESTING Cx TYPE 1000 BLUEFILE + CHAR PORT"
        return self.blue1000PortTests(sid, port_poa, port_name, True)
    
    def testBlue2000CharPortCx(self):
        #######################################################################
        # Test Bluefile Type 2000 Char Functionality
        sid = 'bluefileCharCx'
        port_poa = BULKIO__POA.dataChar
        port_name = 'dataChar_in'
        print "\n**TESTING Cx TYPE 2000 BLUEFILE + CHAR PORT"
        return self.blue2000PortTests(sid, port_poa, port_name, True)
    
    def testOctetPort(self):
        #######################################################################
        # Test OCTET Functionality
        print "\n**TESTING OCTET PORT"
        
        #Define test files
        dataFileIn = './data.in'
        dataFileOut = './data.out'
        
        #Create Test Data File if it doesn't exist
        if not os.path.isfile(dataFileIn):
            with open(dataFileIn, 'wb') as dataIn:
                dataIn.write(os.urandom(1024))
        
        #Read in Data from Test File
        size = os.path.getsize(dataFileIn)
        with open (dataFileIn, 'rb') as dataIn:
            data = list(struct.unpack('B'*size, dataIn.read(size)))

        #Create Components and Connections
        comp = sb.launch('../FileWriter.spd.xml')
        comp.destination_uri = dataFileOut
        comp.advanced_properties.existing_file = "TRUNCATE"
        
        source = sb.DataSource(bytesPerPush=64, dataFormat='8u')
        source.connect(comp,providesPortName='dataOctet_in')
        
        #Start Components & Push Data
        sb.start()
        source.push(data)
        time.sleep(2)
        sb.stop()

        #Check that the input and output files are the same
        try:
            self.assertEqual(filecmp.cmp(dataFileIn, dataFileOut), True)
        except self.failureException as e:
            comp.releaseObject()
            source.releaseObject()
            os.remove(dataFileIn)
            os.remove(dataFileOut)
            raise e
        
        #Release the components and remove the generated files
        comp.releaseObject()
        source.releaseObject()
        os.remove(dataFileIn)
        os.remove(dataFileOut)
        
        print "........ PASSED\n"
        return
    
    def octetnotsupported_testBlue1000OctetPort(self):
        #######################################################################
        # Test Bluefile Type 1000 Octet Functionality
        sid = 'bluefileOctet'
        port_poa = BULKIO__POA.dataOctet
        port_name = 'dataOctet_in'
        print "\n**TESTING TYPE 1000 BLUEFILE + OCTET PORT"
        return self.blue1000PortTests(sid, port_poa, port_name)
    
    def octetnotsupported_testBlue2000OctetPort(self):
        #######################################################################
        # Test Bluefile Type 2000 Octet Functionality
        sid = 'bluefileOctet'
        port_poa = BULKIO__POA.dataOctet
        port_name = 'dataOctet_in'
        print "\n**TESTING TYPE 2000 BLUEFILE + OCTET PORT"
        return self.blue2000PortTests(sid, port_poa, port_name)
    
    def octetnotsupported_testBlue1000OctetPortCx(self):
        #######################################################################
        # Test Bluefile Type 1000 Octet Cx Functionality
        sid = 'bluefileOctetCx'
        port_poa = BULKIO__POA.dataOctet
        port_name = 'dataOctet_in'
        print "\n**TESTING Cx TYPE 1000 BLUEFILE + OCTET PORT"
        return self.blue1000PortTests(sid, port_poa, port_name, True)
    
    def octetnotsupported_testBlue2000OctetPortCx(self):
        #######################################################################
        # Test Bluefile Type 2000 Octet Cx Functionality
        sid = 'bluefileOctetCx'
        port_poa = BULKIO__POA.dataOctet
        port_name = 'dataOctet_in'
        print "\n**TESTING Cx TYPE 2000 BLUEFILE + OCTET PORT"
        return self.blue2000PortTests(sid, port_poa, port_name, True)
    
    def testShortPort(self):
        #######################################################################
        # Test SHORT Functionality
        print "\n**TESTING SHORT PORT"
        
        #Define test files
        dataFileIn = './data.in'
        dataFileOut = './data.out'
        
        #Create Test Data File if it doesn't exist
        if not os.path.isfile(dataFileIn):
            with open(dataFileIn, 'wb') as dataIn:
                dataIn.write(os.urandom(1024))
        
        #Read in Data from Test File
        size = os.path.getsize(dataFileIn)
        with open (dataFileIn, 'rb') as dataIn:
            data = list(struct.unpack('h' * (size/2), dataIn.read(size)))

        #Create Components and Connections
        comp = sb.launch('../FileWriter.spd.xml')
        comp.destination_uri = dataFileOut
        comp.advanced_properties.existing_file = "TRUNCATE"
        
        source = sb.DataSource(bytesPerPush=64, dataFormat='16t')
        source.connect(comp,providesPortName='dataShort_in')
        
        #Start Components & Push Data
        sb.start()
        source.push(data)
        time.sleep(2)
        sb.stop()

        #Check that the input and output files are the same
        try:
            self.assertEqual(filecmp.cmp(dataFileIn, dataFileOut), True)
        except self.failureException as e:
            comp.releaseObject()
            source.releaseObject()
            os.remove(dataFileIn)
            os.remove(dataFileOut)
            raise e

        #Release the components and remove the generated files
        comp.releaseObject()
        source.releaseObject()
        os.remove(dataFileIn)
        os.remove(dataFileOut)
        
        print "........ PASSED\n"
        return
    
    def testBlue1000ShortPort(self):
        #######################################################################
        # Test Bluefile Type 1000 SHORT Functionality
        sid = 'bluefileShort'
        port_poa = BULKIO__POA.dataShort
        port_name = 'dataShort_in'
        print "\n**TESTING TYPE 1000 BLUEFILE + SHORT PORT"
        return self.blue1000PortTests(sid, port_poa, port_name)
    
    def testBlue2000ShortPort(self):
        #######################################################################
        # Test Bluefile Type 2000 SHORT Functionality
        sid = 'bluefileShort'
        port_poa = BULKIO__POA.dataShort
        port_name = 'dataShort_in'
        print "\n**TESTING TYPE 2000 BLUEFILE + SHORT PORT"
        return self.blue2000PortTests(sid, port_poa, port_name)
    
    def testBlue1000ShortPortCx(self):
        #######################################################################
        # Test Bluefile Type 1000 SHORT Cx Functionality
        sid = 'bluefileShortCx'
        port_poa = BULKIO__POA.dataShort
        port_name = 'dataShort_in'
        print "\n**TESTING Cx TYPE 1000 BLUEFILE + SHORT PORT"
        return self.blue1000PortTests(sid, port_poa, port_name, True)
    
    def testBlue2000ShortPortCx(self):
        #######################################################################
        # Test Bluefile Type 2000 SHORT Cx Functionality
        sid = 'bluefileShortCx'
        port_poa = BULKIO__POA.dataShort
        port_name = 'dataShort_in'
        print "\n**TESTING Cx TYPE 2000 BLUEFILE + SHORT PORT"
        return self.blue2000PortTests(sid, port_poa, port_name, True)
    
    def testUShortPort(self):
        #######################################################################
        # Test USHORT Functionality
        print "\n**TESTING USHORT PORT"
        
        #Define test files
        dataFileIn = './data.in'
        dataFileOut = './data.out'
        
        #Create Test Data File if it doesn't exist
        if not os.path.isfile(dataFileIn):
            with open(dataFileIn, 'wb') as dataIn:
                dataIn.write(os.urandom(1024))
        
        #Read in Data from Test File
        size = os.path.getsize(dataFileIn)
        with open (dataFileIn, 'rb') as dataIn:
            data = list(struct.unpack('H' * (size/2), dataIn.read(size)))

        #Create Components and Connections
        comp = sb.launch('../FileWriter.spd.xml')
        comp.destination_uri = dataFileOut
        comp.advanced_properties.existing_file = "TRUNCATE"
        
        source = sb.DataSource(bytesPerPush=64, dataFormat='16u')
        source.connect(comp,providesPortName='dataUshort_in')
        
        #Start Components & Push Data
        sb.start()
        source.push(data)
        time.sleep(2)
        sb.stop()

        #Check that the input and output files are the same
        try:
            self.assertEqual(filecmp.cmp(dataFileIn, dataFileOut), True)
        except self.failureException as e:
            comp.releaseObject()
            source.releaseObject()
            os.remove(dataFileIn)
            os.remove(dataFileOut)
            raise e

        #Release the components and remove the generated files
        comp.releaseObject()
        source.releaseObject()
        os.remove(dataFileIn)
        os.remove(dataFileOut)
        
        print "........ PASSED\n"
        return
    
    def testBlue1000UshortPort(self):
        #######################################################################
        # Test Bluefile Type 1000 USHORT Functionality
        sid = 'bluefileUshort'
        port_poa = BULKIO__POA.dataUshort
        port_name = 'dataUshort_in'
        print "\n**TESTING TYPE 1000 BLUEFILE + USHORT PORT"
        return self.blue1000PortTests(sid, port_poa, port_name)
    
    def testBlue2000UshortPort(self):
        #######################################################################
        # Test Bluefile Type 2000 USHORT Functionality
        sid = 'bluefileUshort'
        port_poa = BULKIO__POA.dataUshort
        port_name = 'dataUshort_in'
        print "\n**TESTING TYPE 2000 BLUEFILE + USHORT PORT"
        return self.blue2000PortTests(sid, port_poa, port_name)
    
    def testBlue1000UshortPortCx(self):
        #######################################################################
        # Test Bluefile Type 1000 USHORT Cx Functionality
        sid = 'bluefileUshortCx'
        port_poa = BULKIO__POA.dataUshort
        port_name = 'dataUshort_in'
        print "\n**TESTING Cx TYPE 1000 BLUEFILE + USHORT PORT"
        return self.blue1000PortTests(sid, port_poa, port_name, True)
    
    def testBlue2000UshortPortCx(self):
        #######################################################################
        # Test Bluefile Type 2000 USHORT Cx Functionality
        sid = 'bluefileUshortCx'
        port_poa = BULKIO__POA.dataUshort
        port_name = 'dataUshort_in'
        print "\n**TESTING Cx TYPE 2000 BLUEFILE + USHORT PORT"
        return self.blue2000PortTests(sid, port_poa, port_name, True)

    def testFloatPort(self):
        #######################################################################
        # Test FLOAT Functionality
        print "\n**TESTING FLOAT PORT"
        
        #Define test files
        dataFileIn = './data.in'
        dataFileOut = './data.out'
        
        #Create Test Data File if it doesn't exist
        if not os.path.isfile(dataFileIn):
            with open(dataFileIn, 'wb') as dataIn:
                dataIn.write(os.urandom(1024))
        
        #Read in Data from Test File
        size = os.path.getsize(dataFileIn)
        with open (dataFileIn, 'rb') as dataIn:
            data = list(struct.unpack('f' * (size/4), dataIn.read(size)))

        #Create Components and Connections
        comp = sb.launch('../FileWriter.spd.xml')
        comp.destination_uri = dataFileOut
        comp.advanced_properties.existing_file = "TRUNCATE"
        
        source = sb.DataSource(bytesPerPush=64, dataFormat='32f')
        source.connect(comp,providesPortName='dataFloat_in')
        
        #Start Components & Push Data
        sb.start()
        source.push(data)
        time.sleep(2)
        sb.stop()

        #Check that the input and output files are the same
        try:
            self.assertEqual(filecmp.cmp(dataFileIn, dataFileOut), True)
        except self.failureException as e:
            # unpacked bytes may be NaN, which could cause test to fail unnecessarily
            size = os.path.getsize(dataFileOut)
            with open (dataFileOut, 'rb') as dataOut:
                data2 = list(struct.unpack('f' * (size/4), dataOut.read(size)))
            for a,b in zip(data,data2):
                if a!=b:
                    if a!=a and b!=b:
                        print "Difference in NaN format, ignoring..."
                    else:
                        print "FAILED:",a,"!=",b
                        raise e

        #Release the components and remove the generated files
        finally:
            comp.releaseObject()
            source.releaseObject()
            os.remove(dataFileIn)
            os.remove(dataFileOut)
        
        print "........ PASSED\n"
        return
    
    def testBlue1000FloatPort(self):
        #######################################################################
        # Test Bluefile Type 1000 Float Functionality
        sid = 'bluefileFloat'
        port_poa = BULKIO__POA.dataFloat
        port_name = 'dataFloat_in'
        print "\n**TESTING TYPE 1000 BLUEFILE + FLOAT PORT"
        return self.blue1000PortTests(sid, port_poa, port_name)
    
    def testBlue2000FloatPort(self):
        #######################################################################
        # Test Bluefile Type 2000 Float Functionality
        sid = 'bluefileFloat'
        port_poa = BULKIO__POA.dataFloat
        port_name = 'dataFloat_in'
        print "\n**TESTING TYPE 2000 BLUEFILE + FLOAT PORT"
        return self.blue2000PortTests(sid, port_poa, port_name)
    
    def testBlue1000FloatPortCx(self):
        #######################################################################
        # Test Bluefile Type 1000 Float Cx Functionality
        sid = 'bluefileFloatCx'
        port_poa = BULKIO__POA.dataFloat
        port_name = 'dataFloat_in'
        print "\n**TESTING Cx TYPE 1000 BLUEFILE + FLOAT PORT"
        return self.blue1000PortTests(sid, port_poa, port_name, True)
    
    def testBlue2000FloatPortCx(self):
        #######################################################################
        # Test Bluefile Type 2000 Float Cx Functionality
        sid = 'bluefileFloatCx'
        port_poa = BULKIO__POA.dataFloat
        port_name = 'dataFloat_in'
        print "\n**TESTING Cx TYPE 2000 BLUEFILE + FLOAT PORT"
        return self.blue2000PortTests(sid, port_poa, port_name, True)

    def testDoublePort(self):
        #######################################################################
        # Test DOUBLE Functionality
        print "\n**TESTING DOUBLE PORT"
        
        #Define test files
        dataFileIn = './data.in'
        dataFileOut = './data.out'
        
        #Create Test Data File if it doesn't exist
        if not os.path.isfile(dataFileIn):
            with open(dataFileIn, 'wb') as dataIn:
                dataIn.write(os.urandom(1024))
        
        #Read in Data from Test File
        size = os.path.getsize(dataFileIn)
        with open (dataFileIn, 'rb') as dataIn:
            data = list(struct.unpack('d' * (size/8), dataIn.read(size)))

        #Create Components and Connections
        comp = sb.launch('../FileWriter.spd.xml')
        comp.destination_uri = dataFileOut
        comp.advanced_properties.existing_file = "TRUNCATE"
        
        source = sb.DataSource(bytesPerPush=64, dataFormat='64f')
        source.connect(comp,providesPortName='dataDouble_in')
        
        #Start Components & Push Data
        sb.start()
        source.push(data)
        time.sleep(2)
        sb.stop()

        #Check that the input and output files are the same
        try:
            self.assertEqual(filecmp.cmp(dataFileIn, dataFileOut), True)
        except self.failureException as e:
            comp.releaseObject()
            source.releaseObject()
            os.remove(dataFileIn)
            os.remove(dataFileOut)
            raise e

        #Release the components and remove the generated files
        comp.releaseObject()
        source.releaseObject()
        os.remove(dataFileIn)
        os.remove(dataFileOut)
        
        print "........ PASSED\n"
        return
    
    def testBlue1000DoublePort(self):
        #######################################################################
        # Test Bluefile Type 1000 DOUBLE Functionality
        sid = 'bluefileDouble'
        port_poa = BULKIO__POA.dataDouble
        port_name = 'dataDouble_in'
        print "\n**TESTING TYPE 1000 BLUEFILE + DOUBLE PORT"
        return self.blue1000PortTests(sid, port_poa, port_name)
    
    def testBlue2000DoublePort(self):
        #######################################################################
        # Test Bluefile Type 2000 DOUBLE Functionality
        sid = 'bluefileDouble'
        port_poa = BULKIO__POA.dataDouble
        port_name = 'dataDouble_in'
        print "\n**TESTING TYPE 2000 BLUEFILE + DOUBLE PORT"
        return self.blue2000PortTests(sid, port_poa, port_name)
    
    def testBlue1000DoublePortCx(self):
        #######################################################################
        # Test Bluefile Type 1000 DOUBLE Cx Functionality
        sid = 'bluefileDoubleCx'
        port_poa = BULKIO__POA.dataDouble
        port_name = 'dataDouble_in'
        print "\n**TESTING Cx TYPE 1000 BLUEFILE + DOUBLE PORT"
        return self.blue1000PortTests(sid, port_poa, port_name, True)
    
    def testBlue2000DoublePortCx(self):
        #######################################################################
        # Test Bluefile Type 2000 DOUBLE Cx Functionality
        sid = 'bluefileDoubleCx'
        port_poa = BULKIO__POA.dataDouble
        port_name = 'dataDouble_in'
        print "\n**TESTING Cx TYPE 2000 BLUEFILE + DOUBLE PORT"
        return self.blue2000PortTests(sid, port_poa, port_name, True)
        
    def testXmlPort(self):
        #######################################################################
        # Test XML Functionality
        print "\n**TESTING XML PORT"

        #Create Test Data
        dataFileOut = './data.out'

        with open ('data.xml', 'rb') as file:
                inputData=file.read()

        #Connect DataSource to FileWriter
        comp = sb.launch('../FileWriter.spd.xml')
        comp.destination_uri = dataFileOut
        comp.advanced_properties.existing_file = "TRUNCATE"
        
        source = sb.DataSource(bytesPerPush=64, dataFormat='xml')
        source.connect(comp, providesPortName='dataXML_in')

        #Start Components & Push Data
        sb.start()
        source.push(inputData)
        time.sleep(2)
        sb.stop()

        #Check that the input and output files are the same
        try:
            self.assertEqual(filecmp.cmp('./data.xml', dataFileOut), True)
        except self.failureException as e:
            comp.releaseObject()
            source.releaseObject()
            os.remove(dataFileOut)
            raise e
        
        #Release the components and remove the generated files
        comp.releaseObject()
        source.releaseObject()
        os.remove(dataFileOut)
        
        print "........ PASSED\n"
        return
    
    

    def testBaseUri(self):
        #######################################################################
        # Test base uri w/ keyword substitution
        print "\n**TESTING URI w/ KW Substitution"
        
        #Define test files
        dataFileIn = './data.in'
        STREAMID = 'baseuritest'
        COL_RF = 1.2e6
        CHAN_RF1 = 1.25e6
        CHAN_RF2 = 1.15e6
        COLRF_HZ = '1200000Hz'
        CF_HZ1 = CHANRF_HZ1 = '1250000Hz'
        CF_HZ2 = CHANRF_HZ2 = '1150000Hz'
        MY_KEYWORD = 'customkw'
        dataFileOut_template = './%s.%s.%s.%s.%s.out'
        dataFileOut1 = dataFileOut_template%(STREAMID,CF_HZ1,COLRF_HZ,CHANRF_HZ1,MY_KEYWORD)
        dataFileOut2 = dataFileOut_template%(STREAMID,CF_HZ2,COLRF_HZ,CHANRF_HZ2,MY_KEYWORD)
        
        keywords1 = [sb.io_helpers.SRIKeyword('COL_RF',COL_RF, 'double'),
                     sb.io_helpers.SRIKeyword('CHAN_RF',CHAN_RF1, 'double'),
                     sb.io_helpers.SRIKeyword('MY_KEYWORD',MY_KEYWORD, 'string')]
        
        keywords2 = [sb.io_helpers.SRIKeyword('COL_RF',COL_RF, 'double'),
                     sb.io_helpers.SRIKeyword('CHAN_RF',CHAN_RF2, 'double'),
                     sb.io_helpers.SRIKeyword('MY_KEYWORD',MY_KEYWORD, 'string')]

        
        #Create Test Data File if it doesn't exist
        if not os.path.isfile(dataFileIn):
            with open(dataFileIn, 'wb') as dataIn:
                dataIn.write(os.urandom(1024))
        
        #Read in Data from Test File
        size = os.path.getsize(dataFileIn)
        with open (dataFileIn, 'rb') as dataIn:
            data = list(struct.unpack('f' * (size/4), dataIn.read(size)))



        #Create Components and Connections
        comp = sb.launch('../FileWriter.spd.xml')
        comp.destination_uri = dataFileOut_template%('%STREAMID%','%CF_HZ%','%COLRF_HZ%','%CHANRF_HZ%','%MY_KEYWORD%')
        comp.advanced_properties.existing_file = "TRUNCATE"
        
        source = sb.DataSource(bytesPerPush=64, dataFormat='32f')
        source.connect(comp,providesPortName='dataFloat_in')
        
        #Start Components & Push Data
        sb.start()
        source.push(data, streamID=STREAMID, SRIKeywords=keywords1)
        time.sleep(2)
        source.push(data, streamID=STREAMID, SRIKeywords=keywords2)
        time.sleep(2)
        sb.stop()

        #Check that the input and output files are the same
        try:
            dataFileOut=dataFileOut1
            self.assertEqual(filecmp.cmp(dataFileIn, dataFileOut), True)
            dataFileOut=dataFileOut2
            self.assertEqual(filecmp.cmp(dataFileIn, dataFileOut), True)
        except self.failureException as e:
            # unpacked bytes may be NaN, which could cause test to fail unnecessarily
            size = os.path.getsize(dataFileOut)
            with open (dataFileOut, 'rb') as dataOut:
                data2 = list(struct.unpack('f' * (size/4), dataOut.read(size)))
            for a,b in zip(data,data2):
                if a!=b:
                    if a!=a and b!=b:
                        print "Difference in NaN format, ignoring..."
                    else:
                        print "FAILED:",a,"!=",b
                        raise e

        #Release the components and remove the generated files
        finally:
            comp.releaseObject()
            source.releaseObject()
            os.remove(dataFileIn)
            os.remove(dataFileOut1)
            os.remove(dataFileOut2)
        
        print "........ PASSED\n"
        return
    
    def testRecordingCpuTimers(self):
        #######################################################################
        # Test multiple recording timers using cpu clock
        print "\n**TESTING TIMERS w/ CPU TIMESTAMP"
        return self.timerTests(pkt_ts=False)
    
    def testRecordingPktTimers(self):
        #######################################################################
        # Test multiple recording timers using packet timestamp
        print "\n**TESTING TIMERS w/ PACKET TIMESTAMP"
        return self.timerTests(pkt_ts=True)
    
    def timerTests(self,pkt_ts=False):
        #######################################################################
        # Test multiple recording timers
        
        #Define test files
        dataFileIn = './data.in'
        dataFileOut = './data.out'
        resultsFileOut = './results.out'
        sample_rate = 128.0
        start_delay = 0.5
        stop_delay = 2.0
        
        #Create Test Data File if it doesn't exist
        if not os.path.isfile(dataFileIn):
            with open(dataFileIn, 'wb') as dataIn:
                dataIn.write(os.urandom(1024))
        
        #Read in Data from Test File
        size = os.path.getsize(dataFileIn)
        with open (dataFileIn, 'rb') as dataIn:
            data = list(struct.unpack('f' * (size/4), dataIn.read(size)))
        
        #Create Components and Connections
        comp = sb.launch('../FileWriter.spd.xml')
        comp.destination_uri = dataFileOut
        comp.recording_enabled = False
        
        # Create timers
        ts_start = bulkio.bulkio_helpers.createCPUTimestamp()
        start1_wsec = ts_start.twsec+2.0
        stop1_wsec = start1_wsec+2.0
        start2_wsec = stop1_wsec+2.0
        stop2_wsec = start2_wsec+2.0
        timers = [{'recording_enable':True,'use_pkt_timestamp':pkt_ts,'twsec':start1_wsec,'tfsec':ts_start.tfsec},
                  {'recording_enable':False,'use_pkt_timestamp':pkt_ts,'twsec':stop1_wsec,'tfsec':ts_start.tfsec},
                  {'recording_enable':True,'use_pkt_timestamp':pkt_ts,'twsec':start2_wsec,'tfsec':ts_start.tfsec},
                  {'recording_enable':False,'use_pkt_timestamp':pkt_ts,'twsec':stop2_wsec,'tfsec':ts_start.tfsec}]
        #print timers
        comp.recording_timer = timers
        
        source = sb.DataSource(bytesPerPush=64.0, dataFormat='32f', startTime=ts_start.twsec+ts_start.tfsec+start_delay)
        source.connect(comp,providesPortName='dataFloat_in')
        
        # results will be shifted due to start_delay
        results_offset = int((start1_wsec-ts_start.twsec-start_delay)*sample_rate)
        
        #Start Components & Push Data
        sb.start()
        if pkt_ts:
            source.push(data*5,sampleRate=sample_rate) # 5*256 samples per push
            time.sleep(2)
        else:
            # meter to actual sample rate since based on cpu time
            end_ws = stop2_wsec + stop_delay
            num_samps = 16
            loop_delay = num_samps/sample_rate
            idx = results_offset # necessary to achieve same results as pkt_ts, accounting for start_delay
            ts_now = bulkio.bulkio_helpers.createCPUTimestamp()
            while ts_now.twsec < end_ws:
                source.push(data[idx:idx+num_samps],sampleRate=sample_rate) # 256 samples per push
                idx=(idx+num_samps)%len(data)
                time.sleep(loop_delay)
                ts_now = bulkio.bulkio_helpers.createCPUTimestamp()
        sb.stop()
        
        #Create Test Results Files
        results = data[results_offset:]+data[:results_offset] 
        with open(resultsFileOut, 'wb') as dataIn:
            dataIn.write(struct.pack('f'*len(results), *results))

        #Check that the input and output files are the same
        try:
            try:
                self.assertEqual(filecmp.cmp(resultsFileOut, dataFileOut), True)
            except self.failureException as e:
                # unpacked bytes may be NaN, which could cause test to fail unnecessarily
                size1 = os.path.getsize(dataFileOut)
                with open (dataFileOut, 'rb') as dataOut1:
                    data1 = list(struct.unpack('f' * (size1/4), dataOut1.read(size1)))
                
                offset1 = results.index(max(results))-data1.index(max(data1))
                #print 'offset1 is', offset1
                if offset1 != 0:
                    if abs(offset1) > num_samps: # allow it to be off by one data push
                        print "FAILED: offset1 =",offset1
                        raise e
                    shifted_res1 = results[offset1:]+results[:offset1]
                else:
                    shifted_res1 = results
                for a,b in zip(shifted_res1,data1):
                    if a!=b:
                        if a!=a and b!=b:
                            print "Difference in NaN format, ignoring..."
                        else:
                            print "1st FAILED:",a,"!=",b
                            raise e
            try:
                self.assertEqual(filecmp.cmp(resultsFileOut, dataFileOut+'-1'), True)
            except self.failureException as e:
                # unpacked bytes may be NaN, which could cause test to fail unnecessarily
                size2 = os.path.getsize(dataFileOut+'-1')
                with open (dataFileOut+'-1', 'rb') as dataOut:
                    data2 = list(struct.unpack('f' * (size2/4), dataOut.read(size2)))
            
                offset2 = results.index(max(results))-data2.index(max(data2))
                #print 'offset2 is', offset2
                if offset2 != 0:
                    if abs(offset2) > num_samps: # allow it to be off by one data push
                        print "FAILED: offset2 =",offset2
                        raise e
                    shifted_res2 = results[offset2:]+results[:offset2]
                else:
                    shifted_res2 = results
                for a,b in zip(shifted_res2,data2):
                    if a!=b:
                        if a!=a and b!=b:
                            print "Difference in NaN format, ignoring..."
                        else:
                            print "2nd FAILED:",a,"!=",b
                            raise e
        except:
            raise e
        #Release the components and remove the generated files
        finally:
            comp.releaseObject()
            source.releaseObject()
            os.remove(dataFileIn)
            os.remove(dataFileOut)
            os.remove(dataFileOut+'-1')
            os.remove(resultsFileOut)
        
        #TODO - validate timestamps, perhaps using BLUEFILEs
        
        print "........ PASSED\n"
        return
    
    def blue1000PortTests(self, sid, port_poa, port_name, cxmode=False):
        #######################################################################
        # Test Bluefile Type 1000 Functionality
        
        #Define test files
        dataFileIn = './bluefile.in'
        dataFileOut = './bluefile.out'
        
        #Create Test Data File, remove existing file if necessary
        if os.path.isfile(dataFileIn):
            os.remove(dataFileIn)
        #if not os.path.isfile(dataFileIn):
        if 1:
            tmpSink = bluefile_helpers.BlueFileWriter(dataFileIn, port_poa)
            tmpSink.start()
            srate = 5e3 # 5 kHz
            kws = props_from_dict({'TEST_KW':1234})
            tmpSri = BULKIO.StreamSRI(hversion=1,
                                      xstart=0.0,
                                      xdelta= 1.0/srate, 
                                      xunits=1,
                                      subsize=0,
                                      ystart=0.0,
                                      ydelta=0.0,
                                      yunits=0,
                                      mode=cxmode,
                                      streamID=sid,
                                      blocking=False,
                                      keywords=kws)
            tmpSink.pushSRI(tmpSri)
            tmpTs = createTs()
            tmpData = range(0, 1024*(cxmode+1)) # double number of samples to account for complex pairs
            tmpSink.pushPacket(tmpData, tmpTs, True, sid)
            
        #Read in Data from Test File
        #hdr, d = bluefile.read(dataFileIn, dict)
        #data = list(numpy.reshape(d,(-1,))) # flatten data
        #sri = hdr_to_sri(hdr, sid)

        #Create Components and Connections
        comp = sb.launch('../FileWriter.spd.xml')
        comp.destination_uri = dataFileOut
        comp.file_format = 'BLUEFILE'
        comp.advanced_properties.existing_file = 'TRUNCATE'
        comp.advanced_properties.use_tc_prec = False
        
        #Create BlueFileReader
        source = bluefile_helpers.BlueFileReader(port_poa)
        source.connectPort(comp.getPort(port_name), 'conn_id1'+sid)
        
        #Start Components & Push Data
        sb.start()
        source.run(dataFileIn, streamID=sid, pktsize=4096) # but in BlueFileReader if sending more than one "packet", so size it large
        time.sleep(2)
        sb.stop()

        #Check that the input and output files are the same
        try:
            self.assertEqual(filecmp.cmp(dataFileIn, dataFileOut), True, msg='BLUE file output is not identical to BLUE file input.')
        except self.failureException as e:
            comp.releaseObject()
            #source.releaseObject() - this has no releaseObject function
            
            # DEBUG
            if 1:
                from pprint import pprint as pp
                #Read in Data from Test Files
                hdr1, d1 = bluefile.read(dataFileIn, dict)
                hdr2, d2 = bluefile.read(dataFileOut, dict)
                
                print_hdrs = False
                if hdr1.keys() != hdr2.keys():
                    print_hdrs = True
                else:
                    for key in hdr1.keys():
                        if hdr1[key] != hdr2[key]:
                            print "HCB['%s'] in: %s  out: %s"%(key,hdr1[key],hdr2[key])
                            if key != 'file_name':
                                print_hdrs = True
                if print_hdrs:
                    print 'DEBUG - input header:'
                    pp(hdr1)
                    print 'DEBUG - output header:'
                    pp(hdr2)
                
                print 'DEBUG - len(input_data)=%s'%(len(d1))
                print 'DEBUG - len(output_data)=%s'%(len(d2))
                data1 = list(numpy.reshape(d1,(-1,))) # flatten data
                data2 = list(numpy.reshape(d2,(-1,))) # flatten data
                print 'DEBUG - len(input_data)=%s'%(len(data1))
                print 'DEBUG - len(output_data)=%s'%(len(data2))
                
                raise e
            
            try: os.remove(dataFileIn)
            except: pass
            try: os.remove(dataFileOut)
            except: pass
            raise e

        #Release the components and remove the generated files
        comp.releaseObject()
        #source.releaseObject() - this has no releaseObject function
        try: os.remove(dataFileIn)
        except: pass
        try: os.remove(dataFileOut)
        except: pass
        
        print "........ PASSED\n"
        return
    
    def blue2000PortTests(self, sid, port_poa, port_name, cxmode=False):
        #######################################################################
        # Test Bluefile Type 2000 Functionality
        
        #Define test files
        dataFileIn = './bluefile.in'
        dataFileOut = './bluefile.out'
        
        #Create Test Data File, remove existing file if necessary
        if os.path.isfile(dataFileIn):
            os.remove(dataFileIn)
        #if not os.path.isfile(dataFileIn):
        if 1:
            tmpSink = bluefile_helpers.BlueFileWriter(dataFileIn, port_poa)
            tmpSink.start()
            srate = 5e3 # 5 kHz
            framesize = 64 # 64 complex sample frames
            frames = 16 # 16 frames (total 1024 complex samples, 2048 total short samples)
            kws = props_from_dict({'TEST_KW':1234})
            tmpSri = BULKIO.StreamSRI(hversion=1,
                                      xstart=-1.0*srate/2.0,
                                      xdelta= srate/framesize, 
                                      xunits=3,
                                      subsize=framesize,
                                      ystart=0.0,
                                      ydelta=framesize/srate,
                                      yunits=1,
                                      mode=cxmode,
                                      streamID=sid,
                                      blocking=False,
                                      keywords=kws)
            tmpSink.pushSRI(tmpSri)
            tmpTs = createTs()
            tmpData = []
            for i in xrange(frames):
                #tmpData.append(range(i,i+framesize*(cxmode+1))) # this would create framed data, but we need flat data
                tmpData.extend(range(i,i+framesize*(cxmode+1))) # double number of samples to account for complex pairs
            tmpSink.pushPacket(tmpData, tmpTs, True, sid)
            
        #Read in Data from Test File
        #hdr, d = bluefile.read(dataFileIn, dict)
        #data = list(numpy.reshape(d,(-1,))) # flatten data
        #sri = hdr_to_sri(hdr, sid)

        #Create Components and Connections
        comp = sb.launch('../FileWriter.spd.xml')
        comp.destination_uri = dataFileOut
        comp.file_format = 'BLUEFILE'
        comp.advanced_properties.existing_file = 'TRUNCATE'
        comp.advanced_properties.use_tc_prec = False
        
        #Create BlueFileReader
        source = bluefile_helpers.BlueFileReader(port_poa)
        source.connectPort(comp.getPort(port_name), 'conn_id2'+sid)
        
        #Start Components & Push Data
        sb.start()
        source.run(dataFileIn, streamID=sid, pktsize=4096) # but in BlueFileReader if sending more than one "packet", so size it large
        time.sleep(2)
        sb.stop()

        #Check that the input and output files are the same
        try:
            self.assertEqual(filecmp.cmp(dataFileIn, dataFileOut), True, msg='BLUE file output is not identical to BLUE file input.')
        except self.failureException as e:
            comp.releaseObject()
            #source.releaseObject() - this has no releaseObject function
            
            # DEBUG
            if 0:
                from pprint import pprint as pp
                #Read in Data from Test Files
                hdr1, d1 = bluefile.read(dataFileIn, dict)
                hdr2, d2 = bluefile.read(dataFileOut, dict)
                
                print_hdrs = False
                if hdr1.keys() != hdr2.keys():
                    print_hdrs = True
                else:
                    for key in hdr1.keys():
                        if hdr1[key] != hdr2[key]:
                            print "HCB['%s'] in: %s  out: %s"%(key,hdr1[key],hdr2[key])
                            if key != 'file_name':
                                print_hdrs = True
                if print_hdrs:
                    print 'DEBUG - input header:'
                    pp(hdr1)
                    print 'DEBUG - output header:'
                    pp(hdr2)
                
                print 'DEBUG - len(input_data)=%s - len(input_data[0])=%s'%(len(d1),len(d1[0]))
                print 'DEBUG - len(output_data)=%s - len(output_data[0])=%s'%(len(d2),len(d2[0]))
                data1 = list(numpy.reshape(d1,(-1,))) # flatten data
                data2 = list(numpy.reshape(d2,(-1,))) # flatten data
                print 'DEBUG - len(input_data)=%s'%(len(data1))
                print 'DEBUG - len(output_data)=%s'%(len(data2))
                
                raise e
            
            try: os.remove(dataFileIn)
            except: pass
            try: os.remove(dataFileOut)
            except: pass
            raise e

        #Release the components and remove the generated files
        comp.releaseObject()
        #source.releaseObject() - this has no releaseObject function
        try: os.remove(dataFileIn)
        except: pass
        try: os.remove(dataFileOut)
        except: pass
        
        print "........ PASSED\n"
        return
    
    def testBlueTimestampPrecision(self):
        #######################################################################
        # Test BLUE file high precision timecode using TC_PREC keyword
        print "\n**TESTING BLUE file with TC_PREC"
        
        #Define test files
        dataFileIn = './data.in'
        dataFileOut = './data.out'
        
        #Create Test Data File if it doesn't exist
        if not os.path.isfile(dataFileIn):
            with open(dataFileIn, 'wb') as dataIn:
                dataIn.write(os.urandom(1024))
        
        #Read in Data from Test File
        size = os.path.getsize(dataFileIn)
        with open (dataFileIn, 'rb') as dataIn:
            data = list(struct.unpack('b'*size, dataIn.read(size)))
        
        #Create Components and Connections
        comp = sb.launch('../FileWriter.spd.xml')
        comp.destination_uri = dataFileOut
        comp.file_format = 'BLUEFILE'
        comp.advanced_properties.existing_file = 'TRUNCATE'
        
        # Create timestamp
        ts_start = bulkio.bulkio_helpers.createCPUTimestamp()
        print 'Using timestamp', repr(ts_start)
        
        source = sb.DataSource(bytesPerPush=64, dataFormat='8t', startTime=ts_start.twsec+ts_start.tfsec)
        source.connect(comp,providesPortName='dataChar_in')
        
        #Start Components & Push Data
        sb.start()
        source.push(data)
        time.sleep(2)
        sb.stop()
        
        # Calculate various timestamp values  
        ts_ws = ts_start.twsec
        ts_fs_u = int(ts_start.tfsec*1.0e6)*1.0e-6
        ts_fs_p = int( (ts_start.tfsec-ts_fs_u)*1.0e12 ) * 1.0e-12
        
        try:
            hdr, data = bluefile.read(dataFileOut, list)
            #print hdr
            self.assertAlmostEqual(hdr['timecode']-631152000, ts_ws+ts_fs_u, places=5, msg='BLUE file timecode does not match expected.')
            self.assertTrue(hdr['keylength'] > 0, msg='No keywords in BLUE file header.')
            self.assertTrue('TC_PREC' in hdr['keywords'], msg='TC_PREC keyword not present in BLUE file header.')
            self.assertAlmostEqual(float(hdr['keywords']['TC_PREC']), ts_fs_p, msg='BLUE file keyword TC_PREC does not match expected.')
        except self.failureException as e:
            comp.releaseObject()
            source.releaseObject()
            os.remove(dataFileIn)
            os.remove(dataFileOut)
            raise e
        
        #Release the components and remove the generated files
        comp.releaseObject()
        source.releaseObject()
        os.remove(dataFileIn)
        os.remove(dataFileOut)
        
        print "........ PASSED\n"
        return
    
    def testBlueTimestampNoPrecision(self):
        #######################################################################
        # Test BLUE file without high precision timecode
        print "\n**TESTING BLUE file w/o TC_PREC"
        
        #Define test files
        dataFileIn = './data.in'
        dataFileOut = './data.out'
        
        #Create Test Data File if it doesn't exist
        if not os.path.isfile(dataFileIn):
            with open(dataFileIn, 'wb') as dataIn:
                dataIn.write(os.urandom(1024))
        
        #Read in Data from Test File
        size = os.path.getsize(dataFileIn)
        with open (dataFileIn, 'rb') as dataIn:
            data = list(struct.unpack('b'*size, dataIn.read(size)))
        
        #Create Components and Connections
        comp = sb.launch('../FileWriter.spd.xml')
        comp.destination_uri = dataFileOut
        comp.file_format = 'BLUEFILE'
        comp.advanced_properties.existing_file = 'TRUNCATE'
        comp.advanced_properties.use_tc_prec = False
        
        # Create timestamp
        ts_start = bulkio.bulkio_helpers.createCPUTimestamp()
        print 'Using timestamp', repr(ts_start)
        
        source = sb.DataSource(bytesPerPush=64, dataFormat='8t', startTime=ts_start.twsec+ts_start.tfsec)
        source.connect(comp,providesPortName='dataChar_in')
        
        #Start Components & Push Data
        sb.start()
        source.push(data)
        time.sleep(2)
        sb.stop()
        
        try:
            hdr, data = bluefile.read(dataFileOut, list)
            #print hdr
            self.assertFalse('TC_PREC' in hdr['keywords'], msg='TC_PREC keyword present in BLUE file header.')
            #print repr(hdr['timecode']-631152000)            
            self.assertAlmostEqual(hdr['timecode']-631152000, ts_start.twsec+ts_start.tfsec,places=5, msg='BLUE file timecode does not match expected.')
        except self.failureException as e:
            comp.releaseObject()
            source.releaseObject()
            os.remove(dataFileIn)
            os.remove(dataFileOut)
            raise e
        
        #Release the components and remove the generated files
        comp.releaseObject()
        source.releaseObject()
        os.remove(dataFileIn)
        os.remove(dataFileOut)
        
        print "........ PASSED\n"
        return

    def testMixedCaseRawFile(self):
        #######################################################################
        # Test Mixed case RAW file output
        print "\n**TESTING Mixed Case RAW file "
        self.filenameCaseTests(0,'RAW')

    def testLowerCaseRawFile(self):
        #######################################################################
        # Test Lower case RAW file output
        print "\n**TESTING Lower Case RAW file "
        self.filenameCaseTests(1,'RAW')

    def testUpperCaseRawFile(self):
        #######################################################################
        # Test Upper case RAW file output
        print "\n**TESTING Upper Case RAW file "
        self.filenameCaseTests(2,'RAW')

    def testMixedCaseBlueFile(self):
        #######################################################################
        # Test Mixed case BLUE file output
        print "\n**TESTING Mixed Case BLUE file "
        self.filenameCaseTests(0,'BLUEFILE')

    def testLowerCaseBlueFile(self):
        #######################################################################
        # Test Lower case BLUE file output
        print "\n**TESTING Lower Case BLUE file "
        self.filenameCaseTests(1,'BLUEFILE')

    def testUpperCaseBlueFile(self):
        #######################################################################
        # Test Upper case BLUE file output
        print "\n**TESTING Upper Case BLUE file "
        self.filenameCaseTests(2,'BLUEFILE')

    def filenameCaseTests(self, case=0, fformat='RAW'):
        #######################################################################
        # Test output filename case
        print "\n**TESTING Case %s of %s file "%(case,fformat)
        
        #Define test files
        dataFileIn = './data.in'
        STREAMID = 'Case%s%stest'%(case,fformat)
        COL_RF = 1.2e6
        CHAN_RF = 1.25e6
        COLRF_HZ = '1200000Hz'
        CF_HZ = CHANRF_HZ = '1250000Hz'
        MY_KEYWORD = 'CustomKW'
        dataFileOut_template = './%s.%s.%s.%s.%s.out'
        if case == 1:
            dataFileOut = (dataFileOut_template%(STREAMID,CF_HZ,COLRF_HZ,CHANRF_HZ,MY_KEYWORD)).lower()
        elif case == 2:
            dataFileOut = (dataFileOut_template%(STREAMID,CF_HZ,COLRF_HZ,CHANRF_HZ,MY_KEYWORD)).upper()
        else: #if case == 0:
            dataFileOut = dataFileOut_template%(STREAMID,CF_HZ,COLRF_HZ,CHANRF_HZ,MY_KEYWORD)
        
        keywords = [sb.io_helpers.SRIKeyword('COL_RF',COL_RF, 'double'),
                    sb.io_helpers.SRIKeyword('CHAN_RF',CHAN_RF, 'double'),
                    sb.io_helpers.SRIKeyword('MY_KEYWORD',MY_KEYWORD, 'string')]

        
        #Create Test Data File if it doesn't exist
        if not os.path.isfile(dataFileIn):
            with open(dataFileIn, 'wb') as dataIn:
                dataIn.write(os.urandom(1024))
        
        #Read in Data from Test File
        size = os.path.getsize(dataFileIn)
        with open (dataFileIn, 'rb') as dataIn:
            data = list(struct.unpack('f' * (size/4), dataIn.read(size)))



        #Create Components and Connections
        comp = sb.launch('../FileWriter.spd.xml')
        comp.destination_uri = dataFileOut_template%('%STREAMID%','%CF_HZ%','%COLRF_HZ%','%CHANRF_HZ%','%MY_KEYWORD%')
        comp.file_format = fformat
        comp.advanced_properties.existing_file = "TRUNCATE"
        comp.advanced_properties.output_filename_case = case
        
        source = sb.DataSource(bytesPerPush=64, dataFormat='32f')
        source.connect(comp,providesPortName='dataFloat_in')
        
        #Start Components & Push Data
        sb.start()
        source.push(data, streamID=STREAMID, SRIKeywords=keywords)
        time.sleep(2)
        sb.stop()

        #Check that the input and output files are the same
        self.assertEqual(os.path.exists(dataFileOut), True, msg='Output file does not exist on filesystem')
        if fformat == 'BLUEFILE':
            comp.releaseObject()
            source.releaseObject()
            os.remove(dataFileIn)
            os.remove(dataFileOut)
        else:
            try:
                self.assertEqual(filecmp.cmp(dataFileIn, dataFileOut), True, msg='Output file exists, but contents of file are incorrect')
            except self.failureException as e:
                # unpacked bytes may be NaN, which could cause test to fail unnecessarily
                size = os.path.getsize(dataFileOut)
                with open (dataFileOut, 'rb') as dataOut:
                    data2 = list(struct.unpack('f' * (size/4), dataOut.read(size)))
                for a,b in zip(data,data2):
                    if a!=b:
                        if a!=a and b!=b:
                            print "Difference in NaN format, ignoring..."
                        else:
                            print "FAILED:",a,"!=",b
                            raise e

            #Release the components and remove the generated files
            finally:
                comp.releaseObject()
                source.releaseObject()
                os.remove(dataFileIn)
                os.remove(dataFileOut)

        
        print "........ PASSED\n"
        return
    def testBlueFileKeywords(self):
        
        dataFileOut = './testdata.out'
        
        # Setup FileWriter
        comp = sb.launch('../FileWriter.spd.xml')
        comp.destination_uri = dataFileOut
        
        comp.file_format = 'BLUEFILE'
        comp.advanced_properties.use_hidden_files = False
        port = comp.getPort('dataShort_in')
        comp.start()
        
        # Create an SRI with 2 keywords (a,b)
        kws = props_from_dict({'TEST_KW1':1111,'TEST_KW2':'2222'})
        srate = 10.0e6
        sri1 = BULKIO.StreamSRI(hversion=1,
                                  xstart=0,
                                  xdelta= 1.0/srate, 
                                  xunits=1,
                                  subsize=0,
                                  ystart=0.0,
                                  ydelta=0,
                                  yunits=1,
                                  mode=0,
                                  streamID="test_streamID",
                                  blocking=False,
                                  keywords=kws)
        data = range(1000)
        
        # Push SRI
        port.pushSRI(sri1)
        #Push packet of data 
        port.pushPacket(data, bio.timestamp.now(), False, "test_streamID")   
        port.pushPacket(data, bio.timestamp.now(), False, "test_streamID")
        port.pushPacket(data, bio.timestamp.now(), False, "test_streamID")       

 
         
        # Create an SRI with 2 keywords (1 same as above with new value (a'), 1 new keyword(c))
        kws = props_from_dict({'TEST_KW1':0,'TEST_KW3':'3333'})
         
        sri2 = BULKIO.StreamSRI(hversion=1,
                                  xstart=0,
                                  xdelta= 1/srate, 
                                  xunits=1,
                                  subsize=0,
                                  ystart=0.0,
                                  ydelta=0,
                                  yunits=1,
                                  mode=0,
                                  streamID="test_streamID",
                                  blocking=False,
                                  keywords=kws)
        # Push SRI
        port.pushSRI(sri2)
         
        #Push packet of data 
        port.pushPacket(data, bio.timestamp.now(), False, "test_streamID")   
        port.pushPacket(data, bio.timestamp.now(), False, "test_streamID")
        #Push packet of data with EOS
        port.pushPacket(data, bio.timestamp.now(), True, "test_streamID")   
        
        time.sleep(1)
        
        #Open up bluefile
        self.assertEqual(os.path.exists(dataFileOut), True, msg='Output file does not exist on filesystem')
        
        header, data = bluefile.read(dataFileOut,ext_header_type=dict)
        #Check for three keywords verify a', b, c, 
        
        keywords = header['ext_header'].copy()
        keywords.update(header['keywords'])
         
        self.assertTrue('TEST_KW1' in keywords,msg="Keyword 1 missing")
        self.assertEqual(keywords['TEST_KW1'],0,msg="Keyword 1 has wrong value")
 
        self.assertTrue('TEST_KW2' in keywords,msg="Keyword 2 missing")
        self.assertEqual(keywords['TEST_KW2'],'2222',msg="Keyword 2 has wrong value")
         
        self.assertTrue('TEST_KW3' in keywords,msg="Keyword 3 missing")
        self.assertEqual(keywords['TEST_KW3'],'3333',msg="Keyword 3 has wrong value")        
                
        dataFileOut2 = './testdata2.out'
        comp.destination_uri = dataFileOut2
        
        # Create an SRI with 2 keywords (a,b)
        kws = props_from_dict({'TEST_KW6':'6666','TEST_KW7':'7777'})
        srate = 10.0e6
        sri1 = BULKIO.StreamSRI(hversion=1,
                                  xstart=0,
                                  xdelta= 1.0/srate, 
                                  xunits=1,
                                  subsize=0,
                                  ystart=0.0,
                                  ydelta=0,
                                  yunits=1,
                                  mode=0,
                                  streamID="test_streamID2",
                                  blocking=False,
                                  keywords=kws)
        data = range(1000)
        
        # Push SRI
        port.pushSRI(sri1)
        #Push packet of data 
        port.pushPacket(data, bio.timestamp.now(), False, "test_streamID2")   
        port.pushPacket(data, bio.timestamp.now(), False, "test_streamID2")
        port.pushPacket(data, bio.timestamp.now(), True, "test_streamID2")  
        
        time.sleep(1)
        
                #Open up bluefile
        self.assertEqual(os.path.exists(dataFileOut2), True, msg='Output file 2 does not exist on filesystem')
        
        header, data = bluefile.read(dataFileOut2,ext_header_type=dict)
        #Check for three keywords verify a', b, c, 
        
        keywords = header['ext_header'].copy()
        keywords.update(header['keywords'])
         
        self.assertTrue('TEST_KW6' in keywords,msg="Keyword 1 missing")
        self.assertEqual(keywords['TEST_KW6'],'6666',msg="Keyword 6 has wrong value")
 
        self.assertTrue('TEST_KW7' in keywords,msg="Keyword 2 missing")
        self.assertEqual(keywords['TEST_KW7'],'7777',msg="Keyword 7 has wrong value")
         
        self.assertFalse('TEST_KW3' in keywords,msg="Keyword 3 still present")
        self.assertFalse('TEST_KW2' in keywords,msg="Keyword 2 still present")
        self.assertFalse('TEST_KW1' in keywords,msg="Keyword 1 still present")
       
        os.remove(dataFileOut2)
        os.remove(dataFileOut)
        
    def testBlueFileKeywordsMultipleFiles(self):
        
        dataFileOut = './testdata.out'
        seconddataFileOut = dataFileOut+'-1'
        
        # Setup FileWriter
        comp = sb.launch('../FileWriter.spd.xml')
        comp.destination_uri = dataFileOut
        
        comp.file_format = 'BLUEFILE'
        #comp.advanced_properties.enable_metadata_file=True
        
        #comp.advanced_properties.existing_file = 'TRUNCATE'
        comp.advanced_properties.use_hidden_files = False
        
        # With a max file size of 6000, the first data file written will be done before the second SRI comes in.
        comp.advanced_properties.max_file_size = "6000"
        port = comp.getPort('dataShort_in')
        comp.start()
        
        # Create an SRI with 2 keywords (1,2)
        kws = props_from_dict({'TEST_KW1':1111,'TEST_KW2':'2222'})
        srate = 10.0e6
        sri1 = BULKIO.StreamSRI(hversion=1,
                                  xstart=0,
                                  xdelta= 1.0/srate, 
                                  xunits=1,
                                  subsize=0,
                                  ystart=0.0,
                                  ydelta=0,
                                  yunits=1,
                                  mode=0,
                                  streamID="test_streamID",
                                  blocking=False,
                                  keywords=kws)
        data = range(1000)
        
        # Push SRI
        port.pushSRI(sri1)
        #Push packet of data 
        port.pushPacket(data, bio.timestamp.now(), False, "test_streamID")   
        port.pushPacket(data, bio.timestamp.now(), False, "test_streamID")
        port.pushPacket(data, bio.timestamp.now(), False, "test_streamID")       

 
         
        # Create an SRI with 2 keywords (1 same as above with new value (1'), 1 new keyword(3))
        kws = props_from_dict({'TEST_KW1':0,'TEST_KW3':'3333'})
         
        sri2 = BULKIO.StreamSRI(hversion=1,
                                  xstart=0,
                                  xdelta= 1/srate, 
                                  xunits=1,
                                  subsize=0,
                                  ystart=0.0,
                                  ydelta=0,
                                  yunits=1,
                                  mode=0,
                                  streamID="test_streamID",
                                  blocking=False,
                                  keywords=kws)
        # Push SRI
        port.pushSRI(sri2)
         
        #Push packet of data 
        port.pushPacket(data, bio.timestamp.now(), False, "test_streamID")   
        port.pushPacket(data, bio.timestamp.now(), False, "test_streamID")
        #Push packet of data with EOS
        port.pushPacket(data, bio.timestamp.now(), True, "test_streamID")   
        
        time.sleep(1)
        
        #Open up bluefile
        self.assertEqual(os.path.exists(dataFileOut), True, msg='Output file does not exist on filesystem')
        
        header, data = bluefile.read(dataFileOut,ext_header_type=dict)
        #Check for three keywords verify a', b, c, 
        
        keywords = header['ext_header'].copy()
        keywords.update(header['keywords'])
        
        # With a max file size of 6000, the first data file written will be done before the second SRI comes in.
        self.assertTrue('TEST_KW1' in keywords,msg="Keyword 1 missing")
        self.assertEqual(keywords['TEST_KW1'],1111,msg="Keyword 1 has wrong value")
 
        self.assertTrue('TEST_KW2' in keywords,msg="Keyword 2 missing")
        self.assertEqual(keywords['TEST_KW2'],'2222',msg="Keyword 2 has wrong value")
     
        self.assertEqual(os.path.exists(dataFileOut), True, msg='Output file does not exist on filesystem')
        
        header, data = bluefile.read(seconddataFileOut,ext_header_type=dict)

        
        keywords2 = header['ext_header'].copy()
        keywords2.update(header['keywords'])

        # With a max file size of 6000, the second data file written will have the updated keywords in it.
        self.assertTrue('TEST_KW1' in keywords2,msg="Keyword 1 missing")
        self.assertEqual(keywords2['TEST_KW1'],0,msg="Keyword 1 has wrong value")
 
        self.assertTrue('TEST_KW2' in keywords2,msg="Keyword 2 missing")
        self.assertEqual(keywords2['TEST_KW2'],'2222',msg="Keyword 2 has wrong value")
         
        self.assertTrue('TEST_KW3' in keywords2,msg="Keyword 3 missing")
        self.assertEqual(keywords2['TEST_KW3'],'3333',msg="Keyword 3 has wrong value")   

        os.remove(seconddataFileOut)
        os.remove(dataFileOut)
        
        
if __name__ == "__main__":
    ossie.utils.testing.main("../FileWriter.spd.xml") # By default tests all implementations
