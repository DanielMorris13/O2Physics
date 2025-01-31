// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "Framework/runDataProcessing.h"
#include "Framework/AnalysisTask.h"
#include "Framework/AnalysisDataModel.h"
#include "Framework/ASoAHelpers.h"
#include "ReconstructionDataFormats/Track.h"
#include "ReconstructionDataFormats/PID.h"
#include "Common/Core/trackUtilities.h"
#include "PWGLF/DataModel/LFStrangenessTables.h"
#include "Common/Core/TrackSelection.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/PIDResponse.h"

using namespace o2;
using namespace o2::track;
using namespace o2::framework;
using namespace o2::framework::expressions;

using PIDTracks = soa::Join<aod::Tracks, aod::TracksExtra, aod::pidTPCFullPi, aod::pidTPCFullPr, aod::pidTOFFullPi>;
using PIDTracksIU = soa::Join<aod::TracksIU, aod::TracksExtra, aod::pidTPCFullPi, aod::pidTPCFullPr, aod::pidTOFFullPi>;
using SelectedCollisions = soa::Join<aod::Collisions, aod::EvSels>;

struct perfK0sResolution {
  // Configurable bins
  ConfigurableAxis mBins{"mBins", {200, 0.4f, 0.6f}, "Mass binning"};
  ConfigurableAxis pTBins{"pTBins", {200, 0.f, 10.f}, "pT binning"};
  ConfigurableAxis pTResBins{"pTResBins", {200, -1.2f, 1.2f}, "pT resolution binning"};
  ConfigurableAxis pTResRelBins{"pTResRelBins", {200, -0.2f, 0.2f}, "pT relative resolution binning"};
  ConfigurableAxis invpTResBins{"invpTResBins", {200, -1.2f, 1.2f}, "inv pT resolution binning"};
  ConfigurableAxis etaBins{"etaBins", {2, -1.f, 1.f}, "eta binning"};
  ConfigurableAxis etaBinsDauthers{"etaBinsDauthers", {100, -1.f, 1.f}, "eta binning for daughters"};
  ConfigurableAxis phiBins{"phiBins", {100, 0.f, 6.28f}, "phi binning"};

  // Selection criteria
  Configurable<float> v0setting_cospa{"v0setting_cospa", 0.995, "V0 CosPA"}; // shoudl be double in future
  Configurable<float> v0setting_dcav0dau{"v0setting_dcav0dau", 1., "DCA V0 Daughters"};
  Configurable<float> v0setting_dcapostopv{"v0setting_dcapostopv", 0.1, "DCA Pos To PV"};
  Configurable<float> v0setting_dcanegtopv{"v0setting_dcanegtopv", 0.1, "DCA Neg To PV"};
  Configurable<float> v0setting_radius{"v0setting_radius", 0.9, "V0 Radius"};
  Configurable<float> v0setting_rapidity{"v0setting_rapidity", 0.5, "rapidity"};

  Configurable<float> v0lifetime{"v0lifetime", 3., "n ctau"};
  Configurable<float> nMaxTPCNsigma{"nMaxTPCNsigma", 10., "Maximum TPC nsigma for pions"};
  Configurable<int> itsIbSelectionPos{"itsIbSelectionPos", 0, "Flag for the ITS IB selection on positive daughters: -1 no ITS IB, 0 no selection, 1 ITS IB"};
  Configurable<int> itsIbSelectionNeg{"itsIbSelectionNeg", 0, "Flag for the ITS IB IB selection on negative daughters: -1 no ITS IB, 0 no selection, 1 ITS IB"};
  Configurable<int> trdSelectionPos{"trdSelectionPos", 0, "Flag for the TRD selection on positive daughters: -1 no TRD, 0 no selection, 1 TRD"};
  Configurable<int> trdSelectionNeg{"trdSelectionNeg", 0, "Flag for the TRD selection on negative daughters: -1 no TRD, 0 no selection, 1 TRD"};
  Configurable<int> tofSelectionPos{"tofSelectionPos", 0, "Flag for the TOF selection on positive daughters: -1 no TOF, 0 no selection, 1 TOF"};
  Configurable<int> tofSelectionNeg{"tofSelectionNeg", 0, "Flag for the TOF selection on negative daughters: -1 no TOF, 0 no selection, 1 TOF"};
  Configurable<int> pidHypoPos{"pidHypoPos", -1, "Index for the PID hypothesis used in tracking for the positive daughters: -1 no selection, 0 Electron, 1 Muon, 2 Pion, 3 Kaon, 4 Proton"};
  Configurable<int> pidHypoNeg{"pidHypoNeg", -1, "Index for the PID hypothesis used in tracking for the negative daughters: -1 no selection, 0 Electron, 1 Muon, 2 Pion, 3 Kaon, 4 Proton"};
  Configurable<float> extraCutTPCClusters{"extraCutTPCClusters", -1.0f, "Extra cut on daugthers for TPC clusters"};

  // Configure plots to enable
  Configurable<bool> useMultidimHisto{"useMultidimHisto", false, "use multidimentional histograms"};
  Configurable<bool> enableTPCPlot{"enableTPCPlot", false, "Enable the TPC plot"};
  Configurable<bool> computeInvMassFromDaughters{"computeInvMassFromDaughters", false, "Compute the invariant mass from the daughters"};

  // Configurable for event selection
  Configurable<float> cutzvertex{"cutzvertex", 10.0f, "Accepted z-vertex range (cm)"};
  Configurable<bool> eventSelection{"eventSelection", true, "event selection"};

  HistogramRegistry rK0sResolution{"K0sResolution", {}, OutputObjHandlingPolicy::AnalysisObject, true, true};
  HistogramRegistry rK0sDauResolution{"K0sDauResolution", {}, OutputObjHandlingPolicy::AnalysisObject, true, true};

  void init(InitContext const&)
  {
    const AxisSpec eventAxis{10, 0, 10, "Events"};
    const AxisSpec mAxis{mBins, "#it{m} (GeV/#it{c}^{2})"};
    const AxisSpec pTAxis{pTBins, "#it{p}_{T} (GeV/#it{c})"};
    const AxisSpec pTResAxis{pTResBins, "#Delta#it{p}_{T} (GeV/#it{c})"};
    const AxisSpec pTResRelAxis{pTResRelBins, "(#it{p}_{T}^{rec} - #it{p}_{T}^{MC})/#it{p}_{T}^{MC}"};
    const AxisSpec invpTResAxis{invpTResBins, "1/#it{p}_{T}-1/#it{p}_{T}^{MC} (GeV/#it{c})^{-1}"};
    const AxisSpec etaAxis{etaBins, "#eta"};
    const AxisSpec etaAxisPosD{etaBinsDauthers, "#eta pos."};
    const AxisSpec etaAxisNegD{etaBinsDauthers, "#eta neg."};
    const AxisSpec phiAxis{phiBins, "#phi"};
    const AxisSpec trueK0Axis{2, -0.5, 1.5, "True K0"};

    int nProc = 0;
    if (doprocessData) {
      LOG(info) << "processData enabled";
      nProc++;
    }
    if (doprocessMC) {
      LOG(info) << "processMC enabled";
      nProc++;
    }

    rK0sResolution.add("h1_events", "h1_events", {HistType::kTH1F, {eventAxis}});
    if (doprocessMC) {
      rK0sDauResolution.add("h2_massPosPtRes", "h2_massPosPtRes", {HistType::kTH2F, {mAxis, pTResAxis}});
      rK0sDauResolution.add("h2_massNegPtRes", "h2_massNegPtRes", {HistType::kTH2F, {mAxis, pTResAxis}});

      rK0sDauResolution.add("h2_genPtPosPtRes", "h2_genPtPosPtRes", {HistType::kTH2F, {pTResRelAxis, pTAxis}});
      rK0sDauResolution.add("h2_genPxPosPxRes", "h2_genPxPosPxRes", {HistType::kTH2F, {pTResRelAxis, pTAxis}});
      rK0sDauResolution.add("h2_genPyPosPyRes", "h2_genPyPosPyRes", {HistType::kTH2F, {pTResRelAxis, pTAxis}});
      rK0sDauResolution.add("h2_genPzPosPzRes", "h2_genPzPosPzRes", {HistType::kTH2F, {pTResRelAxis, pTAxis}});

      rK0sDauResolution.add("h2_genPtNegPtRes", "h2_genPtNegPtRes", {HistType::kTH2F, {pTResRelAxis, pTAxis}});
      rK0sDauResolution.add("h2_genPxNegPxRes", "h2_genPxNegPxRes", {HistType::kTH2F, {pTResRelAxis, pTAxis}});
      rK0sDauResolution.add("h2_genPyNegPyRes", "h2_genPyNegPyRes", {HistType::kTH2F, {pTResRelAxis, pTAxis}});
      rK0sDauResolution.add("h2_genPzNegPzRes", "h2_genPzNegPzRes", {HistType::kTH2F, {pTResRelAxis, pTAxis}});
    }
    rK0sResolution.add("h2_masspT", "h2_masspT", {HistType::kTH2F, {mAxis, pTAxis}});
    rK0sResolution.add("h2_masseta", "h2_masseta", {HistType::kTH2F, {mAxis, etaAxis}});
    rK0sResolution.add("h2_massphi", "h2_massphi", {HistType::kTH2F, {mAxis, phiAxis}});
    if (useMultidimHisto) {
      if (doprocessMC) {
        rK0sResolution.add("thn_mass", "thn_mass", kTHnSparseF, {mAxis, pTAxis, etaAxis, phiAxis, etaAxisPosD, etaAxisNegD, invpTResAxis, invpTResAxis, trueK0Axis});
      } else {
        rK0sResolution.add("thn_mass", "thn_mass", kTHnSparseF, {mAxis, pTAxis, etaAxis, phiAxis, etaAxisPosD, etaAxisNegD});
      }
    }

    if (enableTPCPlot) {
      rK0sDauResolution.add("h3_tpc_vs_pid_hypothesis", "h3_tpc_vs_pid_hypothesis", {HistType::kTH3F, {{200, -10.f, 10.f, "#it{p}/Z (GeV/#it{c})"}, {1000, 0, 1000.f, "dE/dx (a.u.)"}, {10, -0.5, 9.5f, "PID hypothesis"}}});
    }
  }

  template <typename T1, typename T2, typename C>
  bool acceptV0(const T1& v0, const T2& ntrack, const T2& ptrack, const C& collision)
  {
    // Apply selections on V0
    if (TMath::Abs(v0.yK0Short()) > v0setting_rapidity) {
      return false;
    }
    if (v0.v0radius() < v0setting_radius) {
      return false;
    }
    if (v0.distovertotmom(collision.posX(), collision.posY(), collision.posZ()) * pid_constants::sMasses[PID::K0] > 2.684 * v0lifetime) {
      return false;
    }

    // Apply selections on V0 daughters

    // ITS selection
    switch (itsIbSelectionPos) {
      case -1:
        if (ptrack.itsNClsInnerBarrel() > 0) {
          return false;
        }
        break;
      case 0:
        break;
      case 1:
        if (ptrack.itsNClsInnerBarrel() < 1) {
          return false;
        }
        break;
      default:
        LOG(fatal) << "Invalid ITS selection for positive daughter";
        break;
    }
    switch (itsIbSelectionNeg) {
      case -1:
        if (ntrack.itsNClsInnerBarrel() > 0) {
          return false;
        }
        break;
      case 0:
        break;
      case 1:
        if (ntrack.itsNClsInnerBarrel() < 1) {
          return false;
        }
        break;
      default:
        LOG(fatal) << "Invalid ITS selection for negative daughter";
        break;
    }

    // TPC selection
    if (!ntrack.hasTPC() || !ptrack.hasTPC()) {
      return false;
    }
    if (abs(ntrack.tpcNSigmaPi()) > nMaxTPCNsigma) {
      return false;
    }
    if (abs(ptrack.tpcNSigmaPi()) > nMaxTPCNsigma) {
      return false;
    }
    if (ntrack.tpcNClsCrossedRows() < extraCutTPCClusters || ptrack.tpcNClsCrossedRows() < extraCutTPCClusters) {
      return false;
    }

    // TOF selection
    switch (tofSelectionPos) {
      case -1:
        if (ptrack.hasTOF()) {
          return false;
        }
        break;
      case 0:
        break;
      case 1:
        if (!ptrack.hasTOF()) {
          return false;
        }
        break;
      default:
        LOG(fatal) << "Invalid TOF selection for positive daughter";
        break;
    }
    switch (tofSelectionNeg) {
      case -1:
        if (ntrack.hasTOF()) {
          return false;
        }
        break;
      case 0:
        break;
      case 1:
        if (!ntrack.hasTOF()) {
          return false;
        }
        break;
      default:
        LOG(fatal) << "Invalid TOF selection for negative daughter";
        break;
    }

    // TRD selection
    switch (trdSelectionPos) {
      case -1:
        if (ptrack.hasTRD()) {
          return false;
        }
        break;
      case 0:
        break;
      case 1:
        if (!ptrack.hasTRD()) {
          return false;
        }
        break;
      default:
        LOG(fatal) << "Invalid TRD selection for positive daughter";
        break;
    }
    switch (trdSelectionNeg) {
      case -1:
        if (ntrack.hasTRD()) {
          return false;
        }
        break;
      case 0:
        break;
      case 1:
        if (!ntrack.hasTRD()) {
          return false;
        }
        break;
      default:
        LOG(fatal) << "Invalid TRD selection for negative daughter";
        break;
    }

    // PID hypothesis selection
    switch (pidHypoPos) {
      case -1:
        break;
      case 0:
      case 1:
      case 2:
      case 3:
      case 4:
        if (ptrack.pidForTracking() != static_cast<uint32_t>(pidHypoPos)) {
          return false;
        }
        break;
      default:
        LOG(fatal) << "Invalid PID selection for positive daughter";
        break;
    }
    switch (pidHypoNeg) {
      case -1:
        break;
      case 0:
      case 1:
      case 2:
      case 3:
      case 4:
        if (ntrack.pidForTracking() != static_cast<uint32_t>(pidHypoNeg)) {
          return false;
        }
        break;
      default:
        LOG(fatal) << "Invalid PID selection for negative daughter";
        break;
    }
    return true;
  }

  // Filters on V0s
  Filter v0Filter = (nabs(aod::v0data::dcapostopv) > v0setting_dcapostopv &&
                     nabs(aod::v0data::dcanegtopv) > v0setting_dcanegtopv &&
                     aod::v0data::dcaV0daughters < v0setting_dcav0dau &&
                     aod::v0data::v0cosPA > v0setting_cospa);

  // Event selection
  Filter eventFilter = (eventSelection && o2::aod::evsel::sel8 == true);
  Filter posZFilter = (nabs(o2::aod::collision::posZ) < cutzvertex);

  void processData(soa::Filtered<SelectedCollisions>::iterator const& collision,
                   soa::Filtered<aod::V0Datas> const& fullV0s,
                   PIDTracks const& tracks)
  {
    rK0sResolution.fill(HIST("h1_events"), 0.5);
    for (auto& v0 : fullV0s) {
      rK0sResolution.fill(HIST("h1_events"), 1.5);
      const auto& posTrack = v0.posTrack_as<PIDTracks>();
      const auto& negTrack = v0.negTrack_as<PIDTracks>();
      if (!acceptV0(v0, negTrack, posTrack, collision))
        continue;

      float mass = v0.mK0Short();
      if (computeInvMassFromDaughters) {
        mass = RecoDecay::m(std::array{std::array{posTrack.px(), posTrack.py(), posTrack.pz()},
                                       std::array{negTrack.px(), negTrack.py(), negTrack.pz()}},
                            std::array{o2::constants::physics::MassPionCharged, o2::constants::physics::MassPionCharged});
      }

      rK0sResolution.fill(HIST("h2_masspT"), mass, v0.pt());
      rK0sResolution.fill(HIST("h2_masseta"), mass, v0.eta());
      rK0sResolution.fill(HIST("h2_massphi"), mass, v0.phi());
      if (useMultidimHisto) {
        rK0sResolution.fill(HIST("thn_mass"), mass, v0.pt(), v0.eta(), v0.phi(), posTrack.eta(), negTrack.eta());
      }
      if (enableTPCPlot) {
        rK0sDauResolution.fill(HIST("h3_tpc_vs_pid_hypothesis"), posTrack.tpcInnerParam(), posTrack.tpcSignal(), posTrack.pidForTracking());
        rK0sDauResolution.fill(HIST("h3_tpc_vs_pid_hypothesis"), -negTrack.tpcInnerParam(), negTrack.tpcSignal(), negTrack.pidForTracking());
      }
    }
  }
  PROCESS_SWITCH(perfK0sResolution, processData, "Process data", true);

  void processMC(soa::Filtered<SelectedCollisions>::iterator const& collision, soa::Filtered<soa::Join<aod::V0Datas, aod::McV0Labels>> const& fullV0s,
                 soa::Join<PIDTracks, aod::McTrackLabels> const& tracks, aod::McParticles const&)
  {
    rK0sResolution.fill(HIST("h1_events"), 0.5);
    for (auto& v0 : fullV0s) {
      rK0sResolution.fill(HIST("h1_events"), 1.5);
      const auto& posTrack = v0.posTrack_as<soa::Join<PIDTracks, aod::McTrackLabels>>();
      const auto& negTrack = v0.negTrack_as<soa::Join<PIDTracks, aod::McTrackLabels>>();
      if (!acceptV0(v0, negTrack, posTrack, collision))
        continue;
      if (!posTrack.has_mcParticle()) {
        continue;
      }
      if (!negTrack.has_mcParticle()) {
        continue;
      }
      if (posTrack.mcParticle().pdgCode() != 211 || negTrack.mcParticle().pdgCode() != -211) {
        continue;
      }
      float mass = v0.mK0Short();
      if (computeInvMassFromDaughters) {
        mass = RecoDecay::m(std::array{std::array{posTrack.px(), posTrack.py(), posTrack.pz()},
                                       std::array{negTrack.px(), negTrack.py(), negTrack.pz()}},
                            std::array{o2::constants::physics::MassPionCharged, o2::constants::physics::MassPionCharged});
      }
      const bool isTrueK0s = (v0.has_mcParticle() && v0.mcParticle().pdgCode() == 310);
      rK0sDauResolution.fill(HIST("h2_genPtPosPtRes"), (v0.positivept() - posTrack.mcParticle().pt()) / posTrack.mcParticle().pt(), posTrack.mcParticle().pt());
      rK0sDauResolution.fill(HIST("h2_genPxPosPxRes"), (v0.pxpos() - posTrack.mcParticle().px()) / posTrack.mcParticle().px(), posTrack.mcParticle().px());
      rK0sDauResolution.fill(HIST("h2_genPyPosPyRes"), (v0.pypos() - posTrack.mcParticle().py()) / posTrack.mcParticle().py(), posTrack.mcParticle().py());
      rK0sDauResolution.fill(HIST("h2_genPzPosPzRes"), (v0.pzpos() - posTrack.mcParticle().pz()) / posTrack.mcParticle().pz(), posTrack.mcParticle().pz());

      rK0sDauResolution.fill(HIST("h2_genPtNegPtRes"), (v0.negativept() - negTrack.mcParticle().pt()) / negTrack.mcParticle().pt(), negTrack.mcParticle().pt());
      rK0sDauResolution.fill(HIST("h2_genPxNegPxRes"), (v0.pxneg() - negTrack.mcParticle().px()) / negTrack.mcParticle().px(), negTrack.mcParticle().px());
      rK0sDauResolution.fill(HIST("h2_genPyNegPyRes"), (v0.pyneg() - negTrack.mcParticle().py()) / negTrack.mcParticle().py(), negTrack.mcParticle().py());
      rK0sDauResolution.fill(HIST("h2_genPzNegPzRes"), (v0.pzneg() - negTrack.mcParticle().pz()) / negTrack.mcParticle().pz(), negTrack.mcParticle().pz());

      rK0sDauResolution.fill(HIST("h2_massPosPtRes"), mass, v0.positivept() - posTrack.mcParticle().pt());
      rK0sDauResolution.fill(HIST("h2_massNegPtRes"), mass, v0.negativept() - negTrack.mcParticle().pt());
      rK0sResolution.fill(HIST("h2_masspT"), mass, v0.pt());
      rK0sResolution.fill(HIST("h2_masseta"), mass, v0.eta());
      rK0sResolution.fill(HIST("h2_massphi"), mass, v0.phi());
      if (useMultidimHisto) {
        rK0sResolution.fill(HIST("thn_mass"), mass, v0.pt(), v0.eta(), v0.phi(), posTrack.eta(), negTrack.eta(),
                            1. / v0.positivept() - 1. / posTrack.mcParticle().pt(),
                            1. / v0.negativept() - 1. / negTrack.mcParticle().pt(),
                            isTrueK0s);
      }
    }
  }
  PROCESS_SWITCH(perfK0sResolution, processMC, "Process MC", false);
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{adaptAnalysisTask<perfK0sResolution>(cfgc)};
}
