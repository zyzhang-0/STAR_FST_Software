#include "./FstTracking.h"

#include <iostream>
#include <fstream>
#include <cmath>

#include <TFile.h>
#include <TChain.h>
#include <TGraph.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TMath.h>
#include <TVector3.h>

using namespace std;

ClassImp(FstTracking)

//------------------------------------------
FstTracking::FstTracking() : mList("../../list/FST/FstPed_HV140.list"), mOutPutFile("./FstPed_HV140.root")
{
  cout << "FstTracking::FstTracking() -------- Constructor!  --------" << endl;
  mHome = getenv("HOME");
}

FstTracking::~FstTracking()
{
  cout << "FstTracking::~FstTracking() -------- Release Memory!  --------" << endl;
}
//------------------------------------------
int FstTracking::Init()
{
  cout << "FstTracking::Init => " << endl;
  File_mOutPut = new TFile(mOutPutFile.c_str(),"RECREATE");

  bool isInPut = initChain(); // initialize input data/ped TChain;
  bool isTracking_Hits = initTrackingQA_Hits(); // initialize tracking with Hits
  initTrackAngle();
  initTracking_Hits();
  initTracking_Clusters();
  initEfficiency_Hits();
  initEfficiency_Clusters();

  if(!isInPut) 
  {
    cout << "Failed to initialize input data!" << endl;
    return -1;
  }
  if(!isTracking_Hits) 
  {
    cout << "Failed to initialize Hits based Tracking!" << endl;
    return -1;
  }

  return 1;
}

int FstTracking::Make()
{
  cout << "FstTracking::Make => " << endl;

  long NumOfEvents = (long)mChainInPut->GetEntries();
  // if(NumOfEvents > 1000) NumOfEvents = 1000;
  // NumOfEvents = 1000;
  mChainInPut->GetEntry(0);

  std::vector<FstRawHit *> rawHitVec;
  std::vector<FstCluster *> clusterVec;
  std::vector<FstTrack *> trackHitsVec;
  std::vector<FstTrack *> trackClustersVec;
  for(int i_event = 0; i_event < NumOfEvents; ++i_event)
  {
    if(i_event%1000==0) cout << "processing events:  " << i_event << "/" << NumOfEvents << endl;
    mChainInPut->GetEntry(i_event);

    rawHitVec.clear(); // clear the container for hits
    for(int i_hit = 0; i_hit < mFstEvent_InPut->getNumRawHits(); ++i_hit)
    { // get Hits info
      FstRawHit *fstRawHit = mFstEvent_InPut->getRawHit(i_hit);
      rawHitVec.push_back(fstRawHit);
    }
    fillTrackingQA_Hits(rawHitVec); // do tracking based on the first hit of each layer
    fillTrackAngle(mFstEvent_InPut);

    clusterVec.clear(); // clear the container for clusters
    for(int i_cluster = 0; i_cluster < mFstEvent_InPut->getNumClusters(); ++i_cluster)
    { // get Clusters info
      FstCluster *fstCluster = mFstEvent_InPut->getCluster(i_cluster);
      clusterVec.push_back(fstCluster);
    }

    trackHitsVec.clear(); // clear the container for clusters
    trackClustersVec.clear(); // clear the container for clusters
    for(int i_track = 0; i_track < mFstEvent_InPut->getNumTracks(); ++i_track)
    { // get Tracks info
      FstTrack *fstTrack = mFstEvent_InPut->getTrack(i_track);
      if(fstTrack->getTrackType() == 0) // track reconstructed with hits
      {
	trackHitsVec.push_back(fstTrack);
      }
      if(fstTrack->getTrackType() == 1) // track reconstructed with clusters
      {
	trackClustersVec.push_back(fstTrack);
      }
    }

    calResolution_Hits(mFstEvent_InPut);
    calResolution_SimpleClusters(mFstEvent_InPut);
    calResolution_ScanClusters(mFstEvent_InPut);

    calEfficiency_Hits(mFstEvent_InPut);
    calEfficiency_SimpleClusters(mFstEvent_InPut);
    calEfficiency_ScanClusters(mFstEvent_InPut);
  }

  cout << "processed events:  " << NumOfEvents << "/" << NumOfEvents << endl;


  return 1;
}

int FstTracking::Finish()
{
  cout << "FstTracking::Finish => " << endl;
  writeTrackingQA_Hits();
  writeTrackAngle();
  writeTracking_Hits();
  writeTracking_Clusters();
  writeEfficiency_Hits();
  writeEfficiency_Clusters();

  return 1;
}

// init Input TChain
bool FstTracking::initChain()
{
  cout << "FstTracking::initChain -> " << endl;

  mChainInPut = new TChain("mTree_FstEvent");

  if (!mList.empty())   // if input file is ok
  {
    cout << "Open input probability file list" << endl;
    ifstream in(mList.c_str());  // input stream
    if(in)
    {
      cout << "input file probability list is ok" << endl;
      char str[255];       // char array for each file name
      Long64_t entries_save = 0;
      while(in)
      {
	in.getline(str,255);  // take the lines of the file list
	if(str[0] != 0)
	{
	  string addfile;
	  addfile = str;
	  mChainInPut->AddFile(addfile.c_str(),-1,"mTree_FstEvent");
	  Long64_t file_entries = mChainInPut->GetEntries();
	  cout << "File added to data chain: " << addfile.c_str() << " with " << (file_entries-entries_save) << " entries" << endl;
	  entries_save = file_entries;
	}
      }
    }
    else
    {
      cout << "WARNING: input probability file input is problemtic" << endl;
      return false;
    }
  }

  mFstEvent_InPut = new FstEvent();
  mChainInPut->SetBranchAddress("FstEvent",&mFstEvent_InPut);

  long NumOfEvents = (long)mChainInPut->GetEntries();
  cout << "total number of events: " << NumOfEvents << endl;

  return true;
}
// init Input TChain

//--------------Track QA with Hits---------------------
bool FstTracking::initTrackingQA_Hits()
{
  const string CorrNameXR[4] = {"ist1x_ist3x","ist1x_fstR","ist3x_fstR","ist1x_ist3x_fstR"};
  const string CorrNameYPhi[4] = {"ist1y_ist3y","ist1y_fstPhi","ist3y_fstPhi","ist1y_ist3y_fstPhi"};
  for(int i_corr = 0; i_corr < 4; ++i_corr)
  { // for QA
    string HistName = Form("h_mHitsCorrXR_%s",CorrNameXR[i_corr].c_str());
    if(i_corr == 0) h_mHitsCorrXR[i_corr] = new TH2F(HistName.c_str(),HistName.c_str(),FST::noColumns,-0.5,FST::noColumns-0.5,FST::noColumns,-0.5,FST::noColumns-0.5);
    else h_mHitsCorrXR[i_corr] = new TH2F(HistName.c_str(),HistName.c_str(),FST::noColumns,-0.5,FST::noColumns-0.5,FST::numRStrip,-0.5,FST::numRStrip-0.5);

    HistName = Form("h_mHitsCorrYPhi_%s",CorrNameYPhi[i_corr].c_str());
    if(i_corr == 0) h_mHitsCorrYPhi[i_corr] = new TH2F(HistName.c_str(),HistName.c_str(),FST::noRows,-0.5,FST::noRows-0.5,FST::noRows,-0.5,FST::noRows-0.5);
    else h_mHitsCorrYPhi[i_corr] = new TH2F(HistName.c_str(),HistName.c_str(),FST::noRows,-0.5,FST::noRows-0.5,FST::numPhiSeg,-0.5,FST::numPhiSeg-0.5);
  }

  h_mXResidual_Hits_Before = new TH1F("h_mXResidual_Hits_Before","h_mXResidual_Hits_Before",100,-500.0,500.0);
  h_mYResidual_Hits_Before = new TH1F("h_mYResidual_Hits_Before","h_mYResidual_Hits_Before",100,-50.0,50.0);
  h_mXResidual_Hits = new TH1F("h_mXResidual_Hits","h_mXResidual_Hits",100,-80.0,80.0);
  h_mYResidual_Hits = new TH1F("h_mYResidual_Hits","h_mYResidual_Hits",100,-16.0,16.0);
  h_mRResidual_Hits = new TH1F("h_mRResidual_Hits","h_mRResidual_Hits",100,-80.0,80.0);
  h_mPhiResidual_Hits = new TH1F("h_mPhiResidual_Hits","h_mPhiResidual_Hits",100,-0.05,0.05);

  return true;
}

void FstTracking::fillTrackingQA_Hits(std::vector<FstRawHit *> rawHitVec_orig)
{
  int numOfHits[4]; // 0 for fst | 1-3 for ist
  std::vector<FstRawHit *> rawHitVec[4];
  for(int i_layer = 0; i_layer < 4; ++i_layer)
  {
    numOfHits[i_layer] = 0;
    rawHitVec[i_layer].clear();
  }

  for(int i_hit = 0; i_hit < rawHitVec_orig.size(); ++i_hit)
  { // set temp ist hit container for each layer
    int layer = rawHitVec_orig[i_hit]->getLayer();
    numOfHits[layer]++;
    rawHitVec[layer].push_back(rawHitVec_orig[i_hit]);
  }

  if(numOfHits[0] > 0 && numOfHits[1] > 0 && numOfHits[3] > 0)
  { // only do tracking when at least 1 hit is found in fst & ist1 & ist3

    // QA Histograms
    h_mHitsCorrXR[0]->Fill(rawHitVec[1][0]->getColumn(),rawHitVec[3][0]->getColumn());
    h_mHitsCorrXR[1]->Fill(rawHitVec[1][0]->getColumn(),rawHitVec[0][0]->getColumn());
    h_mHitsCorrXR[2]->Fill(rawHitVec[3][0]->getColumn(),rawHitVec[0][0]->getColumn());
    h_mHitsCorrXR[3]->Fill(5.25/2*rawHitVec[1][0]->getColumn()-3.25/2*rawHitVec[3][0]->getColumn(),rawHitVec[0][0]->getColumn());

    h_mHitsCorrYPhi[0]->Fill(rawHitVec[1][0]->getRow(),rawHitVec[3][0]->getRow());
    h_mHitsCorrYPhi[1]->Fill(rawHitVec[1][0]->getRow(),rawHitVec[0][0]->getRow());
    h_mHitsCorrYPhi[2]->Fill(rawHitVec[3][0]->getRow(),rawHitVec[0][0]->getRow());
    h_mHitsCorrYPhi[3]->Fill(5.25/2*rawHitVec[1][0]->getRow()-3.25/2*rawHitVec[3][0]->getRow()+20,rawHitVec[0][0]->getRow());

    double r_fst   = rawHitVec[0][0]->getPosX();
    double phi_fst = rawHitVec[0][0]->getPosY();
    double x0_fst  = r_fst*TMath::Cos(phi_fst); // x = r*cos(phi)
    double y0_fst  = r_fst*TMath::Sin(phi_fst); // y = r*sin(phi)
    double z0_fst  = FST::pitchLayer03;

    double x1_ist = rawHitVec[1][0]->getPosX();
    double y1_ist = rawHitVec[1][0]->getPosY();
    double z1_ist = FST::pitchLayer12 + FST::pitchLayer23;
    double x1_corr_IST = x1_ist*TMath::Cos(FST::phi_rot_ist1) + y1_ist*TMath::Sin(FST::phi_rot_ist1) + FST::x1_shift; // aligned to IST2
    double y1_corr_IST = y1_ist*TMath::Cos(FST::phi_rot_ist1) - x1_ist*TMath::Sin(FST::phi_rot_ist1) + FST::y1_shift;
    double x1_corr_FST = x1_corr_IST*TMath::Cos(FST::phi_rot_ist2) + y1_corr_IST*TMath::Sin(FST::phi_rot_ist2) + FST::x2_shift; // aligned to FST
    double y1_corr_FST = y1_corr_IST*TMath::Cos(FST::phi_rot_ist2) - x1_corr_IST*TMath::Sin(FST::phi_rot_ist2) + FST::y2_shift;

    double x3_ist = rawHitVec[3][0]->getPosX();
    double y3_ist = rawHitVec[3][0]->getPosY();
    double z3_ist = 0.0;
    double x3_corr_IST = x3_ist*TMath::Cos(FST::phi_rot_ist3) + y3_ist*TMath::Sin(FST::phi_rot_ist3) + FST::x3_shift; // aligned to IST2
    double y3_corr_IST = y3_ist*TMath::Cos(FST::phi_rot_ist3) - x3_ist*TMath::Sin(FST::phi_rot_ist3) + FST::y3_shift;
    double x3_corr_FST = x3_corr_IST*TMath::Cos(FST::phi_rot_ist2) + y3_corr_IST*TMath::Sin(FST::phi_rot_ist2) + FST::x2_shift; // aligned to FST
    double y3_corr_FST = y3_corr_IST*TMath::Cos(FST::phi_rot_ist2) - x3_corr_IST*TMath::Sin(FST::phi_rot_ist2) + FST::y2_shift;

    double x0_proj = x3_corr_IST + (x1_corr_IST-x3_corr_IST)*z0_fst/z1_ist; // before aligned to FST
    double y0_proj = y3_corr_IST + (y1_corr_IST-y3_corr_IST)*z0_fst/z1_ist;

    double x0_corr = x3_corr_FST + (x1_corr_FST-x3_corr_FST)*z0_fst/z1_ist; // after aligned to FST
    double y0_corr = y3_corr_FST + (y1_corr_FST-y3_corr_FST)*z0_fst/z1_ist;

    if(abs(rawHitVec[1][0]->getRow()-rawHitVec[3][0]->getRow()) < 17)
    {
      h_mXResidual_Hits_Before->Fill(x0_fst-x0_proj);
      h_mYResidual_Hits_Before->Fill(y0_fst-y0_proj);

      double xResidual = x0_fst-x0_corr;
      double yResidual = y0_fst-y0_corr;
      h_mXResidual_Hits->Fill(xResidual);
      h_mYResidual_Hits->Fill(yResidual);

      double r_corr = TMath::Sqrt(x0_corr*x0_corr+y0_corr*y0_corr);
      double phi_corr = TMath::ATan2(y0_corr,x0_corr);
      double rResidual = r_fst-r_corr;
      double phiResidual = phi_fst-phi_corr;
      h_mRResidual_Hits->Fill(rResidual);
      h_mPhiResidual_Hits->Fill(phiResidual);
    }
  }
}

void FstTracking::writeTrackingQA_Hits()
{
  for(int i_corr = 0; i_corr < 4; ++i_corr)
  {
    h_mHitsCorrXR[i_corr]->Write();
    h_mHitsCorrYPhi[i_corr]->Write();
  }
  h_mXResidual_Hits_Before->Write();
  h_mYResidual_Hits_Before->Write();
  h_mXResidual_Hits->Write();
  h_mYResidual_Hits->Write();
  h_mRResidual_Hits->Write();
  h_mPhiResidual_Hits->Write();
}
//--------------Track QA with Hits---------------------

//--------------Incident Angle with IST Simple Clusters---------------------
void FstTracking::initTrackAngle()
{
  h_mClustersTrackAngle = new TH1F("h_mClustersTrackAngle","h_mClustersTrackAngle",25,0.0,0.5*TMath::Pi());
}

void FstTracking::fillTrackAngle(FstEvent *fstEvent)
{
  std::vector<FstTrack *> trackClusterVec;
  trackClusterVec.clear(); // clear the container for clusters
  for(int i_track = 0; i_track < fstEvent->getNumTracks(); ++i_track)
  { // get Tracks info
    FstTrack *fstTrack = fstEvent->getTrack(i_track);
    if(fstTrack->getTrackType() == 1) // track reconstructed with clusters
    {
      trackClusterVec.push_back(fstTrack);
    }
  }

  if(trackClusterVec.size() == 1)
  {
    for(int i_track = 0; i_track < trackClusterVec.size(); ++i_track)
    {
      // original hit postion on IST1 & IST3
      TVector3 posOrig_ist1 = trackClusterVec[i_track]->getPosOrig(1);
      double y1Orig_ist = posOrig_ist1.Y(); 
      TVector3 posOrig_ist3 = trackClusterVec[i_track]->getPosOrig(3);
      double y3Orig_ist = posOrig_ist3.Y();

      if( abs(y1Orig_ist-y3Orig_ist) < 17.0*FST::pitchRow )
      {
	// aligned hit position on IST1 & IST3
	TVector3 pos_ist1 = trackClusterVec[i_track]->getPosition(1);
	TVector3 pos_ist3 = trackClusterVec[i_track]->getPosition(3);
	TVector3 istTrack = pos_ist1-pos_ist3;
	TVector3 normVec;
	normVec.SetXYZ(0.0,0.0,pos_ist1.Z()-pos_ist3.Z());
	double pAngle = istTrack.Angle(normVec);
	// cout << "pos_ist1.X = " << pos_ist1.X() << ", pos_ist1.Y = " << pos_ist1.Y() << ", pos_ist1.Z = " << pos_ist1.Z() << endl;
	// cout << "pos_ist3.X = " << pos_ist3.X() << ", pos_ist3.Y = " << pos_ist3.Y() << ", pos_ist3.Z = " << pos_ist3.Z() << endl;
	// cout << "pos_ist.XDiff = " << pos_ist1.X()-pos_ist3.X() << ", pos_ist.YDiff = " << pos_ist1.Y()-pos_ist3.Y() << ", pos_ist.ZDiff = " << pos_ist1.Z()-pos_ist3.Z() << endl;
	// cout << "istTrack.X = " << istTrack.X() << ", istTrack.Y = " << istTrack.Y() << ", istTrack.Z = " << istTrack.Z() << ", pAngle = " << pAngle << endl;
	// cout << endl;

	h_mClustersTrackAngle->Fill(pAngle);
      }
    }
  }
}

void FstTracking::writeTrackAngle()
{
  h_mClustersTrackAngle->Write();

}
//--------------Incident Angle with IST Simple Clusters---------------------

//--------------Track Resolution with Hits---------------------
void FstTracking::initTracking_Hits()
{
  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    string HistName;
    HistName = Form("h_mHitsTrackFstResX_Sensor%d",i_sensor);
    h_mHitsTrackFstResX[i_sensor]    = new TH1F(HistName.c_str(),HistName.c_str(),100,-80.0,80.0);
    HistName = Form("h_mHitsTrackFstResY_Sensor%d",i_sensor);
    h_mHitsTrackFstResY[i_sensor]    = new TH1F(HistName.c_str(),HistName.c_str(),100,-16.0,16.0);
    HistName = Form("h_mHitsTrackFstResXY_Sensor%d",i_sensor);
    h_mHitsTrackFstResXY[i_sensor]   = new TH2F(HistName.c_str(),HistName.c_str(),100,-80.0,80.0,100,-16.0,16.0);

    HistName = Form("h_mHitsTrackFstResR_Sensor%d",i_sensor);
    h_mHitsTrackFstResR[i_sensor]    = new TH1F(HistName.c_str(),HistName.c_str(),100,-80.0,80.0);
    HistName = Form("h_mHitsTrackFstResPhi_Sensor%d",i_sensor);
    h_mHitsTrackFstResPhi[i_sensor]  = new TH1F(HistName.c_str(),HistName.c_str(),100,-0.05,0.05);
    HistName = Form("h_mHitsTrackFstResRPhi_Sensor%d",i_sensor);
    h_mHitsTrackFstResRPhi[i_sensor] = new TH2F(HistName.c_str(),HistName.c_str(),100,-80.0,80.0,100,-0.05,0.05);
  }
}

void FstTracking::calResolution_Hits(FstEvent *fstEvent)
{
  std::vector<FstTrack *> trackHitVec;
  trackHitVec.clear(); // clear the container for clusters
  for(int i_track = 0; i_track < mFstEvent_InPut->getNumTracks(); ++i_track)
  { // get Tracks info
    FstTrack *fstTrack = mFstEvent_InPut->getTrack(i_track);
    if(fstTrack->getTrackType() == 0) // track reconstructed with hits
    {
      trackHitVec.push_back(fstTrack);
    }
  }

  std::vector<FstRawHit *> fstRawHitVec;
  fstRawHitVec.clear(); // clear the container for Raw Hits
  for(int i_hit = 0; i_hit < fstEvent->getNumRawHits(); ++i_hit)
  { // get Hits info
    FstRawHit *fstRawHit = fstEvent->getRawHit(i_hit);
    if(fstRawHit->getLayer() == 0 && fstRawHit->getIsHit())
    { // only use Hits
      fstRawHitVec.push_back(fstRawHit);
    }
  }
  int numOfFstHits = fstRawHitVec.size();

  for(int i_track = 0; i_track < trackHitVec.size(); ++i_track)
  {
    TVector3 proj_fst = trackHitVec[i_track]->getProjection(0);
    double r_proj     = proj_fst.X();
    double phi_proj   = proj_fst.Y();
    double x0_proj    = r_proj*TMath::Cos(phi_proj);
    double y0_proj    = r_proj*TMath::Sin(phi_proj);;

    if(numOfFstHits > 0)
    {
      double minDistance_FST = 1000.0;
      int minHitId_FST = -1;
      double preDistance_FST = minDistance_FST;

      for(int i_hit = 0; i_hit < numOfFstHits; ++i_hit)
      { // fill residual histograms with the hit of minimum distance
	double r_fst = fstRawHitVec[i_hit]->getPosX(); // r for fst
	double phi_fst = fstRawHitVec[i_hit]->getPosY(); // phi for fst
	double x0_fst = r_fst*TMath::Cos(phi_fst);
	double y0_fst = r_fst*TMath::Sin(phi_fst);

	double distance_FST = abs(r_fst-r_proj)/FST::pitchR + abs(phi_fst-phi_proj)/FST::pitchPhi;
	if(distance_FST < preDistance_FST)
	{ 
	  minDistance_FST = distance_FST;
	  minHitId_FST = i_hit;
	  preDistance_FST = minDistance_FST;
	}
      }

      if(minHitId_FST > -1)
      {
	int sensorId = fstRawHitVec[minHitId_FST]->getSensor();
	double r_fst = fstRawHitVec[minHitId_FST]->getPosX(); // r for fst
	double phi_fst = fstRawHitVec[minHitId_FST]->getPosY(); // phi for fst
	double x0_fst = r_fst*TMath::Cos(phi_fst);
	double y0_fst = r_fst*TMath::Sin(phi_fst);

	h_mHitsTrackFstResX[sensorId]->Fill(x0_fst-x0_proj);
	h_mHitsTrackFstResY[sensorId]->Fill(y0_fst-y0_proj);
	h_mHitsTrackFstResXY[sensorId]->Fill(x0_fst-x0_proj,y0_fst-y0_proj);

	h_mHitsTrackFstResR[sensorId]->Fill(r_fst-r_proj);
	h_mHitsTrackFstResPhi[sensorId]->Fill(phi_fst-phi_proj);
	h_mHitsTrackFstResRPhi[sensorId]->Fill(r_fst-r_proj,phi_fst-phi_proj);
      }
    }
  }
}

void FstTracking::writeTracking_Hits()
{
  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    h_mHitsTrackFstResX[i_sensor]->Write();
    h_mHitsTrackFstResY[i_sensor]->Write();
    h_mHitsTrackFstResXY[i_sensor]->Write();

    h_mHitsTrackFstResR[i_sensor]->Write();
    h_mHitsTrackFstResPhi[i_sensor]->Write();
    h_mHitsTrackFstResRPhi[i_sensor]->Write();
  }
}
//--------------Track Resolution with Hits---------------------

//--------------Track Resolution with Clusters---------------------
void FstTracking::initTracking_Clusters()
{
  // IST simple clusters
  h_mSimpleClustersTrackIstResX_2Layer    = new TH1F("h_mSimpleClustersTrackIstResX_2Layer","h_mSimpleClustersTrackIstResX_2Layer",15,-20.0,20.0);
  h_mSimpleClustersTrackIstResY_2Layer    = new TH1F("h_mSimpleClustersTrackIstResY_2Layer","h_mSimpleClustersTrackIstResY_2Layer",100,-5.0,5.0);
  h_mSimpleClustersTrackIstResXY_2Layer   = new TH2F("h_mSimpleClustersTrackIstResXY_2Layer","h_mSimpleClustersTrackIstResXY_2Layer",15,-20.0,20.0,100,-5.0,5.0);

  h_mSimpleClustersTrackIstResX_3Layer    = new TH1F("h_mSimpleClustersTrackIstResX_3Layer","h_mSimpleClustersTrackIstResX_3Layer",15,-20.0,20.0);
  h_mSimpleClustersTrackIstResY_3Layer    = new TH1F("h_mSimpleClustersTrackIstResY_3Layer","h_mSimpleClustersTrackIstResY_3Layer",100,-5.0,5.0);
  h_mSimpleClustersTrackIstResXY_3Layer   = new TH2F("h_mSimpleClustersTrackIstResXY_3Layer","h_mSimpleClustersTrackIstResXY_3Layer",15,-20.0,20.0,100,-5.0,5.0);

  // FST simple clusters
  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    string HistName;
    HistName = Form("h_mSimpleClustersTrackFstResX_2Layer_Sensor%d",i_sensor);
    h_mSimpleClustersTrackFstResX_2Layer[i_sensor]    = new TH1F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0);
    HistName = Form("h_mSimpleClustersTrackFstResY_2Layer_Sensor%d",i_sensor);
    h_mSimpleClustersTrackFstResY_2Layer[i_sensor]    = new TH1F(HistName.c_str(),HistName.c_str(),100,-16.0,16.0);
    HistName = Form("h_mSimpleClustersTrackFstResXY_2Layer_Sensor%d",i_sensor);
    h_mSimpleClustersTrackFstResXY_2Layer[i_sensor]   = new TH2F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0,100,-16.0,16.0);

    HistName = Form("h_mSimpleClustersTrackFstResR_2Layer_Sensor%d",i_sensor);
    h_mSimpleClustersTrackFstResR_2Layer[i_sensor]    = new TH1F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0);
    HistName = Form("h_mSimpleClustersTrackFstResPhi_2Layer_Sensor%d",i_sensor);
    h_mSimpleClustersTrackFstResPhi_2Layer[i_sensor]  = new TH1F(HistName.c_str(),HistName.c_str(),200,-0.10,0.10);
    HistName = Form("h_mSimpleClustersTrackFstResRPhi_2Layer_Sensor%d",i_sensor);
    h_mSimpleClustersTrackFstResRPhi_2Layer[i_sensor] = new TH2F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0,200,-0.10,0.10);

    for(int i_rstrip = 0; i_rstrip < FST::mFstNumRstripPerSensor; ++i_rstrip)
    {
      HistName = Form("h_mSimpleClustersTrackFstResX_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // X Residual
      if(i_sensor > 0) HistName = Form("h_mSimpleClustersTrackFstResX_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mSimpleClustersTrackFstResX_2Layer_Rstrips[i_sensor][i_rstrip]  = new TH1F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0);
      HistName = Form("h_mSimpleClustersTrackFstResY_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // Y Residual
      if(i_sensor > 0) HistName = Form("h_mSimpleClustersTrackFstResY_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mSimpleClustersTrackFstResY_2Layer_Rstrips[i_sensor][i_rstrip]  = new TH1F(HistName.c_str(),HistName.c_str(),100,-16.0,16.0);
      HistName = Form("h_mSimpleClustersTrackFstResXY_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // XY Residual
      if(i_sensor > 0) HistName = Form("h_mSimpleClustersTrackFstResXY_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mSimpleClustersTrackFstResXY_2Layer_Rstrips[i_sensor][i_rstrip] = new TH2F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0,100,-16.0,16.0);

      HistName = Form("h_mSimpleClustersTrackFstResR_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // R Residual
      if(i_sensor > 0) HistName = Form("h_mSimpleClustersTrackFstResR_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mSimpleClustersTrackFstResR_2Layer_Rstrips[i_sensor][i_rstrip]    = new TH1F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0);
      HistName = Form("h_mSimpleClustersTrackFstResPhi_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // Phi Residual
      if(i_sensor > 0) HistName = Form("h_mSimpleClustersTrackFstResPhi_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mSimpleClustersTrackFstResPhi_2Layer_Rstrips[i_sensor][i_rstrip]  = new TH1F(HistName.c_str(),HistName.c_str(),200,-0.10,0.10);
      HistName = Form("h_mSimpleClustersTrackFstResRPhi_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // RPhi Residual
      if(i_sensor > 0) HistName = Form("h_mSimpleClustersTrackFstResRPhi_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mSimpleClustersTrackFstResRPhi_2Layer_Rstrips[i_sensor][i_rstrip] = new TH2F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0,200,-0.10,0.10);
    }
  }

  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    string HistName;
    HistName = Form("h_mSimpleClustersTrackFstResX_3Layer_Sensor%d",i_sensor);
    h_mSimpleClustersTrackFstResX_3Layer[i_sensor]    = new TH1F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0);
    HistName = Form("h_mSimpleClustersTrackFstResY_3Layer_Sensor%d",i_sensor);
    h_mSimpleClustersTrackFstResY_3Layer[i_sensor]    = new TH1F(HistName.c_str(),HistName.c_str(),100,-16.0,16.0);
    HistName = Form("h_mSimpleClustersTrackFstResXY_3Layer_Sensor%d",i_sensor);
    h_mSimpleClustersTrackFstResXY_3Layer[i_sensor]   = new TH2F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0,100,-16.0,16.0);

    HistName = Form("h_mSimpleClustersTrackFstResR_3Layer_Sensor%d",i_sensor);
    h_mSimpleClustersTrackFstResR_3Layer[i_sensor]    = new TH1F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0);
    HistName = Form("h_mSimpleClustersTrackFstResPhi_3Layer_Sensor%d",i_sensor);
    h_mSimpleClustersTrackFstResPhi_3Layer[i_sensor]  = new TH1F(HistName.c_str(),HistName.c_str(),200,-0.10,0.10);
    HistName = Form("h_mSimpleClustersTrackFstResRPhi_3Layer_Sensor%d",i_sensor);
    h_mSimpleClustersTrackFstResRPhi_3Layer[i_sensor] = new TH2F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0,200,-0.10,0.10);

    for(int i_rstrip = 0; i_rstrip < FST::mFstNumRstripPerSensor; ++i_rstrip)
    {
      HistName = Form("h_mSimpleClustersTrackFstResX_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // X Residual
      if(i_sensor > 0) HistName = Form("h_mSimpleClustersTrackFstResX_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mSimpleClustersTrackFstResX_3Layer_Rstrips[i_sensor][i_rstrip]  = new TH1F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0);
      HistName = Form("h_mSimpleClustersTrackFstResY_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // Y Residual
      if(i_sensor > 0) HistName = Form("h_mSimpleClustersTrackFstResY_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mSimpleClustersTrackFstResY_3Layer_Rstrips[i_sensor][i_rstrip]  = new TH1F(HistName.c_str(),HistName.c_str(),100,-16.0,16.0);
      HistName = Form("h_mSimpleClustersTrackFstResXY_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // XY Residual
      if(i_sensor > 0) HistName = Form("h_mSimpleClustersTrackFstResXY_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mSimpleClustersTrackFstResXY_3Layer_Rstrips[i_sensor][i_rstrip] = new TH2F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0,100,-16.0,16.0);

      HistName = Form("h_mSimpleClustersTrackFstResR_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // R Residual
      if(i_sensor > 0) HistName = Form("h_mSimpleClustersTrackFstResR_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mSimpleClustersTrackFstResR_3Layer_Rstrips[i_sensor][i_rstrip]    = new TH1F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0);
      HistName = Form("h_mSimpleClustersTrackFstResPhi_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // Phi Residual
      if(i_sensor > 0) HistName = Form("h_mSimpleClustersTrackFstResPhi_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mSimpleClustersTrackFstResPhi_3Layer_Rstrips[i_sensor][i_rstrip]  = new TH1F(HistName.c_str(),HistName.c_str(),200,-0.10,0.10);
      HistName = Form("h_mSimpleClustersTrackFstResRPhi_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // RPhi Residual
      if(i_sensor > 0) HistName = Form("h_mSimpleClustersTrackFstResRPhi_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mSimpleClustersTrackFstResRPhi_3Layer_Rstrips[i_sensor][i_rstrip] = new TH2F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0,200,-0.10,0.10);
    }
  }
 
  // FST scan clusters
  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    string HistName;
    HistName = Form("h_mScanClustersTrackFstResX_2Layer_Sensor%d",i_sensor);
    h_mScanClustersTrackFstResX_2Layer[i_sensor]    = new TH1F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0);
    HistName = Form("h_mScanClustersTrackFstResY_2Layer_Sensor%d",i_sensor);
    h_mScanClustersTrackFstResY_2Layer[i_sensor]    = new TH1F(HistName.c_str(),HistName.c_str(),100,-16.0,16.0);
    HistName = Form("h_mScanClustersTrackFstResXY_2Layer_Sensor%d",i_sensor);
    h_mScanClustersTrackFstResXY_2Layer[i_sensor]   = new TH2F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0,100,-16.0,16.0);

    HistName = Form("h_mScanClustersTrackFstResR_2Layer_Sensor%d",i_sensor);
    h_mScanClustersTrackFstResR_2Layer[i_sensor]    = new TH1F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0);
    HistName = Form("h_mScanClustersTrackFstResPhi_2Layer_Sensor%d",i_sensor);
    h_mScanClustersTrackFstResPhi_2Layer[i_sensor]  = new TH1F(HistName.c_str(),HistName.c_str(),200,-0.10,0.10);
    HistName = Form("h_mScanClustersTrackFstResRPhi_2Layer_Sensor%d",i_sensor);
    h_mScanClustersTrackFstResRPhi_2Layer[i_sensor] = new TH2F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0,200,-0.10,0.10);

    for(int i_rstrip = 0; i_rstrip < FST::mFstNumRstripPerSensor; ++i_rstrip)
    {
      HistName = Form("h_mScanClustersTrackFstResX_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // X Residual
      if(i_sensor > 0) HistName = Form("h_mScanClustersTrackFstResX_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mScanClustersTrackFstResX_2Layer_Rstrips[i_sensor][i_rstrip]  = new TH1F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0);
      HistName = Form("h_mScanClustersTrackFstResY_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // Y Residual
      if(i_sensor > 0) HistName = Form("h_mScanClustersTrackFstResY_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mScanClustersTrackFstResY_2Layer_Rstrips[i_sensor][i_rstrip]  = new TH1F(HistName.c_str(),HistName.c_str(),100,-16.0,16.0);
      HistName = Form("h_mScanClustersTrackFstResXY_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // XY Residual
      if(i_sensor > 0) HistName = Form("h_mScanClustersTrackFstResXY_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mScanClustersTrackFstResXY_2Layer_Rstrips[i_sensor][i_rstrip] = new TH2F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0,100,-16.0,16.0);

      HistName = Form("h_mScanClustersTrackFstResR_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // R Residual
      if(i_sensor > 0) HistName = Form("h_mScanClustersTrackFstResR_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mScanClustersTrackFstResR_2Layer_Rstrips[i_sensor][i_rstrip]    = new TH1F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0);
      HistName = Form("h_mScanClustersTrackFstResPhi_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // Phi Residual
      if(i_sensor > 0) HistName = Form("h_mScanClustersTrackFstResPhi_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mScanClustersTrackFstResPhi_2Layer_Rstrips[i_sensor][i_rstrip]  = new TH1F(HistName.c_str(),HistName.c_str(),200,-0.10,0.10);
      HistName = Form("h_mScanClustersTrackFstResRPhi_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // RPhi Residual
      if(i_sensor > 0) HistName = Form("h_mScanClustersTrackFstResRPhi_2Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mScanClustersTrackFstResRPhi_2Layer_Rstrips[i_sensor][i_rstrip] = new TH2F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0,200,-0.10,0.10);
    }
  }

  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    string HistName;
    HistName = Form("h_mScanClustersTrackFstResX_3Layer_Sensor%d",i_sensor);
    h_mScanClustersTrackFstResX_3Layer[i_sensor]    = new TH1F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0);
    HistName = Form("h_mScanClustersTrackFstResY_3Layer_Sensor%d",i_sensor);
    h_mScanClustersTrackFstResY_3Layer[i_sensor]    = new TH1F(HistName.c_str(),HistName.c_str(),100,-16.0,16.0);
    HistName = Form("h_mScanClustersTrackFstResXY_3Layer_Sensor%d",i_sensor);
    h_mScanClustersTrackFstResXY_3Layer[i_sensor]   = new TH2F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0,100,-16.0,16.0);

    HistName = Form("h_mScanClustersTrackFstResR_3Layer_Sensor%d",i_sensor);
    h_mScanClustersTrackFstResR_3Layer[i_sensor]    = new TH1F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0);
    HistName = Form("h_mScanClustersTrackFstResPhi_3Layer_Sensor%d",i_sensor);
    h_mScanClustersTrackFstResPhi_3Layer[i_sensor]  = new TH1F(HistName.c_str(),HistName.c_str(),200,-0.10,0.10);
    HistName = Form("h_mScanClustersTrackFstResRPhi_3Layer_Sensor%d",i_sensor);
    h_mScanClustersTrackFstResRPhi_3Layer[i_sensor] = new TH2F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0,200,-0.10,0.10);

    for(int i_rstrip = 0; i_rstrip < FST::mFstNumRstripPerSensor; ++i_rstrip)
    {
      HistName = Form("h_mScanClustersTrackFstResX_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // X Residual
      if(i_sensor > 0) HistName = Form("h_mScanClustersTrackFstResX_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mScanClustersTrackFstResX_3Layer_Rstrips[i_sensor][i_rstrip]  = new TH1F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0);
      HistName = Form("h_mScanClustersTrackFstResY_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // Y Residual
      if(i_sensor > 0) HistName = Form("h_mScanClustersTrackFstResY_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mScanClustersTrackFstResY_3Layer_Rstrips[i_sensor][i_rstrip]  = new TH1F(HistName.c_str(),HistName.c_str(),100,-16.0,16.0);
      HistName = Form("h_mScanClustersTrackFstResXY_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // XY Residual
      if(i_sensor > 0) HistName = Form("h_mScanClustersTrackFstResXY_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mScanClustersTrackFstResXY_3Layer_Rstrips[i_sensor][i_rstrip] = new TH2F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0,100,-16.0,16.0);

      HistName = Form("h_mScanClustersTrackFstResR_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // R Residual
      if(i_sensor > 0) HistName = Form("h_mScanClustersTrackFstResR_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mScanClustersTrackFstResR_3Layer_Rstrips[i_sensor][i_rstrip]    = new TH1F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0);
      HistName = Form("h_mScanClustersTrackFstResPhi_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // Phi Residual
      if(i_sensor > 0) HistName = Form("h_mScanClustersTrackFstResPhi_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mScanClustersTrackFstResPhi_3Layer_Rstrips[i_sensor][i_rstrip]  = new TH1F(HistName.c_str(),HistName.c_str(),200,-0.10,0.10);
      HistName = Form("h_mScanClustersTrackFstResRPhi_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip); // RPhi Residual
      if(i_sensor > 0) HistName = Form("h_mScanClustersTrackFstResRPhi_3Layer_Sensor%d_Rstrip%d",i_sensor,i_rstrip+4);
      h_mScanClustersTrackFstResRPhi_3Layer_Rstrips[i_sensor][i_rstrip] = new TH2F(HistName.c_str(),HistName.c_str(),50,-160.0,160.0,200,-0.10,0.10);
    }
  }
}

void FstTracking::calResolution_SimpleClusters(FstEvent *fstEvent)
{
  std::vector<FstTrack *> trackClusterVec;
  trackClusterVec.clear(); // clear the container for clusters
  for(int i_track = 0; i_track < fstEvent->getNumTracks(); ++i_track)
  { // get Tracks info
    FstTrack *fstTrack = fstEvent->getTrack(i_track);
    if(fstTrack->getTrackType() == 1) // track reconstructed with clusters
    {
      trackClusterVec.push_back(fstTrack);
    }
  }

  std::vector<FstCluster *> clusterVec_fst;
  std::vector<FstCluster *> clusterVec_ist2;
  clusterVec_fst.clear();
  clusterVec_ist2.clear();

  for(int i_cluster = 0; i_cluster < fstEvent->getNumClusters(); ++i_cluster)
  { // get Simple Clusters 
    FstCluster *fstCluster = fstEvent->getCluster(i_cluster);
    if(fstCluster->getLayer() == 0 && fstCluster->getClusterType() == 1)
    {
      clusterVec_fst.push_back(fstCluster);
    }
    if(fstCluster->getLayer() == 2 && fstCluster->getClusterType() == 1)
    {
      clusterVec_ist2.push_back(fstCluster);
    }
  }

  // 2-Layer Tracking
  for(int i_track = 0; i_track < trackClusterVec.size(); ++i_track)
  {
    TVector3 pos_ist1 = trackClusterVec[i_track]->getPosOrig(1);
    double y1_ist = pos_ist1.Y(); // original hit postion on IST1
    TVector3 pos_ist3 = trackClusterVec[i_track]->getPosOrig(3);
    double y3_ist = pos_ist3.Y(); // original hit postion on IST3

    TVector3 proj_fst = trackClusterVec[i_track]->getProjection(0);
    double r_proj   = proj_fst.X(); // get aligned projected position
    double phi_proj = proj_fst.Y(); // r & phi for fst
    double x0_proj  = r_proj*TMath::Cos(phi_proj);
    double y0_proj  = r_proj*TMath::Sin(phi_proj);

    TVector3 proj_ist2 = trackClusterVec[i_track]->getProjection(2);
    double x2_proj = proj_ist2.X(); // get aligned projected position
    double y2_proj = proj_ist2.Y(); // x & y for ist2

    if( abs(y1_ist-y3_ist) < 17.0*FST::pitchRow )
    {
      if(clusterVec_fst.size() > 0)
      {
	double minDistance_FST = 1000.0;
	int minClusterId_FST = -1;
	double preDistance_FST = minDistance_FST;
	for(int i_cluster = 0; i_cluster < clusterVec_fst.size(); ++i_cluster)
	{ // fill residual histograms with the cluster of minimum distance
	  if( clusterVec_fst[i_cluster]->getIsSeed() ) // select cluste with seed
	  {
	    double r_fst   = clusterVec_fst[i_cluster]->getMeanX(); // r for fst
	    double phi_fst = clusterVec_fst[i_cluster]->getMeanY(); // phi for fst
	    double x0_fst  = r_fst*TMath::Cos(phi_fst);
	    double y0_fst  = r_fst*TMath::Sin(phi_fst);

	    double distance_FST = abs(r_fst-r_proj)/FST::pitchR + abs(phi_fst-phi_proj)/FST::pitchPhi;
	    if(distance_FST < preDistance_FST)
	    { 
	      minDistance_FST = distance_FST;
	      minClusterId_FST = i_cluster;
	      preDistance_FST = minDistance_FST;
	    }
	  }
	}

	if(minClusterId_FST > -1)
	{
	  int sensorId   = clusterVec_fst[minClusterId_FST]->getSensor(); // sensor Id for the cluster
	  double r_fst   = clusterVec_fst[minClusterId_FST]->getMeanX(); // r for fst
	  double phi_fst = clusterVec_fst[minClusterId_FST]->getMeanY(); // phi for fst
	  double x0_fst  = r_fst*TMath::Cos(phi_fst);
	  double y0_fst  = r_fst*TMath::Sin(phi_fst);

	  h_mSimpleClustersTrackFstResX_2Layer[sensorId]->Fill(x0_fst-x0_proj);
	  h_mSimpleClustersTrackFstResY_2Layer[sensorId]->Fill(y0_fst-y0_proj);
	  h_mSimpleClustersTrackFstResXY_2Layer[sensorId]->Fill(x0_fst-x0_proj,y0_fst-y0_proj);

	  h_mSimpleClustersTrackFstResR_2Layer[sensorId]->Fill(r_fst-r_proj);
	  h_mSimpleClustersTrackFstResPhi_2Layer[sensorId]->Fill(phi_fst-phi_proj);
	  h_mSimpleClustersTrackFstResRPhi_2Layer[sensorId]->Fill(r_fst-r_proj,phi_fst-phi_proj);

	  for(int i_rstrip = 0; i_rstrip < FST::mFstNumRstripPerSensor; ++i_rstrip)
	  {
	    double rStart = FST::rInner + i_rstrip*FST::pitchR;
	    double rStop  = FST::rInner + (i_rstrip+1)*FST::pitchR;
	    if(sensorId > 0) rStart = FST::rOuter + i_rstrip*FST::pitchR;
	    if(sensorId > 0) rStop  = FST::rOuter + (i_rstrip+1)*FST::pitchR;
	    if(r_proj > rStart && r_proj <= rStop)
	    // check the position of the projected r is within a specific r_strip and fill accordingly
	    {
	      h_mSimpleClustersTrackFstResX_2Layer_Rstrips[sensorId][i_rstrip]->Fill(x0_fst-x0_proj);
	      h_mSimpleClustersTrackFstResY_2Layer_Rstrips[sensorId][i_rstrip]->Fill(y0_fst-y0_proj);
	      h_mSimpleClustersTrackFstResXY_2Layer_Rstrips[sensorId][i_rstrip]->Fill(x0_fst-x0_proj,y0_fst-y0_proj);

	      h_mSimpleClustersTrackFstResR_2Layer_Rstrips[sensorId][i_rstrip]->Fill(r_fst-r_proj);
	      h_mSimpleClustersTrackFstResPhi_2Layer_Rstrips[sensorId][i_rstrip]->Fill(phi_fst-phi_proj);
	      h_mSimpleClustersTrackFstResRPhi_2Layer_Rstrips[sensorId][i_rstrip]->Fill(r_fst-r_proj,phi_fst-phi_proj);
	    }
	  }
	}
      }
      if(clusterVec_ist2.size() > 0)
      {
	double minDistance_IST = 1000.0;
	int minClusterId_IST = -1;
	double preDistance_IST = minDistance_IST;
	for(int i_cluster = 0; i_cluster < clusterVec_ist2.size(); ++i_cluster)
	{ // fill residual histograms with the cluster of minimum distance
	  double x2_ist = clusterVec_ist2[i_cluster]->getMeanX(); // x for ist2
	  double y2_ist = clusterVec_ist2[i_cluster]->getMeanY(); // y for ist2

	  double distance_IST = abs(x2_ist-x2_proj)/FST::pitchColumn + abs(y2_ist-y2_proj)/FST::pitchRow;
	  if(distance_IST < preDistance_IST)
	  { 
	    minDistance_IST = distance_IST;
	    minClusterId_IST = i_cluster;
	    preDistance_IST = minDistance_IST;
	  }
	}

	if(minClusterId_IST > -1)
	{
	  double x2_ist = clusterVec_ist2[minClusterId_IST]->getMeanX(); // x for ist2
	  double y2_ist = clusterVec_ist2[minClusterId_IST]->getMeanY(); // y for ist2

	  h_mSimpleClustersTrackIstResX_2Layer->Fill(x2_ist-x2_proj);
	  h_mSimpleClustersTrackIstResY_2Layer->Fill(y2_ist-y2_proj);
	  h_mSimpleClustersTrackIstResXY_2Layer->Fill(x2_ist-x2_proj,y2_ist-y2_proj);
	}
      }
    }
  }

  // 3-Layer Tracking
  for(int i_track = 0; i_track < trackClusterVec.size(); ++i_track)
  {
    TVector3 pos_ist1 = trackClusterVec[i_track]->getPosOrig(1);
    double y1_ist = pos_ist1.Y(); // original hit postion on IST1
    TVector3 pos_ist3 = trackClusterVec[i_track]->getPosOrig(3);
    double y3_ist = pos_ist3.Y(); // original hit postion on IST3

    TVector3 proj_fst = trackClusterVec[i_track]->getProjection(0);
    double r_proj   = proj_fst.X(); // get aligned projected position
    double phi_proj = proj_fst.Y(); // r & phi for fst
    double x0_proj  = r_proj*TMath::Cos(phi_proj);
    double y0_proj  = r_proj*TMath::Sin(phi_proj);

    TVector3 proj_ist2 = trackClusterVec[i_track]->getProjection(2);
    double x2_proj = proj_ist2.X(); // get aligned projected position
    double y2_proj = proj_ist2.Y(); // x & y for ist2

    if( abs(y1_ist-y3_ist) < 17.0*FST::pitchRow && x2_proj >= 20.0*FST::pitchColumn && x2_proj <= 24.0*FST::pitchColumn )
    {
      if(clusterVec_ist2.size() > 0)
      {
	double minDistance_FST = 1000.0;
	int minClusterId_FST = -1;
	double preDistance_FST = minDistance_FST;

	double minDistance_IST = 1000.0;
	int minClusterId_IST = -1;
	double preDistance_IST = minDistance_IST;

	for(int i_ist2 = 0; i_ist2 < clusterVec_ist2.size(); ++i_ist2)
	{ // fill residual histograms with the clusters of minimum distance
	  double x2_ist = clusterVec_ist2[i_ist2]->getMeanX(); // x for ist2
	  double y2_ist = clusterVec_ist2[i_ist2]->getMeanY(); // y for ist2

	  if( abs(x2_ist-x2_proj) < 6.0 && abs(y2_ist-y2_proj) < 0.6 )
	  { // IST2 matching cut
	    if(clusterVec_fst.size() > 0)
	    {
	      for(int i_cluster = 0; i_cluster < clusterVec_fst.size(); ++i_cluster)
	      { // fill residual histograms
		if( clusterVec_fst[i_cluster]->getIsSeed() ) // select cluste with seed
		{
		  double r_fst   = clusterVec_fst[i_cluster]->getMeanX(); // r for fst
		  double phi_fst = clusterVec_fst[i_cluster]->getMeanY(); // phi for fst
		  double x0_fst  = r_fst*TMath::Cos(phi_fst);
		  double y0_fst  = r_fst*TMath::Sin(phi_fst);

		  double distance_FST = abs(r_fst-r_proj)/FST::pitchR + abs(phi_fst-phi_proj)/FST::pitchPhi;
		  if(distance_FST < preDistance_FST)
		  { 
		    minDistance_FST = distance_FST;
		    minClusterId_FST = i_cluster;
		    preDistance_FST = minDistance_FST;
		  }
		}
	      }
	    }

	    double distance_IST = abs(x2_ist-x2_proj)/FST::pitchColumn + abs(y2_ist-y2_proj)/FST::pitchRow;
	    if(distance_IST < preDistance_IST)
	    { 
	      minDistance_IST = distance_IST;
	      minClusterId_IST = i_ist2;
	      preDistance_IST = minDistance_IST;
	    }
	  }
	}

	if(minClusterId_FST > -1)
	{
	  int sensorId   = clusterVec_fst[minClusterId_FST]->getSensor(); // sensor Id for the cluster
	  double r_fst   = clusterVec_fst[minClusterId_FST]->getMeanX(); // r for fst
	  double phi_fst = clusterVec_fst[minClusterId_FST]->getMeanY(); // phi for fst
	  double x0_fst  = r_fst*TMath::Cos(phi_fst);
	  double y0_fst  = r_fst*TMath::Sin(phi_fst);

	  h_mSimpleClustersTrackFstResX_3Layer[sensorId]->Fill(x0_fst-x0_proj);
	  h_mSimpleClustersTrackFstResY_3Layer[sensorId]->Fill(y0_fst-y0_proj);
	  h_mSimpleClustersTrackFstResXY_3Layer[sensorId]->Fill(x0_fst-x0_proj,y0_fst-y0_proj);

	  h_mSimpleClustersTrackFstResR_3Layer[sensorId]->Fill(r_fst-r_proj);
	  h_mSimpleClustersTrackFstResPhi_3Layer[sensorId]->Fill(phi_fst-phi_proj);
	  h_mSimpleClustersTrackFstResRPhi_3Layer[sensorId]->Fill(r_fst-r_proj,phi_fst-phi_proj);

	  for(int i_rstrip = 0; i_rstrip < FST::mFstNumRstripPerSensor; ++i_rstrip)
	  {
	    double rStart = FST::rInner + i_rstrip*FST::pitchR;
	    double rStop  = FST::rInner + (i_rstrip+1)*FST::pitchR;
	    if(sensorId > 0) rStart = FST::rOuter + i_rstrip*FST::pitchR;
	    if(sensorId > 0) rStop  = FST::rOuter + (i_rstrip+1)*FST::pitchR;
	    if(r_proj > rStart && r_proj <= rStop)
	    // check the position of the projected r is within a specific r_strip and fill accordingly
	    {
	      h_mSimpleClustersTrackFstResX_3Layer_Rstrips[sensorId][i_rstrip]->Fill(x0_fst-x0_proj);
	      h_mSimpleClustersTrackFstResY_3Layer_Rstrips[sensorId][i_rstrip]->Fill(y0_fst-y0_proj);
	      h_mSimpleClustersTrackFstResXY_3Layer_Rstrips[sensorId][i_rstrip]->Fill(x0_fst-x0_proj,y0_fst-y0_proj);

	      h_mSimpleClustersTrackFstResR_3Layer_Rstrips[sensorId][i_rstrip]->Fill(r_fst-r_proj);
	      h_mSimpleClustersTrackFstResPhi_3Layer_Rstrips[sensorId][i_rstrip]->Fill(phi_fst-phi_proj);
	      h_mSimpleClustersTrackFstResRPhi_3Layer_Rstrips[sensorId][i_rstrip]->Fill(r_fst-r_proj,phi_fst-phi_proj);
	    }
	  }
	}

	if(minClusterId_IST > -1)
	{
	  double x2_ist = clusterVec_ist2[minClusterId_IST]->getMeanX(); // x for ist2
	  double y2_ist = clusterVec_ist2[minClusterId_IST]->getMeanY(); // y for ist2

	  h_mSimpleClustersTrackIstResX_3Layer->Fill(x2_ist-x2_proj);
	  h_mSimpleClustersTrackIstResY_3Layer->Fill(y2_ist-y2_proj);
	  h_mSimpleClustersTrackIstResXY_3Layer->Fill(x2_ist-x2_proj,y2_ist-y2_proj);
	}
      }
    }
  }
}

void FstTracking::calResolution_ScanClusters(FstEvent *fstEvent)
{
  std::vector<FstTrack *> trackClusterVec;
  trackClusterVec.clear(); // clear the container for clusters
  for(int i_track = 0; i_track < fstEvent->getNumTracks(); ++i_track)
  { // get Tracks info
    FstTrack *fstTrack = fstEvent->getTrack(i_track);
    if(fstTrack->getTrackType() == 1) // track reconstructed with clusters
    {
      trackClusterVec.push_back(fstTrack);
    }
  }

  std::vector<FstCluster *> clusterVec_fst;
  std::vector<FstCluster *> clusterVec_ist2;
  clusterVec_fst.clear();
  clusterVec_ist2.clear();

  for(int i_cluster = 0; i_cluster < fstEvent->getNumClusters(); ++i_cluster)
  { // get Scan Clusters for FST
    FstCluster *fstCluster = fstEvent->getCluster(i_cluster);
    if(fstCluster->getLayer() == 0 && fstCluster->getClusterType() == 2)
    {
      clusterVec_fst.push_back(fstCluster);
    }
    if(fstCluster->getLayer() == 2 && fstCluster->getClusterType() == 1)
    {
      clusterVec_ist2.push_back(fstCluster);
    }
  }

  // 2-Layer Tracking
  for(int i_track = 0; i_track < trackClusterVec.size(); ++i_track)
  {
    TVector3 pos_ist1 = trackClusterVec[i_track]->getPosOrig(1);
    double y1_ist = pos_ist1.Y(); // original hit postion on IST1
    TVector3 pos_ist3 = trackClusterVec[i_track]->getPosOrig(3);
    double y3_ist = pos_ist3.Y(); // original hit postion on IST3

    TVector3 proj_fst = trackClusterVec[i_track]->getProjection(0);
    double r_proj   = proj_fst.X(); // get aligned projected position
    double phi_proj = proj_fst.Y(); // r & phi for fst
    double x0_proj  = r_proj*TMath::Cos(phi_proj);
    double y0_proj  = r_proj*TMath::Sin(phi_proj);

    if( abs(y1_ist-y3_ist) < 17.0*FST::pitchRow )
    {
      if(clusterVec_fst.size() > 0)
      {
	double minDistance_FST = 1000.0;
	int minClusterId_FST = -1;
	double preDistance_FST = minDistance_FST;
	for(int i_cluster = 0; i_cluster < clusterVec_fst.size(); ++i_cluster)
	{ // fill residual histograms with the cluster of minimum distance
	  if( clusterVec_fst[i_cluster]->getIsSeed() ) // select cluste with seed
	  {
	    double r_fst   = clusterVec_fst[i_cluster]->getMeanX(); // r for fst
	    double phi_fst = clusterVec_fst[i_cluster]->getMeanY(); // phi for fst
	    double x0_fst  = r_fst*TMath::Cos(phi_fst);
	    double y0_fst  = r_fst*TMath::Sin(phi_fst);

	    double distance_FST = abs(r_fst-r_proj)/FST::pitchR + abs(phi_fst-phi_proj)/FST::pitchPhi;
	    if(distance_FST < preDistance_FST)
	    { 
	      minDistance_FST = distance_FST;
	      minClusterId_FST = i_cluster;
	      preDistance_FST = minDistance_FST;
	    }
	  }
	}

	if(minClusterId_FST > -1)
	{
	  int sensorId   = clusterVec_fst[minClusterId_FST]->getSensor(); // sensor Id for the cluster
	  double r_fst   = clusterVec_fst[minClusterId_FST]->getMeanX(); // r for fst
	  double phi_fst = clusterVec_fst[minClusterId_FST]->getMeanY(); // phi for fst
	  double x0_fst  = r_fst*TMath::Cos(phi_fst);
	  double y0_fst  = r_fst*TMath::Sin(phi_fst);

	  h_mScanClustersTrackFstResX_2Layer[sensorId]->Fill(x0_fst-x0_proj);
	  h_mScanClustersTrackFstResY_2Layer[sensorId]->Fill(y0_fst-y0_proj);
	  h_mScanClustersTrackFstResXY_2Layer[sensorId]->Fill(x0_fst-x0_proj,y0_fst-y0_proj);

	  h_mScanClustersTrackFstResR_2Layer[sensorId]->Fill(r_fst-r_proj);
	  h_mScanClustersTrackFstResPhi_2Layer[sensorId]->Fill(phi_fst-phi_proj);
	  h_mScanClustersTrackFstResRPhi_2Layer[sensorId]->Fill(r_fst-r_proj,phi_fst-phi_proj);

	  for(int i_rstrip = 0; i_rstrip < FST::mFstNumRstripPerSensor; ++i_rstrip)
	  {
	    double rStart = FST::rInner + i_rstrip*FST::pitchR;
	    double rStop  = FST::rInner + (i_rstrip+1)*FST::pitchR;
	    if(sensorId > 0) rStart = FST::rOuter + i_rstrip*FST::pitchR;
	    if(sensorId > 0) rStop  = FST::rOuter + (i_rstrip+1)*FST::pitchR;
	    if(r_proj > rStart && r_proj <= rStop)
	    // check the position of the projected r is within a specific r_strip and fill accordingly
	    {
	      h_mScanClustersTrackFstResX_2Layer_Rstrips[sensorId][i_rstrip]->Fill(x0_fst-x0_proj);
	      h_mScanClustersTrackFstResY_2Layer_Rstrips[sensorId][i_rstrip]->Fill(y0_fst-y0_proj);
	      h_mScanClustersTrackFstResXY_2Layer_Rstrips[sensorId][i_rstrip]->Fill(x0_fst-x0_proj,y0_fst-y0_proj);

	      h_mScanClustersTrackFstResR_2Layer_Rstrips[sensorId][i_rstrip]->Fill(r_fst-r_proj);
	      h_mScanClustersTrackFstResPhi_2Layer_Rstrips[sensorId][i_rstrip]->Fill(phi_fst-phi_proj);
	      h_mScanClustersTrackFstResRPhi_2Layer_Rstrips[sensorId][i_rstrip]->Fill(r_fst-r_proj,phi_fst-phi_proj);
	    }
	  }
	}
      }
    }
  }

  // 3-Layer Tracking
  for(int i_track = 0; i_track < trackClusterVec.size(); ++i_track)
  {
    TVector3 pos_ist1 = trackClusterVec[i_track]->getPosOrig(1);
    double y1_ist = pos_ist1.Y(); // original hit postion on IST1
    TVector3 pos_ist3 = trackClusterVec[i_track]->getPosOrig(3);
    double y3_ist = pos_ist3.Y(); // original hit postion on IST3

    TVector3 proj_fst = trackClusterVec[i_track]->getProjection(0);
    double r_proj   = proj_fst.X(); // get aligned projected position
    double phi_proj = proj_fst.Y(); // r & phi for fst
    double x0_proj  = r_proj*TMath::Cos(phi_proj);
    double y0_proj  = r_proj*TMath::Sin(phi_proj);

    TVector3 proj_ist2 = trackClusterVec[i_track]->getProjection(2);
    double x2_proj = proj_ist2.X(); // get aligned projected position
    double y2_proj = proj_ist2.Y(); // x & y for ist2

    if( abs(y1_ist-y3_ist) < 17.0*FST::pitchRow && x2_proj >= 20.0*FST::pitchColumn && x2_proj <= 24.0*FST::pitchColumn )
    {
      if(clusterVec_ist2.size() > 0)
      {
	double minDistance_FST = 1000.0;
	int minClusterId_FST = -1;
	double preDistance_FST = minDistance_FST;

	for(int i_ist2 = 0; i_ist2 < clusterVec_ist2.size(); ++i_ist2)
	{ // fill residual histograms with the clusters of minimum distance
	  double x2_ist = clusterVec_ist2[i_ist2]->getMeanX(); // x for ist2
	  double y2_ist = clusterVec_ist2[i_ist2]->getMeanY(); // y for ist2

	  if( abs(x2_ist-x2_proj) < 6.0 && abs(y2_ist-y2_proj) < 0.6 )
	  { // IST2 matching cut
	    if(clusterVec_fst.size() > 0)
	    {
	      for(int i_cluster = 0; i_cluster < clusterVec_fst.size(); ++i_cluster)
	      { // fill residual histograms
		if( clusterVec_fst[i_cluster]->getIsSeed() ) // select cluste with seed
		{
		  double r_fst   = clusterVec_fst[i_cluster]->getMeanX(); // r for fst
		  double phi_fst = clusterVec_fst[i_cluster]->getMeanY(); // phi for fst
		  double x0_fst  = r_fst*TMath::Cos(phi_fst);
		  double y0_fst  = r_fst*TMath::Sin(phi_fst);

		  double distance_FST = abs(r_fst-r_proj)/FST::pitchR + abs(phi_fst-phi_proj)/FST::pitchPhi;
		  if(distance_FST < preDistance_FST)
		  { 
		    minDistance_FST = distance_FST;
		    minClusterId_FST = i_cluster;
		    preDistance_FST = minDistance_FST;
		  }
		}
	      }
	    }
	  }
	}

	if(minClusterId_FST > -1)
	{
	  int sensorId   = clusterVec_fst[minClusterId_FST]->getSensor(); // sensor Id for the cluster
	  double r_fst   = clusterVec_fst[minClusterId_FST]->getMeanX(); // r for fst
	  double phi_fst = clusterVec_fst[minClusterId_FST]->getMeanY(); // phi for fst
	  double x0_fst  = r_fst*TMath::Cos(phi_fst);
	  double y0_fst  = r_fst*TMath::Sin(phi_fst);

	  h_mScanClustersTrackFstResX_3Layer[sensorId]->Fill(x0_fst-x0_proj);
	  h_mScanClustersTrackFstResY_3Layer[sensorId]->Fill(y0_fst-y0_proj);
	  h_mScanClustersTrackFstResXY_3Layer[sensorId]->Fill(x0_fst-x0_proj,y0_fst-y0_proj);

	  h_mScanClustersTrackFstResR_3Layer[sensorId]->Fill(r_fst-r_proj);
	  h_mScanClustersTrackFstResPhi_3Layer[sensorId]->Fill(phi_fst-phi_proj);
	  h_mScanClustersTrackFstResRPhi_3Layer[sensorId]->Fill(r_fst-r_proj,phi_fst-phi_proj);

	  for(int i_rstrip = 0; i_rstrip < FST::mFstNumRstripPerSensor; ++i_rstrip)
	  {
	    double rStart = FST::rInner + i_rstrip*FST::pitchR;
	    double rStop  = FST::rInner + (i_rstrip+1)*FST::pitchR;
	    if(sensorId > 0) rStart = FST::rOuter + i_rstrip*FST::pitchR;
	    if(sensorId > 0) rStop  = FST::rOuter + (i_rstrip+1)*FST::pitchR;
	    if(r_proj > rStart && r_proj <= rStop)
	    // check the position of the projected r is within a specific r_strip and fill accordingly
	    {
	      h_mScanClustersTrackFstResX_3Layer_Rstrips[sensorId][i_rstrip]->Fill(x0_fst-x0_proj);
	      h_mScanClustersTrackFstResY_3Layer_Rstrips[sensorId][i_rstrip]->Fill(y0_fst-y0_proj);
	      h_mScanClustersTrackFstResXY_3Layer_Rstrips[sensorId][i_rstrip]->Fill(x0_fst-x0_proj,y0_fst-y0_proj);

	      h_mScanClustersTrackFstResR_3Layer_Rstrips[sensorId][i_rstrip]->Fill(r_fst-r_proj);
	      h_mScanClustersTrackFstResPhi_3Layer_Rstrips[sensorId][i_rstrip]->Fill(phi_fst-phi_proj);
	      h_mScanClustersTrackFstResRPhi_3Layer_Rstrips[sensorId][i_rstrip]->Fill(r_fst-r_proj,phi_fst-phi_proj);
	    }
	  }
	}
      }
    }
  }
}

void FstTracking::writeTracking_Clusters()
{
  // IST simple clusters
  h_mSimpleClustersTrackIstResX_2Layer->Write();
  h_mSimpleClustersTrackIstResY_2Layer->Write();
  h_mSimpleClustersTrackIstResXY_2Layer->Write();

  h_mSimpleClustersTrackIstResX_3Layer->Write();
  h_mSimpleClustersTrackIstResY_3Layer->Write();
  h_mSimpleClustersTrackIstResXY_3Layer->Write();

  // FST simple clusters
  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    h_mSimpleClustersTrackFstResX_2Layer[i_sensor]->Write();
    h_mSimpleClustersTrackFstResY_2Layer[i_sensor]->Write();
    h_mSimpleClustersTrackFstResXY_2Layer[i_sensor]->Write();

    h_mSimpleClustersTrackFstResR_2Layer[i_sensor]->Write();
    h_mSimpleClustersTrackFstResPhi_2Layer[i_sensor]->Write();
    h_mSimpleClustersTrackFstResRPhi_2Layer[i_sensor]->Write();

    for(int i_rstrip = 0; i_rstrip < FST::mFstNumRstripPerSensor; ++i_rstrip)
    {
      h_mSimpleClustersTrackFstResX_2Layer_Rstrips[i_sensor][i_rstrip]->Write();
      h_mSimpleClustersTrackFstResY_2Layer_Rstrips[i_sensor][i_rstrip]->Write();
      h_mSimpleClustersTrackFstResXY_2Layer_Rstrips[i_sensor][i_rstrip]->Write();

      h_mSimpleClustersTrackFstResR_2Layer_Rstrips[i_sensor][i_rstrip]->Write();
      h_mSimpleClustersTrackFstResPhi_2Layer_Rstrips[i_sensor][i_rstrip]->Write();
      h_mSimpleClustersTrackFstResRPhi_2Layer_Rstrips[i_sensor][i_rstrip]->Write();
    }
  }

  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    h_mSimpleClustersTrackFstResX_3Layer[i_sensor]->Write();
    h_mSimpleClustersTrackFstResY_3Layer[i_sensor]->Write();
    h_mSimpleClustersTrackFstResXY_3Layer[i_sensor]->Write();

    h_mSimpleClustersTrackFstResR_3Layer[i_sensor]->Write();
    h_mSimpleClustersTrackFstResPhi_3Layer[i_sensor]->Write();
    h_mSimpleClustersTrackFstResRPhi_3Layer[i_sensor]->Write();

    for(int i_rstrip = 0; i_rstrip < FST::mFstNumRstripPerSensor; ++i_rstrip)
    {
      h_mSimpleClustersTrackFstResX_3Layer_Rstrips[i_sensor][i_rstrip]->Write();
      h_mSimpleClustersTrackFstResY_3Layer_Rstrips[i_sensor][i_rstrip]->Write();
      h_mSimpleClustersTrackFstResXY_3Layer_Rstrips[i_sensor][i_rstrip]->Write();

      h_mSimpleClustersTrackFstResR_3Layer_Rstrips[i_sensor][i_rstrip]->Write();
      h_mSimpleClustersTrackFstResPhi_3Layer_Rstrips[i_sensor][i_rstrip]->Write();
      h_mSimpleClustersTrackFstResRPhi_3Layer_Rstrips[i_sensor][i_rstrip]->Write();
    }
  }
 
  // FST scan clusters
  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    h_mScanClustersTrackFstResX_2Layer[i_sensor]->Write();
    h_mScanClustersTrackFstResY_2Layer[i_sensor]->Write();
    h_mScanClustersTrackFstResXY_2Layer[i_sensor]->Write();

    h_mScanClustersTrackFstResR_2Layer[i_sensor]->Write();
    h_mScanClustersTrackFstResPhi_2Layer[i_sensor]->Write();
    h_mScanClustersTrackFstResRPhi_2Layer[i_sensor]->Write();

    for(int i_rstrip = 0; i_rstrip < FST::mFstNumRstripPerSensor; ++i_rstrip)
    {
      h_mScanClustersTrackFstResX_2Layer_Rstrips[i_sensor][i_rstrip]->Write();
      h_mScanClustersTrackFstResY_2Layer_Rstrips[i_sensor][i_rstrip]->Write();
      h_mScanClustersTrackFstResXY_2Layer_Rstrips[i_sensor][i_rstrip]->Write();

      h_mScanClustersTrackFstResR_2Layer_Rstrips[i_sensor][i_rstrip]->Write();
      h_mScanClustersTrackFstResPhi_2Layer_Rstrips[i_sensor][i_rstrip]->Write();
      h_mScanClustersTrackFstResRPhi_2Layer_Rstrips[i_sensor][i_rstrip]->Write();
    }
  }

  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    h_mScanClustersTrackFstResX_3Layer[i_sensor]->Write();
    h_mScanClustersTrackFstResY_3Layer[i_sensor]->Write();
    h_mScanClustersTrackFstResXY_3Layer[i_sensor]->Write();

    h_mScanClustersTrackFstResR_3Layer[i_sensor]->Write();
    h_mScanClustersTrackFstResPhi_3Layer[i_sensor]->Write();
    h_mScanClustersTrackFstResRPhi_3Layer[i_sensor]->Write();

    for(int i_rstrip = 0; i_rstrip < FST::mFstNumRstripPerSensor; ++i_rstrip)
    {
      h_mScanClustersTrackFstResX_3Layer_Rstrips[i_sensor][i_rstrip]->Write();
      h_mScanClustersTrackFstResY_3Layer_Rstrips[i_sensor][i_rstrip]->Write();
      h_mScanClustersTrackFstResXY_3Layer_Rstrips[i_sensor][i_rstrip]->Write();

      h_mScanClustersTrackFstResR_3Layer_Rstrips[i_sensor][i_rstrip]->Write();
      h_mScanClustersTrackFstResPhi_3Layer_Rstrips[i_sensor][i_rstrip]->Write();
      h_mScanClustersTrackFstResRPhi_3Layer_Rstrips[i_sensor][i_rstrip]->Write();
    }
  }
}
//--------------Track Resolution with Clusters---------------------

//--------------Efficiency with Hits---------------------
void FstTracking::initEfficiency_Hits()
{
  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    for(int i_match = 0; i_match < FST::mFstNumMatching; ++i_match)
    {
      string HistName;
      HistName = Form("h_mHitsTrackIstCounts_Sensor%d_SF%d",i_sensor,i_match);
      h_mHitsTrackIstCounts[i_sensor][i_match] = new TH2F(HistName.c_str(),HistName.c_str(),60,FST::rMin[i_sensor],FST::rMax[i_sensor],60,FST::phiMin[i_sensor],FST::phiMax[i_sensor]);
      HistName = Form("h_mHitsTrackFstCounts_Sensor%d_SF%d",i_sensor,i_match);
      h_mHitsTrackFstCounts[i_sensor][i_match] = new TH2F(HistName.c_str(),HistName.c_str(),60,FST::rMin[i_sensor],FST::rMax[i_sensor],60,FST::phiMin[i_sensor],FST::phiMax[i_sensor]);
    }
  }
}

void FstTracking::calEfficiency_Hits(FstEvent *fstEvent)
{
  std::vector<FstTrack *> trackHitVec;
  trackHitVec.clear(); // clear the container for clusters
  for(int i_track = 0; i_track < fstEvent->getNumTracks(); ++i_track)
  { // get Tracks info
    FstTrack *fstTrack = fstEvent->getTrack(i_track);
    if(fstTrack->getTrackType() == 0) // track reconstructed with hits
    {
      trackHitVec.push_back(fstTrack);
    }
  }

  int numOfFstHits[FST::mFstNumSensorsPerModule];
  std::vector<FstRawHit *> fstRawHitVec[FST::mFstNumSensorsPerModule];
  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    fstRawHitVec[i_sensor].clear(); // clear the container for Raw Hits
    numOfFstHits[i_sensor] = 0;
  }
  for(int i_hit = 0; i_hit < fstEvent->getNumRawHits(); ++i_hit)
  { // get Hits info
    FstRawHit *fstRawHit = fstEvent->getRawHit(i_hit);
    if(fstRawHit->getLayer() == 0 && fstRawHit->getIsHit())
    { // only use Hits
      int sensorId = fstRawHit->getSensor();
      fstRawHitVec[sensorId].push_back(fstRawHit);
      numOfFstHits[sensorId]++;
    }
  }

  // fill Efficiency Histograms
  for(int i_track = 0; i_track < trackHitVec.size(); ++i_track)
  {
    TVector3 proj_fst = trackHitVec[i_track]->getProjection(0);
    double r_proj = proj_fst.X();
    double phi_proj = proj_fst.Y();

    for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
    {
      if(r_proj >= FST::rMin[i_sensor] && r_proj <= FST::rMax[i_sensor] && phi_proj >= FST::phiMin[i_sensor] && phi_proj <= FST::phiMax[i_sensor])
      { // used for efficiency only if the projected position is within FST acceptance
	for(int i_match = 0; i_match < FST::mFstNumMatching; ++i_match)
	{
	  h_mHitsTrackIstCounts[i_sensor][i_match]->Fill(r_proj,phi_proj);
	  int nMatchedTrack = 0;
	  if(numOfFstHits[i_sensor] > 0)
	  {
	    for(int i_hit = 0; i_hit < numOfFstHits[i_sensor]; ++i_hit)
	    { // loop over all possible hits
	      double r_fst = fstRawHitVec[i_sensor][i_hit]->getPosX();
	      double phi_fst = fstRawHitVec[i_sensor][i_hit]->getPosY();
	      if(i_match == 0)
	      {
		nMatchedTrack++;
	      }
	      // if(i_match > 0 && abs(r_fst-r_proj) <= i_match*0.5*FST::pitchR && abs(phi_fst-phi_proj) <= 3.0*FST::pitchPhi)
	      if(i_match > 0 && abs(r_fst-r_proj) <= i_match*0.5*FST::pitchR && abs(phi_fst-phi_proj) <= 6.0*FST::pitchPhi)
	      {
		nMatchedTrack++;
	      }
	    }
	  }
	  if(nMatchedTrack > 0) h_mHitsTrackFstCounts[i_sensor][i_match]->Fill(r_proj,phi_proj);
	}
      }
    }
  }
}

void FstTracking::writeEfficiency_Hits()
{
  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    for(int i_match = 0; i_match < FST::mFstNumMatching; ++i_match)
    {
      h_mHitsTrackIstCounts[i_sensor][i_match]->Write();
      h_mHitsTrackFstCounts[i_sensor][i_match]->Write();
    }
  }
}
//--------------Efficiency with Hits---------------------

//--------------Efficiency with Clusters---------------------
void FstTracking::initEfficiency_Clusters()
{
  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    for(int i_match = 0; i_match < FST::mFstNumMatching; ++i_match)
    {
      string HistName;
      // simple clusters
      HistName = Form("h_mSimpleClustersTrackIstCounts_2Layer_Sensor%d_SF%d",i_sensor,i_match);
      h_mSimpleClustersTrackIstCounts_2Layer[i_sensor][i_match] = new TH2F(HistName.c_str(),HistName.c_str(),40,FST::rMin[i_sensor],FST::rMax[i_sensor],40,FST::phiMin[i_sensor],FST::phiMax[i_sensor]);
      HistName = Form("h_mSimpleClustersTrackFstCounts_2Layer_Sensor%d_SF%d",i_sensor,i_match);
      h_mSimpleClustersTrackFstCounts_2Layer[i_sensor][i_match] = new TH2F(HistName.c_str(),HistName.c_str(),40,FST::rMin[i_sensor],FST::rMax[i_sensor],40,FST::phiMin[i_sensor],FST::phiMax[i_sensor]);

      HistName = Form("h_mSimpleClustersTrackIstCounts_3Layer_Sensor%d_SF%d",i_sensor,i_match);
      h_mSimpleClustersTrackIstCounts_3Layer[i_sensor][i_match] = new TH2F(HistName.c_str(),HistName.c_str(),40,FST::rMin[i_sensor],FST::rMax[i_sensor],40,FST::phiMin[i_sensor],FST::phiMax[i_sensor]);
      HistName = Form("h_mSimpleClustersTrackFstCounts_3Layer_Sensor%d_SF%d",i_sensor,i_match);
      h_mSimpleClustersTrackFstCounts_3Layer[i_sensor][i_match] = new TH2F(HistName.c_str(),HistName.c_str(),40,FST::rMin[i_sensor],FST::rMax[i_sensor],40,FST::phiMin[i_sensor],FST::phiMax[i_sensor]);

      // scan clusters
      HistName = Form("h_mScanClustersTrackIstCounts_2Layer_Sensor%d_SF%d",i_sensor,i_match);
      h_mScanClustersTrackIstCounts_2Layer[i_sensor][i_match] = new TH2F(HistName.c_str(),HistName.c_str(),40,FST::rMin[i_sensor],FST::rMax[i_sensor],40,FST::phiMin[i_sensor],FST::phiMax[i_sensor]);
      HistName = Form("h_mScanClustersTrackFstCounts_2Layer_Sensor%d_SF%d",i_sensor,i_match);
      h_mScanClustersTrackFstCounts_2Layer[i_sensor][i_match] = new TH2F(HistName.c_str(),HistName.c_str(),40,FST::rMin[i_sensor],FST::rMax[i_sensor],40,FST::phiMin[i_sensor],FST::phiMax[i_sensor]);

      HistName = Form("h_mScanClustersTrackIstCounts_3Layer_Sensor%d_SF%d",i_sensor,i_match);
      h_mScanClustersTrackIstCounts_3Layer[i_sensor][i_match] = new TH2F(HistName.c_str(),HistName.c_str(),40,FST::rMin[i_sensor],FST::rMax[i_sensor],40,FST::phiMin[i_sensor],FST::phiMax[i_sensor]);
      HistName = Form("h_mScanClustersTrackFstCounts_3Layer_Sensor%d_SF%d",i_sensor,i_match);
      h_mScanClustersTrackFstCounts_3Layer[i_sensor][i_match] = new TH2F(HistName.c_str(),HistName.c_str(),40,FST::rMin[i_sensor],FST::rMax[i_sensor],40,FST::phiMin[i_sensor],FST::phiMax[i_sensor]);
    }
  }
}

void FstTracking::calEfficiency_SimpleClusters(FstEvent *fstEvent)
{
  std::vector<FstTrack *> trackClusterVec;
  trackClusterVec.clear(); // clear the container for clusters
  for(int i_track = 0; i_track < fstEvent->getNumTracks(); ++i_track)
  { // get Tracks info
    FstTrack *fstTrack = fstEvent->getTrack(i_track);
    if(fstTrack->getTrackType() == 1) // track reconstructed with clusters
    {
      trackClusterVec.push_back(fstTrack);
    }
  }

  std::vector<FstCluster *> clusterVec_ist2;
  clusterVec_ist2.clear();
  for(int i_cluster = 0; i_cluster < fstEvent->getNumClusters(); ++i_cluster)
  { // get Clusters info IST2
    FstCluster *fstCluster = fstEvent->getCluster(i_cluster);
    if(fstCluster->getLayer() == 2 && fstCluster->getClusterType() == 1)
    {
      clusterVec_ist2.push_back(fstCluster);
    }
  }

  std::vector<FstCluster *> clusterVec_fst[FST::mFstNumSensorsPerModule];
  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    clusterVec_fst[i_sensor].clear();
  }
  for(int i_cluster = 0; i_cluster < fstEvent->getNumClusters(); ++i_cluster)
  { // get Clusters info for FST
    FstCluster *fstCluster = fstEvent->getCluster(i_cluster);
    if(fstCluster->getLayer() == 0 && fstCluster->getClusterType() == 1)
    {
      int sensorId = fstCluster->getSensor();
      clusterVec_fst[sensorId].push_back(fstCluster);
    }
  }

  if(trackClusterVec.size() == 1)
  { // only event with 1 track is used
    // fill Efficiency Histograms with 2-Layer Tracking
    for(int i_track = 0; i_track < trackClusterVec.size(); ++i_track)
    {
      TVector3 pos_ist1 = trackClusterVec[i_track]->getPosOrig(1);
      double y1_ist = pos_ist1.Y(); // original hit postion on IST1
      TVector3 pos_ist3 = trackClusterVec[i_track]->getPosOrig(3);
      double y3_ist = pos_ist3.Y(); // original hit postion on IST3

      TVector3 proj_fst = trackClusterVec[i_track]->getProjection(0);
      double r_proj   = proj_fst.X(); // get aligned projected position w.r.t. FST
      double phi_proj = proj_fst.Y(); // r & phi for fst

      if( abs(y1_ist-y3_ist) < 17.0*FST::pitchRow )
      {
	for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
	{
	  if(r_proj >= FST::rMin[i_sensor] && r_proj <= FST::rMax[i_sensor] && phi_proj >= FST::phiMin[i_sensor] && phi_proj <= FST::phiMax[i_sensor])
	  { // used for efficiency only if the projected position is within FST acceptance
	    for(int i_match = 0; i_match < FST::mFstNumMatching; ++i_match)
	    {
	      h_mSimpleClustersTrackIstCounts_2Layer[i_sensor][i_match]->Fill(r_proj,phi_proj);
	      int nMatchedTrack = 0;
	      if(clusterVec_fst[i_sensor].size() > 0)
	      {
		for(int i_cluster = 0; i_cluster < clusterVec_fst[i_sensor].size(); ++i_cluster)
		{ // loop over all possible clusters
		  if( clusterVec_fst[i_sensor][i_cluster]->getIsSeed() ) // select cluste with seed
		  {
		    double r_fst = clusterVec_fst[i_sensor][i_cluster]->getMeanX();
		    double phi_fst = clusterVec_fst[i_sensor][i_cluster]->getMeanY();
		    if(i_match == 0)
		    {
		      nMatchedTrack++;
		    }
		    // if(i_match > 0 && abs(r_fst-r_proj) <= i_match*0.5*FST::pitchR && abs(phi_fst-phi_proj) <= 3.0*FST::pitchPhi)
		    if(i_match > 0 && abs(r_fst-r_proj) <= i_match*0.5*FST::pitchR && abs(phi_fst-phi_proj) <= 6.0*FST::pitchPhi)
		    {
		      nMatchedTrack++;
		    }
		  }
		}
	      }
	      if(nMatchedTrack > 0) h_mSimpleClustersTrackFstCounts_2Layer[i_sensor][i_match]->Fill(r_proj,phi_proj);
	    }
	  }
	}
      }
    }

    // fill Efficiency Histograms with 3-Layer Tracking
    for(int i_track = 0; i_track < trackClusterVec.size(); ++i_track)
    {
      TVector3 pos_ist1 = trackClusterVec[i_track]->getPosOrig(1);
      double y1_ist = pos_ist1.Y(); // original hit postion on IST1
      TVector3 pos_ist3 = trackClusterVec[i_track]->getPosOrig(3);
      double y3_ist = pos_ist3.Y(); // original hit postion on IST3

      TVector3 proj_fst = trackClusterVec[i_track]->getProjection(0);
      double r_proj   = proj_fst.X(); // get aligned projected position w.r.t. FST
      double phi_proj = proj_fst.Y(); // r & phi for fst

      TVector3 proj_ist2 = trackClusterVec[i_track]->getProjection(2);
      double x2_proj = proj_ist2.X(); // get aligned projected position w.r.t. IST2
      double y2_proj = proj_ist2.Y(); // x & y for ist2

      if( abs(y1_ist-y3_ist) < 17.0*FST::pitchRow)
      {
	if(clusterVec_ist2.size() > 0)
	{ // IST2 match cut
	  int nMatchedTrack_IST2 = 0;
	  for(int i_ist2 = 0; i_ist2 < clusterVec_ist2.size(); ++i_ist2)
	  { // fill residual histograms
	    double x2_ist = clusterVec_ist2[i_ist2]->getMeanX(); // x for ist2
	    double y2_ist = clusterVec_ist2[i_ist2]->getMeanY(); // y for ist2

	    if( x2_proj >= 20.0*FST::pitchColumn && x2_proj <= 24.0*FST::pitchColumn )
	    {
	      if( abs(x2_ist-x2_proj) < 6.0 && abs(y2_ist-y2_proj) < 0.6 )
	      { // IST2 matching cut
		nMatchedTrack_IST2++;
	      }
	    }
	  }
	  if(nMatchedTrack_IST2 > 0)
	  {
	    for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
	    {
	      if(r_proj >= FST::rMin[i_sensor] && r_proj <= FST::rMax[i_sensor] && phi_proj >= FST::phiMin[i_sensor] && phi_proj <= FST::phiMax[i_sensor])
	      { // used for efficiency only if the projected position is within FST acceptance
		for(int i_match = 0; i_match < FST::mFstNumMatching; ++i_match)
		{
		  h_mSimpleClustersTrackIstCounts_3Layer[i_sensor][i_match]->Fill(r_proj,phi_proj);
		  int nMatchedTrack = 0;
		  if(clusterVec_fst[i_sensor].size() > 0)
		  {
		    for(int i_cluster = 0; i_cluster < clusterVec_fst[i_sensor].size(); ++i_cluster)
		    { // loop over all possible clusters
		      if( clusterVec_fst[i_sensor][i_cluster]->getIsSeed() ) // select cluste with seed
		      {
			double r_fst = clusterVec_fst[i_sensor][i_cluster]->getMeanX();
			double phi_fst = clusterVec_fst[i_sensor][i_cluster]->getMeanY();
			if(i_match == 0)
			{
			  nMatchedTrack++;
			}
			// if(i_match > 0 && abs(r_fst-r_proj) <= i_match*0.5*FST::pitchR && abs(phi_fst-phi_proj) <= 3.0*FST::pitchPhi)
			if(i_match > 0 && abs(r_fst-r_proj) <= i_match*0.5*FST::pitchR && abs(phi_fst-phi_proj) <= 6.0*FST::pitchPhi)
			{
			  nMatchedTrack++;
			}
		      }
		    }
		  }
		  if(nMatchedTrack > 0) h_mSimpleClustersTrackFstCounts_3Layer[i_sensor][i_match]->Fill(r_proj,phi_proj);
		}
	      }
	    }
	  }
	}
      }
    }
  }
}

void FstTracking::calEfficiency_ScanClusters(FstEvent *fstEvent)
{
  std::vector<FstTrack *> trackClusterVec;
  trackClusterVec.clear(); // clear the container for clusters
  for(int i_track = 0; i_track < fstEvent->getNumTracks(); ++i_track)
  { // get Tracks info
    FstTrack *fstTrack = fstEvent->getTrack(i_track);
    if(fstTrack->getTrackType() == 1) // track reconstructed with clusters
    {
      trackClusterVec.push_back(fstTrack);
    }
  }

  std::vector<FstCluster *> clusterVec_ist2;
  clusterVec_ist2.clear();
  for(int i_cluster = 0; i_cluster < fstEvent->getNumClusters(); ++i_cluster)
  { // get Clusters info IST2
    FstCluster *fstCluster = fstEvent->getCluster(i_cluster);
    if(fstCluster->getLayer() == 2 && fstCluster->getClusterType() == 1)
    {
      clusterVec_ist2.push_back(fstCluster);
    }
  }

  std::vector<FstCluster *> clusterVec_fst[FST::mFstNumSensorsPerModule];
  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    clusterVec_fst[i_sensor].clear();
  }
  for(int i_cluster = 0; i_cluster < fstEvent->getNumClusters(); ++i_cluster)
  { // get Clusters info for FST
    FstCluster *fstCluster = fstEvent->getCluster(i_cluster);
    if(fstCluster->getLayer() == 0 && fstCluster->getClusterType() == 2)
    {
      int sensorId = fstCluster->getSensor();
      clusterVec_fst[sensorId].push_back(fstCluster);
    }
  }

  if(trackClusterVec.size() == 1)
  { // only event with 1 track is used
    // fill Efficiency Histograms with 2-Layer Tracking
    for(int i_track = 0; i_track < trackClusterVec.size(); ++i_track)
    {
      TVector3 pos_ist1 = trackClusterVec[i_track]->getPosOrig(1);
      double y1_ist = pos_ist1.Y(); // original hit postion on IST1
      TVector3 pos_ist3 = trackClusterVec[i_track]->getPosOrig(3);
      double y3_ist = pos_ist3.Y(); // original hit postion on IST3

      TVector3 proj_fst = trackClusterVec[i_track]->getProjection(0);
      double r_proj   = proj_fst.X(); // get aligned projected position w.r.t. FST
      double phi_proj = proj_fst.Y(); // r & phi for fst

      if( abs(y1_ist-y3_ist) < 17.0*FST::pitchRow )
      {
	for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
	{
	  if(r_proj >= FST::rMin[i_sensor] && r_proj <= FST::rMax[i_sensor] && phi_proj >= FST::phiMin[i_sensor] && phi_proj <= FST::phiMax[i_sensor])
	  { // used for efficiency only if the projected position is within FST acceptance
	    for(int i_match = 0; i_match < FST::mFstNumMatching; ++i_match)
	    {
	      h_mScanClustersTrackIstCounts_2Layer[i_sensor][i_match]->Fill(r_proj,phi_proj);
	      int nMatchedTrack = 0;
	      if(clusterVec_fst[i_sensor].size() > 0)
	      {
		for(int i_cluster = 0; i_cluster < clusterVec_fst[i_sensor].size(); ++i_cluster)
		{ // loop over all possible clusters
		  if( clusterVec_fst[i_sensor][i_cluster]->getIsSeed() ) // select cluste with seed
		  {
		    double r_fst = clusterVec_fst[i_sensor][i_cluster]->getMeanX();
		    double phi_fst = clusterVec_fst[i_sensor][i_cluster]->getMeanY();
		    if(i_match == 0)
		    {
		      nMatchedTrack++;
		    }
		    // if(i_match > 0 && abs(r_fst-r_proj) <= i_match*0.5*FST::pitchR && abs(phi_fst-phi_proj) <= 3.0*FST::pitchPhi)
		    if(i_match > 0 && abs(r_fst-r_proj) <= i_match*0.5*FST::pitchR && abs(phi_fst-phi_proj) <= 6.0*FST::pitchPhi)
		    {
		      nMatchedTrack++;
		    }
		  }
		}
	      }
	      if(nMatchedTrack > 0) h_mScanClustersTrackFstCounts_2Layer[i_sensor][i_match]->Fill(r_proj,phi_proj);
	    }
	  }
	}
      }
    }

    // fill Efficiency Histograms with 3-Layer Tracking
    for(int i_track = 0; i_track < trackClusterVec.size(); ++i_track)
    {
      TVector3 pos_ist1 = trackClusterVec[i_track]->getPosOrig(1);
      double y1_ist = pos_ist1.Y(); // original hit postion on IST1
      TVector3 pos_ist3 = trackClusterVec[i_track]->getPosOrig(3);
      double y3_ist = pos_ist3.Y(); // original hit postion on IST3

      TVector3 proj_fst = trackClusterVec[i_track]->getProjection(0);
      double r_proj   = proj_fst.X(); // get aligned projected position w.r.t. FST
      double phi_proj = proj_fst.Y(); // r & phi for fst

      TVector3 proj_ist2 = trackClusterVec[i_track]->getProjection(2);
      double x2_proj = proj_ist2.X(); // get aligned projected position w.r.t. IST2
      double y2_proj = proj_ist2.Y(); // x & y for ist2

      if( abs(y1_ist-y3_ist) < 17.0*FST::pitchRow)
      {
	if(clusterVec_ist2.size() > 0)
	{ // IST2 match cut
	  int nMatchedTrack_IST2 = 0;
	  for(int i_ist2 = 0; i_ist2 < clusterVec_ist2.size(); ++i_ist2)
	  { // fill residual histograms
	    double x2_ist = clusterVec_ist2[i_ist2]->getMeanX(); // x for ist2
	    double y2_ist = clusterVec_ist2[i_ist2]->getMeanY(); // y for ist2

	    if( x2_proj >= 20.0*FST::pitchColumn && x2_proj <= 24.0*FST::pitchColumn )
	    {
	      if( abs(x2_ist-x2_proj) < 6.0 && abs(y2_ist-y2_proj) < 0.6 )
	      { // IST2 matching cut
		nMatchedTrack_IST2++;
	      }
	    }
	  }
	  if(nMatchedTrack_IST2 > 0)
	  {
	    for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
	    {
	      if(r_proj >= FST::rMin[i_sensor] && r_proj <= FST::rMax[i_sensor] && phi_proj >= FST::phiMin[i_sensor] && phi_proj <= FST::phiMax[i_sensor])
	      { // used for efficiency only if the projected position is within FST acceptance
		for(int i_match = 0; i_match < FST::mFstNumMatching; ++i_match)
		{
		  h_mScanClustersTrackIstCounts_3Layer[i_sensor][i_match]->Fill(r_proj,phi_proj);
		  int nMatchedTrack = 0;
		  if(clusterVec_fst[i_sensor].size() > 0)
		  {
		    for(int i_cluster = 0; i_cluster < clusterVec_fst[i_sensor].size(); ++i_cluster)
		    { // loop over all possible clusters
		      if( clusterVec_fst[i_sensor][i_cluster]->getIsSeed() ) // select cluste with seed
		      {
			double r_fst = clusterVec_fst[i_sensor][i_cluster]->getMeanX();
			double phi_fst = clusterVec_fst[i_sensor][i_cluster]->getMeanY();
			if(i_match == 0)
			{
			  nMatchedTrack++;
			}
			// if(i_match > 0 && abs(r_fst-r_proj) <= i_match*0.5*FST::pitchR && abs(phi_fst-phi_proj) <= 3.0*FST::pitchPhi)
			if(i_match > 0 && abs(r_fst-r_proj) <= i_match*0.5*FST::pitchR && abs(phi_fst-phi_proj) <= 6.0*FST::pitchPhi)
			{
			  nMatchedTrack++;
			}
		      }
		    }
		  }
		  if(nMatchedTrack > 0) h_mScanClustersTrackFstCounts_3Layer[i_sensor][i_match]->Fill(r_proj,phi_proj);
		}
	      }
	    }
	  }
	}
      }
    }
  }
}

void FstTracking::writeEfficiency_Clusters()
{
  for(int i_sensor = 0; i_sensor < FST::mFstNumSensorsPerModule; ++i_sensor)
  {
    for(int i_match = 0; i_match < FST::mFstNumMatching; ++i_match)
    {
      // simple clusters
      h_mSimpleClustersTrackIstCounts_2Layer[i_sensor][i_match]->Write();
      h_mSimpleClustersTrackFstCounts_2Layer[i_sensor][i_match]->Write();

      h_mSimpleClustersTrackIstCounts_3Layer[i_sensor][i_match]->Write();
      h_mSimpleClustersTrackFstCounts_3Layer[i_sensor][i_match]->Write();

      // scan clusters
      h_mScanClustersTrackIstCounts_2Layer[i_sensor][i_match]->Write();
      h_mScanClustersTrackFstCounts_2Layer[i_sensor][i_match]->Write();

      h_mScanClustersTrackIstCounts_3Layer[i_sensor][i_match]->Write();
      h_mScanClustersTrackFstCounts_3Layer[i_sensor][i_match]->Write();
    }
  }
}
//--------------Efficiency with Clusters---------------------
