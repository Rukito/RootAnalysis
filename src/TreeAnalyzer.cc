#include <sstream>
#include <fstream>
#include <iterator>
#include <string>
#include <omp.h>
#include "TreeAnalyzer.h"
//#include "SummaryAnalyzer.h"
#include "ObjectMessenger.h"
#include "EventProxyBase.h"

#include "boost/functional/hash.hpp"

#include "EventProxyBase.h"
#include "AnalysisHistograms.h"
#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/ini_parser.hpp"
#include "boost/tokenizer.hpp"

#include "TFile.h"
#include "TH1D.h"
#include "TProofOutputFile.h"
#include "TTree.h"
#include "TChain.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
TreeAnalyzer::TreeAnalyzer(const std::string & aName,
				       const std::string & cfgFileName,
				       EventProxyBase *aProxy,
				       TProofOutputFile * proofFile):Analyzer(aName){

  ///Analysis control
  nEventsToAnalyze_ = 0;

  cfgFileName_ = cfgFileName;

  parseCfg(cfgFileName_);

  if(proofFile){   
    proofFile->SetOutputFileName((filePath_+"/RootAnalysis_"+sampleName_+".root").c_str());
    store_ = new TFileService(proofFile->OpenFile("RECREATE"));
  }
  else{ 
    // Create histogram store
    store_ = new TFileService((filePath_+"/RootAnalysis_"+sampleName_+".root").c_str());
  }

  ///Histogram with processing statistics. Necessary for the PROOF based analysis
  hStats_ = store_->mkdir("Statistics").make<TH1D>("hStats","Various statistics",21,-0.5,20.5);
  hStats_->GetXaxis()->SetBinLabel(1,"Xsection");
  hStats_->GetXaxis()->SetBinLabel(2,"external scaling factor");
  hStats_->GetXaxis()->SetBinLabel(3,"number of runs");
  hStats_->GetXaxis()->SetBinLabel(4,"number of parrarel nodes");
  hStats_->GetXaxis()->SetBinLabel(5,"number of event analyzed");
  hStats_->GetXaxis()->SetBinLabel(6,"number of event skipped");
  hStats_->GetXaxis()->SetBinLabel(7,"generator preselection eff.");
  hStats_->GetXaxis()->SetBinLabel(8,"number of events processed at RECO/AOD");
  hStats_->GetXaxis()->SetBinLabel(9,"number of events saved from RECO/AOD");
  hStats_->GetXaxis()->SetBinLabel(10,"reco preselection eff.");

  myStrSelections_ = new pat::strbitset(); 

  myObjMessenger_ = new ObjectMessenger("ObjMessenger");

  myProxy_ = aProxy;

}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
TreeAnalyzer::~TreeAnalyzer(){

  std::cout<<"TreeAnalyzer::~TreeAnalyzer() Begin"<<std::endl;

  //delete mySummary_;
  delete store_;

  std::cout<<"TreeAnalyzer::~TreeAnalyzer() Done"<<std::endl;
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void TreeAnalyzer::scaleHistograms(){

  float weight = 1.0;

  for(unsigned int i=0;i<myAnalyzers_.size();++i){
    std::string name = myAnalyzers_[i]->name();  
    TDirectoryFile* summary = (TDirectoryFile*)store_->file().Get(name.c_str());
    if(!summary){
      std::cout<<"Histogram directory for analyzer: "<<name.c_str()
	       <<" not found!"<<std::endl;
      continue;
    }
    TList *list = summary->GetList();
    TIter next(list);
    TObject *obj = 0;
    while ((obj = next())){
      if(obj->IsA()->InheritsFrom("TH1")){ 
	TH1 *h = (TH1*)summary->Get(obj->GetName());
	h->Scale(weight);
      }
      if(obj->IsA()->InheritsFrom("TDirectory")){ 
	TDirectory* aDir = (TDirectory*)summary->Get(obj->GetName());
	TList *listSubDir = aDir->GetList();
	TIter next2(listSubDir);
	TObject *obj2 = 0;
	while ((obj2 = next2())){
	  if(obj2->IsA()->InheritsFrom("TH1")){ 
	    TH1 *h1 = (TH1*)aDir->Get(obj2->GetName());
	    h1->Scale(weight);
	  }
	}
      }
    }
  }
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void TreeAnalyzer::parseCfg(const std::string & cfgFileName){

  eventWeight_ = 1.0;

  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini(cfgFileName, pt);

  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  boost::char_separator<char> sep(",");
  std::string str = pt.get<std::string>("TreeAnalyzer.inputFile");
  tokenizer tokens(str, sep);
  std::cout<<"Reading files: "<<std::endl;
  for (auto it: tokens){
    std::cout<<it<<std::endl;
    fileNames_.push_back(it);
  }
  
  filePath_ = pt.get<std::string>("TreeAnalyzer.outputPath");
  
  nEventsToAnalyze_ = pt.get("TreeAnalyzer.eventsToAnalyze",-1);
  nEventsToPrint_ = pt.get("TreeAnalyzer.eventsToPrint",100);
  nThreads = pt.get("TreeAnalyzer.threads",1);
  omp_set_num_threads(nThreads);  
  
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void  TreeAnalyzer::init(std::vector<Analyzer*> myAnalyzers){

  myProxy_->init(fileNames_);
  myAnalyzers_ = myAnalyzers;

  for(unsigned int i=0;i<myAnalyzers_.size();++i){ 
    myDirectories_.push_back(store_->mkdir(myAnalyzers_[i]->name()));
    myAnalyzers_[i]->initialize(myDirectories_[myDirectories_.size()-1],
				myStrSelections_);
  }

 for(unsigned int iThread=0;iThread<omp_get_max_threads();++iThread){
   myProxiesThread_[iThread] = myProxy_->clone();
   myProxiesThread_[iThread]->init(fileNames_);
    for(unsigned int iAnalyzer=0;iAnalyzer<myAnalyzers_.size();++iAnalyzer){ 
      myAnalyzersThreads_[iThread].push_back(myAnalyzers_[iAnalyzer]->clone());
    }
  }
  
/*
 for(unsigned int i=0;i<myAnalyzers_.size();++i){
   myAnalyzers_[i]->addBranch(mySummary_->getTree());  
   myAnalyzers_[i]->addCutHistos(mySummary_->getHistoList());  
 }
 */
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void  TreeAnalyzer::finalize(){
  for(unsigned int i=0;i<myAnalyzers_.size();++i) myAnalyzers_[i]->finalize();
}
//////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
int TreeAnalyzer::loop(){

  std::cout<<"Events total: "<<myProxy_->size()<<std::endl;
  TH1::AddDirectory(kFALSE);
  nEventsAnalyzed_ = 0;
  nEventsSkipped_ = 0;
  if(nEventsToAnalyze_<0 || nEventsToAnalyze_>myProxy_->size()) nEventsToAnalyze_ = myProxy_->size();
  int eventPreviouslyPrinted=-1;
  ///////
  
#pragma omp parallel for schedule(dynamic)
  for(int aEvent=0;aEvent<nEventsToAnalyze_;++aEvent){

    if( nEventsAnalyzed_ < nEventsToPrint_ || nEventsAnalyzed_%100000==0)
      std::cout<<"Events analyzed: "<<nEventsAnalyzed_<<" thread: "<<omp_get_thread_num()<<std::endl;
    analyze(myProxiesThread_[omp_get_thread_num()]->toN(aEvent));
  }
  
  return nEventsAnalyzed_;    
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
bool TreeAnalyzer::analyze(const EventProxyBase& iEvent){

 //clear();
    ///////
    for(unsigned int i=0;i<myAnalyzers_.size();++i){
      ///If analyzer returns false, skip to the last one, the Summary, unless filtering is disabled for this analyzer.
      //myAnalyzers_[i]->analyze(iEvent,myObjMessenger_);
      //myAnalyzersThreads_[omp_get_thread_num()][i]->analyze(iEvent,myObjMessenger_);
      //if(!myAnalyzers_[i]->analyze(iEvent,myObjMessenger_) && myAnalyzers_[i]->filter() && myAnalyzers_.size()>1) i = myAnalyzers_.size()-2;
    }
    ///Clear all the analyzers, even if it was not called in this event.
    ///Important for proper TTree filling.
  // for(unsigned int i=0;i<myAnalyzers_.size();++i) myAnalyzers_[i]->clear(); 
        
  // myObjMessenger_->clear();

    #pragma omp atomic
    ++nEventsAnalyzed_;
  
  return 1;
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void TreeAnalyzer::clear(){ myStrSelections_->set(false); }
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
