from Gaudi.Configuration import *

# Data service
from Configurables import FCCDataSvc
batch_dir = "/eos/user/t/toprice/private/FCC/FCCSW/v8.0/XYZ/"
fccsw_version = "FCCSW0.8"
det_config = "<DETCONFIG>"
run_config = "<RUNCONFIG>"
file = "<FILE>"
#inputfile = batch_dir+"/"+det_config+"_"+fccsw_version+"/"+run_config+"/"+file
inputfile = batch_dir+"/"+det_config+"/"+run_config+"/"+file
podiosvc   = FCCDataSvc("EventDataSvc", input=inputfile)


from Configurables import PodioInput
podioinput = PodioInput("PodioReader", collections=["positionedCaloHits"], OutputLevel=DEBUG)

# DD4hep geometry service
from Configurables import GeoSvc
geoservice = GeoSvc("GeoSvc", detectors=[ 'file:/afs/cern.ch/user/t/toprice/private/FCC/FCCSW/Detector/DetFCChhBaseline1/compact/FCChh_DectEmptyMaster.xml',
                                          'file:/afs/cern.ch/user/t/toprice/private/FCC/FCCSW/Detector/DetFCChhECalDigital/compact/FCChh_DECalBarrel_'+det_config[:-9]+'.xml'
],
                    OutputLevel = INFO)



from Configurables import DECalLongitudinalTest
hist = DECalLongitudinalTest("hists",   
                                 readoutName = "BarDECal_Readout",
                                 layerFieldName = "layer",
                                 numLayers = 50, # one more because index starts at 1 - layer 0 will be always empty
                                 OutputLevel = DEBUG)
hist.deposits.Path="positionedCaloHits"

#THistSvc().Output = ["rec DATAFILE='"+batch_dir+"/"+det_config+"_"+fccsw_version+"/"+run_config+"/<OUTPUT>' TYP='ROOT' OPT='RECREATE'"]
THistSvc().Output = ["rec DATAFILE='"+batch_dir+"/"+det_config+"/"+run_config+"/<OUTPUT>' TYP='ROOT' OPT='RECREATE'"]
THistSvc().PrintAll=True
THistSvc().AutoSave=True
THistSvc().AutoFlush=False
THistSvc().OutputLevel=INFO

#CPU information
from Configurables import AuditorSvc, ChronoAuditor
chra = ChronoAuditor()
audsvc = AuditorSvc()
audsvc.Auditors = [chra]
hist.AuditExecute = True

# ApplicationMgr
from Configurables import ApplicationMgr
ApplicationMgr( TopAlg = [podioinput, hist],
                EvtSel = 'NONE',
#                EvtMax = 10,
                # order is important, as GeoSvc is needed by G4SimSvc
                ExtSvc = [podiosvc,geoservice, audsvc],
                OutputLevel = DEBUG
)
