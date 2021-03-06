
#include "DECalAnalysis.h"

// FCCSW
#include "DetInterface/IGeoSvc.h"

// datamodel
#include "datamodel/PositionedCaloHitCollection.h"
#include "datamodel/CaloHitCollection.h"
#include "datamodel/MCParticleCollection.h"

#include "CLHEP/Vector/ThreeVector.h"
#include "GaudiKernel/ITHistSvc.h"
#include "TH1F.h"
#include "TVector2.h"
#include "TMath.h"
#include "TString.h"
#include "string"

// DD4hep
#include "DD4hep/Detector.h"
#include "DD4hep/Readout.h"

DECLARE_ALGORITHM_FACTORY(DECalAnalysis)

DECalAnalysis::DECalAnalysis(const std::string& aName, ISvcLocator* aSvcLoc)
    : GaudiAlgorithm(aName, aSvcLoc),
      m_histSvc("THistSvc", "DECalAnalysis"),
      m_geoSvc("GeoSvc", "DECalAnalysis")
{
  declareProperty("pixels", m_deposits, "Energy deposits in sampling calorimeter (input)");
  declareProperty("pads", m_pads, "Energy deposits in Pad mode (input)");
  declareProperty("truth", m_truth, "Generated particle truth");

  m_calP0 = 239.74;
  m_calP1 = 99.1602;
  m_calP2 = -0.0143;

}
DECalAnalysis::~DECalAnalysis() {}

StatusCode DECalAnalysis::initialize() {
  if (GaudiAlgorithm::initialize().isFailure()) {
    return StatusCode::FAILURE;
  }
  // check if readouts exist
  if (m_geoSvc->lcdd()->readouts().find(m_pixelReadoutName) == m_geoSvc->lcdd()->readouts().end()) {
    error() << "Readout <<" << m_pixelReadoutName << ">> does not exist." << endmsg;
    return StatusCode::FAILURE;
  }
  
  m_sumPixelsLayers.assign(m_numLayers+1,0);
  m_sumPadsLayers.assign(m_numLayers+1,0);
  m_sumParticlesLayers.assign(m_numLayers+1,0);

  m_treeInfo = new TTree("decal_info", "for Kostas");
  m_treeInfo->Branch("event_number", &m_treeEvtNumber);
  m_treeInfo->Branch("truth_energy", &m_truthEnergy);
  m_treeInfo->Branch("edep_tot", &m_sumEnergyDep);
  m_treeInfo->Branch("pixels_cal", &m_calPixels);
  m_treeInfo->Branch("pixels_tot", &m_sumPixels);
  m_treeInfo->Branch("particles_tot", &m_sumParticles);
  for(uint l(0); l<m_numLayers+1; l++) {
    m_treeInfo->Branch(Form("pixels_l%i", l), &m_sumPixelsLayers[l]);
  }
  m_treeInfo->Branch("pads_tot", &m_sumPads);
  for(uint l(0); l<m_numLayers+1; l++) {
    m_treeInfo->Branch(Form("pads_l%i", l), &m_sumPadsLayers[l]);
  }
  for(uint l(0); l<m_numLayers+1; l++) {
    m_treeInfo->Branch(Form("particles_l%i", l), &m_sumParticlesLayers[l]);
  }
  if(m_histSvc->regTree("/rec/decalInfo", m_treeInfo).isFailure()) {
	error() << "Couldn't register tree" << endmsg;
	return StatusCode::FAILURE;
  }
  // create histograms
  m_particlesPerPixel = new TProfile("decal_ParticlesPerPixel", "Particles Per Pixels; Layer Number; Particles / Pixel", m_numLayers+1, -0.5, m_numLayers+0.5);
  if (m_histSvc->regHist("/rec/profile_test", m_particlesPerPixel).isFailure()) {
    error() << "Couldn't register histogram" << endmsg;
    return StatusCode::FAILURE;
  }


  m_totalHits = new TH1F("decal_totalHits", "Total hits in an event", 15000, 0, 150000);
  if (m_histSvc->regHist("/rec/decal_hits", m_totalHits).isFailure()) {
    error() << "Couldn't register histogram" << endmsg;
    return StatusCode::FAILURE;
  }
  m_totalHitsVsLayer = new TH1F("decal_totalHitsVsLayer", "Total hits per layer", m_numLayers+1, -0.5, m_numLayers+0.5);
  m_totalHitsVsLayer->Sumw2(kFALSE);
  if(m_histSvc->regHist("/rec/decal_hitsVsLayer", m_totalHitsVsLayer).isFailure()) {
    error() << "Couldn't register histogram" << endmsg;
    return StatusCode::FAILURE;
  } 

  m_totalPadsVsLayer = new TH1F("decal_totalPadsVsLayer", "Total pads per layer", m_numLayers+1, -0.5, m_numLayers+0.5);
  m_totalPadsVsLayer->Sumw2(kFALSE);
  if(m_histSvc->regHist("/rec/decal_padsVsLayer", m_totalPadsVsLayer).isFailure()) {
    error() << "Couldn't register histogram" << endmsg;
    return StatusCode::FAILURE;
  }

  for(uint n(0); n<100; n++) {
    m_hitsVsLayerEvent[n] = new TH1F(("decal_hitsVsLayerEvent"+std::to_string(n)).c_str(), "", m_numLayers+1, -0.5, m_numLayers+0.5);
    m_hitsVsLayerEvent[n]->Sumw2(kFALSE);
    if(m_histSvc->regHist("/rec/decal_hitsVsLayerEvent"+std::to_string(n), m_hitsVsLayerEvent[n]).isFailure()) {
      error() << "Couldn't register histogram" << endmsg;
      return StatusCode::FAILURE;
    }
  }

  m_hitsInMaxLayer = new TH1F("decal_hitsInMaxLayer", "Total hits in max layer", 150, 0, 15000);
  if (m_histSvc->regHist("/rec/decal_hitsInMaxLayer", m_hitsInMaxLayer).isFailure()) {
    error() << "Couldn't register histogram" << endmsg;
    return StatusCode::FAILURE; 
  }

  m_maxLayer = new TH1F("decal_maxLayer", "Layer with the maximum hits in an event",m_numLayers+1, -0.5, m_numLayers+0.5 );
  if (m_histSvc->regHist("/rec/decal_maxLayer", m_maxLayer).isFailure()) {
    error() << "Couldn't register histogram" << endmsg;
    return StatusCode::FAILURE; 
  } 

  m_totalPadsAbovePixelThreshold = new TH1F("decal_totalPadsAbovePixelThreshold","", 1000,0,10000);
  if (m_histSvc->regHist("/rec/decal_totalPadsAbovePixelThreshold", m_totalPadsAbovePixelThreshold).isFailure()) {
    error() << "Couldn't register histogram" << endmsg;
    return StatusCode::FAILURE; 
  }  

  m_totalPads = new TH1F("decal_totalPads","", 1000,0,10000);
  if (m_histSvc->regHist("/rec/decal_totalPads", m_totalPads).isFailure()) {
    error() << "Couldn't register histogram" << endmsg;
    return StatusCode::FAILURE; 
  } 

  for (uint i = 0; i < m_numLayers+1; i++) {
    m_totalHitsLayers.push_back(new TH1F(("decal_Hits_layer" + std::to_string(i)).c_str(),
                                       ("Total pixel hits in layer " + std::to_string(i)).c_str(), 1000, 0,
                                       10000));
    if (m_histSvc->regHist("/rec/decal_total_hits_layer" + std::to_string(i), m_totalHitsLayers.back()).isFailure()) {
      error() << "Couldn't register histogram" << endmsg;
      return StatusCode::FAILURE;
    }

    m_totalPadsLayers.push_back(new TH1F(("decal_Pads_layer" + std::to_string(i)).c_str(),
                                       ("Total pads with hits in layer " + std::to_string(i)).c_str(), 100, 0,
                                       1000));
    if (m_histSvc->regHist("/rec/decal_total_pads_layer" + std::to_string(i), m_totalPadsLayers.back()).isFailure()) {
      error() << "Couldn't register histogram" << endmsg;
      return StatusCode::FAILURE;
    }  


    m_totalPixelsPerPadLayers.push_back(new TH1F(("decal_Pixels_Per_Pad_layer" + std::to_string(i)).c_str(),
                                       ("Total pixels per pad in layer " + std::to_string(i)).c_str(), 200, 0,
                                       200));
    if (m_histSvc->regHist("/rec/decal_Pixels_Per_Pad_layer" + std::to_string(i), m_totalPixelsPerPadLayers.back()).isFailure()) {
      error() << "Couldn't register histogram" << endmsg;
      return StatusCode::FAILURE;
    }   
  }
  m_XYEvent0 = new TH2F("decal_XYEvent0", "XY Positions for Event 0", 5000, -2500, 2500, 5000, -2500, 2500);
  if (m_histSvc->regHist("/rec/decal_XYEvent0", m_XYEvent0).isFailure()) {
    error() << "Couldn't register histogram" << endmsg;
    return StatusCode::FAILURE;
  }

  m_EtaPhiEvent0 = new TH2F("decal_EtaPhiEvent0", "Eta Phi Positions for Event 0", 222, -0.004, 0.002, 222, -0.8305,-0.8245);
  if (m_histSvc->regHist("/rec/decal_EtaPhiEvent0", m_EtaPhiEvent0).isFailure()) {
    error() << "Couldn't register histogram" << endmsg;
    return StatusCode::FAILURE;
  }

  m_EtaPhiSeparation = new TH1F("decal_EtaSeparation", "Eta Phi Positions", 1200, 0, 1);
  if (m_histSvc->regHist("/rec/decal_EtaPhiSeparation", m_EtaPhiSeparation).isFailure()) {
    error() << "Couldn't register histogram" << endmsg;
    return StatusCode::FAILURE;
  }

  m_nEventsFilled = 0;

  m_calFit = new TF1("calFit", "pol2", 0,3000);
  m_calFit->SetParameter("p0", m_calP0);
  m_calFit->SetParameter("p1", m_calP1);
  m_calFit->SetParameter("p2", m_calP2);
  
  // if it gets to here then it has registered all histograms
  return StatusCode::SUCCESS;
}

StatusCode DECalAnalysis::execute() {
  // get the decoder for the readout 
  // this will allow to extract layers etc.
  auto decoder = m_geoSvc->lcdd()->readout(m_pixelReadoutName).idSpec().decoder();

  // calculate the MC truth energy of the incident particle  
  const auto truth = m_truth.get();
  for (const auto& t : *truth) {
    double px = t.core().p4.px;
    double py = t.core().p4.py;
    double pz = t.core().p4.pz;
    double m = t.core().p4.mass;

    m_truthEnergy = TMath::Sqrt(m*m + px*px + py*py + pz*pz);   
  }

  // added a little print out to ensure it is still working
  if(m_treeEvtNumber < 10 or m_treeEvtNumber % 100 == 0) {
    info() << "Event Number " << m_treeEvtNumber << endmsg;
     info() << "Truth energy of single particle = " << m_truthEnergy << endmsg;
  }


  // set some variables to hold information for an event
  m_sumPixels = 0.;
  m_sumParticles = 0.;
  m_sumPixelsLayers.assign(m_numLayers+1, 0);
  m_sumParticlesLayers.assign(m_numLayers+1, 0);
  m_meanEta.assign(m_numLayers+1, 0);
  m_meanPhi.assign(m_numLayers+1, 0);

  m_sumEnergyDep = 0;

  bool fillXY = true;
  if (m_XYEvent0->GetEntries()>0) {
    fillXY = false;
  }

  int isdigital = 0, notdigital = 0;

  const auto deposits = m_deposits.get();
  for (const auto& hit : *deposits) {
    decoder->setValue(hit.core().cellId);
    
    int layer = (*decoder)[m_layerFieldName];
    m_sumPixelsLayers[layer] += 1;
    
    // check if energy was deposited in the calorimeter (active/passive material)
    // layers are numbered starting from 1, layer == 0 is cryostat/bath
    if (layer > 0) {
      m_sumPixels++;
      decoder->setValue(hit.core().cellId);  
      int layer = (*decoder)[m_layerFieldName];
      // get the energy deposited in the active area
      double edep = hit.core().energy;
      m_sumEnergyDep += edep;
      double particles = hit.core().time; // I overwrite now the time for number of particles
      m_sumParticles += particles;
      m_sumParticlesLayers[layer] += particles; // call this particles for DEcal as changed SD readout to pass nparticles / pixel not enerrgy dep / pixel 
      m_particlesPerPixel->Fill(layer, particles);
      // calculate eta and phi
      double r = TMath::Sqrt(hit.position().x*hit.position().x + hit.position().y*hit.position().y + hit.position().z*hit.position().z);
      double theta = TMath::ACos(hit.position().z/r);
      double eta = -TMath::Log2(TMath::Tan(theta/2.0));
      double phi = TMath::ATan(hit.position().y / hit.position().x); 

      m_meanEta[layer] += eta;
      m_meanPhi[layer] += phi;
      
    }
  }

  std::cout << "isdigital = " << isdigital << std::endl;
  std::cout << "notdigital = " << notdigital << std::endl;

  if(m_sumPixels>0 || m_sumPads>0) {
    m_treeInfo->Fill();
  }

  m_treeEvtNumber++;

  return StatusCode::SUCCESS;
}

StatusCode DECalAnalysis::finalize() { 
  debug() << "StatusCode DECalAnalysis::finalize()" << endmsg;

  // count from 1 to avoid the hits in the Trkr volume
  for (uint i = 1; i < m_numLayers+1; i++) {
      m_totalHitsVsLayer->SetBinContent(i+1,m_totalHitsLayers[i]->GetMean());
      m_totalPadsVsLayer->SetBinContent(i+1,m_totalPadsLayers[i]->GetMean());
  }

return GaudiAlgorithm::finalize(); }

Double_t DECalAnalysis::FitLongProfile( Double_t *x, Double_t *par)
{
	double E_0 = par[0];
	double a = par[1];
	double b = par[2];
	double led =  E_0*b * (pow( (b*x[0]), a-1 )* exp(-(b*x[0])) ) / TMath::Gamma(a);
	
	return led;
}
