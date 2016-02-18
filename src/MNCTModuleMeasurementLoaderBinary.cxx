/*
 * MNCTModuleMeasurementLoaderBinary.cxx
 *
 *
 * Copyright (C) by Alex Lowell & Andreas Zoglauer.
 * All rights reserved.
 *
 *
 * This code implementation is the intellectual property of
 * Andreas Zoglauer.
 *
 * By copying, distributing or modifying the Program (or any work
 * based on the Program) you indicate your acceptance of this statement,
 * and all its terms.
 *
 */




////////////////////////////////////////////////////////////////////////////////
//
// MNCTModuleMeasurementLoaderBinary
//
////////////////////////////////////////////////////////////////////////////////


// Include the header:
#include "MNCTModuleMeasurementLoaderBinary.h"

// Standard libs:
#include <algorithm>
#include <cstdio>
using namespace std;
#include <time.h>

// ROOT libs:
#include "TGClient.h"

// MEGAlib libs:
#include "MGUIOptionsMeasurementLoaderBinary.h"


////////////////////////////////////////////////////////////////////////////////


#ifdef ___CINT___
ClassImp(MNCTModuleMeasurementLoaderBinary)
#endif


////////////////////////////////////////////////////////////////////////////////


MNCTModuleMeasurementLoaderBinary::MNCTModuleMeasurementLoaderBinary() : MModule(), MNCTBinaryFlightDataParser()
{
  // Construct an instance of MNCTModuleMeasurementLoaderBinary

  // Set all modules, which have to be done before this module
  // None
  
  // Set all types this modules handles
  AddModuleType(MAssembly::c_EventLoader);
  AddModuleType(MAssembly::c_EventLoaderMeasurement);
  AddModuleType(MAssembly::c_EventOrdering);
  AddModuleType(MAssembly::c_Aspect);

  // Set all modules, which can follow this module
  AddSucceedingModuleType(MAssembly::c_NoRestriction);
  
  // Set the module name --- has to be unique
  m_Name = "Data packet loader, sorter, and aspect reconstructor for COSI 2014";
  
  // Set the XML tag --- has to be unique --- no spaces allowed
  m_XmlTag = "XmlTagMeasurementLoaderBinary";  
  
  m_HasOptionsGUI = true;
  
  // Set the histogram display
  m_ExpoAspectViewer = new MGUIExpoAspectViewer(this);
  m_Expos.push_back(m_ExpoAspectViewer);
  
  // Allow the use of multiple threads and instances
  m_AllowMultiThreading = true;
  m_AllowMultipleInstances = false;
  
  m_IsStartModule = true;
  
  m_IgnoreAspect = true;
  m_FileIsDone = false;
}


////////////////////////////////////////////////////////////////////////////////


MNCTModuleMeasurementLoaderBinary::~MNCTModuleMeasurementLoaderBinary()
{
  // Delete this instance of MNCTModuleMeasurementLoaderBinary
}


////////////////////////////////////////////////////////////////////////////////

FILE * f_TOnly;


bool MNCTModuleMeasurementLoaderBinary::Initialize()
{
  // Initialize the module 

  m_FileIsDone = false;

  if (m_In.is_open()) m_In.close();
  m_In.clear();
  m_In.open(m_FileName, ios::binary);
  if (m_In.is_open() == false) {
    if (g_Verbosity >= c_Error) cout<<m_XmlTag<<": Error: unable to load file \""<<m_FileName<<"\""<<endl;
    return false;
  }
  
  if (MNCTBinaryFlightDataParser::Initialize() == false) return false;
  //f_TOnly = fopen("TOnly.txt","w");
  
  return MModule::Initialize();
}


////////////////////////////////////////////////////////////////////////////////


bool MNCTModuleMeasurementLoaderBinary::IsReady() 
{
  if (m_Events.size() > 0) {
    if (m_IgnoreAspect == true) {
      return true;
    } else {
      MNCTAspect* A = m_Events[0]->GetAspect();
      if( A != 0 ){
        return true;
      }
    }
  }
  
  //unsigned int Size = 1000000; // We have to do a large chunk here or the main thread is going to sleep...
  //vector<char> Stream(Size);
  // Check if we reached the end of the file, if yes, truncate, and set the OK flag to false
  // when the end of the file is reached, we want to 

  //AWL restructured this so that we don't allocate/fill a 1MB array when there is nothing to read.  

  vector<char> Stream;
  unsigned int Size = 1000000;
  streamsize Read;
  if( m_FileIsDone ){
	  Read = 0;
  } else {
	  Stream.reserve(Size);
	  m_In.read(&Stream[0], Size);
	  Read = m_In.gcount();
  }



  /*
  if (Read < Size) {
    m_IsOK = false;
  }
  */

  if (Read < Size) {
    m_FileIsDone = true;
	  m_IgnoreBufTime = true;
  }

  if( m_FileIsDone && (m_EventsBuf.size() == 0) ){
	  //m_IsOK = false;
	  m_IsFinished = true;
  }

  vector<uint8_t> Received(Read);
  for (unsigned int i = 0; i < Read; ++i) {
//	  cout << "char: " << Received[i] << endl;
//	 printf("char:%02X\n",(uint8_t)Stream[i]);
    Received[i] = (uint8_t) Stream[i];
  }
  
  return ParseData(Received);
}


////////////////////////////////////////////////////////////////////////////////


bool MNCTModuleMeasurementLoaderBinary::AnalyzeEvent(MReadOutAssembly* Event) 
{
  // IsReady() ensured that the oldest event in the list has a reconstructed aspect
  MReadOutAssembly * NewEvent;
  
  if (m_Events.size() == 0) {
    cout<<"ERROR in MNCTModuleMeasurementLoaderBinary::AnalyzeEvent: No events"<<endl;
    cout<<"This function should have never been called when we have no events"<<endl;
    return false;
  }

  NewEvent = m_Events[0];
  m_Events.pop_front();

  //print TOnly info for these events
  /*
  if( NewEvent->GetNStripHitsTOnly() > 0 ){
	  fprintf(f_TOnly,">>>\n");
	  for(unsigned int i = 0; i < NewEvent->GetNStripHits(); ++i){
		  MNCTStripHit* SH = NewEvent->GetStripHit(i);
		  int id = SH->GetStripID();
		  int T = (int) SH->GetTiming();
		  if( SH->IsXStrip() ){
		  	fprintf(f_TOnly,"X%d---%d, ",id,T);
		  } else {
			fprintf(f_TOnly,"Y%d---%d; ",id,T);
		  }
	  }
	  fprintf(f_TOnly,"\n###\n");
	  for(unsigned int i = 0; i < NewEvent->GetNStripHitsTOnly(); ++i){
		  MNCTStripHit* SH = NewEvent->GetStripHitTOnly(i);
		  int id = SH->GetStripID();
		  int T = (int) SH->GetTiming();
		  if( SH->IsXStrip() ){
		  	fprintf(f_TOnly,"X%d---%d, ",id,T);
		  } else {
			  fprintf(f_TOnly,"Y%d---%d, ",id,T);
		  }
	  }
	  fprintf(f_TOnly,"\n<<<\n");
  }
  */


  // This checks if the event's aspect data was within the range of the retrieved aspect info
  if (NewEvent->GetAspect() != 0 && NewEvent->GetAspect()->GetOutOfRange()) {
    delete NewEvent;
    return false;
  }

  //transfer over strip hits that have ADC
  while (NewEvent->GetNStripHits() > 0) {
    Event->AddStripHit( NewEvent->GetStripHit(0) );
    NewEvent->RemoveStripHit(0);
  }

  /*
  //transfer over strip hits that have timing and no ADC
  while (NewEvent->GetNStripHitsTOnly() > 0){
	  Event->AddStripHitTOnly( NewEvent->GetStripHitTOnly(0));
	  NewEvent->RemoveStripHitTOnly(0);
  }*/


  Event->SetID( NewEvent->GetID() );
  Event->SetFC( NewEvent->GetFC() );
  Event->SetTI( NewEvent->GetTI() );
  Event->SetCL( NewEvent->GetCL() );
  Event->SetTime( NewEvent->GetTime() );
  Event->SetMJD( NewEvent->GetMJD() );
  if (NewEvent->GetAspect() != 0) {
    MNCTAspect* A = new MNCTAspect(*(NewEvent->GetAspect()));
    Event->SetAspect(A);
    //cout<<"Adding: "<<NewEvent->GetTime()<<":"<<A->GetHeading()<<endl;
    m_ExpoAspectViewer->AddHeading(NewEvent->GetTime(), A->GetHeading(), A->GetGPS_or_magnetometer(), A->GetBRMS(), A->GetAttFlag());
    Event->SetAnalysisProgress(MAssembly::c_Aspect);
  }
  Event->SetAnalysisProgress(MAssembly::c_EventLoader | 
                             MAssembly::c_EventLoaderMeasurement | 
                             MAssembly::c_EventOrdering);
  
  delete NewEvent;
  
  return true;
}


////////////////////////////////////////////////////////////////////////////////


void MNCTModuleMeasurementLoaderBinary::Finalize()
{
  // Close the tranceiver 

  MModule::Finalize();
  MNCTBinaryFlightDataParser::Finalize();
  
  m_In.close();
  m_In.clear();
  
  return;
}


///////////////////////////////////////////////////////////////////////////////


void MNCTModuleMeasurementLoaderBinary::ShowOptionsGUI()
{
  // Show the options GUI

  MGUIOptionsMeasurementLoaderBinary* Options = new MGUIOptionsMeasurementLoaderBinary(this);
  Options->Create();
  gClient->WaitForUnmap(Options);
}


////////////////////////////////////////////////////////////////////////////////


bool MNCTModuleMeasurementLoaderBinary::ReadXmlConfiguration(MXmlNode* Node)
{
  //! Read the configuration data from an XML node

  MXmlNode* FileNameNode = Node->GetNode("FileName");
  if (FileNameNode != 0) {
    m_FileName = FileNameNode->GetValueAsString();
  }
  
  MXmlNode* DataSelectionModeNode = Node->GetNode("DataSelectionMode");
  if (DataSelectionModeNode != 0) {
    m_DataSelectionMode = (MNCTBinaryFlightDataParserDataModes) DataSelectionModeNode->GetValueAsInt();
  }

  MXmlNode* AspectSelectionModeNode = Node->GetNode("AspectSelectionMode");
  if( AspectSelectionModeNode != 0 ){
	  m_AspectMode = (MNCTBinaryFlightDataParserAspectModes) AspectSelectionModeNode->GetValueAsInt();
  }

  MXmlNode* CoincidenceMergingNode = Node->GetNode("CoincidenceMerging");
  if( CoincidenceMergingNode != NULL ){
	  m_CoincidenceEnabled = (bool) CoincidenceMergingNode->GetValueAsInt();
  }


  return true;
}


////////////////////////////////////////////////////////////////////////////////


MXmlNode* MNCTModuleMeasurementLoaderBinary::CreateXmlConfiguration() 
{
  //! Create an XML node tree from the configuration
  
  MXmlNode* Node = new MXmlNode(0, m_XmlTag);  
  new MXmlNode(Node, "FileName", m_FileName);
  new MXmlNode(Node, "DataSelectionMode", (unsigned int) m_DataSelectionMode);
  new MXmlNode(Node, "AspectSelectionMode", (unsigned int) m_AspectMode);
  new MXmlNode(Node, "CoincidenceMerging",(unsigned int) m_CoincidenceEnabled);
  
  return Node;
}


// MNCTModuleMeasurementLoaderBinary.cxx: the end...
////////////////////////////////////////////////////////////////////////////////
