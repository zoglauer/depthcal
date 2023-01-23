/*
 * MModuleDepthCalibrationLinearPixel.h
 *
 * Copyright (C) 2008-2008 by Mark Bandstra.
 * All rights reserved.
 *
 * Please see the source-file for the copyright-notice.
 *
 */


#ifndef __MModuleDepthCalibration3rdPolyPixel__
#define __MModuleDepthCalibration3rdPolyPixel__


////////////////////////////////////////////////////////////////////////////////


// Standard libs:

// ROOT libs:

// MEGAlib libs:
#include "MGlobal.h"

// Forward declarations:
#include "MModule.h"
#include "MGUIExpoDepthCalibration.h"

////////////////////////////////////////////////////////////////////////////////


class MModuleDepthCalibration3rdPolyPixel : public MModule
{
  // public interface:
 public:
  //! Default constructor
  MModuleDepthCalibration3rdPolyPixel();
  //! Default destructor
  virtual ~MModuleDepthCalibration3rdPolyPixel();
  
  //! Create a new object of this class 
  virtual MModuleDepthCalibration3rdPolyPixel* Clone() { return new MModuleDepthCalibration3rdPolyPixel(); }

  //! Set the calibration file name
  void SetFileName(const MString& FileName) { m_FileName = FileName;}
  //! Get the calibration file name
  MString GetFileName() const {return m_FileName;}
  
  //! Create the expos
  virtual void CreateExpos();

  //! Initialize the module
  virtual bool Initialize();

  //! Main data analysis routine, which updates the event to a new level 
  virtual bool AnalyzeEvent(MReadOutAssembly* Event);

  //! Show the options GUI
  virtual void ShowOptionsGUI();

  virtual bool ReadXmlConfiguration(MXmlNode* Node);
  virtual MXmlNode* CreateXmlConfiguration();


  // protected methods:
 protected:
  //!  The calibration file name
  string m_FileName;


  // private methods:
 private:


  // public members:
 public:
  // DetectorMap
  struct DetectorMapping {
    int CCNumber;
    int DetectorNumber;
    MString DetectorName;
    int DisplayID;
    MString DisplayName;
  };
   

  // protected members:
 protected:


  // private members:
 private:
  //! The depth calibration UI 
  MGUIExpoDepthCalibration* m_ExpoDepthCalibration;

  bool m_IsCalibrationLoaded[12];
  bool m_IsCalibrationLoadedPixel[12][37][37];
  double m_Pixel_CTD2Depth[12][37][37][4];
  double m_Pixel_CTD_FWHM_Positive[12][37][37];
  double m_Pixel_CTD_FWHM_Negative[12][37][37];
  double m_Default_CTD2Depth[4];
  double m_Default_CTD_FWHM;
  unsigned long ShareHitNumber0,ShareHitNumber1, NotValidStripNumber, OutofBoundsDepth;
  unsigned long SingleHitNumber,OtherHitNumber, LLDNumber, InvalidEventNumber;
  unsigned long ShareEventNumber0,ShareEventNumber1, SingleEventNumber,OtherEventNumber;




  map<int, DetectorMapping> DetMap;
  vector<MDVolume*> m_DetectorVolumes;



#ifdef ___CLING___
 public:
  ClassDef(MModuleDepthCalibration3rdPolyPixel, 0) // no description
#endif

};

#endif


///////////////////////////////////////////////////////////////////////////////