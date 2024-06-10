/*
 * MModuleDepthCalibration2024.cxx
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
// MModuleDepthCalibration2024
//
////////////////////////////////////////////////////////////////////////////////


// Include the header:
#include "MModuleDepthCalibration2024.h"
#include "MGUIOptionsDepthCalibration2024.h"

// Standard libs:

// ROOT libs:
#include "TGClient.h"
#include "TH1.h"

// MEGAlib libs:


////////////////////////////////////////////////////////////////////////////////


#ifdef ___CLING___
ClassImp(MModuleDepthCalibration2024)
#endif


////////////////////////////////////////////////////////////////////////////////


MModuleDepthCalibration2024::MModuleDepthCalibration2024() : MModule()
{
  // Construct an instance of MModuleDepthCalibration2024

  // Set all module relevant information

  // Set the module name --- has to be unique
  m_Name = "Depth calibration 2024"; // - Determining the depth of each event (by Sean);

  // Set the XML tag --- has to be unique --- no spaces allowed
  m_XmlTag = "DepthCalibration2024";

  // Set all modules, which have to be done before this module
  AddPreceedingModuleType(MAssembly::c_EventLoader, true);
  AddPreceedingModuleType(MAssembly::c_EnergyCalibration, true);
  AddPreceedingModuleType(MAssembly::c_StripPairing, true);
//  AddPreceedingModuleType(MAssembly::c_CrosstalkCorrection, false); // Soft requirement

  // Set all types this modules handles
  AddModuleType(MAssembly::c_DepthCorrection);
  AddModuleType(MAssembly::c_PositionDetermiation);

  // Set all modules, which can follow this module
  AddSucceedingModuleType(MAssembly::c_NoRestriction);

  // Set if this module has an options GUI
  // If true, overwrite ShowOptionsGUI() with the call to the GUI!
  m_HasOptionsGUI = true;
  // If true, you have to derive a class from MGUIOptions (use MGUIOptionsTemplate)
  // and implement all your GUI options
  
  // Allow the use of multiple threads and instances
  m_AllowMultiThreading = true;
  m_AllowMultipleInstances = false;

  m_NoError = 0;
  m_Error1 = 0;
  m_Error2 = 0;
  m_Error3 = 0;
  m_Error4 = 0;
  m_ErrorSH = 0;
}


////////////////////////////////////////////////////////////////////////////////


MModuleDepthCalibration2024::~MModuleDepthCalibration2024()
{
  // Delete this instance of MModuleDepthCalibration2024
}


////////////////////////////////////////////////////////////////////////////////


bool MModuleDepthCalibration2024::Initialize()
{

  if( LoadCoeffsFile(m_CoeffsFile) == false ){
    return false;
  }
  if( LoadSplinesFile(m_SplinesFile) == false ){
    return false;
  }

  // The detectors need to be in the same order as DetIDs.
  // ie DetID=0 should be the 0th detector in m_Detectors, DetID=1 should the 1st, etc.
  m_Detectors = m_Geometry->GetDetectorList();

  // Look through the Geometry and get the names and thicknesses of all the detectors.
  for(unsigned int i = 0; i < m_Detectors.size(); ++i){
    MDDetector* det = m_Detectors[i];
    MString det_name = det->GetName();
    m_DetectorNames.push_back(det_name);
    MDVolume* vol = det->GetSensitiveVolume(0); MDShapeBRIK* shape = dynamic_cast<MDShapeBRIK*>(vol->GetShape());
    double thickness = shape->GetSizeZ();
    m_Thicknesses.push_back(thickness);
    MDStrip3D* strip = dynamic_cast<MDStrip3D*>(det);
    m_XPitches.push_back(strip->GetPitchX());
    m_YPitches.push_back(strip->GetPitchY());
    m_NXStrips.push_back(strip->GetNStripsX());
    m_NYStrips.push_back(strip->GetNStripsY());
    cout << "Found detector " << det_name << " with thickness " << thickness << " cm, corresponding to DetID=" << i << "." << endl;
    }

  MSupervisor* S = MSupervisor::GetSupervisor();
  m_EnergyCalibration = (MModuleEnergyCalibrationUniversal*) S->GetAvailableModuleByXmlTag("EnergyCalibrationUniversal");
  if (m_EnergyCalibration == nullptr) {
    cout << "MModuleDepthCalibration2024: couldn't resolve pointer to Energy Calibration Module... need access to this module for energy resolution lookup!" << endl;
    return false;
  }

  return MModule::Initialize();
}


////////////////////////////////////////////////////////////////////////////////


bool MModuleDepthCalibration2024::AnalyzeEvent(MReadOutAssembly* Event) 
{
  
  for( unsigned int i = 0; i < Event->GetNHits(); ++i ){
    // Each event represents one photon. It contains Hits, representing interaction sites.
    // H is a pointer to an instance of the MHit class. Each Hit has activated strips, represented by
    // instances of the MStripHit class.
    MHit* H = Event->GetHit(i);

    int Grade = GetHitGrade(H);

    // Handle different grades differently    
    // GRADE=-1 is an error. Break from the loop and continue.
    if ( Grade == -1 ){
      H->SetNoDepth();
      Event->SetDepthCalibrationIncomplete();
      ++m_ErrorSH;
    }

    // GRADE=5 is some complicated geometry with separated strips. No depth in this case.
    else if( Grade == 5 ){
      H->SetNoDepth();
      ++m_Error4;
      Event->SetDepthCalibrationIncomplete();
    }

    // If the Grade is 0-4, we can handle it.
    else {

      MVector LocalPosition, PositionResolution, GlobalPosition;

      // Calculate the position. If error is thrown, record and no depth.
      // Take a Hit and separate its activated X- and Y-strips into separate vectors.
      std::vector<MStripHit*> XStrips;
      std::vector<MStripHit*> YStrips;
      for( unsigned int j = 0; j < H->GetNStripHits(); ++j){
        MStripHit* SH = H->GetStripHit(j);
        if( SH->IsXStrip() ) XStrips.push_back(SH); else YStrips.push_back(SH);
      }

      double XEnergyFraction;
      double YEnergyFraction;
      MStripHit* XSH = GetDominantStrip(XStrips, XEnergyFraction); 
      MStripHit* YSH = GetDominantStrip(YStrips, YEnergyFraction); 

      double CTD_s = 0.0;

      //now try and get z position
      int DetID = XSH->GetDetectorID();
      int pixel_code = 10000*DetID + 100*XSH->GetStripID() + YSH->GetStripID();

      // TODO: Calculate X and Y positions more rigorously using charge sharing.
      double Xpos = m_XPitches[DetID]*((m_NXStrips[DetID]/2.0) - ((double)XSH->GetStripID()));
      double Ypos = m_YPitches[DetID]*((m_NYStrips[DetID]/2.0) - ((double)YSH->GetStripID()));
      double Zpos = 0.0;

      double Xsigma = m_XPitches[DetID]/sqrt(12.0);
      double Ysigma = m_YPitches[DetID]/sqrt(12.0);
      double Zsigma = m_Thicknesses[DetID]/sqrt(12.0);

      vector<double>* Coeffs = GetPixelCoeffs(pixel_code);

      // If there aren't coefficients loaded, then calibration is incomplete.
      if( Coeffs == nullptr ){
        //set the bad flag for depth
        H->SetNoDepth();
        Event->SetDepthCalibrationIncomplete();
        ++m_Error1;
      }
      // If there isn't timing information, set no depth, but don't set global bad flag.
      // Alex's old comments suggest assigning the event to the middle of the detector and the position resolution to be large.
      else if( (XSH->GetTiming() < 1.0E-6) || (YSH->GetTiming() < 1.0E-6) ){
          ++m_Error3;
          H->SetNoDepth();
      }

      // If there are coefficients and timing information is loaded, try calculating the CTD and depth
      else {

        vector<double> ctdvec = *GetCTD(DetID, Grade);
        vector<double> depthvec = *GetDepth(DetID);

        // TODO: For Card Cage, may need to add noise
        double CTD;
        if ( XSH->IsPositiveStrip() ){
          CTD = (XSH->GetTiming() - YSH->GetTiming());
        }
        else {
          CTD = (YSH->GetTiming() - XSH->GetTiming());
        }

        // Confirmed that this matches SP's python code.
        CTD_s = (CTD - Coeffs->at(1))/Coeffs->at(0); //apply inverse stretch and offset

        double Xmin = * std::min_element(ctdvec.begin(), ctdvec.end());
        double Xmax = * std::max_element(ctdvec.begin(), ctdvec.end());

        double noise = GetTimingNoiseFWHM(pixel_code, 662.0);

        //if the CTD is out of range, check if we should reject the event.
        if( (CTD_s < (Xmin - 2.0*noise)) || (CTD_s > (Xmax + 2.0*noise)) ){
          H->SetNoDepth();
          Event->SetDepthCalibrationIncomplete();
          ++m_Error1;
        }

        // If the CTD is in range, calculate the depth
        else {

          // Calculate the probability given timing noise of CTD_s corresponding to the values of depth in depthvec
          // Utlize symmetry of the normal distribution.
          // TODO: Get the energy of the event and pass it to GetTimingNoiseFWHM
          vector<double> prob_dist = norm_pdf(ctdvec, CTD_s, noise/2.355);
          
          // Weight the depth by probability
          double prob_sum = std::accumulate(prob_dist.begin(), prob_dist.end(), 0);
          double weighted_depth = 0.0;
          for( int i = 0; i < depthvec.size(); ++i ){
            weighted_depth += prob_dist[i]*depthvec[i];
          }
          // Calculate the expectation value of the depth
          double mean_depth = weighted_depth/prob_sum;

          // Calculate the standard deviation of the depth
          double depth_var = 0.0;
          for( int i=0; i<depthvec.size(); ++i ){
            depth_var += prob_dist[i]*pow(depthvec[i]-mean_depth, 2.0);
          }

          Zsigma =  sqrt(depth_var/prob_sum);
          Zpos = m_Thicknesses[DetID]/2.0 - mean_depth;
        }
      }

    LocalPosition.SetXYZ(Xpos, Ypos, Zpos);
    GlobalPosition = m_Geometry->GetGlobalPosition(LocalPosition, m_DetectorNames[DetID]);
    PositionResolution.SetXYZ(Xsigma, Ysigma, Zsigma);
    
    H->SetPosition(GlobalPosition); H->SetPositionResolution(PositionResolution);

    }
  }
  
  Event->SetAnalysisProgress(MAssembly::c_DepthCorrection | MAssembly::c_PositionDetermiation);

  return true;
}

MStripHit* MModuleDepthCalibration2024::GetDominantStrip(vector<MStripHit*>& Strips, double& EnergyFraction)
{
  double MaxEnergy = -numeric_limits<double>::max(); // AZ: When both energies are zero (which shouldn't happen) we still pick one
  double TotalEnergy = 0.0;
  MStripHit* MaxStrip = nullptr;

  // Iterate through strip hits and get the strip with highest energy
  for (const auto SH : Strips) {
    double Energy = SH->GetEnergy();
    TotalEnergy += Energy;
    if (Energy > MaxEnergy) {
      MaxStrip = SH;
      MaxEnergy = Energy;
    }
  }
  if (TotalEnergy == 0) {
    EnergyFraction = 0;
  } else {
    EnergyFraction = MaxEnergy/TotalEnergy;
  }
  return MaxStrip;
}

double MModuleDepthCalibration2024::GetTimingNoiseFWHM(int pixel_code, double Energy)
{
  // Placeholder for determining the timing noise with energy, and possibly even on a pixel-by-pixel basis.
  // Should follow 1/E relation
  // TODO: Fill this in
  return 6.0*2.355;
}

bool MModuleDepthCalibration2024::LoadCoeffsFile(MString FName)
{
  // Read in the stretch and offset file, which should contain for each pixel:
  // Pixel code (10000*det + 100*Xchannel + Ychannel), Stretch, Offset, Timing/CTD noise, Chi2 for the CTD fit (for diagnostics mainly)
  MFile F;
  if( F.Open(FName) == false ){
    cout << "MModuleDepthCalibration2024: failed to open coefficients file..." << endl;
    return false;
  } else {
    MString Line;
    while( F.ReadLine( Line ) ){
      if( !Line.BeginsWith("#") ){
        std::vector<MString> Tokens = Line.Tokenize(" ");
        if( Tokens.size() == 5 ){
          int pixel_code = Tokens[0].ToInt();
          double Stretch = Tokens[1].ToDouble();
          double Offset = Tokens[2].ToDouble();
          double CTD_FWHM = Tokens[3].ToDouble();
          double Chi2 = Tokens[4].ToDouble();
          // Previous iteration of depth calibration read in "Scale" instead of ctd resolution.
          vector<double> coeffs;
          coeffs.push_back(Stretch); coeffs.push_back(Offset); coeffs.push_back(CTD_FWHM); coeffs.push_back(Chi2);
          m_Coeffs[pixel_code] = coeffs;
        }
      }
    }
    F.Close();
  }

  m_CoeffsFileIsLoaded = true;

  return true;

}

std::vector<double>* MModuleDepthCalibration2024::GetPixelCoeffs(int pixel_code)
{
  // Check to see if the stretch and offset have been loaded. If so, try to get the coefficients for the specified pixel.
  if( m_CoeffsFileIsLoaded ){
    if( m_Coeffs.count(pixel_code) > 0 ){
      return &m_Coeffs[pixel_code];
    } else {
      cout << "MModuleDepthCalibration2024::GetPixelCoeffs: cannot get stretch and offset; pixel code " << pixel_code << " not found." << endl;
      return nullptr;
    }
  } else {
    cout << "MModuleDepthCalibration2024::GetPixelCoeffs: cannot get stretch and offset; file has not yet been loaded." << endl;
    return nullptr;
  }

}

vector<double> MModuleDepthCalibration2024::norm_pdf(vector<double> x, double mu, double sigma)
{
  vector<double> result;
  for( int i=0; x.size(); ++i ){
    result.push_back(1.0 / (sigma * sqrt(2.0 * M_PI)) * exp(-(pow((x[i] - mu)/sigma, 2)/2.0)));
  }
  return result;
}

bool MModuleDepthCalibration2024::LoadSplinesFile(MString FName)
{
  //when invert flag is set to true, the splines returned are CTD->Depth
  // Previously saved cathode and anode timing in addition to CTD. This may be redundant, commenting out for now.
  // Input spline files should have the following format:
  // ### DetID, HV, Temperature, Photopeak Energy (TODO: More? Fewer?)
  // depth, ctd0, ctd1, ctd2.... (Basically, allow for CTDs for different subpixel regions)
  // '' '' ''
  // The minimum depth listed should be 0.0 and the maximum should be equal to the total depth of the detector.
  MFile F; 
  if( F.Open(FName) == false ){
    return false;
  }
  // vector<double> depthvec, ctdvec, anovec, catvec;
  vector<double> depthvec;
  vector<vector<double>> ctdarr;
  MString line;
  int DetID, NewDetID;
  while( F.ReadLine(line) ){
    if( line.Length() != 0 ){
      if( line.BeginsWith("#") ){
        // If we've reached a new ctd spline then record the previous one in the m_SplineMaps and start a new one.
        vector<MString> tokens = line.Tokenize(" ");
        NewDetID = tokens[1].ToInt();
        if( depthvec.size() > 0 ) {
          AddDepthCTD(depthvec, ctdarr, DetID, m_DepthGrid, m_CTDMap);        
        }
        depthvec.clear(); ctdarr.clear(); 
        DetID = NewDetID;
      } else {
        vector<MString> tokens = line.Tokenize(" ");
        depthvec.push_back(tokens[0].ToDouble()); 

        // Multiple CTDs allowed
        for( unsigned int i = 0; i < (tokens.size() - 1); ++i ){
          ctdarr[i].push_back(tokens[1+i].ToDouble());
        }
      }
    }
  }
  //make last spline
  if( depthvec.size() > 0 ){
    AddDepthCTD(depthvec, ctdarr, DetID, m_DepthGrid, m_CTDMap);
  }

  m_SplinesFileIsLoaded = true;
  return true;

}

int MModuleDepthCalibration2024::GetHitGrade(MHit* H){
  // Function for choosing which Depth-to-CTD relation to use for a given event.
  // At time of writing, intention is to choose a CTD based on sub-pixel region determined via charge sharing (Event "grade").
  // 5 possible grades, and one Error Grade, -1. GRADE 4 is as yet uncategorized complicated geometry. GRADE 5 means multiple, presumably separated strip hits.

  //organize x and y strips into vectors
  if( H == NULL ){
    return -1;
  }
  if( H->GetNStripHits() == 0 ){
    // Error if no strip hits listed. Bad grad is returned
    cout << "ERROR in MModuleDepthCalibration2024: HIT WITH NO STRIP HITS" << endl;
    return -1;
  }
   
  // Take a Hit and separate its activated p and n strips into separate vectors.
  std::vector<MStripHit*> PStrips;
  std::vector<MStripHit*> NStrips;
  for( unsigned int j = 0; j < H->GetNStripHits(); ++j){
    MStripHit* SH = H->GetStripHit(j);
    if( SH == NULL ) { cout << "ERROR in MModuleDepthCalibration2024: Depth Calibration: got NULL strip hit :( " << endl; return -1;}
    if( SH->GetEnergy() == 0 ) { cout << "ERROR in MModuleDepthCalibration2024: Depth Calibration: got strip without energy :( " << endl; return -1;}
    if( SH->IsPositiveStrip() ) PStrips.push_back(SH); else NStrips.push_back(SH);
  }

  // if multiple, presumably separated strip hits, bad grade.
  bool MultiHitX = H->GetStripHitMultipleTimesX();
  bool MultiHitY = H->GetStripHitMultipleTimesY();
  if( MultiHitX || MultiHitY ){
    return 5;  
  }

  // If 1 strip on each side, GRADE=0
  // This represents the center of the pixel
  if( (PStrips.size() == 1) && (NStrips.size() == 1) ){
    return 0;
  } 
  // If 2 hits on N side and 1 on P, GRADE=1
  // This represents the middle of the edges of the pixel
  else if( (PStrips.size() == 1) && (NStrips.size() == 2) ){
    return 1;
  } 

  // If 2 hits on P and 1 on N, GRADE=2
  // This represents the middle of the edges of the pixel
  else if( (PStrips.size() == 2) && (NStrips.size() == 1) ){
    return 2;
  } 
  // If 2 strip hits on both sides, GRADE=3
  // This represents the corners the pixel
  else if( (PStrips.size() == 2) && (NStrips.size() == 2) ){
    return 3;
  } else {
    // If more complicated than the above cases, return 4 for now.
    // TODO: Handle more complicated charge distributions.
    return 4;
  }
}

bool MModuleDepthCalibration2024::AddDepthCTD(vector<double> depthvec, vector<vector<double>> ctdarr, int DetID, unordered_map<int, vector<double>>& DepthGrid, unordered_map<int,vector<vector<double>>>& CTDMap){

  // Saves a CTD array, basically allowing for multiple CTDs as a function of depth 
  // depthvec: list of simulated depth values
  // ctdarr: vector of vectors of simulated CTD values. Each vector of CTDs must be the same length as depthvec
  // DetID: An integer which specifies which detector.
  // CTDMap: unordered map into which the array of CTDs should be placed

  // TODO: Possible energy dependence of CTD?
  // TODO: Depth values need to be evenly spaced. Check this when reading the files in.

  // Check to make sure things look right.
  // First check that the CTDs all have the right length.
  for( int i = 0; i < ctdarr.size(); ++i ){
    if( ctdarr[i].size() != depthvec.size() ){
      cout << "MModuleDepthCalibration2024::AddDepthCTD: The number of values in the CTD list is not equal to the number of depth values." << endl;
      return false;
    }
  }
  // Now make sure the values for the depth start with 0.0.
  if( * std::min_element(depthvec.begin(), depthvec.end()) != 0.0){
      cout << "MModuleDepthCalibration2024::AddDepthCTD: The minimum depth is not zero." << endl;
      return false;
  }

  CTDMap[DetID] = ctdarr;
  DepthGrid[DetID] = depthvec;
  m_Thicknesses[DetID] = * std::max_element(depthvec.begin(), depthvec.end());
  cout << "MModuleDepthCalibration2024::AddDepthCTD: The thickness of detector " << DetID << " is " << m_Thicknesses[DetID] << endl;
  return true;
}


vector<double>* MModuleDepthCalibration2024::GetCTD(int DetID, int Grade)
{
  // Retrieves the appropriate CTD vector given the Detector ID and Event Grade passed

  if( !m_SplinesFileIsLoaded ){
    cout << "MModuleDepthCalibration2024::GetCTD: cannot return Depth to CTD relation because the file was not loaded." << endl;
    return nullptr;
  }
  // If there is a CTD array for the given detector, return it.
  // If the Grade is larger than the number of CTD vectors stored, then just return Grade 0 vector.
  if( m_CTDMap.count(DetID) > 0 ){
    if ( m_CTDMap[DetID].size() > Grade) {
      return &(m_CTDMap[DetID][Grade]);
    }
    else {
      cout << "MModuleDepthCalibration2024::GetCTD: No CTD map is loaded for Grade " << Grade << ". Returning Grade 0 CTD." << endl;
      return &(m_CTDMap[DetID][0]);
    }
  } else {
    cout << "MModuleDepthCalibration2024::GetCTD: No CTD map is loaded for Det " << DetID << "." << endl;
    return nullptr;
  }
}

vector<double>* MModuleDepthCalibration2024::GetDepth(int DetID)
{
  // Retrieves the appropriate CTD vector given the Detector ID and Event Grade passed

  if( !m_SplinesFileIsLoaded ){
    cout << "MModuleDepthCalibration2024::GetDepth: cannot return Depth grid because the file was not loaded." << endl;
    return nullptr;
  }

  // If there is a CTD array for the given detector, return it.
  // If the Grade is larger than the number of CTD vectors stored, then just return Grade 0 vector.
  if( m_DepthGrid.count(DetID) > 0 ){
    return &m_DepthGrid[DetID];
    } else {
      cout << "MModuleDepthCalibration2024::GetDepth: No Depth grid is loaded for Det " << DetID << "." << endl;
      return nullptr;
  }
} 

void MModuleDepthCalibration2024::ShowOptionsGUI()
{
  // Show the options GUI - or do nothing
  MGUIOptionsDepthCalibration2024* Options = new MGUIOptionsDepthCalibration2024(this);
  Options->Create();
  gClient->WaitForUnmap(Options);
}


bool MModuleDepthCalibration2024::ReadXmlConfiguration(MXmlNode* Node)
{
  //! Read the configuration data from an XML node

  MXmlNode* CoeffsFileNameNode = Node->GetNode("CoeffsFileName");
  if (CoeffsFileNameNode != 0) {
  m_CoeffsFile = CoeffsFileNameNode->GetValue();
  }

  MXmlNode* SplinesFileNameNode = Node->GetNode("SplinesFileName");
  if (SplinesFileNameNode != 0) {
  m_SplinesFile = SplinesFileNameNode->GetValue();
  }


  return true;
}


/////////////////////////////////////////////////////////////////////////////////

MXmlNode* MModuleDepthCalibration2024::CreateXmlConfiguration()
{
  //! Create an XML node tree from the configuration

  MXmlNode* Node = new MXmlNode(0,m_XmlTag);
  new MXmlNode(Node, "CoeffsFileName", m_CoeffsFile);
  new MXmlNode(Node, "SplinesFileName", m_SplinesFile);

  return Node;
}

void MModuleDepthCalibration2024::Finalize()
{

  MModule::Finalize();
  cout << "###################" << endl;
  cout << "AWL depth cal stats" << endl;
  cout << "###################" << endl;
  cout << "Good hits: " << m_NoError << endl;
  cout << "Number of hits missing calibration coefficients: " << m_Error1 << endl;
  cout << "Number of hits too far outside of detector: " << m_Error2 << endl;
  cout << "Number of hits missing timing information: " << m_Error3 << endl;
  cout << "Number of hits with strips hit multiple times: " << m_Error4 << endl;
  cout << "Number of hits with too many strip hits: " << m_ErrorSH << endl;
  /*
  TFile* rootF = new TFile("EHist.root","recreate");
  rootF->WriteTObject( EHist );
  rootF->Close();
  */

}



// MModuleDepthCalibration2024.cxx: the end...
////////////////////////////////////////////////////////////////////////////////