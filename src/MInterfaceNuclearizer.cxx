/*
* MInterfaceNuclearizer.cxx
*
*
* Copyright (C) 2008-2008 by Andreas Zoglauer.
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
// MInterfaceNuclearizer
//
////////////////////////////////////////////////////////////////////////////////


// Include the header:
#include "MInterfaceNuclearizer.h"

// Standard libs:
#include <iostream>
#include <sstream>
#include <vector>
using namespace std;

// ROOT libs:
#include "TROOT.h"
#include "MString.h"
#include "TCanvas.h"
#include "TView.h"
#include "TGMsgBox.h"
#include "TH2.h"
#include "TCanvas.h"
#include "TApplication.h"

// MEGAlib libs:
#include "MGlobal.h"
#include "MAssert.h"
#include "MStreams.h"
#include "MTimer.h"

// Nuclearizer libs:
#include "MGUIMainNuclearizer.h"
#include "MNCTFile.h"
#include "MNCTEvent.h"
#include "MNCTData.h"
#include "MNCTModule.h"
#include "MNCTPreprocessor.h"
#include "MNCTFileEventsDat.h"
#include "MGUIExpoCombinedViewer.h"


////////////////////////////////////////////////////////////////////////////////


#ifdef ___CINT___
ClassImp(MInterfaceNuclearizer)
#endif


////////////////////////////////////////////////////////////////////////////////


MInterfaceNuclearizer::MInterfaceNuclearizer()
{
  // standard constructor
  
  m_Gui = 0;
  m_ExpoCombinedViewer = 0;
  m_UseGui = true;
  
  m_Data = new MNCTData();
  m_Data->Load(gSystem->ConcatFileName(gSystem->HomeDirectory(), ".nuclearizer.cfg"));
  
  m_Interrupt = false;
  m_Terminate = false;
  
  m_Verbosity = 2;
  m_UseMultiThreading = false;
  m_IsAnalysisRunning = false;
}



////////////////////////////////////////////////////////////////////////////////


MInterfaceNuclearizer::~MInterfaceNuclearizer()
{
  // standard destructor
  
  delete m_Data;
}


////////////////////////////////////////////////////////////////////////////////


bool MInterfaceNuclearizer::ParseCommandLine(int argc, char** argv)
{
  ostringstream Usage;
  Usage<<endl;
  Usage<<"  Usage: Nuclearizer <options>"<<endl;
  Usage<<endl;
  Usage<<"      -c --configuration <filename>.xml.cfg:"<<endl;
  Usage<<"             Use this file as configuration file."<<endl;
  Usage<<"             If no configuration file is give ~/.nuclearizer.xml.cfg is used"<<endl;
  Usage<<"      -a --auto:"<<endl;
  Usage<<"             Automatically start analysis without GUI"<<endl;
  Usage<<"      -m --multithreading:"<<endl;
  Usage<<"             0: false (default), else: true"<<endl;
  Usage<<"      -v --verbosity:"<<endl;
  Usage<<"             Verbosity: 0: Quiet, 1: Errors, 2: Warnings, 3: Info"<<endl;
  Usage<<"      -h --help:"<<endl;
  Usage<<"             You know the answer..."<<endl;
  Usage<<endl;
  
  // Store some options temporarily:
  MString Option;
  
  // Check for help
  for (int i = 1; i < argc; i++) {
    Option = argv[i];
    if (Option == "-h" || Option == "--help" || Option == "?" || Option == "-?") {
      cout<<Usage.str()<<endl;
      return false;
    }
  }
  
  // First check if all options are ok:
  for (int i = 1; i < argc; i++) {
    Option = argv[i];
    
    // Single argument
    if (Option == "-c" || Option == "--configuration" ||
        Option == "-m" || Option == "--multithreading") {
      if (!((argc > i+1) && argv[i+1][0] != '-')){
        cout<<"Error: Option "<<argv[i][1]<<" needs a second argument!"<<endl;
        cout<<Usage.str()<<endl;
        return false;
      }
    }		
  }
  
  // Now parse all low level options
  for (int i = 1; i < argc; i++) {
    Option = argv[i];
    if (Option == "--configuration" || Option == "-c") {
      m_Data->Load(argv[++i]);
      cout<<"Command-line parser: Use configuration file "<<argv[i]<<endl;
    } else if (Option == "--verbosity" || Option == "-v") {
      m_Verbosity = atoi(argv[++i]);
      g_Verbosity = m_Verbosity;
      cout<<"Command-line parser: Verbosity "<<m_Verbosity<<endl;
    } else if (Option == "--multithreading" || Option == "-m") {
      if (atoi(argv[++i]) != 0) m_UseMultiThreading = true;
      cout<<"Command-line parser: Using multithreading: "<<(m_UseMultiThreading ? "yes" : "no")<<endl;
    } else if (Option == "--auto" || Option == "-a") {
      // Parse later
    }
  }
  
  // Now parse all high level options
  for (int i = 1; i < argc; i++) {
    Option = argv[i];
    if (Option == "--auto" || Option == "-a") {
      m_UseGui = false;
      gROOT->SetBatch(true);
      Analyze();
      Exit();
      return false;
    }
  }
  
  if (m_UseGui == true) {
    m_Gui = new MGUINuclearizerMain(this, m_Data);
  }
  
  return true;
}


////////////////////////////////////////////////////////////////////////////////


void MInterfaceNuclearizer::SetInterrupt(bool Flag) 
{ 
  // Set the interrupt which will break the analysis
  m_Interrupt = Flag; 
  for (unsigned int m = 0; m < m_Data->GetNModules(); ++m) {
    m_Data->GetModule(m)->SetInterrupt(m_Interrupt);
  }
}
 
////////////////////////////////////////////////////////////////////////////////


bool MInterfaceNuclearizer::Analyze()
{
  if (m_UseMultiThreading == true) {
    return AnalyzeMultiThreaded();
  } else {
    return AnalyzeSingleThreaded();
  }
}
 
////////////////////////////////////////////////////////////////////////////////


bool MInterfaceNuclearizer::AnalyzeSingleThreaded()
{
  if (m_IsAnalysisRunning == true) return false;
  m_IsAnalysisRunning = true;

  // Start with saving the data:
  m_Data->Save(gSystem->ConcatFileName(gSystem->HomeDirectory(), ".nuclearizer.cfg"));

  m_Interrupt = false;
  
  // Start a global timer:
  MTimer Timer;

  // Load the geometry:
  if (m_Data->LoadGeometry() == false) return false;

  // Create a bunch of individual timers
  vector<MTimer> ModuleTimers(m_Data->GetNModules(), MTimer(false));
  
  // Initialize the modules:
  for (unsigned int m = 0; m < m_Data->GetNModules(); ++m) {
    ModuleTimers[m].Continue();
    m_Data->GetModule(m)->SetInterrupt(false);
    m_Data->GetModule(m)->SetVerbosity(m_Verbosity);
    if (m_Data->GetModule(m)->Initialize() == false) {
      ModuleTimers[m].Pause();
      if (m_Interrupt == true) {
        break;
      }
      mout<<"Initialization of module "<<m_Data->GetModule(m)->GetName()<<" failed"<<endl;
      m_IsAnalysisRunning = false;
      return false;
    }
    ModuleTimers[m].Pause();
  }
  
  // Create the expo viewer:
  if (m_ExpoCombinedViewer == 0) {
    m_ExpoCombinedViewer = new MGUIExpoCombinedViewer();
    m_ExpoCombinedViewer->Create();
  }
  m_ExpoCombinedViewer->RemoveExpos();
  for (unsigned int m = 0; m < m_Data->GetNModules(); ++m) {
    if (m_Data->GetModule(m)->HasExpos() == true) {
      m_ExpoCombinedViewer->AddExpos(m_Data->GetModule(m)->GetExpos());
    }
  }
  m_ExpoCombinedViewer->OnReset();
  m_ExpoCombinedViewer->ShowExpos();
  
  // Do the pipeline
  MNCTEvent* Event = new MNCTEvent(); // will be loaded on start
  while (m_Interrupt == false) {
    // Reset the event to zero
    Event->Clear();

    // Some modules need to be made ready for the next event, do this here
    bool AllReady = true;
    bool AllOK = true;
    do {
      AllReady = true;
      AllOK = true;
      for (unsigned int m = 0; m < m_Data->GetNModules(); ++m) {
        ModuleTimers[m].Continue();
        if (m_Data->GetModule(m)->IsReady() == false) {
          if (m_Interrupt == true) break;
          mout<<"Module \""<<m_Data->GetModule(m)->GetName()<<"\" is not yet ready..."<<endl;
          AllReady = false;
        }
        if (m_Data->GetModule(m)->IsOK() == false) {
          mout<<"Module \""<<m_Data->GetModule(m)->GetName()<<"\" is no longer OK... exiting analysis loop..."<<endl;
          AllOK = false;
        }
        ModuleTimers[m].Pause();
      }
      if (AllReady == false && AllOK == true) {
        cout<<"Not all modules ready (probably waiting for more data)... sleeping 100 ms"<<endl;
        gSystem->Sleep(100);
        gSystem->ProcessEvents();
      }
    } while (AllReady == false && AllOK == true && m_Interrupt == false);
    if (AllOK == false) {
      cout<<"One module had problems, exiting analysis loop"<<endl;
      break;
    }
    
    if (m_Interrupt == true) break;
      
    // Loop over all modules and do the analysis
    for (unsigned int m = 0; m < m_Data->GetNModules(); ++m) {
      ModuleTimers[m].Continue();
      // Do the analysis
      if (m_Data->GetModule(m)->AnalyzeEvent(Event) == false) {
        if (Event->GetID() != g_UnsignedIntNotDefined) {
          mout<<"Analysis failed for event "<<Event->GetID()
              <<" in module \""<<m_Data->GetModule(m)->GetName()<<"\""<<endl;
        } 
        ModuleTimers[m].Pause();
        break;
      }
      ModuleTimers[m].Pause();
      if (Event->IsDataRead() == false) break;
      // Only analyze non-vetoed, triggered events
      if (Event->GetVeto() == true || Event->GetTrigger() == false) {
        break;
      }
    }
    // if (Event->IsDataRead() == false) break;
    
    gSystem->ProcessEvents();    
  }
  
  // Finalize the modules:
  for (unsigned int m = 0; m < m_Data->GetNModules(); ++m) {
    ModuleTimers[m].Continue();
    m_Data->GetModule(m)->Finalize();
    ModuleTimers[m].Pause();
  }
  
  mout<<endl;
  if (m_Interrupt == true) {
    mout<<"Nuclearizer: Analysis INTERRUPTED after "<<Timer.ElapsedTime()<<"s"<<endl;
  } else {
    mout<<"Nuclearizer: Analysis finished in "<<Timer.ElapsedTime()<<"s"<<endl;
  }
  mout<<endl;
  
  if (m_Verbosity >= 2) {
    cout<<"Timings: "<<endl;
    for (unsigned int m = 0; m < m_Data->GetNModules(); ++m) {
      cout<<"Spent "<<ModuleTimers[m].GetElapsed()<<" sec in module "<<m_Data->GetModule(m)->GetName()<<endl;
    }
  }
  
  m_IsAnalysisRunning = false;
  
  if (m_Terminate == true) Terminate();

  return true;
}


////////////////////////////////////////////////////////////////////////////////


bool MInterfaceNuclearizer::AnalyzeMultiThreaded()
{
  if (m_IsAnalysisRunning == true) return false;
  m_IsAnalysisRunning = true;
  
  // Start with saving the data:
  m_Data->Save(gSystem->ConcatFileName(gSystem->HomeDirectory(), ".nuclearizer.cfg"));

  m_Interrupt = false;
  
  // Start a global timer:
  MTimer Timer;

  // Load the geometry:
  if (m_Data->LoadGeometry() == false) return false;

  // Create a bunch of individual timers
  vector<MTimer> ModuleTimers(m_Data->GetNModules(), MTimer(false));
  
  
  // Initialize the modules:
  if (m_Data->GetNModules() == 0) {
    if (g_Verbosity >= c_Error) mout<<"Error: No modules"<<endl;
    return false;
  }
  if (m_Data->GetNModules() == 1 && m_Data->GetModule(0)->IsStartModule() == false) {
    if (g_Verbosity >= c_Error) mout<<"Error: The first module must either load or generate the events"<<endl;
    return false;
  }
  
  for (unsigned int m = 0; m < m_Data->GetNModules(); ++m) {
    ModuleTimers[m].Continue();
    m_Data->GetModule(m)->SetInterrupt(false);
    m_Data->GetModule(m)->SetVerbosity(m_Verbosity);
    m_Data->GetModule(m)->UseMultiThreading(true);
    if (m_Data->GetModule(m)->Initialize() == false) {
      ModuleTimers[m].Pause();
      if (m_Interrupt == true) {
        break;
      }
      mout<<"Initialization of module "<<m_Data->GetModule(m)->GetName()<<" failed"<<endl;
      m_IsAnalysisRunning = false;
      return false;
    }
    ModuleTimers[m].Pause();
  }
  
  
  // Create the expo viewer:
  if (m_ExpoCombinedViewer == 0) {
    m_ExpoCombinedViewer = new MGUIExpoCombinedViewer();
    m_ExpoCombinedViewer->Create();
  }
  m_ExpoCombinedViewer->RemoveExpos();
  for (unsigned int m = 0; m < m_Data->GetNModules(); ++m) {
    if (m_Data->GetModule(m)->HasExpos() == true) {
      m_ExpoCombinedViewer->AddExpos(m_Data->GetModule(m)->GetExpos());
    }
  }
  m_ExpoCombinedViewer->OnReset();
  m_ExpoCombinedViewer->ShowExpos();
  
  
  // Do the analysis pipeline
  bool AllOK = true;
  while (m_Interrupt == false && AllOK == true) {

    for (unsigned int m = 0; m < m_Data->GetNModules(); ++m) {
      ModuleTimers[m].Continue();
      MNCTModule* M = m_Data->GetModule(m);
      if (M->IsMultiThreaded() == false) { // We have to do the heavy lifing
        M->DoSingleAnalysis(); 
      }
      if (M->HasAnalyzedEvents() == true) {
        MNCTEvent* E = M->GetAnalyzedEvent();
        if (m < m_Data->GetNModules()-1) {
          m_Data->GetModule(m+1)->AddEvent(E);
        } else {
          delete E;
        }
      }

      if (M->IsOK() == false) {
        mout<<"Module \""<<m_Data->GetModule(m)->GetName()<<"\" is no longer OK... exiting analysis loop..."<<endl;
        AllOK = false;
      }
      ModuleTimers[m].Pause();
    }
    
    gSystem->ProcessEvents();    
  }
  
  
  // Finalize the modules:
  for (unsigned int m = 0; m < m_Data->GetNModules(); ++m) {
    ModuleTimers[m].Continue();
    m_Data->GetModule(m)->Finalize();
    ModuleTimers[m].Pause();
  }
  
  mout<<endl;
  if (m_Interrupt == true) {
    mout<<"Nuclearizer: Analysis INTERRUPTED after "<<Timer.ElapsedTime()<<"s"<<endl;
  } else {
    mout<<"Nuclearizer: Analysis finished in "<<Timer.ElapsedTime()<<"s"<<endl;
  }
  mout<<endl;
  
  if (m_Verbosity >= 2) {
    cout<<"Timings: "<<endl;
    for (unsigned int m = 0; m < m_Data->GetNModules(); ++m) {
      cout<<"Spent "<<ModuleTimers[m].GetElapsed()<<" sec in module "<<m_Data->GetModule(m)->GetName()<<endl;
    }
  }
  
  m_IsAnalysisRunning = false;

  if (m_Terminate == true) Terminate();
  
  return true;
}


////////////////////////////////////////////////////////////////////////////////


void MInterfaceNuclearizer::View()
{
  // Show the view
  
  if (m_ExpoCombinedViewer != 0) {
     m_ExpoCombinedViewer->MapWindow();
  }
}


////////////////////////////////////////////////////////////////////////////////


void MInterfaceNuclearizer::Exit()
{
  // Prepare to exit the application
  
  if (m_IsAnalysisRunning == true) {
    m_Interrupt = true;
    m_Terminate = true;
  } else {
    Terminate();
  }
}


////////////////////////////////////////////////////////////////////////////////


void MInterfaceNuclearizer::Terminate()
{
  // Exit the application
  
  m_Data->Save(gSystem->ConcatFileName(gSystem->HomeDirectory(), ".nuclearizer.cfg"));
  gApplication->Terminate(0);
}


// MInterfaceNuclearizer: the end...
////////////////////////////////////////////////////////////////////////////////
