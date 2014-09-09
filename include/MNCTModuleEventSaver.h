/*
 * MNCTModuleEventSaver.h
 *
 * Copyright (C) by Andreas Zoglauer.
 * All rights reserved.
 *
 * Please see the source-file for the copyright-notice.
 *
 */


#ifndef __MNCTModuleEventSaver__
#define __MNCTModuleEventSaver__


////////////////////////////////////////////////////////////////////////////////


// Standard libs:
#include <fstream>
using namespace std;

// ROOT libs:

// MEGAlib libs:
#include "MGlobal.h"
#include "MString.h"

// Nuclearizer libs:
#include "MModule.h"

// Forward declarations:


////////////////////////////////////////////////////////////////////////////////


class MNCTModuleEventSaver : public MModule
{
  // public interface:
 public:
  //! Default constructor
  MNCTModuleEventSaver();
  //! Default destructor
  virtual ~MNCTModuleEventSaver();

  //! Get the mode
  unsigned int GetMode() const { return m_Mode; }
  //! Set the mode
  void SetMode(unsigned int Mode) { m_Mode = Mode; }
  
  //! Get the file name
  MString GetFileName() const { return m_FileName; }
  //! Set the file name
  void SetFileName(const MString& Name) { m_FileName = Name; }
  
  //! Get the file name
  bool GetSaveBadEvents() const { return m_SaveBadEvents; }
  //! Set the file name
  void SetSaveBadEvents(bool SaveBadEvents) { m_SaveBadEvents = SaveBadEvents; }
  
  //! Initialize the module
  virtual bool Initialize();

  //! Finalize the module
  virtual void Finalize();

  //! Main data analysis routine, which updates the event to a new level 
  virtual bool AnalyzeEvent(MReadOutAssembly* Event);

  //! Show the options GUI
  virtual void ShowOptionsGUI();

  //! Read the configuration data from an XML node
  virtual bool ReadXmlConfiguration(MXmlNode* Node);
  //! Create an XML node tree from the configuration
  virtual MXmlNode* CreateXmlConfiguration();

  static const unsigned int c_RoaFile  = 0;
  static const unsigned int c_DatFile  = 1;
  static const unsigned int c_EvtaFile = 2;
  static const unsigned int c_SimFile  = 3;
  
  // protected methods:
 protected:
  //!
  void WriteHeader();

  //!
  void DumpBasic(MReadOutAssembly* Event);

  //!
  void DumpHitsSim(MNCTHit* HitSim);

  //!
  void DumpStripHits(MNCTStripHit* StrpHit);

  //!
  void DumpHits(MNCTHit* Hit);
  
  // private methods:
 private:

  // protected members:
 protected:


  // private members:
 private:
  //! The operation mode
  unsigned int m_Mode;
  //! The file name
  MString m_FileName;
  //! Save bad events
  bool m_SaveBadEvents;
  
  //! Output stream for dat file
  ofstream m_Out;

  
#ifdef ___CINT___
 public:
  ClassDef(MNCTModuleEventSaver, 0) // no description
#endif

};

#endif


////////////////////////////////////////////////////////////////////////////////
