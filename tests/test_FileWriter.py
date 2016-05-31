#!/usr/bin/env python
#
# This file is protected by Copyright. Please refer to the COPYRIGHT file distributed with this 
# source distribution.
# 
# This file is part of REDHAWK Basic Components FileWriter.
# 
# REDHAWK Basic Components FileWriter is free software: you can redistribute it and/or modify it under the terms of 
# the GNU General Public License as published by the Free Software Foundation, either 
# version 3 of the License, or (at your option) any later version.
# 
# REDHAWK Basic Components FileWriter is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
# PURPOSE.  See the GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License along with this 
# program.  If not, see http://www.gnu.org/licenses/.
#

import ossie.utils.testing
import os
import time
from omniORB import any
from ossie.utils import sb, bulkio
import filecmp
import struct

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
        return self.timerTests(pkt_ts=False)
    
# TODO - sb.DataSource doesn't produce accurate timestamps, can't test using it    
#    def testRecordingPktTimers(self):
#        #######################################################################
#        # Test multiple recording timers using packet timestamp
#        return self.timerTests(pkt_ts=True)
    
    
    def timerTests(self,pkt_ts=False):
        #######################################################################
        # Test multiple recording timers
        
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
        comp.recording_enabled = False
        
        source = sb.DataSource(bytesPerPush=64, dataFormat='32f')
        source.connect(comp,providesPortName='dataFloat_in')
        
        # Create timers
        ts_now = bulkio.bulkio_helpers.createCPUTimestamp()
        start1_wsec = ts_now.twsec+2.0
        stop1_wsec = start1_wsec+2.0
        start2_wsec = stop1_wsec+2.0
        stop2_wsec = start2_wsec+2.0
        timer1 = {'recording_enable':True,'use_pkt_timestamp':pkt_ts,'twsec':start1_wsec,'tfsec':0.0}
        timer2 = {'recording_enable':False,'use_pkt_timestamp':pkt_ts,'twsec':stop1_wsec,'tfsec':0.0}
        timer3 = {'recording_enable':True,'use_pkt_timestamp':pkt_ts,'twsec':start2_wsec,'tfsec':0.0}
        timer4 = {'recording_enable':False,'use_pkt_timestamp':pkt_ts,'twsec':stop2_wsec,'tfsec':0.0}
        timers = [timer1,timer2,timer3,timer4]
        comp.recording_timer = timers
        end_ws = stop2_wsec + 3.0
        
        #Start Components & Push Data
        sb.start()
        ts_now = bulkio.bulkio_helpers.createCPUTimestamp()
        while ts_now.twsec < end_ws:
            source.push(data,sampleRate=512) # 1024 samples per push
            time.sleep(2)
            ts_now = bulkio.bulkio_helpers.createCPUTimestamp()
        sb.stop()

        #Check that the input and output files are the same
        try:
            outfile = dataFileOut
            self.assertEqual(filecmp.cmp(dataFileIn, outfile), True)
            outfile = dataFileOut+'-1'
            self.assertEqual(filecmp.cmp(dataFileIn, outfile), True)
        except self.failureException as e:
            # unpacked bytes may be NaN, which could cause test to fail unnecessarily
            size = os.path.getsize(outfile)
            with open (outfile, 'rb') as dataOut:
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
            os.remove(dataFileOut+'-1')
        
        #TODO - validate timestamps, perhaps using BLUEFILEs
        
        print "........ PASSED\n"
        return

if __name__ == "__main__":
    ossie.utils.testing.main("../FileWriter.spd.xml") # By default tests all implementations
